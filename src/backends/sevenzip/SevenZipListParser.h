#ifndef SEVENZIPLISTPARSER_H
#define SEVENZIPLISTPARSER_H

// QArchiver / 小Q解压
// Parser for the machine-readable "7z l -slt" output.
// “7z l -slt”机器可读输出的解析器。

#include <QList>
#include <QString>

#include "core/ArchiveEntry.h"

// Parses 7-Zip `l -slt` (technical) output.  The same key/value format is
// emitted for 7z, zip, tar, rar and the other formats handled by 7-Zip.
// 解析 7-Zip `l -slt`（技术模式）输出。7z、zip、tar、rar 以及
// 7-Zip 支持的其他格式都会输出相同的键值格式。
class SevenZipListParser {
public:
    static QList<ArchiveEntry> parse(const QString &output);
};

#endif // SEVENZIPLISTPARSER_H
