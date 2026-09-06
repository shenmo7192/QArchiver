#ifndef COMPRESSDIALOG_H
#define COMPRESSDIALOG_H

#include <QDialog>
#include "core/ArchiveOptions.h"
#include "core/ArchiveConstants.h"

class QLineEdit;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;

// New archive creation dialog (design doc 5.2.1/5.2.2)
class CompressDialog : public QDialog {
    Q_OBJECT
public:
    explicit CompressDialog(const QStringList &sourceFiles, QWidget *parent = nullptr);

    CompressOptions options() const;
    ArchiveFormat selectedFormat() const;

private slots:
    void browseTarget();
    void onFormatChanged();

private:
    QStringList m_sourceFiles;
    QLineEdit *m_targetEdit = nullptr;
    QComboBox *m_formatCombo = nullptr;
    QSpinBox *m_levelSpin = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QCheckBox *m_encryptHeadersCheck = nullptr;
    QSpinBox *m_volumeSpin = nullptr;
    QComboBox *m_volumeUnit = nullptr;
    QCheckBox *m_followSymlinksCheck = nullptr;
};

#endif // COMPRESSDIALOG_H
