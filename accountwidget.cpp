#include "accountwidget.hpp"
#include "ui_accountwidget.h"
#include <QBitmap>
#include <QLineEdit>

QStandardItem* CreateCheckableItem( QString const &caption, bool checked )
{
  auto item = new QStandardItem( caption );
  item->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
  item->setData( checked ? Qt::Checked : Qt::Unchecked, Qt::CheckStateRole );
  return item;
}

AccountWidget::AccountWidget(QString const &phone_number, QWidget *parent ):
  QWidget(parent), ui(new Ui::AccountWidget), group_names_{}
{
  ui->setupUi(this);
  ui->phone_number_label->setText( phone_number );
  ui->groups_combo->setEditable( true );
  ui->groups_combo->installEventFilter( this );
  ui->groups_combo->setVisible( false );
  SetStatus( user_status_e::offline );
}

AccountWidget::~AccountWidget()
{
  delete ui;
}

void AccountWidget::PopulateModel()
{
  group_model_.clear();
  group_model_.setColumnCount( 1 );

  QFontMetrics const fm( ui->groups_combo->font() );
  int const max_length = 400;
  int new_max_length = max_length;

  for( auto const & group_name: group_names_ ){
    auto elided_text = fm.elidedText( group_name,Qt::ElideMiddle, max_length );
    while( elided_texts_.contains( elided_text ) && new_max_length >= 80 ){
      new_max_length -= 1;
      elided_text = fm.elidedText( group_name,Qt::ElideMiddle, new_max_length );
    }
    if( new_max_length < 30 ){
      new_max_length = max_length;
    }
    elided_texts_[elided_text] = group_name;
    group_model_.appendRow( CreateCheckableItem( elided_text, false ));
  }

  if( group_model_.rowCount() > 1 ){
    group_model_.insertRow( 0, CreateCheckableItem( tr("Select all"), false ) );
  }
  ui->groups_combo->setModel( &group_model_ );
  QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                    &AccountWidget::OnSelectionChanged );
  if( !ui->groups_combo->isVisible() && group_model_.rowCount() > 0 ){
    ui->groups_combo->setVisible( true );
  }
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

void AccountWidget::HideGroup()
{
  group_names_.clear();
  group_model_.clear();
  ui->groups_combo->clear();
  ui->groups_combo->hide();
}

void AccountWidget::SetStatus( user_status_e const status )
{
  QString filename{};
  if( status == user_status_e::offline ){
    filename = ":/icons/images/offline.png";
  } else {
    filename = ":/icons/images/online.png";
  }

  QPixmap const offline_pixmap_temp{ filename };
  auto const offline_pixmap = offline_pixmap_temp.scaled( 20, 20 );
  ui->status_label->setPixmap( offline_pixmap );
  ui->status_label->setMask( offline_pixmap.mask() );
}

bool AccountWidget::IsSelected() const
{
  return ui->checkBox->isChecked();
}

QString AccountWidget::PhoneNumber() const {
  return ui->phone_number_label->text();
}

void AccountWidget::AddGroupName( QPair<std::int64_t const, QString> const &group_name )
{
  if( group_names_.contains(group_name.first) ){
    return;
  }
  group_names_.insert( group_name.first, group_name.second );
  PopulateModel();
}

QVector<std::int64_t> AccountWidget::SelectedItems() const
{
  int const index = ( group_model_.rowCount() > 0 && group_model_.item( 0 )->text()
                      != tr( "Select all" ) ) ? 0: 1;

  QVector<std::int64_t> selected_items{};
  for( int i = index; i < group_model_.rowCount(); ++i ){
    if( auto item = group_model_.item( i ); item->checkState() == Qt::Checked ){
      selected_items.push_back( group_names_.key( elided_texts_[item->text()] ) );
    }
  }
  return selected_items;
}

void AccountWidget::OnSelectionChanged( QStandardItem *item )
{
  if( group_model_.indexFromItem( item ).row() == 0 ){
    auto const check_state = item->checkState();
    QObject::disconnect( &group_model_, &QStandardItemModel::itemChanged, this,
                         &AccountWidget::OnSelectionChanged );
    total_selected = 0;
    for( int i = 1; i != group_model_.rowCount(); ++i ){
      auto current_item = group_model_.item( i );
      if( check_state == Qt::Checked ){
        ++total_selected;
      }
      current_item->setCheckState( check_state );
    }
    QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                      &AccountWidget::OnSelectionChanged );
    return;
  }

  if( item->checkState() == Qt::Checked ){
    ++total_selected;
  } else {
    --total_selected;
  }

  if( total_selected == 0 ){
    if( group_model_.item( 0 )->checkState() == Qt::Checked ){
      QObject::disconnect( &group_model_, &QStandardItemModel::itemChanged, this,
                           &AccountWidget::OnSelectionChanged );
      group_model_.item( 0 )->setCheckState( Qt::Unchecked );
      QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                        &AccountWidget::OnSelectionChanged );
    }
  } else if( total_selected == 1 ) {
    if( group_model_.item( 0 )->checkState() == Qt::Checked &&
        group_model_.rowCount() - 1 != 1 )
    {
      group_model_.disconnect( this );
      group_model_.item( 0 )->setCheckState( Qt::Unchecked );
      QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                        &AccountWidget::OnSelectionChanged );
    }
  } else {
    if( group_model_.item( 0 )->checkState() == Qt::Checked &&
        item->checkState() == Qt::Unchecked )
    {
      group_model_.disconnect( this );
      group_model_.item( 0 )->setCheckState( Qt::Unchecked );
      QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                        &AccountWidget::OnSelectionChanged );
    } else if( group_model_.item( 0 )->checkState() == Qt::Unchecked &&
               ( group_model_.rowCount() - 1 ) == total_selected )
    {
      group_model_.disconnect( this );
      group_model_.item( 0 )->setCheckState( Qt::Checked );
      QObject::connect( &group_model_, &QStandardItemModel::itemChanged, this,
                        &AccountWidget::OnSelectionChanged );
      return;
    }
  }
}
