#ifndef LIBARCHIVEBACKEND_H
#define LIBARCHIVEBACKEND_H

#include "backends/ArchiveBackend.h"

// Read-only backend built on the system libarchive (design doc 5.6).
//
// Handles, without spawning any external process:
//   * .zip            (plain, ZipCrypto, AES-128/AES-256)
//   * .7z             (unencrypted; encrypted archives must use the CLI)
//   * .tar/.tar.gz/.tar.bz2/.tar.xz and the short aliases .tgz/.tbz2/.txz
//   * pure .gz/.bz2/.xz streams (exposed as a single entry, like StreamBackend)
//
// Everything libarchive refuses (encrypted 7z, split volumes, ...) is reported
// through errorOccurred(); ArchiveManager then transparently falls back to the
// command-line backend for the same archive.
//
// Writing is not supported: compress()/addFiles()/deleteFiles() report
// ArchiveErrorCode::OperationNotSupported and the CLI backends keep handling
// every write path (7z/zip creation, volumes, append, delete).
class LibArchiveBackend : public ArchiveBackend {
    Q_OBJECT
public:
    explicit LibArchiveBackend(ArchiveFormat fmt,
                               QObject *parent = nullptr);

    void openArchive(const QString &filePath) override;
    void listContents(const ListOptions &options = {}) override;
    void extract(const ExtractOptions &options) override;
    void compress(const CompressOptions &options) override;
    void addFiles(const AddFilesOptions &options) override;
    void deleteFiles(const DeleteFilesOptions &options) override;
    void testArchive(const TestOptions &options = {}) override;
    void closeArchive() override;

    ArchiveFormat format() const override { return m_format; }
    bool isReadOnly() const override { return true; }
    bool supportsEncryption() const override { return true; }
    bool supportsVolume() const override { return false; }
    bool supportsAppend() const override { return false; }

private:
    // Translate a libarchive error string into the QArchiver error model.
    // Emits passwordRequested() when a password is missing, so the manager can
    // ask the user and call retryWithPassword().
    void reportFailure(const QString &message, const QString &password,
                       const QString &context);

    // Name inferred from the file name for pure-stream archives ("a.gz" -> "a").
    QString inferStreamName() const;

    ArchiveFormat m_format;
    QList<ArchiveEntry> m_entries;
};

#endif // LIBARCHIVEBACKEND_H
