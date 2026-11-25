// QArchiver / 小Q解压
// Edge/corner resize support for the frameless main window.
// 无边框主窗口的边缘/角落缩放支持。

#include "ui/widgets/WindowResizeBorders.h"

#include <QEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QWidget>
#include <QWindow>

WindowResizeBorders::WindowResizeBorders(QWidget *window, QObject *parent)
    : QObject(parent ? parent : static_cast<QObject *>(window))
    , m_window(window)
{
    createBorder(Qt::LeftEdge, Qt::SizeHorCursor);
    createBorder(Qt::RightEdge, Qt::SizeHorCursor);
    createBorder(Qt::TopEdge, Qt::SizeVerCursor);
    createBorder(Qt::BottomEdge, Qt::SizeVerCursor);
    createBorder(Qt::LeftEdge | Qt::TopEdge, Qt::SizeFDiagCursor);
    createBorder(Qt::RightEdge | Qt::TopEdge, Qt::SizeBDiagCursor);
    createBorder(Qt::LeftEdge | Qt::BottomEdge, Qt::SizeBDiagCursor);
    createBorder(Qt::RightEdge | Qt::BottomEdge, Qt::SizeFDiagCursor);

    if (m_window) {
        m_window->installEventFilter(this);
        updateGeometry();
        raiseBorders();
    }
}

WindowResizeBorders::~WindowResizeBorders()
{
    if (m_window) {
        m_window->removeEventFilter(this);
    }
}

void WindowResizeBorders::createBorder(Qt::Edges edges, Qt::CursorShape cursor)
{
    auto *widget = new QWidget(m_window);
    widget->setObjectName(QStringLiteral("WindowResizeBorder"));
    widget->setCursor(cursor);
    widget->setFocusPolicy(Qt::NoFocus);
    // Transparent hit zones must not paint and must not forward clicks to the
    // widget underneath (that is what the native border would prevent).
    // 透明命中区不绘制，也不把点击继续传给下层控件（原生边框同样会拦截）。
    widget->setAutoFillBackground(false);
    widget->setAttribute(Qt::WA_NoMousePropagation, true);
    widget->installEventFilter(this);
    m_borders.append({widget, edges});
}

void WindowResizeBorders::updateGeometry()
{
    if (!m_window) {
        return;
    }

    const bool enabled = !m_window->isMaximized()
                         && !m_window->isFullScreen();
    for (int i = 0; i < m_borders.size(); ++i) {
        m_borders.at(i).widget->setVisible(enabled);
    }
    if (!enabled) {
        return;
    }

    const int border = borderWidth();
    const int width = m_window->width();
    const int height = m_window->height();
    const int horizontalSpan = qMax(0, width - 2 * border);
    const int verticalSpan = qMax(0, height - 2 * border);

    for (int i = 0; i < m_borders.size(); ++i) {
        const Border &item = m_borders.at(i);
        const bool left = item.edges.testFlag(Qt::LeftEdge);
        const bool right = item.edges.testFlag(Qt::RightEdge);
        const bool top = item.edges.testFlag(Qt::TopEdge);
        const bool bottom = item.edges.testFlag(Qt::BottomEdge);

        QRect rect;
        if (left && top) {
            rect = QRect(0, 0, border, border);
        } else if (right && top) {
            rect = QRect(width - border, 0, border, border);
        } else if (left && bottom) {
            rect = QRect(0, height - border, border, border);
        } else if (right && bottom) {
            rect = QRect(width - border, height - border, border, border);
        } else if (left) {
            rect = QRect(0, border, border, verticalSpan);
        } else if (right) {
            rect = QRect(width - border, border, border, verticalSpan);
        } else if (top) {
            rect = QRect(border, 0, horizontalSpan, border);
        } else if (bottom) {
            rect = QRect(border, height - border, horizontalSpan, border);
        }
        item.widget->setGeometry(rect);
    }
}

void WindowResizeBorders::raiseBorders()
{
    for (int i = 0; i < m_borders.size(); ++i) {
        m_borders.at(i).widget->raise();
    }
}

bool WindowResizeBorders::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window) {
        switch (event->type()) {
        case QEvent::Resize:
            updateGeometry();
            break;
        case QEvent::WindowStateChange:
            updateGeometry();
            raiseBorders();
            break;
        case QEvent::Show:
            updateGeometry();
            // QMainWindow may re-stack its internal children while showing;
            // raise once more after the event has been fully processed.
            // QMainWindow 显示时可能重排内部子控件，事件处理完后再置顶一次。
            QTimer::singleShot(0, this, [this]() {
                if (!m_window) {
                    return;
                }
                updateGeometry();
                raiseBorders();
            });
            break;
        default:
            break;
        }
        return QObject::eventFilter(watched, event);
    }

    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton && m_window
            && !m_window->isMaximized() && !m_window->isFullScreen()) {
            for (int i = 0; i < m_borders.size(); ++i) {
                const Border &item = m_borders.at(i);
                if (watched != item.widget) {
                    continue;
                }
                QWindow *handle = m_window->windowHandle();
                if (handle && handle->startSystemResize(item.edges)) {
                    event->accept();
                    return true;
                }
                break;
            }
        }
    }

    return QObject::eventFilter(watched, event);
}
