#ifndef EXTRACTDIALOG_H
#define EXTRACTDIALOG_H

// QArchiver / 小Q解压
// Dialog for extraction destination and overwrite options.
// 选择解压目标与覆盖选项的对话框。

#include <QDialog>
#include "common/ArchiveOptions.h"

class QLineEdit;
class QCheckBox;
class QComboBox;
class QPushButton;

// Extract options dialog (design doc 5.1.1/5.1.2).
// 解压选项对话框（设计文档 5.1.1/5.1.2）。
class ExtractDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExtractDialog(const QString &archivePath, QWidget *parent = nullptr);

    void setSelectedFiles(const QStringList &paths);
    ExtractOptions options() const;

private slots:
    void browseDestDir();

private:
    QString m_archivePath;
    QStringList m_selectedFiles;
    QLineEdit *m_destEdit = nullptr;
    QCheckBox *m_preservePathsCheck = nullptr;
    QCheckBox *m_openAfterCheck = nullptr;
    QCheckBox *m_testBeforeCheck = nullptr;
    QComboBox *m_overwriteCombo = nullptr;
};

#endif // EXTRACTDIALOG_H
