#ifndef MODERNNAVBAR_H
#define MODERNNAVBAR_H

// QArchiver / 小Q解压
// Navigation and search strip for the modern layout.
// 现代布局的导航与搜索栏。

#include <QPoint>
#include <QWidget>

class QLineEdit;
class QToolButton;
class PathLabel;
class QButtonGroup;

// Back / forward / up buttons, an elided location label and the archive search
// field.  The widget only emits requests; MainWindow owns navigation state.
// 后退/前进/上级按钮、省略显示的位置标签以及压缩包搜索框。
// 控件只发出请求，导航状态由 MainWindow 持有。
class ModernNavBar : public QWidget {
    Q_OBJECT
public:
    explicit ModernNavBar(QWidget *parent = nullptr);

    void setLocation(const QString &archiveName, const QString &internalDir);
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
    PathLabel *m_locationLabel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QButtonGroup *m_viewGroup = nullptr;
};

#endif // MODERNNAVBAR_H
