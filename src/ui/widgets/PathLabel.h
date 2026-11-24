#ifndef PATHLABEL_H
#define PATHLABEL_H

// QArchiver / 小Q解压
// Shared elided path display used by the modern and classic layouts.
// 现代与经典布局共用的路径显示控件（超出宽度时中间省略）。

#include <QLabel>
#include <QString>

class QResizeEvent;

// A QLabel that elides long archive/local paths in the middle and exposes the
// full text through its tooltip.  The visual style is provided by the layout
// theme through the object name (ModernPathLabel / ClassicPathLabel).
// 中间省略过长压缩包/本地路径的标签，并通过 tooltip 显示完整文本。
// 外观由各布局主题根据对象名（ModernPathLabel / ClassicPathLabel）提供。
class PathLabel : public QLabel {
public:
    explicit PathLabel(QWidget *parent = nullptr);

    // archiveName is the archive file name or a local directory path;
    // internalDir is the current directory inside the archive.
    // archiveName 为压缩包文件名或本地目录路径；internalDir 为压缩包内部目录。
    void setLocation(const QString &archiveName,
                     const QString &internalDir = QString());

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateElision();
    QString m_fullText;
};

#endif // PATHLABEL_H
