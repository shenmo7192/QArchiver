// QArchiver / 小Q解压
// Read-only .iso backend: write operations are rejected explicitly.
// 只读 .iso 后端：显式拒绝写操作。

#include "backends/iso/IsoBackend.h"

void IsoBackend::compress(const CompressOptions &options)
{
    Q_UNUSED(options);
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                       tr("Disc images cannot be created by QArchiver"));
}

void IsoBackend::addFiles(const AddFilesOptions &options)
{
    Q_UNUSED(options);
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                       tr("Disc images are read-only"));
}

void IsoBackend::deleteFiles(const DeleteFilesOptions &options)
{
    Q_UNUSED(options);
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                       tr("Disc images are read-only"));
}
