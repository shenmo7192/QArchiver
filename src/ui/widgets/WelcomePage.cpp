// QArchiver / 小Q解压
// Welcome page construction and recent-file refresh.
// 起始页构建与最近文件刷新。

#include "WelcomePage.h"

#include "core/RecentFilesManager.h"
#include "utils/FileTypeHelper.h"

#include <QFileInfo>
#include <QIcon>
#include <QPoint>
#include <QSize>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

WelcomePage::WelcomePage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("WelcomePage"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(36, 28, 36, 24);
    outer->setSpacing(18);
    outer->addStretch(1);

    // Drop zone / 拖放区
    auto *dropZone = new QFrame(this);
    dropZone->setObjectName(QStringLiteral("WelcomeDropZone"));
    dropZone->setMaximumWidth(820);
    auto *dropLayout = new QVBoxLayout(dropZone);
    dropLayout->setContentsMargins(32, 30, 32, 30);
    dropLayout->setSpacing(10);

    auto *icon = new QLabel(dropZone);
    icon->setPixmap(QIcon(QStringLiteral(":/modern/app.png")).pixmap(78, 78));
    icon->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(icon);

    auto *title = new QLabel(tr("Drop archives here"), dropZone);
    title->setObjectName(QStringLiteral("WelcomeTitle"));
    title->setAlignment(Qt::AlignCenter);
    dropLayout->addWidget(title);

    auto *subtitle = new QLabel(
        tr("Open, extract or compress 7z / ZIP / RAR / TAR / GZ / BZ2 / XZ "
           "archives. The modern interface contains no ads, bundles or "
           "security pop-ups."),
        dropZone);
    subtitle->setObjectName(QStringLiteral("WelcomeSubtitle"));
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setWordWrap(true);
    dropLayout->addWidget(subtitle);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(10);
    buttonRow->addStretch(1);
    auto *openButton = new QPushButton(tr("Open Archive..."), dropZone);
    openButton->setObjectName(QStringLiteral("WelcomePrimary"));
    openButton->setIcon(QIcon(QStringLiteral(":/modern/open.png")));
    openButton->setIconSize(QSize(18, 18));
    auto *oneClickButton = new QPushButton(tr("One-click Extract"), dropZone);
    oneClickButton->setObjectName(QStringLiteral("WelcomeSecondary"));
    oneClickButton->setIcon(QIcon(QStringLiteral(":/modern/oneclick.png")));
    oneClickButton->setIconSize(QSize(18, 18));
    buttonRow->addWidget(openButton);
    buttonRow->addWidget(oneClickButton);
    buttonRow->addStretch(1);
    dropLayout->addSpacing(4);
    dropLayout->addLayout(buttonRow);

    auto *hint = new QLabel(
        tr("Tip: drop a single archive to open it, or several files to "
           "create a new archive."),
        dropZone);
    hint->setObjectName(QStringLiteral("WelcomeSubtitle"));
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    dropLayout->addWidget(hint);

    dropZone->setMaximumWidth(900);
    outer->addWidget(dropZone);

    // Recent archives / 最近打开
    auto *recentHeader = new QHBoxLayout;
    auto *recentTitle = new QLabel(tr("Recent archives"), this);
    recentTitle->setObjectName(QStringLiteral("ModernPanelHeader"));
    auto *clearButton = new QToolButton(this);
    clearButton->setObjectName(QStringLiteral("ModernSmallToolButton"));
    clearButton->setText(tr("Clear"));
    clearButton->setToolTip(tr("Clear the recent archives list"));
    clearButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    recentHeader->addWidget(recentTitle);
    recentHeader->addStretch(1);
    recentHeader->addWidget(clearButton);

    m_recentList = new QListWidget(this);
    m_recentList->setObjectName(QStringLiteral("WelcomeRecentList"));
    m_recentList->setIconSize(QSize(20, 20));
    m_recentList->setMaximumHeight(250);
    m_recentList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_recentList->setToolTip(tr("Double-click an archive to open it."));

    m_recentEmpty = new QLabel(
        tr("Archives you open will appear here."), this);
    m_recentEmpty->setObjectName(QStringLiteral("WelcomeSubtitle"));
    m_recentEmpty->setAlignment(Qt::AlignCenter);

    auto *recentBox = new QVBoxLayout;
    recentBox->setContentsMargins(0, 0, 0, 0);
    recentBox->setSpacing(4);
    recentBox->addLayout(recentHeader);
    recentBox->addWidget(m_recentList);
    recentBox->addWidget(m_recentEmpty);

    auto *recentContainer = new QWidget(this);
    recentContainer->setMaximumWidth(900);
    recentContainer->setLayout(recentBox);
    outer->addWidget(recentContainer);
    outer->addStretch(1);

    connect(openButton, &QPushButton::clicked,
            this, &WelcomePage::openRequested);
    connect(oneClickButton, &QPushButton::clicked,
            this, &WelcomePage::oneClickExtractRequested);
    connect(clearButton, &QToolButton::clicked, this, []() {
        RecentFilesManager::instance().clear();
    });
    connect(&RecentFilesManager::instance(), &RecentFilesManager::changed,
            this, &WelcomePage::refreshRecent);
    connect(m_recentList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item) { activateItem(item); });
    connect(m_recentList, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
        QListWidgetItem *item = m_recentList->itemAt(pos);
        if (!item) {
            return;
        }
        const QString path = item->data(Qt::UserRole).toString();
        QMenu menu(this);
        menu.addAction(tr("Open"), this, [this, path]() {
            emit recentFileActivated(path);
        });
        menu.addAction(tr("Remove from list"), this, [path]() {
            RecentFilesManager::instance().remove(path);
        });
        menu.exec(m_recentList->viewport()->mapToGlobal(pos));
    });

    refreshRecent();
}

void WelcomePage::refreshRecent()
{
    if (!m_recentList) {
        return;
    }
    m_recentList->clear();
    const QStringList paths = RecentFilesManager::instance().validatedList();
    for (const QString &path : paths) {
        const QFileInfo info(path);
        auto *item = new QListWidgetItem(
            FileTypeHelper::icon(info.fileName(), false, true),
            info.fileName(), m_recentList);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }
    const bool empty = m_recentList->count() == 0;
    m_recentList->setVisible(!empty);
    m_recentEmpty->setVisible(empty);
}

void WelcomePage::activateItem(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    const QString path = item->data(Qt::UserRole).toString();
    if (!path.isEmpty()) {
        emit recentFileActivated(path);
    }
}
