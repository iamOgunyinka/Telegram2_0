#ifndef SCHEDULERDIALOG_HPP
#define SCHEDULERDIALOG_HPP

#include <QDialog>

namespace Ui {
class SchedulerDialog;
}

class SchedulerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SchedulerDialog(QWidget *parent = nullptr);
    ~SchedulerDialog()override;
    [[nodiscard]] QString GetText() const;
    [[nodiscard]] auto GetTime() const -> int;
private:
    Ui::SchedulerDialog *ui;
};

#endif // SCHEDULERDIALOG_HPP
