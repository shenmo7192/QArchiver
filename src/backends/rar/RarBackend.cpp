// QArchiver / 小Q解压
// RarBackend construction; all operations are inherited from SevenZipBackend.
// RarBackend 构造；全部操作继承自 SevenZipBackend。

#include "RarBackend.h"

// RAR is always read-only and reuses the 7-Zip implementation.
// RAR 始终只读，并复用 7-Zip 实现。
RarBackend::RarBackend(ArchiveFormat fmt, QObject *parent)
    : SevenZipBackend(fmt, parent)
{
}
