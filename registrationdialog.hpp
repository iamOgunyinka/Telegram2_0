#ifndef REGISTRATIONDIALOG_HPP
#define REGISTRATIONDIALOG_HPP

#include <QDialog>

namespace Ui {
class RegistrationDialog;
}

class RegistrationDialog : public QDialog
{
    Q_OBJECT
    QString RandomString( int length ) const;
    char AsciiRandomChoice() const;
public:
    explicit RegistrationDialog(QWidget *parent = nullptr);
    ~RegistrationDialog() override;
    [[nodiscard]]QString PhoneNumber() const;
    [[nodiscard]]QString EncryptionKey() const;
private:
    Ui::RegistrationDialog *ui;
};

#endif // REGISTRATIONDIALOG_HPP
