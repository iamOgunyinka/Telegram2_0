#include "registrationdialog.hpp"
#include "ui_registrationdialog.h"
#include <QMessageBox>
#include <random>

enum Constants
{
    ThirtyTwoBits = 32
};

RegistrationDialog::RegistrationDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::RegistrationDialog)
{
    ui->setupUi(this);
    QObject::connect( ui->register_button, &QPushButton::clicked, [=]{
        if( ui->phone_number_line->text().trimmed().isEmpty() ){
            QMessageBox::critical( this, "Error", "Cannot leave this field empty" );
            ui->phone_number_line->setFocus();
            return;
        }
        if( ui->encryption_key_line->text().trimmed().isEmpty() ){
            ui->encryption_key_line->setText( RandomString( ThirtyTwoBits ) );
        }
        this->accept();
    });
}

auto RegistrationDialog::AsciiRandomChoice() -> char
{
    static char const *alphabets { "abcdefghijklmnopqrstuvwxyz_ABCDEFGHIJKLMNOPQRSTUVWXYZ" };
    static std::random_device rd{};
    static std::mt19937  generator{ rd() };
    static std::uniform_int_distribution<> ui_distr( 0, std::strlen( alphabets ) - 1 );
    return alphabets[ui_distr( generator )];
}

auto RegistrationDialog::RandomString( int const length ) -> QString
{
    QString str{};
    for( int i = 0; i != length; ++i ) str.push_back( AsciiRandomChoice() );
    return str;
}

RegistrationDialog::~RegistrationDialog()
{
    delete ui;
}

QString RegistrationDialog::PhoneNumber() const
{
    return ui->phone_number_line->text().trimmed();
}

QString RegistrationDialog::EncryptionKey() const
{
    return ui->encryption_key_line->text().trimmed();
}
