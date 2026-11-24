#ifndef LOGLINEBUFFER_H
#define LOGLINEBUFFER_H

// QArchiver / 小Q解压
// Stream-friendly line splitter for tool output shown in a log view.
// 把工具输出流切分成日志行的辅助类型。

#include <QByteArray>
#include <QString>
#include <QStringList>

// Archive tools produce line-oriented output with three quirks that a plain
// indexOf('\n') loop gets wrong:
//  * 7-Zip rewrites progress lines with a bare '\r', so a '\r' terminates a line
//    too (otherwise the rewritten progress glues itself to the next line);
//  * CRLF is one terminator, not two, otherwise every line is preceded by an
//    empty one;
//  * partial lines must survive between chunks.
// Chunks arrive as raw bytes, so decoding happens per completed line.
//
// 压缩工具的输出有三个容易出错的细节，简单按 '\n' 切分会处理错：
//  * 7-Zip 用单独的 '\r' 重写进度行，所以 '\r' 也算行结束
//    （否则被重写的进度会和下一行粘在一起）；
//  * CRLF 是一个行结束符而不是两个，否则每行前面都会多出空行；
//  * 未完成的行必须跨数据块保留。
// 数据块是原始字节，因此解码在整行完成时进行。
class LogLineBuffer {
public:
    // Append one chunk and return every line it completes.
    // Whitespace-only lines are dropped (they only make the log scroll).
    // 追加一段数据并返回其中已完成的行。
    // 纯空白行会被丢弃（它们只会让日志滚动而不显示内容）。
    QStringList append(const QByteArray &chunk)
    {
        QStringList lines;
        if (chunk.isEmpty()) {
            return lines;
        }
        m_buffer += chunk;
        while (!m_buffer.isEmpty()) {
            const int newline = m_buffer.indexOf('\n');
            const int carriageReturn = m_buffer.indexOf('\r');
            int end = -1;
            int consume = 0;
            if (carriageReturn >= 0
                && (newline < 0 || carriageReturn < newline)) {
                end = carriageReturn;
                consume = (carriageReturn + 1 < m_buffer.size()
                           && m_buffer.at(carriageReturn + 1) == '\n')
                              ? 2
                              : 1;
            } else if (newline >= 0) {
                end = newline;
                consume = 1;
            } else {
                break;
            }
            QByteArray line = m_buffer.left(end);
            m_buffer.remove(0, end + consume);
            if (!line.trimmed().isEmpty()) {
                lines.append(QString::fromUtf8(line));
            }
        }
        return lines;
    }

    // Return and clear the unterminated remainder.
    // 返回并清空尚未结束的残余内容。
    QStringList flush()
    {
        QStringList lines;
        if (m_buffer.isEmpty()) {
            return lines;
        }
        const QByteArray line = m_buffer;
        m_buffer.clear();
        if (!line.trimmed().isEmpty()) {
            lines.append(QString::fromUtf8(line));
        }
        return lines;
    }

    void clear() { m_buffer.clear(); }
    bool isEmpty() const { return m_buffer.isEmpty(); }

private:
    QByteArray m_buffer;
};

#endif // LOGLINEBUFFER_H
