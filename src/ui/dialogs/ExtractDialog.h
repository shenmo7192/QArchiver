#ifndef EXTRACTDIALOG_H
#define EXTRACTDIALOG_H

#include <QDialog>
#include "core/ArchiveOptions.h"

class QLineEdit;
class QCheckBox;
class QComboBox;
class QPushButton;

// Extract options dialog (design doc 5.1.1/5.1.2)
class ExtractDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExtractDialog(const QString &archivePath, QWidget *parent = nullptr);

    ExtractOptions options() const;

private slots:
    void browseDestDir();

private:
    QString m_archivePath;
    QLineEdit *m_destEdit = nullptr;
    QCheckBox *m_preservePathsCheck = nullptr;
    QCheckBox *m_openAfterCheck = nullptr;
    QComboBox *m_overwriteCombo = nullptr;
};

#endif // EXTRACTDIALOG_H
