#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QVector>

namespace Ui {
class AccountWidget;
}

enum user_status_e
{
  offline = 0,
  online
};

class AccountWidget : public QWidget
{
  Q_OBJECT
signals:
  void is_selected( bool const );
public:
  explicit AccountWidget( QString const &, QWidget *parent = nullptr);
  ~AccountWidget();
  bool eventFilter( QObject*, QEvent* ) override;

  void SetChecked( bool );
  void SetStatus( user_status_e );
  bool IsSelected() const;
  QString PhoneNumber() const;
private:
  void OnSelectionChanged( QStandardItem* );
private:
  Ui::AccountWidget *ui;
  QStandardItemModel group_model_{};
  QVector<QStandardItem*> selected_items_{};
};

