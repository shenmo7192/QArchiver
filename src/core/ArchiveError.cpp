#include "ArchiveError.h"

ArchiveException::ArchiveException(ArchiveErrorCode code, const QString &msg)
    : std::runtime_error(msg.toStdString())
    , m_code(code)
    , m_message(msg)
{
}

ArchiveErrorCode ArchiveException::code() const noexcept
{
    return m_code;
}

QString ArchiveException::message() const noexcept
{
    return m_message;
}
