#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>
#include <QString>

class QProgressBar;
class QLabel;
class QPlainTextEdit;
class QPushButton;

// Progress dialog showing extraction/compression progress (design doc 7)
class ProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit ProgressDialog(QWidget *parent = nullptr);

    void setOperation(const QString &title);
    void updateProgress(int percent, int total, int current, int etaSec);
    void appendOutput(const QString &line);
    void setIndeterminate(bool indeterminate);

signals:
    void cancelled();

private:
    QProgressBar *m_progressBar = nullptr;
    QLabel *m_fileLabel = nullptr;
    QLabel *m_statsLabel = nullptr;
    QPlainTextEdit *m_output = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};

#endif // PROGRESSDIALOG_H
