#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>

class QComboBox;
class QSpinBox;
class QCheckBox;

// Preferences dialog (design doc 4.6)
class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget *parent = nullptr);

private slots:
    void saveSettings();

private:
    QComboBox *m_defaultFormatCombo = nullptr;
    QSpinBox *m_defaultLevelSpin = nullptr;
    QComboBox *m_overwriteCombo = nullptr;
    QCheckBox *m_preservePathsCheck = nullptr;
    QCheckBox *m_openAfterCheck = nullptr;
    QSpinBox *m_maxPreviewSpin = nullptr;
    QCheckBox *m_followSymlinksCheck = nullptr;
    QComboBox *m_themeCombo = nullptr;
};

#endif // PREFERENCESDIALOG_H
