#include "ArchivePropertiesDialog.h"

#include "backends/ArchiveBackend.h"

#include <QFormLayout>
#include <QLabel>
#include <QFileInfo>
#include <QLocale>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QList>
#include "core/ArchiveEntry.h"
#include "utils/IconUtils.h"

ArchivePropertiesDialog::ArchivePropertiesDialog(ArchiveBackend *backend, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Archive Properties"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QFileInfo fi(backend->archivePath());

    auto *form = new QFormLayout;

    form->addRow(tr("Path:"), new QLabel(fi.absoluteFilePath()));
    form->addRow(tr("Format:"), new QLabel(formatToString(backend->format())));
    form->addRow(tr("Size:"), new QLabel(QLocale().formattedDataSize(fi.size())));
    form->addRow(tr("Read-only:"), new QLabel(backend->isReadOnly() ? tr("Yes") : tr("No")));
    form->addRow(tr("Supports encryption:"), new QLabel(backend->supportsEncryption() ? tr("Yes") : tr("No")));
    form->addRow(tr("Supports volumes:"), new QLabel(backend->supportsVolume() ? tr("Yes") : tr("No")));
    form->addRow(tr("Supports append:"), new QLabel(backend->supportsAppend() ? tr("Yes") : tr("No")));

    QString comment = backend->archiveComment();
    if (!comment.isEmpty()) {
        form->addRow(tr("Comment:"), new QLabel(comment));
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    if (auto *ok = buttons->button(QDialogButtonBox::Ok)) {
        ok->setIcon(qarchiverIcon(QStringLiteral("yes.ico")));
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}
