// QArchiver / 小Q解压
// Exception constructors and accessors for the archive error model.
// 压缩包错误模型的异常构造函数与访问器。

#include "common/ArchiveError.h"

// Build an exception carrying both a stable code and a user-facing message.
// 构造同时携带稳定错误码和用户可读消息的异常。
ArchiveException::ArchiveException(ArchiveErrorCode code, const QString &msg)
    : std::runtime_error(msg.toStdString())
    , m_code(code)
    , m_message(msg)
{
}

// Stable error code for programmatic handling.
// 供程序判断的稳定错误码。
ArchiveErrorCode ArchiveException::code() const noexcept
{
    return m_code;
}

// Original user-facing message.
// 原始用户可读消息。
const QString &ArchiveException::message() const noexcept
{
    return m_message;
}
