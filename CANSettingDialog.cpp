#include "CANSettingDialog.h"
#include "ui_CANSettingDialog.h"

CANSettingDialog::CANSettingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CANSettingDialog)
{
    ui->setupUi(this);
    setWindowTitle("CAN Setting");

    for ( int i=1; i < 16; i++ ) {
        ui->cbDataLength->addItem(QString::number(ConvertDLtoSize(i)), i);
    }

    connect ( ui->rbStandard, &QRadioButton::clicked, [&](bool isChecked){
        if ( isChecked ) {
            ui->sbCANID->setMaximum(StandardAddrMax);
            ui->sbFlowID->setMaximum(StandardAddrMax);
        }
    });

    connect ( ui->rbExtended, &QRadioButton::clicked, [&](bool isChecked){
        if ( isChecked ) {
            ui->sbCANID->setMaximum(ExtendedAddrMax);
            ui->sbFlowID->setMaximum(ExtendedAddrMax);
        }
    });

}

CANSettingDialog::~CANSettingDialog()
{
    delete ui;
}

CANSettingParam &CANSettingDialog::getParam()
{
    settingResult.sBitrate      = ui->leBitrate->text();
    settingResult.CANID         = ui->sbCANID->value();
    settingResult.FlowID        = ui->sbFlowID->value();
    settingResult.BlockSize     = ui->sbBlockSize->value();
    settingResult.STmin         = ui->sbSTmin->value();
    settingResult.CANDL         = ui->cbDataLength->currentData(Qt::UserRole).toInt();
    settingResult.addrStandard  = ui->rbStandard->isChecked();

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
    ui->cbDataLength->setCurrentIndex(setting.CANDL - 1);   // CAN DL 값은 1부터 시작

    setting.addrStandard ? ui->rbStandard->setChecked(true) : ui->rbExtended->setChecked(true);
}

