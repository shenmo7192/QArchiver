#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

// QArchiver / 小Q解压
// Application preferences editor.
// 应用偏好设置编辑器。

#include <QDialog>

class QComboBox;
class QFormLayout;
class QSpinBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

// Preferences dialog (design doc 4.6).
// 偏好设置对话框（设计文档 4.6）。
class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget *parent = nullptr);

private slots:
    void saveSettings();

private:
    void updateOneClickControls();
    void updateBackgroundControls();
    void updateLayoutVisibility();

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
    QComboBox *m_layoutModeCombo = nullptr;
    QComboBox *m_viewModeCombo = nullptr;
    QComboBox *m_toolbarStyleCombo = nullptr;
    QCheckBox *m_showRecentFilesCheck = nullptr;
    QSpinBox *m_recentFilesLimitSpin = nullptr;

    // Rows whose visibility depends on the selected interface layout.
    // 随所选界面布局切换显示状态的行。
    QFormLayout *m_layoutForm = nullptr;
    QFormLayout *m_recentForm = nullptr;

    // One-click extraction / 一键解压
    QComboBox *m_oneClickDestModeCombo = nullptr;
    QLineEdit *m_oneClickDestEdit = nullptr;
    QPushButton *m_oneClickBrowseButton = nullptr;
    QCheckBox *m_oneClickCloseCheck = nullptr;

    // Optional semi-transparent background image.
    // 可选的半透明背景图片。
    QLineEdit *m_backgroundPathEdit = nullptr;
    QComboBox *m_backgroundAlignCombo = nullptr;
    QSpinBox *m_backgroundWidthSpin = nullptr;
    QSpinBox *m_backgroundHeightSpin = nullptr;
    QSpinBox *m_backgroundOpacitySpin = nullptr;
    QPushButton *m_backgroundClearButton = nullptr;
};

#endif // PREFERENCESDIALOG_H
