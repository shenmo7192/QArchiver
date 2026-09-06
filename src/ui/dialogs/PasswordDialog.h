#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <QDialog>

class QLineEdit;
class QCheckBox;

// Password input dialog (design doc - opened when encrypted archive detected)
class PasswordDialog : public QDialog {
    Q_OBJECT
public:
    explicit PasswordDialog(QWidget *parent = nullptr);

    QString password() const;
    bool rememberPassword() const;

private:
    QLineEdit *m_passwordEdit = nullptr;
    QCheckBox *m_rememberCheck = nullptr;
};

#endif // PASSWORDDIALOG_H
