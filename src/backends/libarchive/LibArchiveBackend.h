#ifndef LIBARCHIVEBACKEND_H
#define LIBARCHIVEBACKEND_H

// QArchiver / 小Q解压
// Read-only in-process backend powered by system libarchive.
// 由系统 libarchive 驱动的只读进程内后端。

#include "backends/ArchiveBackend.h"

struct archive;
struct archive_entry;

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
//
// 基于系统 libarchive 的只读后端（设计文档 5.6）。
// 无需启动外部进程即可处理：
//   * .zip（普通、ZipCrypto、AES-128/AES-256）
//   * .7z（未加密；加密包必须走 CLI）
//   * .tar/.tar.gz/.tar.bz2/.tar.xz 及短扩展名 .tgz/.tbz2/.txz
//   * 纯 .gz/.bz2/.xz 流（与 StreamBackend 一样作为单个条目）
// libarchive 无法处理的情况（加密 7z、分卷等）通过 errorOccurred() 上报，
// ArchiveManager 随后会自动回退到同一压缩包的命令行后端。
// 不支持写入：compress()/addFiles()/deleteFiles() 返回
// OperationNotSupported，所有写路径仍由 CLI 后端处理。
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
    QString nameEncoding() const override { return m_nameEncoding; }
    bool isReadOnly() const override { return true; }
    bool supportsEncryption() const override { return true; }
    bool supportsVolume() const override { return false; }
    bool supportsAppend() const override { return false; }

private:
    // Translate a libarchive error string into the QArchiver error model.
    // Emits passwordRequested() when a password is missing, so the manager can
    // ask the user and call retryWithPassword().
    // 将 libarchive 错误文本转换为 QArchiver 错误模型。
    // 缺少密码时发出 passwordRequested()，以便管理器询问用户并重试。
    void reportFailure(const QString &message, const QString &password,
                       const QString &context);

    // Name inferred from the file name for pure-stream archives ("a.gz" -> "a").
    // 为纯流压缩包从文件名推断内部名称（"a.gz" -> "a"）。
    QString inferStreamName() const;

    // Entry name of @p entry, decoded with m_nameEncoding when libarchive has
    // no UTF-8 form for it (legacy ZIP/TAR without the UTF-8 flag).
    // 条目名称；libarchive 没有 UTF-8 形式时（无 UTF-8 标志的旧式 ZIP/TAR）
    // 按 m_nameEncoding 解码。
    QString entryPathString(struct archive_entry *entry) const;
    // Decode a raw name byte string with the archive's detected encoding.
    // 按压缩包检测到的编码解码原始名称字节串。
    QString decodeEntryString(const QByteArray &raw) const;
    // Decode a symlink/hardlink target: valid UTF-8 is kept as is, everything
    // else follows the archive's name encoding.
    // 解码符号/硬链接目标：合法 UTF-8 原样保留，其余按压缩包名称编码解码。
    QString decodeLinkTarget(const QByteArray &raw) const;

    ArchiveFormat m_format;
    QList<ArchiveEntry> m_entries;
    // Detected (or externally imposed) encoding of legacy entry names.  Set by
    // listContents() and reused by extract() so both agree on the names.
    // 检测到（或外部指定）的旧式条目名编码。由 listContents() 设置，
    // extract() 复用，保证两边对名称的判断一致。
    QString m_nameEncoding;
};

#endif // LIBARCHIVEBACKEND_H
