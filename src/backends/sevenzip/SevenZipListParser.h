#ifndef SEVENZIPLISTPARSER_H
#define SEVENZIPLISTPARSER_H

// QArchiver / 小Q解压
// Parser for the machine-readable "7z l -slt" output.
// “7z l -slt”机器可读输出的解析器。

#include <QList>
#include <QString>

#include "common/ArchiveEntry.h"

// Parses 7-Zip `l -slt` (technical) output.  The same key/value format is
// emitted for 7z, zip, tar, rar and the other formats handled by 7-Zip.
// 解析 7-Zip `l -slt`（技术模式）输出。7z、zip、tar、rar 以及
// 7-Zip 支持的其他格式都会输出相同的键值格式。
class SevenZipListParser {
public:
    SevenZipListParser() = default;

    // Feed one complete output line (without the trailing newline).  Callers
    // can stream the process output instead of retaining it in memory.
    // 输入一行完整输出（不含行尾换行）。调用方可流式处理进程输出，
    // 无需在内存中保留完整列表。
    void feedLine(const QString &line);

    // Commit the current block and return all parsed entries.
    // 提交当前块并返回所有已解析条目。
    QList<ArchiveEntry> takeEntries();

    static QList<ArchiveEntry> parse(const QString &output);

private:
    void commitEntry();

    QList<ArchiveEntry> m_entries;
    ArchiveEntry m_current;
    bool m_currentIsEntry = false;
};

#endif // SEVENZIPLISTPARSER_H
