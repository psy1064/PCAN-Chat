#ifndef PCANCOMM_H
#define PCANCOMM_H

#include <Common.h>
#include <QMainWindow>
#include <Windows.h>
#include <QMessageBox>
#include <QQueue>

class PCANComm
{
public:
    PCANComm(QMainWindow* p = nullptr);
    bool Open(cantp_handle ch, CANSettingParam& param);
    void Close();
    void Read();
    void Write(const char* pData, int nSize, bool isFD);
    void initmapping(uint32_t CANID, uint32_t FlowID, bool isStandard);

    void setReadDataQueue(QQueue<cantp_msg>* q) { readMsgQueue = q; }
private:
    HANDLE recvHandle;
    cantp_status status;
    cantp_handle channel;
    QMainWindow* parent;

    QQueue<cantp_msg>* readMsgQueue;

    uint32_t writecanID;
    uint32_t readcanID;
    bool isStandard;

    void readCallback(cantp_msg& rx_msg);
    void debug(const QString& sMsg) { QMessageBox::warning(parent, "PCAN Comm", sMsg); }
    void isotpReadLoopbackCheck(cantp_msg& rx_msg);
    void isotpWriteLoopbackCheck(cantp_msg& tx_msg);
};

#endif // PCANCOMM_H
