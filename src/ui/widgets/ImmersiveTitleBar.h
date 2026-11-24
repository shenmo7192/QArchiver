#ifndef IMMERSIVETITLEBAR_H
#define IMMERSIVETITLEBAR_H

// QArchiver / 小Q解压
// Frameless-window title bar with menus and window controls.
// 无系统边框窗口的沉浸式标题栏，包含菜单与窗口按钮。

#include <QPoint>
#include <QWidget>

class QLabel;
class QMenuBar;
class QToolButton;

// Lives in the QMainWindow menu-widget slot so it sits above toolbars and
// content while the native decoration is disabled.
// 放在 QMainWindow 的 menuWidget 位置，位于工具栏与内容上方；系统边框关闭。
class ImmersiveTitleBar : public QWidget {
    Q_OBJECT
public:
    explicit ImmersiveTitleBar(QWidget *parent = nullptr);

    QMenuBar *menuBarWidget() const { return m_menuBar; }
    void updateMaximizeState();

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void updateTitle();

    QLabel *m_icon = nullptr;
    QLabel *m_title = nullptr;
    QMenuBar *m_menuBar = nullptr;
    QToolButton *m_minimize = nullptr;
    QToolButton *m_maximize = nullptr;
    QToolButton *m_close = nullptr;
    QPoint m_dragOffset;
    bool m_manualDrag = false;
};

#endif // IMMERSIVETITLEBAR_H
