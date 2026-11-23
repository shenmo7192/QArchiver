#ifndef MODERNNAVBAR_H
#define MODERNNAVBAR_H

// QArchiver / 小Q解压
// Navigation and search strip for the modern layout.
// 现代布局的导航与搜索栏。

#include <QWidget>

class QLineEdit;
class QToolButton;
class QLabel;
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

private:
    QToolButton *m_backButton = nullptr;
    QToolButton *m_forwardButton = nullptr;
    QToolButton *m_upButton = nullptr;
    QLabel *m_locationLabel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QButtonGroup *m_viewGroup = nullptr;
};

#endif // MODERNNAVBAR_H
