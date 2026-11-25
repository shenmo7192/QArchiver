#ifndef MODERNNAVBAR_H
#define MODERNNAVBAR_H

// QArchiver / 小Q解压
// Navigation and search strip for the modern layout.
// 现代布局的导航与搜索栏。

#include <QPoint>
#include <QWidget>

class BreadcrumbPathBar;
class QLineEdit;
class QToolButton;
class QButtonGroup;

// Back / forward / up buttons, a breadcrumb location bar and the archive
// search field.  The widget only emits requests; MainWindow owns navigation
// state.
// 后退/前进/上级按钮、面包屑路径框以及压缩包搜索框。
// 控件只发出请求，导航状态由 MainWindow 持有。
class ModernNavBar : public QWidget {
    Q_OBJECT
public:
    explicit ModernNavBar(QWidget *parent = nullptr);

    void setArchiveLocation(const QString &archiveName,
                            const QString &internalDir);
    void setLocalLocation(const QString &directory);
    void setPlaceholder(const QString &text);
    void setNavigationState(bool canGoBack, bool canGoForward, bool canGoUp);
    void setViewMode(const QString &mode);
    // Updates the sort button tooltip/icon to the active sort state.
    // 更新排序按钮的提示与图标，反映当前排序状态。
    void setSortState(const QString &tooltip, bool descending);
    QString searchText() const;
    void setSearchText(const QString &text);
    void clearSearch();
    void focusSearch();

signals:
    void backRequested();
    void forwardRequested();
    void upRequested();
    void archiveSegmentActivated(const QString &internalDir);
    void localSegmentActivated(const QString &absolutePath);
    void searchChanged(const QString &text);
    void viewModeChanged(const QString &mode);
    // Emitted when the sort button is clicked; globalPos is the menu anchor.
    // 点击排序按钮时发出；globalPos 为菜单锚点。
    void sortMenuRequested(const QPoint &globalPos);

private:
    QToolButton *m_backButton = nullptr;
    QToolButton *m_forwardButton = nullptr;
    QToolButton *m_upButton = nullptr;
    QToolButton *m_sortButton = nullptr;
    BreadcrumbPathBar *m_locationBar = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QButtonGroup *m_viewGroup = nullptr;
};

#endif // MODERNNAVBAR_H
