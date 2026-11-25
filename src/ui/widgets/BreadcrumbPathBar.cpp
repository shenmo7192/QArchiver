// QArchiver / 小Q解压
// Windows 7 style breadcrumb location bar implementation.
// Windows 7 风格面包屑路径框实现。

#include "ui/widgets/BreadcrumbPathBar.h"

#include <QAction>
#include <QDir>
#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStackedLayout>
#include <QToolButton>

namespace {

// Never elide a level below this width; the remainder goes to the last level.
// 单级路径按钮的最小省略宽度；剩余空间优先留给最后一级。
constexpr int kMinElidedWidth = 26;

// Keep a clickable empty strip on the right so the text mode is always
// reachable, even when the path would otherwise fill the whole bar.
// 始终在右侧保留可单击的空白条，即使路径本身能占满整个框。
constexpr int kReservedBlankWidth = 16;

QString normalizeSlashes(const QString &path)
{
    QString normalized = QDir::fromNativeSeparators(path);
    while (normalized.size() > 1 && normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }
    return normalized;
}

} // namespace

BreadcrumbPathBar::BreadcrumbPathBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("BreadcrumbBar"));
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setMinimumWidth(80);

    m_stack = new QStackedLayout(this);
    m_stack->setContentsMargins(0, 0, 0, 0);

    // Breadcrumb page: buttons are packed to the left, the stretch keeps the
    // empty area on the right that switches to text mode.
    // 面包屑页：按钮靠左，右侧拉伸区就是切换到文本模式的空白区域。
    m_crumbPage = new QWidget(this);
    m_crumbPage->installEventFilter(this);
    m_crumbLayout = new QHBoxLayout(m_crumbPage);
    m_crumbLayout->setContentsMargins(4, 2, 4, 2);
    m_crumbLayout->setSpacing(1);
    m_crumbLayout->addStretch(1);
    m_stack->addWidget(m_crumbPage);

    // Read-only path page: the whole location is one selectable line.
    // 只读路径页：完整路径以单行文本显示，可选中复制。
    m_editPage = new QWidget(this);
    auto *editLayout = new QHBoxLayout(m_editPage);
    editLayout->setContentsMargins(4, 2, 4, 2);
    editLayout->setSpacing(0);
    m_edit = new QLineEdit(m_editPage);
    m_edit->setObjectName(QStringLiteral("BreadcrumbEdit"));
    m_edit->setReadOnly(true);
    m_edit->setFrame(false);
    m_edit->setCursorPosition(0);
    m_edit->installEventFilter(this);
    editLayout->addWidget(m_edit);
    m_stack->addWidget(m_editPage);
    m_stack->setCurrentWidget(m_crumbPage);

    m_overflowButton = new QToolButton(m_crumbPage);
    m_overflowButton->setObjectName(QStringLiteral("BreadcrumbOverflowButton"));
    m_overflowButton->setText(QStringLiteral("..."));
    m_overflowButton->setAutoRaise(true);
    m_overflowButton->setCursor(Qt::PointingHandCursor);
    m_overflowButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_overflowButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    m_overflowButton->setToolTip(tr("Show hidden levels"));
    m_overflowButton->hide();
    connect(m_overflowButton, &QToolButton::clicked,
            this, &BreadcrumbPathBar::openOverflowMenu);

    m_overflowMenu = new QMenu(this);
}

void BreadcrumbPathBar::setArchiveLocation(const QString &archiveName,
                                           const QString &internalDir)
{
    QVector<Segment> segments;
    const QString name =
        archiveName.isEmpty() ? tr("Archive") : archiveName;
    segments.append({name, QString()});

    QString normalized = normalizeSlashes(internalDir);
    while (normalized.startsWith(QLatin1Char('/'))) {
        normalized.remove(0, 1);
    }

    QString cumulative;
    const QStringList parts =
        normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        cumulative = cumulative.isEmpty()
                         ? part
                         : cumulative + QLatin1Char('/') + part;
        segments.append({part, cumulative});
    }

    QString fullText = name;
    if (!cumulative.isEmpty()) {
        fullText += QLatin1Char('/') + cumulative;
    }
    setSegments(Mode::Archive, segments, fullText);
}

void BreadcrumbPathBar::setLocalLocation(const QString &directory)
{
    const QString normalized = normalizeSlashes(directory);
    QVector<Segment> segments;

    QString current;
    QString rest = normalized;

    if (rest.startsWith(QStringLiteral("//"))) {
        // UNC path: keep server/share as the first clickable level.
        // UNC 路径：服务器/共享名作为第一级。
        const QStringList unc =
            rest.mid(2).split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (!unc.isEmpty()) {
            current = QStringLiteral("//") + unc.first();
            QString label = unc.first();
            if (unc.size() >= 2) {
                current += QLatin1Char('/') + unc.at(1);
                label = unc.at(1);
                rest = unc.mid(2).join(QLatin1Char('/'));
            } else {
                rest.clear();
            }
            segments.append({label, current});
        }
    } else if (normalized.size() >= 2
               && normalized.at(1) == QLatin1Char(':')) {
        // Windows drive root, e.g. "C:/".
        // Windows 盘符根目录，例如 "C:/"。
        current = normalized.left(2) + QLatin1Char('/');
        segments.append({normalized.left(2), current});
        rest = normalized.mid(2);
    } else if (normalized.startsWith(QLatin1Char('/'))) {
        // POSIX root.
        // POSIX 根目录。
        current = QStringLiteral("/");
        segments.append({QStringLiteral("/"), current});
        rest = normalized.mid(1);
    }

    const QStringList parts =
        rest.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (!current.isEmpty() && !current.endsWith(QLatin1Char('/'))) {
            current += QLatin1Char('/');
        }
        current += part;
        segments.append({part, current});
    }

    setSegments(Mode::Local, segments, QDir::toNativeSeparators(normalized));
}

void BreadcrumbPathBar::setPlaceholder(const QString &text)
{
    m_placeholderText = text;
    setSegments(Mode::Placeholder, {}, QString());
}

void BreadcrumbPathBar::setSegments(Mode mode,
                                    const QVector<Segment> &segments,
                                    const QString &fullText)
{
    m_mode = mode;
    m_segments = segments;
    m_fullText = fullText;
    setToolTip(fullText);

    if (m_edit) {
        const bool blocked = m_edit->blockSignals(true);
        m_edit->setText(fullText);
        m_edit->setCursorPosition(0);
        m_edit->blockSignals(blocked);
    }
    if (m_stack->currentWidget() == m_editPage) {
        m_stack->setCurrentWidget(m_crumbPage);
    }

    rebuildButtons();
    updateElision();
}

void BreadcrumbPathBar::rebuildButtons()
{
    // Keep the overflow button alive but detach it before clearing the layout.
    // 保留 "..." 按钮本身，清理布局前先将其移出。
    m_overflowButton->hide();
    m_crumbLayout->removeWidget(m_overflowButton);
    while (QLayoutItem *item = m_crumbLayout->takeAt(0)) {
        delete item;
    }
    qDeleteAll(m_buttons);
    m_buttons.clear();
    if (m_placeholderLabel) {
        delete m_placeholderLabel;
        m_placeholderLabel = nullptr;
    }
    m_hiddenIndexes.clear();

    if (m_mode == Mode::Placeholder || m_segments.isEmpty()) {
        m_placeholderLabel = new QLabel(m_placeholderText, m_crumbPage);
        m_placeholderLabel->setObjectName(
            QStringLiteral("BreadcrumbPlaceholder"));
        m_placeholderLabel->setSizePolicy(QSizePolicy::Ignored,
                                          QSizePolicy::Preferred);
        m_crumbLayout->addWidget(m_placeholderLabel, 1);
        return;
    }

    for (int i = 0; i < m_segments.size(); ++i) {
        const Segment &segment = m_segments.at(i);
        auto *button = new QToolButton(m_crumbPage);
        button->setObjectName(QStringLiteral("BreadcrumbButton"));
        button->setText(segment.label);
        button->setToolTip(segment.label);
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        button->setProperty("currentSegment",
                            i == m_segments.size() - 1);
        connect(button, &QToolButton::clicked, this, [this, i]() {
            activateSegment(i);
        });
        m_buttons.append(button);
    }

    m_crumbLayout->addWidget(m_buttons.first());
    m_crumbLayout->addWidget(m_overflowButton);
    for (int i = 1; i < m_buttons.size(); ++i) {
        m_crumbLayout->addWidget(m_buttons.at(i));
    }
    m_crumbLayout->addStretch(1);
}

void BreadcrumbPathBar::updateElision()
{
    if (m_updating || m_mode == Mode::Placeholder || m_buttons.isEmpty()) {
        return;
    }
    m_updating = true;

    const QMargins margins = m_crumbLayout->contentsMargins();
    const int available =
        qMax(0, width() - margins.left() - margins.right()
                    - kReservedBlankWidth);
    const int spacing = m_crumbLayout->spacing();
    const int count = m_buttons.size();

    // Start from the full labels and reveal as much as the width allows.
    // 先恢复完整文本，再按可用宽度逐级重新显示。
    for (int i = 0; i < count; ++i) {
        m_buttons.at(i)->setText(m_segments.at(i).label);
        m_buttons.at(i)->show();
    }
    m_overflowButton->hide();
    m_hiddenIndexes.clear();

    const auto buttonWidth = [this](int index) {
        return m_buttons.at(index)->sizeHint().width();
    };
    const auto overflowWidth = [this]() {
        return m_overflowButton->sizeHint().width();
    };

    int allWidth = spacing * (count - 1);
    for (int i = 0; i < count; ++i) {
        allWidth += buttonWidth(i);
    }
    if (allWidth <= available) {
        m_updating = false;
        return;
    }

    if (count == 1) {
        setElidedText(m_buttons.first(), m_segments.first().label, available);
        m_updating = false;
        return;
    }

    // Keep the first and last levels, hide the middle behind "...".  Then add
    // levels back from the end so the deepest directories stay visible.
    // 保留首尾层级，中间收进 "..."；再从末尾向前逐级恢复，优先显示更深目录。
    QVector<bool> visible(count, false);
    visible[0] = true;
    visible[count - 1] = true;
    const bool hasOverflow = count > 2;

    const auto totalWidth = [&](const QVector<bool> &shown,
                                bool withOverflow) {
        int total = 0;
        int shownCount = 0;
        for (int i = 0; i < count; ++i) {
            if (shown.at(i)) {
                total += buttonWidth(i);
                ++shownCount;
            }
        }
        if (withOverflow) {
            total += overflowWidth();
            ++shownCount;
        }
        if (shownCount > 1) {
            total += spacing * (shownCount - 1);
        }
        return total;
    };

    if (hasOverflow) {
        for (int i = count - 2; i >= 1; --i) {
            QVector<bool> candidate = visible;
            candidate[i] = true;
            if (totalWidth(candidate, true) <= available) {
                visible = candidate;
            } else {
                break;
            }
        }
    }

    for (int i = 0; i < count; ++i) {
        m_buttons.at(i)->setVisible(visible.at(i));
    }
    m_overflowButton->setVisible(hasOverflow);

    if (hasOverflow) {
        for (int i = 1; i < count - 1; ++i) {
            if (!visible.at(i)) {
                m_hiddenIndexes.append(i);
            }
        }
    }

    if (totalWidth(visible, hasOverflow) <= available) {
        m_updating = false;
        return;
    }

    // Still too narrow: elide the first level before the last one.
    // 仍然过窄：先省略开头，最后才省略末尾。
    const int overflow = hasOverflow ? overflowWidth() : 0;
    const int gaps = hasOverflow ? 2 : 1;
    const int lastNatural = buttonWidth(count - 1);
    const int firstBudget =
        available - lastNatural - overflow - spacing * gaps;

    if (firstBudget >= kMinElidedWidth) {
        setElidedText(m_buttons.first(), m_segments.first().label,
                      firstBudget);
    } else {
        setElidedText(m_buttons.first(), m_segments.first().label,
                      kMinElidedWidth);
        const int firstActual = buttonWidth(0);
        const int lastBudget =
            available - firstActual - overflow - spacing * gaps;
        setElidedText(m_buttons.at(count - 1),
                      m_segments.at(count - 1).label,
                      qMax(kMinElidedWidth, lastBudget));
    }

    m_updating = false;
}

void BreadcrumbPathBar::setElidedText(QToolButton *button,
                                      const QString &label,
                                      int maxWidth)
{
    if (!button) {
        return;
    }
    button->setText(label);
    const int natural = button->sizeHint().width();
    if (natural <= maxWidth) {
        return;
    }

    const QFontMetrics metrics(button->font());
    const int textWidth = metrics.horizontalAdvance(label);
    const int chrome = qMax(0, natural - textWidth);
    const int budget = qMax(1, maxWidth - chrome);
    QString elided = metrics.elidedText(label, Qt::ElideRight, budget);
    if (elided.isEmpty()) {
        elided = QStringLiteral("…");
    }
    button->setText(elided);
}

void BreadcrumbPathBar::activateSegment(int index)
{
    if (index < 0 || index >= m_segments.size()) {
        return;
    }
    const QString target = m_segments.at(index).target;
    if (m_mode == Mode::Archive) {
        emit archiveSegmentActivated(target);
    } else if (m_mode == Mode::Local) {
        emit localSegmentActivated(target);
    }
    showCrumbs();
}

void BreadcrumbPathBar::openOverflowMenu()
{
    if (!m_overflowMenu || m_hiddenIndexes.isEmpty()) {
        return;
    }
    m_overflowMenu->clear();
    for (int index : m_hiddenIndexes) {
        const Segment &segment = m_segments.at(index);
        QAction *action = m_overflowMenu->addAction(segment.label);
        action->setToolTip(segment.target);
        connect(action, &QAction::triggered, this, [this, index]() {
            activateSegment(index);
        });
    }
    m_overflowMenu->popup(m_overflowButton->mapToGlobal(
        QPoint(0, m_overflowButton->height())));
}

void BreadcrumbPathBar::showTextMode()
{
    if (m_mode == Mode::Placeholder || m_fullText.isEmpty()) {
        return;
    }
    const bool blocked = m_edit->blockSignals(true);
    m_edit->setText(m_fullText);
    m_edit->setCursorPosition(0);
    m_edit->blockSignals(blocked);
    m_stack->setCurrentWidget(m_editPage);
    m_edit->setFocus(Qt::MouseFocusReason);
    m_edit->selectAll();
}

void BreadcrumbPathBar::showCrumbs()
{
    if (!m_stack || !m_crumbPage) {
        return;
    }
    if (m_stack->currentWidget() != m_crumbPage) {
        m_stack->setCurrentWidget(m_crumbPage);
        updateElision();
    }
}

void BreadcrumbPathBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateElision();
}

void BreadcrumbPathBar::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateElision();
}

void BreadcrumbPathBar::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange
        || event->type() == QEvent::StyleChange) {
        updateElision();
    }
}

bool BreadcrumbPathBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_crumbPage) {
        // A click that reaches the page itself (not a level button) lands in
        // the empty area and switches to the selectable path text.
        // 落在页面本身（而非层级按钮）的单击即空白区域，切换为可选中路径文本。
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                showTextMode();
            }
        }
    } else if (watched == m_edit) {
        if (event->type() == QEvent::FocusOut) {
            showCrumbs();
        } else if (event->type() == QEvent::KeyPress) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Escape
                || key->key() == Qt::Key_Return
                || key->key() == Qt::Key_Enter) {
                showCrumbs();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
