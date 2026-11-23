#ifndef FILEASSOCIATIONDIALOG_H
#define FILEASSOCIATIONDIALOG_H

// QArchiver / 小Q解压
// File association manager for archive formats.
// 压缩包格式文件关联管理对话框。

#include <QDialog>

class QLabel;
class QListWidget;
class QPushButton;

// Lets the user choose which archive formats open with QArchiver.  Windows
// writes per-user ProgIDs; Linux updates the desktop MIME defaults.
// 让用户选择哪些压缩包格式由 小Q解压 打开。Windows 写入当前用户 ProgID；
// Linux 更新桌面 MIME 默认值。
class FileAssociationDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileAssociationDialog(QWidget *parent = nullptr);

private slots:
    void applyAssociations();
    void selectAll();
    void selectNone();
    void openSystemDefaults();

private:
    void refreshStatus();

    QListWidget *m_list = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_hint = nullptr;
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_systemButton = nullptr;
};

#endif // FILEASSOCIATIONDIALOG_H
