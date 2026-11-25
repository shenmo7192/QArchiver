#ifndef WINDOWRESIZEBORDERS_H
#define WINDOWRESIZEBORDERS_H

// QArchiver / 小Q解压
// Edge/corner resize support for the frameless main window.
// 无边框主窗口的边缘/角落缩放支持。

#include <QObject>
#include <QVector>
#include <Qt>

class QWidget;

// A frameless top-level window has no native resize border, so the only
// resize affordance left is QStatusBar's QSizeGrip in the bottom-right
// corner.  This helper creates eight transparent hit zones along the client
// edges and starts the platform's native resize operation when one is
// pressed.  Nothing is painted and no layout is changed; the zones are kept
// above the UI and disabled while the window is maximized or fullscreen.
// 无边框顶层窗口没有原生缩放边框，通常只剩下状态栏右下角的 QSizeGrip。
// 本辅助类在客户区四周创建 8 个透明命中区，按下时启动平台原生缩放；
// 不绘制任何内容、不改变布局，并始终置顶，在最大化/全屏时自动禁用。
class WindowResizeBorders : public QObject {
    Q_OBJECT
public:
    explicit WindowResizeBorders(QWidget *window,
                                 QObject *parent = nullptr);
    ~WindowResizeBorders() override;

    // Re-position the hit zones after the window geometry changes.
    // 窗口尺寸变化后重新摆放命中区。
    void updateGeometry();
    // Keep the transparent hit zones above newly created UI children.
    // 让透明命中区保持在新创建的界面子控件之上。
    void raiseBorders();

    static int borderWidth() { return 6; }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Border {
        QWidget *widget = nullptr;
        Qt::Edges edges;
    };

    void createBorder(Qt::Edges edges, Qt::CursorShape cursor);

    QWidget *m_window = nullptr;
    QVector<Border> m_borders;
};

#endif // WINDOWRESIZEBORDERS_H
