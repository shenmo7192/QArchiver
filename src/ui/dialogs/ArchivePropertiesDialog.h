#ifndef ARCHIVEPROPERTIESDIALOG_H
#define ARCHIVEPROPERTIESDIALOG_H

#include <QDialog>
#include "core/ArchiveConstants.h"

class ArchiveBackend;

// Archive properties dialog (design doc 6 - dialogs)
class ArchivePropertiesDialog : public QDialog {
    Q_OBJECT
public:
    explicit ArchivePropertiesDialog(ArchiveBackend *backend, QWidget *parent = nullptr);
};

#endif // ARCHIVEPROPERTIESDIALOG_H
