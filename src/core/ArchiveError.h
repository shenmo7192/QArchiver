#ifndef ARCHIVEERROR_H
#define ARCHIVEERROR_H

// QArchiver / 小Q解压
// Archive error codes and the exception hierarchy used by backends.
// 压缩包错误码以及后端使用的异常类层次结构。

#include <QString>
#include <stdexcept>
#include "ArchiveConstants.h"

// Error code enumeration (matches design doc 4.5.1)
// 错误码枚举（与设计文档 4.5.1 对应）
enum class ArchiveErrorCode {
    Unknown,                // Unknown / unclassified
    OperationNotSupported,  // Read-only backend asked to write
    UnsupportedFormat,      // Format unrecognized / unsupported
    PasswordRequired,       // Password needed but not provided
    WrongPassword,          // Incorrect password
    CorruptedArchive,       // Archive corrupted / CRC error
    InsufficientDiskSpace,  // Not enough disk space
    PathTraversalBlocked,   // Zip Slip path traversal blocked
    VolumeMissing,          // Volume missing
    ToolNotFound,           // Bundled 7-Zip helper not available
    FileNotFound,           // Source/target not found
    PermissionDenied,       // Insufficient permissions
    PathTooLong,            // Exceeds PATH_MAX
    EncodingError,          // Filename encoding unparseable
    Cancelled,              // User cancelled
};

Q_DECLARE_METATYPE(ArchiveErrorCode)

// Base exception class for all archive errors
// 所有压缩包错误的基类异常
class ArchiveException : public std::runtime_error {
public:
    ArchiveException(ArchiveErrorCode code, const QString &msg);
    ArchiveErrorCode code() const noexcept;
    const QString &message() const noexcept;
private:
    ArchiveErrorCode m_code;
    QString m_message;
};

// Derived exception classes with fixed codes
// 使用固定错误码的派生异常类
class OperationNotSupportedException : public ArchiveException {
public:
    explicit OperationNotSupportedException(const QString &msg)
        : ArchiveException(ArchiveErrorCode::OperationNotSupported, msg) {}
};

class UnsupportedFormatException : public ArchiveException {
public:
    explicit UnsupportedFormatException(const QString &msg)
        : ArchiveException(ArchiveErrorCode::UnsupportedFormat, msg) {}
};

class PasswordRequiredException : public ArchiveException {
public:
    explicit PasswordRequiredException(const QString &msg)
        : ArchiveException(ArchiveErrorCode::PasswordRequired, msg) {}
};

class WrongPasswordException : public ArchiveException {
public:
    explicit WrongPasswordException(const QString &msg)
        : ArchiveException(ArchiveErrorCode::WrongPassword, msg) {}
};

class CorruptedArchiveException : public ArchiveException {
public:
    explicit CorruptedArchiveException(const QString &msg)
        : ArchiveException(ArchiveErrorCode::CorruptedArchive, msg) {}
};

class InsufficientDiskSpaceException : public ArchiveException {
public:
    explicit InsufficientDiskSpaceException(const QString &msg)
        : ArchiveException(ArchiveErrorCode::InsufficientDiskSpace, msg) {}
};

class PathTraversalException : public ArchiveException {
public:
    explicit PathTraversalException(const QString &msg)
        : ArchiveException(ArchiveErrorCode::PathTraversalBlocked, msg) {}
};

class VolumeMissingException : public ArchiveException {
public:
    explicit VolumeMissingException(const QString &msg)
        : ArchiveException(ArchiveErrorCode::VolumeMissing, msg) {}
};

class ToolNotFoundException : public ArchiveException {
public:
    explicit ToolNotFoundException(const QString &msg)
        : ArchiveException(ArchiveErrorCode::ToolNotFound, msg) {}
};

#endif // ARCHIVEERROR_H
