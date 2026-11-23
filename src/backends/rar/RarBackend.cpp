// QArchiver / 小Q解压
// RarBackend construction and optional archive creation via the system rar.
// RarBackend 构造，以及通过系统 rar 创建压缩包的可选功能。

#include "backends/rar/RarBackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryFile>

#include "utils/Logger.h"
#include "utils/ProcessRunner.h"

namespace {

// The UI exposes 0-9 like 7-Zip; RAR only understands 0-5.  Map 0->0,
// 6 (the default) -> 3 (normal) and 9 -> 5 (best).
// 界面沿用 7-Zip 的 0-9 等级；RAR 只支持 0-5。
// 映射关系：0->0、默认 6->3（normal）、9->5（best）。
int toRarLevel(int level)
{
    return qBound(0, (qBound(0, level, 9) * 5 + 4) / 9, 5);
}

// Remove archive.partN.rar files produced by a failed/aborted split archive.
// 删除失败或取消的分卷压缩产生的 archive.partN.rar 文件。
void removePartialVolumes(const QString &archivePath)
{
    QFile::remove(archivePath);

    const QFileInfo info(archivePath);
    if (info.fileName().isEmpty()) {
        return;
    }
    const QDir dir(info.absolutePath());
    const QRegularExpression volumeRe(
        QStringLiteral("^%1\\.part\\d+\\.rar$")
            .arg(QRegularExpression::escape(info.completeBaseName())),
        QRegularExpression::CaseInsensitiveOption);
    const QStringList files = dir.entryList(QDir::Files);
    for (const QString &name : files) {
        if (volumeRe.match(name).hasMatch()) {
            QFile::remove(dir.filePath(name));
        }
    }
}

} // namespace

// RAR reading reuses the 7-Zip implementation; creation needs system rar.
// RAR 读取复用 7-Zip 实现；创建需要系统 rar。
RarBackend::RarBackend(ArchiveFormat fmt, QObject *parent)
    : SevenZipBackend(fmt, parent)
{
}

bool RarBackend::isCreateAvailable()
{
    return !createToolPath().isEmpty();
}

QString RarBackend::createToolPath()
{
    return ProcessRunner::resolveToolPath(QStringLiteral("rar"));
}

QString RarBackend::rarPath() const
{
    return createToolPath();
}

void RarBackend::compress(const CompressOptions &options)
{
    const QString rar = rarPath();
    if (rar.isEmpty()) {
        emit errorOccurred(
            ArchiveErrorCode::ToolNotFound,
            tr("The 'rar' command was not found. Install rar to create RAR archives."));
        return;
    }
    if (options.archivePath.trimmed().isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive name specified"));
        return;
    }
    if (options.sourceFiles.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No source files selected"));
        return;
    }

    QStringList args;
    args << QStringLiteral("a")
         << QStringLiteral("-y")
         << QStringLiteral("-r")
         << QStringLiteral("-idq")
         << (m_format == ArchiveFormat::Rar4 ? QStringLiteral("-ma4")
                                             : QStringLiteral("-ma5"))
         << QStringLiteral("-m%1").arg(toRarLevel(options.compressionLevel));

    if (options.solidMode) {
        args << QStringLiteral("-s");
    }
    if (!options.password.isEmpty()) {
        // -hp encrypts headers as well and already includes the password.
        // -hp 同时加密文件头，本身已包含密码参数。
        args << (options.encryptHeaders ? QStringLiteral("-hp")
                                        : QStringLiteral("-p"))
                    + options.password;
    }
    if (options.volumeSize > 0) {
        args << QStringLiteral("-v%1b").arg(options.volumeSize);
    }
    // RAR follows symlinks by default; -ol stores them as links.
    // RAR 默认跟随符号链接；-ol 将符号链接按链接本身保存。
    if (!options.followSymlinks) {
        args << QStringLiteral("-ol");
    }
    if (options.storeFullPath) {
        args << QStringLiteral("-ep2");
    } else {
        // Keep the same member layout as the 7z backend: store the selected
        // file/dir name without its absolute parent directory.
        // 与 7z 后端保持相同的成员布局：保存所选文件/目录名，
        // 不带绝对父目录。
        args << QStringLiteral("-ep1");
    }
    for (const QString &pattern : options.excludePatterns) {
        if (!pattern.trimmed().isEmpty()) {
            args << QStringLiteral("-x") + pattern.trimmed();
        }
    }

    // RAR reads UTF-8 listfiles with -scfl; absolute paths keep the current
    // working directory irrelevant.  A listfile also avoids command-line
    // wildcard and leading-dash surprises.
    // RAR 使用 -scfl 读取 UTF-8 列表文件；绝对路径使当前工作目录无关紧要。
    // 列表文件还能避免命令行通配符和前导短横线被误解析。
    QTemporaryFile sourceListFile(
        QDir::tempPath() + QStringLiteral("/qarchiver-rar-sources-XXXXXX.txt"));
    if (!sourceListFile.open()) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Could not prepare the source files for compression"));
        return;
    }
    QByteArray payload;
    for (const QString &source : options.sourceFiles) {
        if (!source.isEmpty()) {
            payload += source.toUtf8();
            payload += '\n';
        }
    }
    if (payload.isEmpty() || sourceListFile.write(payload) != payload.size()) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Could not prepare the source files for compression"));
        return;
    }
    sourceListFile.flush();
    sourceListFile.close();

    args << QStringLiteral("-scfl")
         << QStringLiteral("--")
         << options.archivePath
         << QStringLiteral("@") + sourceListFile.fileName();

    LOG_DEBUG("RarBackend", "Compress: %s",
              ProcessRunner::maskedCommand(rar, args).toUtf8().constData());

    const ProcessResult result = ProcessRunner::run(
        rar, args, QString(), QProcessEnvironment(),
        QString(), 0, &m_cancelRequested);

    if (result.cancelled) {
        removePartialVolumes(options.archivePath);
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.started) {
        removePartialVolumes(options.archivePath);
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start rar"));
        return;
    }
    if (!result.success()) {
        removePartialVolumes(options.archivePath);
        const QString errorText = result.stdErr.trimmed().isEmpty()
            ? result.stdOut.trimmed()
            : result.stdErr.trimmed();
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Compression failed: %1").arg(errorText));
        return;
    }

    emit progressChanged(100, 1, 1, -1);
    emit operationFinished(true, tr("Archive created: %1").arg(options.archivePath));
}
