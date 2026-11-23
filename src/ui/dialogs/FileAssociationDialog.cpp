// QArchiver / 小Q解压
// File association manager dialog implementation.
// 文件关联管理对话框实现。

#include "ui/dialogs/FileAssociationDialog.h"

#include "utils/FileAssociations.h"
#include "ui/icons/MessageBoxUtils.h"

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

FileAssociationDialog::FileAssociationDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("File Associations"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumWidth(520);

    auto *intro = new QLabel(
        tr("Choose the archive formats that should open with QArchiver. "
           "This changes the current user's associations only."),
        this);
    intro->setWordWrap(true);

    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::NoSelection);
    for (const QString &extension : FileAssociations::extensions()) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  —  %2")
                .arg(extension, FileAssociations::descriptionFor(extension)),
            m_list);
        item->setData(Qt::UserRole, extension);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(
            FileAssociations::isFormatRegistered(extension)
                ? Qt::Checked
                : Qt::Unchecked);
    }

    m_hint = new QLabel(this);
    m_hint->setWordWrap(true);
    m_hint->setObjectName(QStringLiteral("ModernDetailsValue"));
#ifdef Q_OS_WIN
    m_hint->setText(
        tr("On Windows 10/11, Windows may keep the previous default app. "
           "Use \"Open Default Apps\" to confirm the choice if needed."));
#elif defined(Q_OS_LINUX)
    m_hint->setText(
        tr("On Linux the desktop environment stores the default application. "
           "The 'qarchiver.desktop' entry must be installed for associations "
           "to work."));
#else
    m_hint->setText(
        tr("File associations are not supported on this platform."));
#endif

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setObjectName(QStringLiteral("ModernDetailsValue"));

    auto *selectAllButton = new QPushButton(tr("Select All"), this);
    auto *selectNoneButton = new QPushButton(tr("Select None"), this);
    m_applyButton = new QPushButton(tr("Apply"), this);
    m_applyButton->setDefault(true);
    m_systemButton = new QPushButton(tr("Open Default Apps"), this);
#ifdef Q_OS_WIN
    m_systemButton->setVisible(true);
#else
    m_systemButton->setVisible(false);
#endif

    auto *closeButton = new QPushButton(tr("Close"), this);
    closeButton->setIcon(
        style()->standardIcon(QStyle::SP_DialogCloseButton));

    auto *topRow = new QHBoxLayout;
    topRow->addWidget(selectAllButton);
    topRow->addWidget(selectNoneButton);
    topRow->addStretch(1);
    topRow->addWidget(m_systemButton);

    auto *bottomRow = new QHBoxLayout;
    bottomRow->addWidget(m_applyButton);
    bottomRow->addStretch(1);
    bottomRow->addWidget(closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 12);
    layout->setSpacing(10);
    layout->addWidget(intro);
    layout->addWidget(m_list, 1);
    layout->addLayout(topRow);
    layout->addWidget(m_hint);
    layout->addWidget(m_status);
    layout->addLayout(bottomRow);

    connect(selectAllButton, &QPushButton::clicked,
            this, &FileAssociationDialog::selectAll);
    connect(selectNoneButton, &QPushButton::clicked,
            this, &FileAssociationDialog::selectNone);
    connect(m_applyButton, &QPushButton::clicked,
            this, &FileAssociationDialog::applyAssociations);
    connect(m_systemButton, &QPushButton::clicked,
            this, &FileAssociationDialog::openSystemDefaults);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    m_applyButton->setEnabled(FileAssociations::isSupported());
    refreshStatus();
}

void FileAssociationDialog::selectAll()
{
    for (int i = 0; i < m_list->count(); ++i) {
        m_list->item(i)->setCheckState(Qt::Checked);
    }
}

void FileAssociationDialog::selectNone()
{
    for (int i = 0; i < m_list->count(); ++i) {
        m_list->item(i)->setCheckState(Qt::Unchecked);
    }
}

void FileAssociationDialog::applyAssociations()
{
    QStringList selected;
    QStringList unselected;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const QString extension = item->data(Qt::UserRole).toString();
        if (item->checkState() == Qt::Checked) {
            selected << extension;
        } else {
            unselected << extension;
        }
    }

    QString error;
    if (!FileAssociations::unregisterFormats(unselected, &error)
        || !FileAssociations::registerFormats(selected, &error)) {
        m_status->setText(
            error.isEmpty() ? tr("Could not update the file associations.")
                            : error);
        MessageBoxUtils::showWarning(this, tr("File Associations"),
                                     m_status->text());
        return;
    }

    m_status->setText(
        selected.isEmpty()
            ? tr("Associations removed.")
            : tr("Associated %1 format(s).").arg(selected.size()));
    refreshStatus();
}

void FileAssociationDialog::openSystemDefaults()
{
    QString error;
    if (!FileAssociations::openDefaultAppsSettings(&error)) {
        MessageBoxUtils::showWarning(
            this, tr("File Associations"),
            error.isEmpty()
                ? tr("Open the system default apps settings and choose "
                     "QArchiver for the archive formats.")
                : error);
    }
}

void FileAssociationDialog::refreshStatus()
{
    int registered = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        const QString extension =
            m_list->item(i)->data(Qt::UserRole).toString();
        const bool isRegistered =
            FileAssociations::isFormatRegistered(extension);
        m_list->item(i)->setCheckState(isRegistered ? Qt::Checked
                                                    : Qt::Unchecked);
        if (isRegistered) {
            ++registered;
        }
    }
    if (registered == 0) {
        m_status->setText(tr("No archive format is associated with QArchiver."));
    } else {
        m_status->setText(
            tr("%1 of %2 archive formats are associated with QArchiver.")
                .arg(registered)
                .arg(m_list->count()));
    }
}
