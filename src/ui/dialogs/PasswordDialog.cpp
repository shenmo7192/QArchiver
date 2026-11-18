// QArchiver / 小Q解压
// PasswordDialog initialization from preferences and password visibility.
// PasswordDialog 从偏好设置初始化以及密码可见性处理。

#include "PasswordDialog.h"

#include "core/AppSettings.h"
#include "utils/IconUtils.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>

PasswordDialog::PasswordDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Enter Password"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto *label = new QLabel(tr("This archive is encrypted. Please enter the password:"), this);
    label->setWordWrap(true);

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("Password"));

    m_rememberCheck = new QCheckBox(tr("Remember password for this session"), this);
    // The checkbox is only a per-prompt override: it starts from the
    // preference, and changing it here never writes the preference back.
    // 复选框仅覆盖本次提示：初始值来自偏好设置，
    // 在此处修改不会回写偏好设置。
    m_rememberCheck->setChecked(AppSettings::instance().rememberPasswords());

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto *ok = buttons->button(QDialogButtonBox::Ok)) {
        ok->setIcon(qarchiverIcon(QStringLiteral("yes.ico")));
    }
    if (auto *cancel = buttons->button(QDialogButtonBox::Cancel)) {
        cancel->setIcon(qarchiverIcon(QStringLiteral("no.ico")));
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *form = new QFormLayout;
    form->addRow(tr("&Password:"), m_passwordEdit);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addLayout(form);
    layout->addWidget(m_rememberCheck);
    layout->addWidget(buttons);

    connect(m_passwordEdit, &QLineEdit::returnPressed, this, &QDialog::accept);
}

QString PasswordDialog::password() const
{
    return m_passwordEdit->text();
}

bool PasswordDialog::rememberPassword() const
{
    return m_rememberCheck->isChecked();
}
