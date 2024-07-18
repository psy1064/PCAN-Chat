#include "PCANISOTP.h"
#include <QTime>
#include <QMutexLocker>


PCANISOTP::PCANISOTP(QObject *parent)
    : PCANComm{parent} ,
    bWrite(false)
{
}

bool PCANISOTP::Open(cantp_handle ch, char* pBitrate)
{
    bool bResult = false;
    channel = ch;
    status = CANTP_InitializeFD_2016(channel, pBitrate);

    if (!CANTP_StatusIsOk_2016(status)) {
        debug(QString::asprintf("ISOTP Initialization failed"));
        CANTP_Uninitialize_2016(channel);
        return bResult;
    }

    BYTE param;
    int16_t nParam;
    param = CAN_TX_DL;
    status = CANTP_SetValue_2016(channel, PCANTP_PARAMETER_CAN_TX_DL, &param, 1);
    if (!CANTP_StatusIsOk_2016(status)) {
        debug(QString::asprintf("Failed to configure parameter 'PCANTP_PARAMETER_CAN_TX_DL' (sts=0x%04X).", status));
        return bResult;
    }

    param = 0x55;
    status = CANTP_SetValue_2016(channel, PCANTP_PARAMETER_CAN_PADDING_VALUE, &param, 1);
    if (!CANTP_StatusIsOk_2016(status)) {
        debug(QString::asprintf("Failed to configure parameter 'PCANTP_PARAMETER_CAN_PADDING_VALUE' (sts=0x%04X).", status));
        return bResult;
    }

    nParam = m_nBlockSize;
    status = CANTP_SetValue_2016(channel, PCANTP_PARAMETER_BLOCK_SIZE, &nParam, 2);
    if (!CANTP_StatusIsOk_2016(status)) {
        debug(QString::asprintf("Failed to configure parameter 'PCANTP_PARAMETER_BLOCK_SIZE' (sts=0x%04X).", status));
        return bResult;
    }

    nParam = m_nSTmin;
    status = CANTP_SetValue_2016(channel, PCANTP_PARAMETER_SEPARATION_TIME, &nParam, 2);
    if (!CANTP_StatusIsOk_2016(status)) {
        debug(QString::asprintf("Failed to configure parameter 'PCANTP_PARAMETER_SEPARATION_TIME' (sts=0x%04X).", status));
        return bResult;
    }

    recvHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
    status = CANTP_SetValue_2016(channel, PCANTP_PARAMETER_RECEIVE_EVENT, &recvHandle, sizeof(recvHandle));
    if ( !CANTP_StatusIsOk_2016(status) ) {
        debug("Failed to create receieve event");
        return bResult;
    }

    bResult = true;

    return bResult;
}

void PCANISOTP::Close()
{
    CANTP_Uninitialize_2016(channel);
}

void PCANISOTP::Read()
{
    DWORD result = WaitForSingleObject(recvHandle, 100);
    if ( result == WAIT_OBJECT_0 ) {
        read_segmented_message(channel);
    }
}

int PCANISOTP::read_segmented_message(cantp_handle channel)
{
    cantp_msg rx_msg;
    memset(&rx_msg, 0, sizeof(cantp_msg));

    if ( getWriteFlag() ) { return 0; }     // 현재 Write 후 아직 Loopback 체크 전이라면 잠시 대기 => loopback 메시지가 여기서 감지되면 로직이 꼬임
    status = CANTP_Read_2016(channel, &rx_msg, NULL, PCANTP_MSGTYPE_NONE);

    if (CANTP_StatusIsOk_2016(status, PCANTP_STATUS_OK, false) ) {
        qDebug() << rx_msg.msgdata.any->length;
        if ( rx_msg.msgdata.any->length <= nSingleFrameDataSize) {
            emit emit_sendPacketData(enRx, rx_msg);
            return 1;
        }
        if ( recvCANIDList.indexOf(rx_msg.can_info.can_id) == -1 ) { return -1; }

        if (rx_msg.msgdata.any->flags & PCANTP_MSGFLAG_LOOPBACK) {
            return -1;
        } // loopback message의 경우 pass

        if (((PCANTP_MSGTYPE_ISOTP & rx_msg.type) == PCANTP_MSGTYPE_ISOTP)
            && ((rx_msg.msgdata.isotp->netaddrinfo.msgtype & PCANTP_ISOTP_MSGTYPE_FLAG_INDICATION_RX) == PCANTP_ISOTP_MSGTYPE_FLAG_INDICATION_RX)) {
            processRead(rx_msg);
            // The message is received, read it
        } // 잔여 데이터가 있을 경우 계속 체크 후 다 들어왔다면 다시 한번 Read

        emit emit_sendPacketData(enRx, rx_msg);
    }

    return 1;
}

void PCANISOTP::processRead(cantp_msg &rx_msg)
{
    cantp_msgprogress progress;     // ISO-TP 메시지가 다 들어왔는지 체크하기 위함
    memset(&progress, 0, sizeof(cantp_msgprogress));
    cantp_msg tmp;

    do {
        status = CANTP_GetMsgProgress_2016(channel, &rx_msg, PCANTP_MSGDIRECTION_RX, &progress);
    } while (progress.state == PCANTP_MSGPROGRESS_STATE_PROCESSING);

    if ( progress.state == PCANTP_MSGPROGRESS_STATE_COMPLETED ) {
        CANTP_MsgCopy_2016(&tmp, &rx_msg);
        status = CANTP_Read_2016(channel, &rx_msg, NULL, PCANTP_MSGTYPE_NONE);
        if ( recvCANIDList.indexOf(rx_msg.can_info.can_id) == -1 )  {
            if ( recvCANIDList.indexOf(tmp.can_info.can_id) != -1 ) {
                CANTP_MsgCopy_2016(&rx_msg, &tmp);
            }
        }

    }
}

void PCANISOTP::Write(const char *pData, int nSize)
{
    return;
}

void PCANISOTP::WriteCANFD(bool isFD, bool isExtended, uint32_t canID, const char *pData, int nSize)
{
    cantp_msg message = {};

    cantp_msgtype cantype;
    cantp_can_msgtype msgtype;
    if ( isExtended ) { msgtype = PCANTP_CAN_MSGTYPE_EXTENDED; }
    else              { msgtype = PCANTP_CAN_MSGTYPE_STANDARD; }

    if ( isFD ) {
        cantype = PCANTP_MSGTYPE_CANFD;
        msgtype |= (PCANTP_CAN_MSGTYPE_BRS | PCANTP_CAN_MSGTYPE_FD);
    } else {
        cantype = PCANTP_MSGTYPE_CAN;
    }

    status = CANTP_MsgDataAlloc_2016(&message, cantype);
    if (CANTP_StatusIsOk_2016(status)) {
        // initialize ISOTP message
        status = CANTP_MsgDataInit_2016(&message, canID, msgtype, nSize, pData, NULL);
        if (CANTP_StatusIsOk_2016(status)) {
            // write message
            status = CANTP_Write_2016(channel, &message);
            if (CANTP_StatusIsOk_2016(status)) {
                debug(QString::asprintf("Successfully queued FD message: Length %i (sts=0x%04X).", nSize, (int)status));
            }
            else {
                debug(QString::asprintf("Failed to write FD message: Length %i (sts=0x%04X).", nSize, (int)status));
                return;
            }
//            emit emit_sendPacketData(enTx, message);
        }
        else {
            debug(QString::asprintf("Failed to initialize FD message: Length %i (sts=0x%04X).", nSize, (int)status));
            return;
        }
    }
    else {
        debug(QString::asprintf("Failed to allocate message (sts=0x%04X).", status));
        return;
    }
}

void PCANISOTP::WriteISOTP(const char *pData, int nSize, int nTagetIdx)
{
    cantp_msg message = {};
    cantp_msgprogress progress;     // ISO-TP 메시지가 다 들어왔는지 체크하기 위함
    memset(&progress, 0, sizeof(cantp_msgprogress));
    cantp_can_msgtype msgtype = PCANTP_CAN_MSGTYPE_EXTENDED | PCANTP_CAN_MSGTYPE_FD | PCANTP_CAN_MSGTYPE_BRS;

    cantp_netaddrinfo isotp_nai = {};
    isotp_nai.source_addr = N_SA;
    isotp_nai.target_addr = N_TA_PHYS + nTagetIdx;
    isotp_nai.target_type = PCANTP_ISOTP_ADDRESSING_PHYSICAL;
    isotp_nai.msgtype = PCANTP_ISOTP_MSGTYPE_DIAGNOSTIC;
    isotp_nai.format = PCANTP_ISOTP_FORMAT_NORMAL;

    status = CANTP_MsgDataAlloc_2016(&message, PCANTP_MSGTYPE_ISOTP);
    if (CANTP_StatusIsOk_2016(status)) {
        // initialize ISOTP message
        status = CANTP_MsgDataInit_2016(&message, PCANTP_MAPPING_FLOW_CTRL_NONE, msgtype, nSize, pData, &isotp_nai);
        if (CANTP_StatusIsOk_2016(status)) {
            // write message                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                        /
            setWriteFlag(true);     // Write 호출 시 Read 쪽에서 Read 한번 하면 loopback 체크가 불가
            status = CANTP_Write_2016(channel, &message);
            if (CANTP_StatusIsOk_2016(status)) {
                if (checkWriteState(message) ) {
                    setWriteFlag(false);
//                    emit emit_sendPacketData(enTx, message);
//                    debug(QString::asprintf("Successfully queued ISOTP message: Length %i (sts=0x%04X).", nSize, (int)status));
                    return;
                }
            }
            else {
                debug(QString::asprintf("Failed to write ISOTP message: Length %i (sts=0x%04X).", nSize, (int)status));
            }
        }
        else {
            debug(QString::asprintf("Failed to initialize ISOTP message: Length %i (sts=0x%04X).", nSize, (int)status));
        }
        // release message

        if (!CANTP_StatusIsOk_2016(status)) {
            debug(QString::asprintf("Failed to deallocate message (sts=0x%04X).", status));
        }
    }
    else {
        debug(QString::asprintf("Failed to allocate message (sts=0x%04X).", status));
    }

    CANTP_MsgDataFree_2016(&message);
    setWriteFlag(false);
    // write 성공 시에는 emit 호출된 slot에서 free 시켜야 함
}

bool PCANISOTP::checkWriteState(cantp_msg& tx_msg)
{
    if ( tx_msg.msgdata.any->length <= nSingleFrameDataSize) { return true; }
    // ISOTP Header를 제외한 30바이트보다 작게 쓸 경우에는 Single Frame을 쏘기 때문에 Check가 불가
    cantp_msg loopback_msg;
    cantp_msgprogress progress;
    cantp_status result;

    bool bRes = false;
    memset(&loopback_msg, 0, sizeof(loopback_msg));
    memset(&progress, 0, sizeof(progress));

    do {
        do {
            result = CANTP_Read_2016(channel, &loopback_msg, NULL, PCANTP_MSGTYPE_ANY);
            Sleep(10);
        } while ( result == PCANTP_STATUS_NO_rx_msg );
        // Message가 있을 때까지 Read 시도

        if (CANTP_StatusIsOk_2016(result, PCANTP_STATUS_OK, false)) {
            if ( recvCANIDList.indexOf(loopback_msg.can_info.can_id) != -1 ) {
                processRead(loopback_msg);      // Read 신호가 Write Loopback Check에서 감지된 경우 나머지 처리
                break;
            }
            if (((PCANTP_MSGTYPE_ISOTP & loopback_msg.type) == PCANTP_MSGTYPE_ISOTP) && (loopback_msg.msgdata.isotp->flags == PCANTP_MSGFLAG_LOOPBACK)
                && ((loopback_msg.msgdata.isotp->netaddrinfo.msgtype & PCANTP_ISOTP_MSGTYPE_FLAG_INDICATION_TX) == PCANTP_ISOTP_MSGTYPE_FLAG_INDICATION_TX)) {
                do {
                    result = CANTP_GetMsgProgress_2016(channel, &loopback_msg, PCANTP_MSGDIRECTION_TX, &progress);
                } while (progress.state == PCANTP_MSGPROGRESS_STATE_PROCESSING);
                // The message is being received, wait and show progress

            } else {
                continue;
            }

            if ( CANTP_MsgEqual_2016(&tx_msg, &loopback_msg, true) ) {
                bRes = true;
                CANTP_MsgDataFree_2016(&loopback_msg);
            } // loopback msg와 tx msg를 체크하여
        }
    } while ( !bRes );

    return bRes;
}


void PCANISOTP::setWriteFlag(bool b)
{
    QMutexLocker m(&writeMutex);
    bWrite = b;
}

bool PCANISOTP::getWriteFlag()
{
    QMutexLocker m(&writeMutex);
    return bWrite;
}

void PCANISOTP::initmapping(QVector<mappingData>& mappingInfo)
{
    // remove Mapping
    uint32_t count = 256;
    cantp_mapping mappings[256];
    status = CANTP_GetMappings_2016(channel, mappings, &count);
    if (!CANTP_StatusIsOk_2016(status)) {
        debug(QString::asprintf("Failed to get mapping info (sts=0x%04X).", status));
        return;
    }

    for (int i = 0; i < count; i++) {
        status = CANTP_RemoveMapping_2016(channel, mappings[i].uid);

        if (!CANTP_StatusIsOk_2016(status)) {
            debug(QString::asprintf("Failed to remove mapping info (sts=0x%04X).", status));
            return;
        }
    }
    sendCANIDList.clear();
    recvCANIDList.clear();

    for ( int i=0; i < mappingInfo.count(); i++ ) {
        auto mapData = mappingInfo.value(i);
        status = initSendMsgMappings(channel, i, mapData.nCANID, mapData.nFlowID);
        if ( !CANTP_StatusIsOk_2016(status) ) {
            debug(QString::asprintf("Failed to Send initialize mapping"));
            return;
        } // ISO-TP 주소 매핑
        // CAN ID = 내가 쓸 때의 ISOTP CAN ID
        // FlowID = 상대방(GUI)에서 응답하는 FlowControl ID

        status = initRecvMsgMappings(channel, i, mapData.nFlowID, mapData.nCANID);
        if ( !CANTP_StatusIsOk_2016(status) ) {
            debug(QString::asprintf("Failed to Recv initialize mapping"));
            return;
        } // ISO-TP 주소 매핑
        // CAN ID = 상대방의 ISOTP CAN ID
        // FlowID = 내가 보내는 FlowControl ID

        // PCAN-ISO-TP API Usermanual p.265 / ISO-TP Document p.40 참고

        sendCANIDList.append(mapData.nCANID);
        recvCANIDList.append(mapData.nFlowID);
    }

    m_nMappingCount = mappingInfo.count();
}


cantp_status PCANISOTP::initRecvMsgMappings(cantp_handle channel, ushort nTargetIdx, uint32_t can_id, uint32_t can_id_flow_ctrl)
{
    cantp_mapping mapping_phys_rx;

    // clean variables (it is common to leave can_tx_dl uninitialized which can lead to invalid 0xCC values)
    memset(&mapping_phys_rx, 0, sizeof(mapping_phys_rx));

    // configure a mapping to transmit physical message
    mapping_phys_rx.can_id = can_id;
    mapping_phys_rx.can_id_flow_ctrl = can_id_flow_ctrl;
    mapping_phys_rx.can_msgtype = PCANTP_CAN_MSGTYPE_EXTENDED;
    mapping_phys_rx.netaddrinfo.format = PCANTP_ISOTP_FORMAT_NORMAL;
    mapping_phys_rx.netaddrinfo.msgtype = PCANTP_ISOTP_MSGTYPE_DIAGNOSTIC;
    mapping_phys_rx.netaddrinfo.target_type = PCANTP_ISOTP_ADDRESSING_PHYSICAL;
    mapping_phys_rx.netaddrinfo.source_addr = N_TA_PHYS + nTargetIdx;
    mapping_phys_rx.netaddrinfo.target_addr = N_SA;
    status = CANTP_AddMapping_2016(channel, &mapping_phys_rx);
    if (!CANTP_StatusIsOk_2016(status)) {
        return status;
    }

    return status;
}

cantp_status PCANISOTP::initSendMsgMappings(cantp_handle channel, ushort nTargetIdx, uint32_t can_id, uint32_t can_id_flow_ctrl)
{
    cantp_mapping mapping_phys_tx;

    // clean variables (it is common to leave can_tx_dl uninitialized which can lead to invalid 0xCC values)
    memset(&mapping_phys_tx, 0, sizeof(mapping_phys_tx));

    // configure a mapping to transmit physical message
    mapping_phys_tx.can_id = can_id;
    mapping_phys_tx.can_id_flow_ctrl = can_id_flow_ctrl;
    mapping_phys_tx.can_msgtype = PCANTP_CAN_MSGTYPE_EXTENDED;
    mapping_phys_tx.netaddrinfo.format = PCANTP_ISOTP_FORMAT_NORMAL;
    mapping_phys_tx.netaddrinfo.msgtype = PCANTP_ISOTP_MSGTYPE_DIAGNOSTIC;
    mapping_phys_tx.netaddrinfo.target_type = PCANTP_ISOTP_ADDRESSING_PHYSICAL;
    mapping_phys_tx.netaddrinfo.source_addr = N_SA;
    mapping_phys_tx.netaddrinfo.target_addr = N_TA_PHYS + nTargetIdx;
    status = CANTP_AddMapping_2016(channel, &mapping_phys_tx);
    if (!CANTP_StatusIsOk_2016(status)) {
        return status;
    }

    return status;
}
