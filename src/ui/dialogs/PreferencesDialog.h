#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

// QArchiver / 小Q解压
// Application preferences editor.
// 应用偏好设置编辑器。

#include <QDialog>

class QComboBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QLineEdit;

// Preferences dialog (design doc 4.6).
// 偏好设置对话框（设计文档 4.6）。
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
    QSpinBox *m_continuousPreviewSpin = nullptr;
    QCheckBox *m_followSymlinksCheck = nullptr;
    QCheckBox *m_rememberPasswordsCheck = nullptr;
    QCheckBox *m_libArchiveCheck = nullptr;
    QLabel *m_libArchiveHint = nullptr;
    QComboBox *m_themeCombo = nullptr;
    QComboBox *m_viewModeCombo = nullptr;
    QComboBox *m_toolbarStyleCombo = nullptr;
    QCheckBox *m_showRecentFilesCheck = nullptr;
    QSpinBox *m_recentFilesLimitSpin = nullptr;

    // Optional semi-transparent background image.
    // 可选的半透明背景图片。
    QLineEdit *m_backgroundPathEdit = nullptr;
    QComboBox *m_backgroundAlignCombo = nullptr;
    QSpinBox *m_backgroundWidthSpin = nullptr;
    QSpinBox *m_backgroundHeightSpin = nullptr;
    QSpinBox *m_backgroundOpacitySpin = nullptr;
};

#endif // PREFERENCESDIALOG_H
