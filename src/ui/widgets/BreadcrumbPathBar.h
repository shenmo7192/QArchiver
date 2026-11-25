#ifndef BREADCRUMBPATHBAR_H
#define BREADCRUMBPATHBAR_H

// QArchiver / 小Q解压
// Windows 7 style breadcrumb location bar shared by both layouts.
// 现代与经典布局共用的 Windows 7 风格面包屑路径框。

#include <QString>
#include <QVector>
#include <QWidget>

class QAction;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QStackedLayout;
class QToolButton;

// Shows the archive root / local directory as a row of flat buttons.  Long
// locations hide the middle levels behind a "..." button and, when space runs
// out, elide the first level before the last one.  Clicking the empty area on
// the right switches to a read-only, selectable text field containing the
// whole path.
// 将压缩包根名/本地目录显示为一行扁平按钮。路径过长时用 "..." 按钮收纳
// 中间层级；空间继续缩小时先省略开头、最后才省略末尾。单击右侧空白区域
// 切换为只读、可选中复制的完整路径文本框。
class BreadcrumbPathBar : public QWidget {
    Q_OBJECT
public:
    explicit BreadcrumbPathBar(QWidget *parent = nullptr);

    // Archive mode: root button is the archive file name, then every component
    // of internalDir.  target values are archive-internal directory paths.
    // 压缩包模式：根按钮为压缩包文件名，其后为 internalDir 的每一级。
    void setArchiveLocation(const QString &archiveName,
                            const QString &internalDir = QString());
    // Local mode: one button per filesystem level (drive/root included).
    // 本地模式：文件系统每一级一个按钮（包含盘符/根）。
    void setLocalLocation(const QString &directory);
    // Non-clickable message such as "No archive open".
    // 不可点击的提示文本，例如“未打开压缩包”。
    void setPlaceholder(const QString &text);

    QString fullPathText() const { return m_fullText; }

signals:
    // Emitted when an archive-internal level is activated.
    // 激活压缩包内部某一级时发出。
    void archiveSegmentActivated(const QString &internalDir);
    // Emitted when a filesystem level is activated (absolute path).
    // 激活文件系统某一级时发出（绝对路径）。
    void localSegmentActivated(const QString &absolutePath);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Mode {
        Placeholder,
        Archive,
        Local
    };

    struct Segment {
        QString label;
        QString target;
    };

    void setSegments(Mode mode, const QVector<Segment> &segments,
                     const QString &fullText);
    void rebuildButtons();
    void updateElision();
    void setElidedText(QToolButton *button, const QString &label,
                       int maxWidth);
    void activateSegment(int index);
    void openOverflowMenu();
    void showTextMode();
    void showCrumbs();

    QStackedLayout *m_stack = nullptr;
    QWidget *m_crumbPage = nullptr;
    QWidget *m_editPage = nullptr;
    QHBoxLayout *m_crumbLayout = nullptr;
    QLabel *m_placeholderLabel = nullptr;
    QLineEdit *m_edit = nullptr;
    QToolButton *m_overflowButton = nullptr;
    QMenu *m_overflowMenu = nullptr;
    QVector<QToolButton *> m_buttons;
    QVector<Segment> m_segments;
    QVector<int> m_hiddenIndexes;
    QString m_placeholderText;
    QString m_fullText;
    Mode m_mode = Mode::Placeholder;
    bool m_updating = false;
};

#endif // BREADCRUMBPATHBAR_H
