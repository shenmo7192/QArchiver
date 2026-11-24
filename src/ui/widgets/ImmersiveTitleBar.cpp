// QArchiver / 小Q解压
// Immersive title bar implementation.
// 沉浸式标题栏实现。

#include "ui/widgets/ImmersiveTitleBar.h"

#include "ui/icons/IconUtils.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenuBar>
#include <QMouseEvent>
#include <QToolButton>
#include <QWindow>

ImmersiveTitleBar::ImmersiveTitleBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ImmersiveTitleBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedHeight(38);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 6, 0);
    layout->setSpacing(6);

    m_icon = new QLabel(this);
    m_icon->setObjectName(QStringLiteral("TitleBarIcon"));
    m_icon->setPixmap(Icons::appLogo(20));
    m_icon->setFixedSize(20, 20);
    layout->addWidget(m_icon);

    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("TitleBarTitle"));
    m_title->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    layout->addWidget(m_title);

    m_menuBar = new QMenuBar(this);
    m_menuBar->setObjectName(QStringLiteral("TitleBarMenuBar"));
    m_menuBar->setNativeMenuBar(false);
    m_menuBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    layout->addWidget(m_menuBar, 1, Qt::AlignVCenter);

    const auto makeButton = [this](const QString &text, const QString &name,
                                   const QString &tooltip) {
        auto *button = new QToolButton(this);
        button->setObjectName(name);
        button->setText(text);
        button->setToolTip(tooltip);
        button->setFixedSize(34, 26);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);
        return button;
    };
    m_minimize = makeButton(QStringLiteral("−"),
                            QStringLiteral("TitleBarButton"),
                            tr("Minimize"));
    m_maximize = makeButton(QStringLiteral("□"),
                            QStringLiteral("TitleBarButton"),
                            tr("Maximize"));
    m_close = makeButton(QStringLiteral("×"),
                         QStringLiteral("TitleBarCloseButton"),
                         tr("Close"));
    layout->addWidget(m_minimize);
    layout->addWidget(m_maximize);
    layout->addWidget(m_close);

    connect(m_minimize, &QToolButton::clicked,
            this, &ImmersiveTitleBar::minimizeRequested);
    connect(m_maximize, &QToolButton::clicked,
            this, &ImmersiveTitleBar::maximizeRequested);
    connect(m_close, &QToolButton::clicked,
            this, &ImmersiveTitleBar::closeRequested);

    if (window()) {
        connect(window(), &QWidget::windowTitleChanged,
                this, &ImmersiveTitleBar::updateTitle);
    }
    updateTitle();
    updateMaximizeState();
}

void ImmersiveTitleBar::updateTitle()
{
    if (m_title && window()) {
        m_title->setText(window()->windowTitle());
    }
}

void ImmersiveTitleBar::updateMaximizeState()
{
    if (!m_maximize || !window()) {
        return;
    }
    const bool maximized = window()->isMaximized();
    m_maximize->setText(maximized ? QStringLiteral("❐")
                                  : QStringLiteral("□"));
    m_maximize->setToolTip(maximized ? tr("Restore") : tr("Maximize"));
}

void ImmersiveTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    // Prefer the platform move so Wayland and Windows keep native snapping.
    // 优先使用平台移动，保证 Wayland/Windows 的原生贴边行为。
    if (window() && window()->windowHandle()
        && window()->windowHandle()->startSystemMove()) {
        m_manualDrag = false;
        event->accept();
        return;
    }
    m_manualDrag = true;
    m_dragOffset = event->globalPosition().toPoint()
        - window()->frameGeometry().topLeft();
    event->accept();
}

void ImmersiveTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_manualDrag || !(event->buttons() & Qt::LeftButton) || !window()) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    if (window()->isMaximized()) {
        window()->showNormal();
    }
    window()->move(event->globalPosition().toPoint() - m_dragOffset);
    event->accept();
}

void ImmersiveTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_manualDrag = false;
    QWidget::mouseReleaseEvent(event);
}

void ImmersiveTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit maximizeRequested();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}
