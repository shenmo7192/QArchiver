#ifndef SEVENZIPTOOL_H
#define SEVENZIPTOOL_H

// QArchiver / 小Q解压
// Runtime discovery and capability probing for the 7-Zip CLI fallback.
// 7-Zip 命令行兜底工具的运行时发现与能力探测。

#include <QString>
#include <QStringList>

// Result of locating the 7-Zip command-line helper.
// 定位 7-Zip 命令行辅助程序的结果。
struct SevenZipToolInfo {
    enum class Source {
        None,     // No usable tool / 没有可用工具
        Bundled,  // Found in a program-relative search directory / 程序相关目录
        System,   // Found through PATH / 系统 PATH
        Override  // QARCHIVER_7Z_PATH / 显式环境变量覆盖
    };

    Source source = Source::None;
    QString path;
    bool supportsRar = false;
    QStringList bundledDirectories;

    bool available() const { return source != Source::None && !path.isEmpty(); }
};

// Discovery, validation and codec probing helpers.
// 发现、校验与编解码器探测辅助函数。
namespace SevenZipTool {

// Detect the first usable 7-Zip CLI.  The result is cached for the process;
// call resetCache() from tests before changing QARCHIVER_7Z_PATH or PATH.
// 检测第一个可用的 7-Zip CLI。结果在进程内缓存；
// 测试中修改 QARCHIVER_7Z_PATH 或 PATH 前应调用 resetCache()。
SevenZipToolInfo detect();
QString resolvePath();
void resetCache();

// Candidate program-relative directories in search order.
// 按搜索顺序返回程序相关候选目录。
QStringList bundledDirectories();

// True when `--help` output identifies a 7-Zip command-line tool.
// `--help` 输出是否表明这是 7-Zip 命令行工具。
bool isSevenZipHelpText(const QString &output);

// True when the Codecs section of `7z i` output contains a RAR codec.
// `7z i` 输出的 Codecs 区段是否包含 RAR 编解码器。
bool codecsContainRar(const QString &output);

// Run `<executable> i` and inspect its Codecs section.
// 运行 `<可执行文件> i` 并检查 Codecs 区段。
bool supportsRar(const QString &executable);

} // namespace SevenZipTool

#endif // SEVENZIPTOOL_H
