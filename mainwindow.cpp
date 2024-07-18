#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "CANSettingDialog.h"
#include <PCANBasic.h>
#include <QTime>
#include <QKeyEvent>

PCANChannelComboboxHandle::PCANChannelComboboxHandle(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT(qobject_cast<QComboBox*>(parent) != nullptr);
}

PCANChannelComboboxHandle::~PCANChannelComboboxHandle()
{
}

/**
 * @brief PCAN Channel Combobox 클릭하여 Show 됐을 때 처리할 Handler 함수
 * @param object
 * @param event
 * @return true
 * @return false
 */
bool PCANChannelComboboxHandle::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress )
    {
        auto comboboxPtr = qobject_cast<QComboBox*>(object);
        if ( comboboxPtr->isEnabled() ) {
            comboboxPtr->clear();
            addPCANChanneltoComboBox(comboboxPtr);
        }
    }
    return false;
}

void PCANChannelComboboxHandle::addPCANChanneltoComboBox(QComboBox* pComboBox)
{
    int nChannelCount = 0;

    if ( CAN_GetValue(PCAN_NONEBUS, PCAN_ATTACHED_CHANNELS_COUNT, &nChannelCount, sizeof(nChannelCount)) == PCAN_ERROR_OK ) {
        if ( nChannelCount > 0 ) {
            TPCANChannelInformation* pChannels = new TPCANChannelInformation[nChannelCount];
            if ( CAN_GetValue(PCAN_NONEBUS, PCAN_ATTACHED_CHANNELS, pChannels, nChannelCount * sizeof(TPCANChannelInformation)) == PCAN_ERROR_OK) {
                for (int i=0; i<nChannelCount; i++ ) {
                    if ( pChannels[i].channel_condition == PCAN_CHANNEL_AVAILABLE ) {
                        int nUSBChannelID = 0;
                        if ( pChannels[i].channel_handle >= PCAN_USBBUS1 && pChannels[i].channel_handle <= PCAN_USBBUS8 ) {
                            nUSBChannelID = pChannels[i].channel_handle - 0x50U;
                        } else if ( pChannels[i].channel_handle >= PCAN_USBBUS9 && pChannels[i].channel_handle <= PCAN_USBBUS16 ) {
                            nUSBChannelID = pChannels[i].channel_handle - 0x509U;
                        } else {
                            continue;
                        } // PCAN USB Handle 값이 1~8 / 9~16이 달라서 채널 값만 빼기 위해 분기처리
                        QString sChannelName = QString::asprintf("%1 %2")
                                                   .arg(pChannels[i].device_name).arg(nUSBChannelID);
                        pComboBox->addItem(sChannelName, pChannels[i].channel_handle);
                    }
                }
            }
        }
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , settingParam()
    , comm(nullptr)
    , readMsgQueue(nullptr)
    , bIsRunning(false)
{
    ui->setupUi(this);

    setWindowTitle("CAN Chat Program");

    initForm();
    initConnect();
    loadCANSettingParam();
}

MainWindow::~MainWindow()
{
    bIsRunning = false;

    delete ui;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if ( event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return ) {
        writeMessage();
    }
}

void MainWindow::initForm()
{
    ui->cbChannel->installEventFilter(new PCANChannelComboboxHandle(ui->cbChannel));
    PCANChannelComboboxHandle::addPCANChanneltoComboBox(ui->cbChannel);

    ui->gbSendCAN->setEnabled(false);
    ui->CANStream->setEnabled(false);
}

void MainWindow::initConnect()
{
    connect ( ui->btnSetting, &QPushButton::clicked, [&] {
        CANSettingDialog* pDialog = new CANSettingDialog(this);
        pDialog->setParam(settingParam);
        pDialog->exec();
        settingParam = pDialog->getParam();
        delete pDialog;

        writeSetting(Key::Bitrate, QString(settingParam.sBitrate));
        writeSetting(Key::BlockSize, settingParam.BlockSize);
        writeSetting(Key::STmin, settingParam.STmin);
        writeSetting(Key::CANID, settingParam.CANID);
        writeSetting(Key::FlowID, settingParam.FlowID);
    });

    connect ( ui->btnOpen, &QPushButton::clicked, [&] {
        if ( ui->btnOpen->text().toLower() == "open" ) {
            comm = new PCANComm(this);
            if ( comm->Open(static_cast<cantp_handle>(ui->cbChannel->currentData().toInt()), settingParam) ) {
                readMsgQueue = new QQueue<cantp_msg>();
                comm->setReadDataQueue(readMsgQueue);

                ui->btnOpen->setText("close");
                ui->gbSendCAN->setEnabled(true);
                ui->CANStream->setEnabled(true);

                ui->cbChannel->setEnabled(false);
                ui->btnSetting->setEnabled(false);

                bIsRunning = true;

                readDataFuture = std::async(std::launch::async, [&] {
                    while(bIsRunning) {
                        comm->Read();
                    }
                });

                readQueueFuture = std::async(std::launch::async, [&] {
                    while(bIsRunning) {
                        while ( !readMsgQueue->isEmpty() ) {
                            auto rx_msg = readMsgQueue->dequeue();
                            QString sMsg = QTime::currentTime().toString("[hh:mm:ss.zzz] (ID : %1,").arg(QString::asprintf("0x%X", rx_msg.can_info.can_id));
                            if ( rx_msg.type == PCANTP_MSGTYPE_CANFD ) {
                                sMsg.append(QString(" %1) - %2")
                                                .arg("CANFD")
                                                .arg(QString(reinterpret_cast<char*>(rx_msg.msgdata.canfd->data))));
                                // CAN-FD로 받는 경우 데이터 뒤에 "\0" 데이터가 들어와서 따로 처리 필요 X

                            } else if ( rx_msg.type == PCANTP_MSGTYPE_ISOTP ) {
                                auto nSize = rx_msg.msgdata.any->length;
                                QString sData = QString(reinterpret_cast<char*>(rx_msg.msgdata.isotp->data));
                                sData.resize(nSize);

                                sMsg.append(QString(" %1) - %2")
                                                .arg("ISOTP")
                                                .arg(sData));
                            }

                            CANTP_MsgDataFree_2016(&rx_msg);

                            QMetaObject::invokeMethod(this, [&, sMsg] {
                                ui->CANStream->append(sMsg);
                            });
                        }
                    }
                });
            } else {
                if ( comm != nullptr ) { delete comm; }

            }
        } else if ( ui->btnOpen->text().toLower() == "close" ) {
            bIsRunning = false;
            readQueueFuture.get();
            readDataFuture.get();

            if ( comm != nullptr ) { delete comm; }
            if ( readMsgQueue != nullptr ) { delete readMsgQueue; }

            ui->btnOpen->setText("Open");
            ui->gbSendCAN->setEnabled(false);
            ui->CANStream->setEnabled(false);

            ui->cbChannel->setEnabled(true);
            ui->btnSetting->setEnabled(true);
        }
    });

    connect ( ui->btnSend, &QPushButton::clicked, [&] {
        writeMessage();
    });
}

void MainWindow::loadCANSettingParam()
{
    settingParam.sBitrate   = getSetting(Key::Bitrate).toString();
    settingParam.BlockSize  = getSetting(Key::BlockSize).toInt();
    settingParam.STmin      = getSetting(Key::STmin).toInt();
    settingParam.CANID      = getSetting(Key::CANID).toInt();
    settingParam.FlowID     = getSetting(Key::FlowID).toInt();

    if ( getSetting(Key::CANType).toString() == "CANFD" ) {
        ui->rbCANFD->setChecked(true);
    } else if ( getSetting(Key::CANType).toString() == "ISOTP") {
        ui->rbISOTP->setChecked(true);
    } else {
        ui->rbISOTP->setChecked(true);
    }
}

void MainWindow::writeMessage()
{
    if ( comm == nullptr ) { return; }

    comm->Write(ui->lineEdit->text().toLocal8Bit().data(),
                    ui->lineEdit->text().toLocal8Bit().size(),
                    ui->rbCANFD->isChecked());
    ui->lineEdit->clear();
    writeSetting(Key::CANType, ui->rbISOTP->isChecked() ? "ISOTP" : "CANFD" );


}

void MainWindow::writeSetting(const QString &sKey, const QVariant &qValue)
{
    QSettings setting(saveFileName, QSettings::IniFormat);
    setting.setValue(sKey, qValue);
}

QVariant MainWindow::getSetting(const QString &sKey)
{
    QSettings setting(saveFileName, QSettings::IniFormat);
    return setting.value(sKey);
}
