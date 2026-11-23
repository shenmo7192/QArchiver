// QArchiver / 小Q解压
// Modern navigation bar implementation.
// 现代导航栏实现。

#include "ui/icons/IconUtils.h"
#include "ui/widgets/ModernNavBar.h"

#include <QAction>
#include <QButtonGroup>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPoint>
#include <QSize>
#include <QLineEdit>
#include <QResizeEvent>
#include <QToolButton>

namespace {

class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        setMinimumWidth(80);
    }

    void setFullText(const QString &text)
    {
        m_fullText = text;
        setToolTip(text);
        updateElision();
    }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QLabel::resizeEvent(event);
        updateElision();
    }

private:
    void updateElision()
    {
        if (m_fullText.isEmpty()) {
            QLabel::setText(QString());
            return;
        }
        const QFontMetrics metrics(font());
        QLabel::setText(metrics.elidedText(m_fullText, Qt::ElideMiddle,
                                           qMax(40, width() - 4)));
    }

    QString m_fullText;
};

QToolButton *makeNavButton(const QString &iconName, const QString &tooltip,
                           QWidget *parent)
{
    auto *button = new QToolButton(parent);
    button->setObjectName(QStringLiteral("ModernNavButton"));
    button->setIcon(Icons::modern(iconName));
    button->setIconSize(QSize(18, 18));
    button->setToolTip(tooltip);
    button->setFixedSize(30, 28);
    return button;
}

} // namespace

ModernNavBar::ModernNavBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("ModernNavBar"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 12, 6);
    layout->setSpacing(4);

    m_backButton = makeNavButton(QStringLiteral("back.png"), tr("Back"), this);
    m_forwardButton = makeNavButton(QStringLiteral("forward.png"),
                                    tr("Forward"), this);
    m_upButton = makeNavButton(QStringLiteral("up.png"), tr("Up"), this);
    layout->addWidget(m_backButton);
    layout->addWidget(m_forwardButton);
    layout->addWidget(m_upButton);

    m_locationLabel = new ElidedLabel(this);
    m_locationLabel->setObjectName(QStringLiteral("ModernPathLabel"));
    m_locationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_locationLabel, 1);

    // Segmented view switch, directly above the archive contents.
    // 分段式视图切换，位于压缩包内容列表正上方。
    auto *viewLabel = new QLabel(tr("View"), this);
    viewLabel->setObjectName(QStringLiteral("ModernViewLabel"));
    layout->addWidget(viewLabel);

    auto *viewSwitch = new QWidget(this);
    viewSwitch->setObjectName(QStringLiteral("ModernViewSwitch"));
    viewSwitch->setFixedHeight(28);
    auto *viewLayout = new QHBoxLayout(viewSwitch);
    viewLayout->setContentsMargins(2, 2, 2, 2);
    viewLayout->setSpacing(1);
    m_viewGroup = new QButtonGroup(this);
    m_viewGroup->setExclusive(true);
    const auto addViewButton = [this, viewLayout, viewSwitch](
        const QString &icon, const QString &mode, const QString &tooltip) {
        auto *button = new QToolButton(viewSwitch);
        button->setObjectName(QStringLiteral("ModernNavViewButton"));
        button->setIcon(Icons::modern(icon));
        button->setIconSize(QSize(16, 16));
        button->setFixedSize(26, 22);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setCheckable(true);
        button->setToolTip(tooltip);
        button->setProperty("viewMode", mode);
        m_viewGroup->addButton(button);
        connect(button, &QToolButton::clicked, this, [this, mode]() {
            emit viewModeChanged(mode);
        });
        viewLayout->addWidget(button);
        return button;
    };
    addViewButton(QStringLiteral("view-grid.png"), QStringLiteral("grid"),
                  tr("Grid view"));
    addViewButton(QStringLiteral("view-list.png"), QStringLiteral("list"),
                  tr("List view"));
    addViewButton(QStringLiteral("view-tree.png"), QStringLiteral("tree"),
                  tr("Tree view"));
    layout->addWidget(viewSwitch);

    // Compact sort menu next to the view switcher (grid view has no header).
    // 视图切换旁的小型排序菜单（网格视图没有表头可用）。
    m_sortButton = new QToolButton(this);
    m_sortButton->setObjectName(QStringLiteral("ModernNavButton"));
    m_sortButton->setIcon(Icons::modern(QStringLiteral("sort-down.png")));
    m_sortButton->setIconSize(QSize(18, 18));
    m_sortButton->setFixedSize(30, 28);
    m_sortButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_sortButton->setToolTip(tr("Sort"));
    connect(m_sortButton, &QToolButton::clicked, this, [this]() {
        emit sortMenuRequested(
            m_sortButton->mapToGlobal(QPoint(0, m_sortButton->height())));
    });
    layout->addWidget(m_sortButton);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("ModernSearchEdit"));
    m_searchEdit->setPlaceholderText(
        tr("Search this archive (wildcards allowed)"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedWidth(280);
    auto *searchAction = new QAction(Icons::modern(QStringLiteral("search.png")),
                                     tr("Search"), m_searchEdit);
    m_searchEdit->addAction(searchAction, QLineEdit::LeadingPosition);
    layout->addWidget(m_searchEdit);

    connect(m_backButton, &QToolButton::clicked,
            this, &ModernNavBar::backRequested);
    connect(m_forwardButton, &QToolButton::clicked,
            this, &ModernNavBar::forwardRequested);
    connect(m_upButton, &QToolButton::clicked,
            this, &ModernNavBar::upRequested);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &ModernNavBar::searchChanged);

    setNavigationState(false, false, false);
}

void ModernNavBar::setLocation(const QString &archiveName,
                               const QString &internalDir)
{
    QString text = archiveName;
    if (!internalDir.isEmpty()) {
        text += QStringLiteral("  /  ") + internalDir;
    }
    static_cast<ElidedLabel *>(m_locationLabel)->setFullText(text);
}

void ModernNavBar::setViewMode(const QString &mode)
{
    if (!m_viewGroup) {
        return;
    }
    for (QAbstractButton *button : m_viewGroup->buttons()) {
        button->setChecked(button->property("viewMode").toString() == mode);
    }
}

void ModernNavBar::setSortState(const QString &tooltip, bool descending)
{
    if (!m_sortButton) {
        return;
    }
    m_sortButton->setToolTip(tooltip);
    m_sortButton->setIcon(
        Icons::modern(descending ? QStringLiteral("sort-down.png")
                                 : QStringLiteral("sort-up.png")));
}

void ModernNavBar::setNavigationState(bool canGoBack, bool canGoForward,
                                      bool canGoUp)
{
    m_backButton->setEnabled(canGoBack);
    m_forwardButton->setEnabled(canGoForward);
    m_upButton->setEnabled(canGoUp);
}

QString ModernNavBar::searchText() const
{
    return m_searchEdit->text();
}

void ModernNavBar::setSearchText(const QString &text)
{
    if (m_searchEdit->text() == text) {
        return;
    }
    const bool wasBlocked = m_searchEdit->blockSignals(true);
    m_searchEdit->setText(text);
    m_searchEdit->blockSignals(wasBlocked);
}

void ModernNavBar::clearSearch()
{
    m_searchEdit->clear();
}

void ModernNavBar::focusSearch()
{
    m_searchEdit->setFocus(Qt::ShortcutFocusReason);
    m_searchEdit->selectAll();
}
