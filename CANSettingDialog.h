#ifndef CANSETTINGDIALOG_H
#define CANSETTINGDIALOG_H

#include <QDialog>
#include <Common.h>

namespace Ui {
class CANSettingDialog;
}

class CANSettingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CANSettingDialog(QWidget *parent = nullptr);
    ~CANSettingDialog();

    CANSettingParam& getParam();
    void setParam(CANSettingParam& setting);

private:
    Ui::CANSettingDialog *ui;
    CANSettingParam settingResult;
};

#endif // CANSETTINGDIALOG_H
