// QArchiver / 小Q解压
// ModernToolBar construction and action buttons.
// ModernToolBar 的构建与动作按钮。

#include "ui/icons/IconUtils.h"
#include "ui/widgets/ModernToolBar.h"

#include <QAction>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QToolButton *makeToolButton(QAction *action, bool large, QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setDefaultAction(action);
    if (large) {
        button->setObjectName(QStringLiteral("ModernToolButton"));
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(40, 40));
        button->setMinimumWidth(62);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    } else {
        button->setObjectName(QStringLiteral("ModernSmallToolButton"));
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIconSize(QSize(20, 20));
        button->setFixedSize(32, 32);
    }
    if (button->toolTip().isEmpty()) {
        button->setToolTip(action->toolTip().isEmpty() ? action->text()
                                                       : action->toolTip());
    }
    return button;
}

} // namespace

ModernToolBar::ModernToolBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ModernHeader"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 16, 8);
    layout->setSpacing(4);

    m_actionLayout = new QHBoxLayout;
    m_actionLayout->setContentsMargins(0, 0, 0, 0);
    m_actionLayout->setSpacing(2);
    layout->addLayout(m_actionLayout);
    layout->addStretch(1);

    auto *separator = new QFrame(this);
    separator->setObjectName(QStringLiteral("ModernToolbarSeparator"));
    separator->setFrameShape(QFrame::VLine);
    layout->addWidget(separator);

    auto *brand = new QWidget(this);
    auto *brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(10, 0, 0, 0);
    brandLayout->setSpacing(10);

    auto *logo = new QLabel(brand);
    logo->setPixmap(Icons::modern(QStringLiteral("app.png")).pixmap(42, 42));
    logo->setFixedSize(46, 46);
    logo->setScaledContents(true);
    brandLayout->addWidget(logo);

    auto *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(0);
    m_brandTitle = new QLabel(tr("小Q解压"), brand);
    m_brandTitle->setObjectName(QStringLiteral("ModernBrandTitle"));
    m_brandTagline = new QLabel(tr("轻巧 · 快速 · 无广告"), brand);
    m_brandTagline->setObjectName(QStringLiteral("ModernBrandTagline"));
    textLayout->addWidget(m_brandTitle);
    textLayout->addWidget(m_brandTagline);
    brandLayout->addLayout(textLayout);
    layout->addWidget(brand);
}

void ModernToolBar::addToolAction(QAction *action)
{
    if (!action) {
        return;
    }
    m_actionLayout->addWidget(makeToolButton(action, true, this));
}

void ModernToolBar::addSmallAction(QAction *action)
{
    if (!action) {
        return;
    }
    m_actionLayout->addWidget(makeToolButton(action, false, this));
}

void ModernToolBar::addSeparator()
{
    auto *separator = new QFrame(this);
    separator->setObjectName(QStringLiteral("ModernToolbarSeparator"));
    separator->setFrameShape(QFrame::VLine);
    m_actionLayout->addWidget(separator);
}
