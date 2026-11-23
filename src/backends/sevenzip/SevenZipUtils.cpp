// QArchiver / 小Q解压
// SevenZipUtils implementation for listfiles and legacy filename renames.
// SevenZipUtils 的列表文件与旧文件名重命名实现。

#include "backends/sevenzip/SevenZipUtils.h"

#include <QDir>
#include <QFile>
#include <QThread>
#include <QUuid>

#include "utils/EncodingDetector.h"
#include "utils/Logger.h"
#include "utils/ProcessRunner.h"

#ifndef Q_OS_WIN
#include <dirent.h>
#include <cstdio>
#include <sys/stat.h>
#endif

namespace SevenZipUtils {

ListFile::ListFile(const QString &tag)
{
    m_path = QDir(QDir::tempPath()).filePath(
        QStringLiteral("qarchiver-%1-%2.txt")
            .arg(tag,
                 QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

ListFile::~ListFile()
{
    if (!m_path.isEmpty()) {
        QFile::remove(m_path);
    }
}

bool ListFile::write(const QByteArray &payload)
{
    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    const bool written = file.write(payload) == payload.size();
    file.flush();
    file.close();
    if (!written) {
        return false;
    }

#ifdef Q_OS_WIN
    // Antivirus scanners and the search indexer can hold a newly created file
    // for a moment.  7-Zip then fails with "the file is being used by another
    // process" while opening the listfile.  Wait until the file can be opened
    // for reading before starting 7-Zip.
    // 杀毒软件或搜索索引可能短暂占用刚创建的列表文件，7-Zip 读取时会报
    // “另一个程序正在使用此文件”。启动 7-Zip 前先确认文件可读。
    for (int attempt = 0; attempt < 20; ++attempt) {
        QFile probe(m_path);
        if (probe.open(QIODevice::ReadOnly)) {
            probe.close();
            break;
        }
        QThread::msleep(25);
    }
#endif
    return true;
}

QString resolveSevenZip()
{
    return ProcessRunner::resolveToolPath(QStringLiteral("7z"));
}

bool appendListFileArgs(ListFile &file, const QStringList &entries,
                        const QString &encoding, QStringList &args)
{
    if (entries.isEmpty()) {
        return true;
    }

    QString charset = QStringLiteral("-scsUTF-8");
    const QString payloadText = entries.join(QLatin1Char('\n')) + QLatin1Char('\n');
    QByteArray payload = EncodingDetector::encode(payloadText, encoding);
    if (payload.isEmpty()) {
        // The legacy encoding cannot represent every name; fall back to UTF-8
        // instead of failing the whole operation.
        // 旧编码无法表示所有名称；回退到 UTF-8，而不是让整个操作失败。
        payload = payloadText.toUtf8();
    } else {
        const int codePage = EncodingDetector::codePageId(encoding);
        if (codePage > 0) {
            charset = QStringLiteral("-scs%1").arg(codePage);
        }
    }

    if (!file.write(payload)) {
        return false;
    }

    args << charset << QStringLiteral("-i@") + file.path();
    return true;
}

void renameLegacyExtractedTree(const QByteArray &dirBytes, const QString &encoding)
{
#ifndef Q_OS_WIN
    DIR *dir = opendir(dirBytes.constData());
    if (!dir) {
        return;
    }

    QList<QByteArray> names;
    while (dirent *entry = readdir(dir)) {
        const QByteArray name(entry->d_name);
        if (name != "." && name != "..") {
            names << name;
        }
    }
    closedir(dir);

    for (const QByteArray &name : names) {
        const QByteArray source = dirBytes + '/' + name;

        struct stat st {};
        if (lstat(source.constData(), &st) == 0 && S_ISDIR(st.st_mode)) {
            renameLegacyExtractedTree(source, encoding);
        }

        if (QString::fromUtf8(name).toUtf8() == name) {
            continue; // already valid UTF-8
        }

        const QString decoded = EncodingDetector::decode(name, encoding);
        if (decoded.isEmpty()) {
            continue;
        }
        const QByteArray target = dirBytes + '/' + decoded.toUtf8();
        if (target == source) {
            continue;
        }
        if (::rename(source.constData(), target.constData()) != 0) {
            LOG_WARN("SevenZipUtils", "Could not rename legacy encoded path: %s",
                     source.constData());
        }
    }
#else
    Q_UNUSED(dirBytes);
    Q_UNUSED(encoding);
#endif
}

} // namespace SevenZipUtils
