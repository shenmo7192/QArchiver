// QArchiver / 小Q解压
// Compressed output staging: pattern matching, atomic promotion and cleanup.
// 压缩输出暂存：文件名匹配、原子提升与清理。

#include "core/CompressOutput.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <algorithm>

namespace CompressOutput {

QRegularExpression outputPattern(const QFileInfo &finalInfo, ArchiveFormat format)
{
    // RAR split archives are named archive.partN.rar instead of
    // archive.rar.001, so replacing an existing set must remove those siblings
    // as well.
    // RAR 分卷命名为 archive.partN.rar 而不是 archive.rar.001，
    // 因此替换已有分卷集时必须一并清理这些文件。
    if (isRarFamily(format)) {
        const QString stem =
            QRegularExpression::escape(finalInfo.completeBaseName());
        return QRegularExpression(
            QStringLiteral("^%1(?:\\.part\\d+\\.rar|\\.rar|\\.r\\d{2,}|\\.\\d{3,})$")
                .arg(stem),
            QRegularExpression::CaseInsensitiveOption);
    }

    return QRegularExpression(
        QStringLiteral("^%1(?:\\.\\d{3,})?$")
            .arg(QRegularExpression::escape(finalInfo.fileName())),
        QRegularExpression::CaseInsensitiveOption);
}

bool outputExists(const QFileInfo &finalInfo, ArchiveFormat format)
{
    const QRegularExpression pattern = outputPattern(finalInfo, format);
    const QDir dir(finalInfo.absolutePath());
    const QStringList files = dir.entryList(QDir::Files);
    return std::any_of(files.cbegin(), files.cend(),
                       [&pattern](const QString &name) {
                           return pattern.match(name).hasMatch();
                       });
}

bool promote(const QFileInfo &finalInfo, ArchiveFormat format,
             const QString &tempPath, bool targetExisted)
{
    QDir finalDir(finalInfo.absolutePath());
    QDir tempDir(tempPath);

    const QRegularExpression targetRe = outputPattern(finalInfo, format);
    const QStringList produced = tempDir.entryList(QDir::Files);
    if (produced.isEmpty()) {
        return false;
    }

    // Stage the old archive set aside instead of deleting it.  Only after all
    // new files are in place is the backup dropped; on any failure the old
    // files are restored and the partial new output removed.
    // 将旧压缩包集移入备份目录而不是直接删除。仅在全部新文件就位后才丢弃
    // 备份；任何失败都会恢复旧文件并删除不完整的新输出。
    QTemporaryDir backupDir(
        finalDir.filePath(QStringLiteral(".qarchiver_backup_XXXXXX")));
    if (!backupDir.isValid()) {
        return false;
    }

    QStringList backedUp;
    QStringList installed;
    auto rollback = [&]() {
        for (const QString &name : installed) {
            QFile::remove(finalDir.filePath(name));
        }
        for (const QString &name : backedUp) {
            const QString source = QDir(backupDir.path()).filePath(name);
            const QString destination = finalDir.filePath(name);
            if (QFileInfo::exists(source) && !QFileInfo::exists(destination)) {
                if (!QFile::rename(source, destination)) {
                    QFile::copy(source, destination);
                }
            }
        }
    };

    if (targetExisted) {
        const QStringList existing = finalDir.entryList(QDir::Files);
        for (const QString &name : existing) {
            if (!targetRe.match(name).hasMatch()) {
                continue;
            }
            const QString source = finalDir.filePath(name);
            const QString destination = QDir(backupDir.path()).filePath(name);
            if (!QFile::rename(source, destination)) {
                rollback();
                return false;
            }
            backedUp << name;
        }
    }

    for (const QString &name : produced) {
        const QString source = tempDir.filePath(name);
        const QString destination = finalDir.filePath(name);
        if (QFileInfo::exists(destination)) {
            rollback();
            return false;
        }
        if (!QFile::rename(source, destination)) {
            if (!QFile::copy(source, destination)) {
                rollback();
                return false;
            }
            QFile::remove(source);
        }
        installed << name;
    }

    // Success: the QTemporaryDir destructor removes the old backup set.
    return true;
}

void removeOutput(const QFileInfo &finalInfo, ArchiveFormat format)
{
    QDir finalDir(finalInfo.absolutePath());
    const QRegularExpression targetRe = outputPattern(finalInfo, format);

    const QStringList existing = finalDir.entryList(QDir::Files);
    for (const QString &name : existing) {
        if (targetRe.match(name).hasMatch()) {
            QFile::remove(finalDir.filePath(name));
        }
    }
}

} // namespace CompressOutput
