#pragma once

#include <QWidget>
#include <QStandardItemModel>
#include <QVector>
#include <QMap>

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
  void HideGroup();
  void SetStatus( user_status_e );
  bool IsSelected() const;
  QString PhoneNumber() const;
  void SetGroupNames( QMap<int64_t, QString> const & );
  QVector<std::int64_t> SelectedItems() const;
private:
  void OnSelectionChanged( QStandardItem* );
  void PopulateModel();
private:
  Ui::AccountWidget *ui;
  QStandardItemModel group_model_{};
  int total_selected = 0;
  QMap<std::int64_t, QString> group_names_;
  QMap<QString, QString> elided_texts_{};
};

