#ifndef PCANISOTP_H
#define PCANISOTP_H

#include "PCANComm.h"
#include <PCAN-ISO-TP_2016.h>
#include <QElapsedTimer>
#include <QMutex>


class PCANISOTP : public PCANComm
{
public:
    explicit PCANISOTP(QObject *parent = nullptr);

    bool Open(cantp_handle ch, char* pBitrate) override;
    void Close() override;
    void Read() override;
    void Write(const char* pData, int nSize) override;

    // Use PCAN-ISOTP API
    /**
     * @brief ISO-TP Receive Message Mapping
     * @details ISO-TP 수신 정보 매핑(GUI -> TestProgram)
     * @param channel Mapping 할 chnnel
     * @param can_msgtype CAN Message Type
     * @param can_id GUI에서 전송하는 메시지의 CAN ID
     * @param can_id_flow_ctrl TestProgram에서 Flow Control을 할 때 사용할 CAN ID
     * @param isotp_msgtype ISO-TP Message Type
     * @param isotp_format ISO-TP Addressing Format
     * @return cantp_status
     */
    cantp_status initRecvMsgMappings(cantp_handle channel, ushort nTargetIdx, uint32_t can_id, uint32_t can_id_flow_ctrl);
    /**
     * @brief ISO-TP Send Message Mapping
     * @details ISO-TP 송신 정보 매핑(TestProgram -> GUI)
     * @param channel Mapping 할 chnnel
     * @param can_msgtype CAN Message Type
     * @param can_id TestProgram에서 전송하는 메시지의 CAN ID
     * @param can_id_flow_ctrl GUI에서 Flow Control을 할 때 사용할 CAN ID
     * @param isotp_msgtype ISO-TP Message Type
     * @param isotp_format ISO-TP Addressing Format
     * @return cantp_status
     */
    void initmapping(QVector<mappingData> &mappingInfo);
    cantp_status initSendMsgMappings(cantp_handle channel, ushort nTargetIdx, uint32_t can_id, uint32_t can_id_flow_ctrl);
    int read_segmented_message(cantp_handle channeld);
    bool checkWriteState(cantp_msg &tx_msg);
    void WriteCANFD(bool isFD, bool isExtended, uint32_t canID, const char* pData, int nSize);
    void WriteISOTP(const char* pData, int nSize, int nTagetIdx);
    void setWriteFlag(bool b);
    bool getWriteFlag();
    void processRead(cantp_msg& rx_msg);

private:
    cantp_can_msgtype m_msgType;

    QList<uint32_t> sendCANIDList;
    QList<uint32_t> recvCANIDList;

    uint8_t nSingleFrameDataSize = 30;

    int m_nMappingCount;
    QMutex writeMutex;
    bool bWrite;
};

#endif // PCANISOTP_H
