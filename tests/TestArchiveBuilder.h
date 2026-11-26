#ifndef QARCHIVER_TESTS_TESTARCHIVEBUILDER_H
#define QARCHIVER_TESTS_TESTARCHIVEBUILDER_H

// QArchiver / 小Q解压
// Byte-exact archive builders shared by the backend and external-edit tests.
// 后端测试与外部编辑测试共用的“按字节精确构造”压缩包辅助函数。

#include <QByteArray>

namespace TestArchiveBuilder {

inline void appendLe16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>(value & 0xff));
    out.append(static_cast<char>((value >> 8) & 0xff));
}

inline void appendLe32(QByteArray &out, quint32 value)
{
    out.append(static_cast<char>(value & 0xff));
    out.append(static_cast<char>((value >> 8) & 0xff));
    out.append(static_cast<char>((value >> 16) & 0xff));
    out.append(static_cast<char>((value >> 24) & 0xff));
}

inline quint32 crc32(const QByteArray &data)
{
    quint32 crc = 0xffffffffu;
    for (const char ch : data) {
        crc ^= static_cast<unsigned char>(ch);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

// Minimal stored ZIP with the filename bytes stored verbatim (no UTF-8 flag).
// Used to exercise legacy GBK member names and to replace a member in place.
// 最小存储型 ZIP，文件名字节原样保存（无 UTF-8 标志）。
// 用于验证旧式 GBK 成员名以及就地替换成员。
inline QByteArray makeStoredZip(const QByteArray &nameBytes,
                                const QByteArray &data)
{
    const quint32 crc = crc32(data);
    const quint16 nameLength = static_cast<quint16>(nameBytes.size());
    const quint32 size = static_cast<quint32>(data.size());

    QByteArray local;
    appendLe32(local, 0x04034b50u);
    appendLe16(local, 20);
    appendLe16(local, 0);
    appendLe16(local, 0);
    appendLe16(local, 0);
    appendLe16(local, 0);
    appendLe32(local, crc);
    appendLe32(local, size);
    appendLe32(local, size);
    appendLe16(local, nameLength);
    appendLe16(local, 0);
    local.append(nameBytes);
    local.append(data);

    QByteArray central;
    appendLe32(central, 0x02014b50u);
    appendLe16(central, 20);
    appendLe16(central, 20);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe32(central, crc);
    appendLe32(central, size);
    appendLe32(central, size);
    appendLe16(central, nameLength);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe32(central, 0);
    appendLe32(central, 0);
    central.append(nameBytes);

    QByteArray eocd;
    appendLe32(eocd, 0x06054b50u);
    appendLe16(eocd, 0);
    appendLe16(eocd, 0);
    appendLe16(eocd, 1);
    appendLe16(eocd, 1);
    appendLe32(eocd, static_cast<quint32>(central.size()));
    appendLe32(eocd, static_cast<quint32>(local.size()));
    appendLe16(eocd, 0);

    return local + central + eocd;
}

} // namespace TestArchiveBuilder

#endif // QARCHIVER_TESTS_TESTARCHIVEBUILDER_H
