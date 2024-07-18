#include "CANSettingDialog.h"
#include "ui_CANSettingDialog.h"

CANSettingDialog::CANSettingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CANSettingDialog)
{
    ui->setupUi(this);

    setWindowTitle("CAN Setting");
}

CANSettingDialog::~CANSettingDialog()
{
    delete ui;
}

CANSettingParam &CANSettingDialog::getParam()
{
    settingResult.sBitrate  = ui->leBitrate->text();
    settingResult.CANID     = ui->sbCANID->value();
    settingResult.FlowID    = ui->sbFlowID->value();
    settingResult.BlockSize = ui->sbBlockSize->value();
    settingResult.STmin     = ui->sbSTmin->value();

    return settingResult;
}

void CANSettingDialog::setParam(CANSettingParam &setting)
{
    if ( setting.sBitrate.isEmpty() ) {
        ui->leBitrate->setText(bitrate);
    } else {
        ui->leBitrate->setText(setting.sBitrate);
    }

    ui->sbCANID->setValue(setting.CANID);
    ui->sbFlowID->setValue(setting.FlowID);
    ui->sbBlockSize->setValue(setting.BlockSize);
    ui->sbSTmin->setValue(setting.STmin);
}
