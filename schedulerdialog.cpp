#include "schedulerdialog.hpp"
#include "ui_schedulerdialog.h"
#include <QMessageBox>
#include <QValidator>

SchedulerDialog::SchedulerDialog(QWidget *parent) :
  QDialog(parent), ui(new Ui::SchedulerDialog)
{
  ui->setupUi( this );
  ui->time_line->setValidator( new QIntValidator() );

  QObject::connect( ui->cancel_button, &QPushButton::clicked, this, &SchedulerDialog::reject );
  QObject::connect( ui->ok_button, &QPushButton::clicked, [this]{
    QString const text { ui->search_line->text().trimmed() };
    QString const time_interval_secs{ ui->time_line->text().trimmed() };
    if( text.isEmpty() || text.isEmpty() ){
      QMessageBox::critical( this, "Error", "This cannot be empty" );
      ui->search_line->setFocus();
      return ;
    }
    bool ok{ true };
    time_interval_secs.toInt(&ok);
    if( !ok ){
      QMessageBox::critical( this, "Error", "Invalid number" );
      ui->time_line->setFocus();
      return;
    }
    this->accept();
  });
}

SchedulerDialog::~SchedulerDialog()
{
  delete ui;
}

int SchedulerDialog::GetTime() const
{
  return ui->time_line->text().trimmed().toInt();
}

QString SchedulerDialog::GetText() const
{
  return ui->search_line->text().toLower();
}
