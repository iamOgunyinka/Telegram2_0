#include "accountwidget.hpp"
#include "ui_accountwidget.h"
#include <QBitmap>
#include <QLineEdit>

AccountWidget::AccountWidget( QString const &phone_number, QWidget *parent) :
  QWidget(parent), ui(new Ui::AccountWidget)
{
  ui->setupUi(this);
  ui->phone_number_label->setText( phone_number );
  ui->groups_combo->setEditable( true );
  ui->groups_combo->installEventFilter( this );
  QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                    &AccountWidget::OnSelectionChanged );
  SetStatus( user_status_e::offline );
  ui->groups_combo->setVisible( false );
}

AccountWidget::~AccountWidget()
{
  delete ui;
}

bool AccountWidget::eventFilter( QObject *object, QEvent *event )
{
  //disable keyboard use in countries combo
  if( object == ui->groups_combo && event->type() == QEvent::KeyPress ){
    return true;
  }
  return QWidget::eventFilter( object, event );
}

void AccountWidget::SetChecked( bool const checked )
{
  ui->checkBox->setChecked( checked );
  emit is_selected( checked );
}

void AccountWidget::SetStatus( user_status_e const status )
{
  QString filename{};
  if( status == user_status_e::offline ){
    filename = ":/images/offline.png";
  } else {
    filename = ":/images/online.png";
  }

  QPixmap const offline_pixmap_temp{ filename };
  auto const offline_pixmap = offline_pixmap_temp.scaled( 20, 20 );
  ui->status_label->setPixmap( offline_pixmap );
  ui->status_label->setMask( offline_pixmap.mask() );
}

QStandardItem* CreateCheckableItem( QString const &caption, bool checked )
{
  auto item = new QStandardItem( caption );
  item->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
  item->setData( checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole );
  return item;
}

void SetComboText( QComboBox *combo, const QString & text )
{
  combo->setCurrentText( "" );
  combo->lineEdit()->clear();
  combo->lineEdit()->setPlaceholderText( text );
}

bool AccountWidget::IsSelected() const
{
  return ui->checkBox->isChecked();
}

QString AccountWidget::PhoneNumber() const {
  return ui->phone_number_label->text();
}

void AccountWidget::OnSelectionChanged( QStandardItem *item )
{
  if( group_model_.indexFromItem( item ).row() == 0 ){
    auto const check_state = item->checkState();
    QObject::disconnect( &group_model_, &QStandardItemModel::itemChanged, this,
                         &AccountWidget::OnSelectionChanged );
    selected_items_.clear();
    for( int i = 1; i != group_model_.rowCount(); ++i ){
      auto current_item = group_model_.item( i );
      if( check_state == Qt::Checked ) selected_items_.push_back( current_item );
      current_item->setCheckState( check_state );
    }
    if( check_state == Qt::Checked ){
      SetComboText( ui->groups_combo, "Select all" );
    }
    QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                      &AccountWidget::OnSelectionChanged );
    return;
  }
  QString const item_text = item->text();
  if( item->checkState() == Qt::Checked ){
    selected_items_.append( item );
  } else {
    selected_items_.removeAll( item );
  }
  QString text {};
  for( auto const & current_item: selected_items_ ){
    text.append( current_item->text() + "," );
  }
  if( text.endsWith( ',' ) ) text.chop( 1 );

  if( selected_items_.isEmpty() ){
    if( group_model_.item( 0 )->checkState() == Qt::Checked ){
      QObject::disconnect( &group_model_, &QStandardItemModel::itemChanged, this,
                           &AccountWidget::OnSelectionChanged );
      group_model_.item( 0 )->setCheckState( Qt::Unchecked );
      QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                        &AccountWidget::OnSelectionChanged );
    }
    SetComboText( ui->groups_combo, "" );
  } else if( selected_items_.size() == 1 ) {
    if( group_model_.item( 0 )->checkState() == Qt::Checked &&
        group_model_.rowCount() - 1 != 1 )
    {
      group_model_.disconnect( this );
      group_model_.item( 0 )->setCheckState( Qt::Unchecked );
      QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                        &AccountWidget::OnSelectionChanged );
    }
    SetComboText( ui->groups_combo, text );
  } else {
    if( group_model_.item( 0 )->checkState() == Qt::Checked &&
        item->checkState() == Qt::Unchecked )
    {
      group_model_.disconnect( this );
      group_model_.item( 0 )->setCheckState( Qt::Unchecked );
      QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                        &AccountWidget::OnSelectionChanged );
    } else if( group_model_.item( 0 )->checkState() == Qt::Unchecked &&
               group_model_.rowCount() - 1 == selected_items_.size() )
    {
      group_model_.disconnect( this );
      group_model_.item( 0 )->setCheckState( Qt::Checked );
      SetComboText( ui->groups_combo, "Select all" );
      QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                        &AccountWidget::OnSelectionChanged );
      return;
    }
    SetComboText( ui->groups_combo, text );
  }
}
