#include "PCANComm.h"

PCANComm::PCANComm(QMainWindow *p) :
    parent(p) ,
    writecanID(0)
{

}

bool PCANComm::Open(cantp_handle ch, CANSettingParam &param)
{
    bool bResult = false;
    channel = ch;
    auto data = param.sBitrate.toLocal8Bit();
    status = CANTP_InitializeFD_2016(channel, param.sBitrate.toLocal8Bit().data());

    if (!CANTP_StatusIsOk_2016(status)) {
        debug(QString::asprintf("ISOTP Initialization failed"));
        CANTP_Uninitialize_2016(channel);
        return bResult;
    }

    status = CANTP_SetValue_2016(channel, PCANTP_PARAMETER_CAN_TX_DL, &param.CANDL, 1);
    if (!CANTP_StatusIsOk_2016(status)) {
        debug(QString::asprintf("Failed to configure parameter 'PCANTP_PARAMETER_CAN_TX_DL' (sts=0x%04X).", status));
        return bResult;
    }

    status = CANTP_SetValue_2016(channel, PCANTP_PARAMETER_BLOCK_SIZE, &param.BlockSize, 2);
    if (!CANTP_StatusIsOk_2016(status)) {
        debug(QString::asprintf("Failed to configure parameter 'PCANTP_PARAMETER_BLOCK_SIZE' (sts=0x%04X).", status));
        return bResult;
    }

    status = CANTP_SetValue_2016(channel, PCANTP_PARAMETER_SEPARATION_TIME, &param.STmin, 2);
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

    initmapping(param.CANID, param.FlowID);
    writecanID = param.CANID;

    bResult = true;

    return bResult;
}

void PCANComm::initmapping(uint32_t CANID, uint32_t FlowID)
{
    cantp_mapping mapping_phys_rx, mapping_phys_tx;

    // clean variables (it is common to leave can_tx_dl uninitialized which can lead to invalid 0xCC values)
    memset(&mapping_phys_rx, 0, sizeof(mapping_phys_rx));
    memset(&mapping_phys_tx, 0, sizeof(mapping_phys_tx));

    // configure a mapping to transmit physical message
    mapping_phys_rx.can_id = FlowID;
    mapping_phys_rx.can_id_flow_ctrl = CANID;
    mapping_phys_rx.can_msgtype = PCANTP_CAN_MSGTYPE_EXTENDED;
    mapping_phys_rx.netaddrinfo.format = PCANTP_ISOTP_FORMAT_NORMAL;
    mapping_phys_rx.netaddrinfo.msgtype = PCANTP_ISOTP_MSGTYPE_DIAGNOSTIC;
    mapping_phys_rx.netaddrinfo.target_type = PCANTP_ISOTP_ADDRESSING_PHYSICAL;
    mapping_phys_rx.netaddrinfo.source_addr = N_TA_PHYS;
    mapping_phys_rx.netaddrinfo.target_addr = N_SA;
    status = CANTP_AddMapping_2016(channel, &mapping_phys_rx);
    if (!CANTP_StatusIsOk_2016(status)) {
        return ;
    }

    mapping_phys_tx = mapping_phys_rx;
    mapping_phys_tx.can_id = CANID;
    mapping_phys_tx.can_id_flow_ctrl = FlowID;
    mapping_phys_tx.netaddrinfo.source_addr = N_SA;
    mapping_phys_tx.netaddrinfo.target_addr = N_TA_PHYS;

    status = CANTP_AddMapping_2016(channel, &mapping_phys_tx);
    if (!CANTP_StatusIsOk_2016(status)) {
        return ;
    }
}


void PCANComm::Close()
{
    CloseHandle(recvHandle);
    CANTP_Uninitialize_2016(channel);
}

void PCANComm::Read()
{
    DWORD result = WaitForSingleObject(recvHandle, 100);
    if ( result == WAIT_OBJECT_0 ) {
        bool readFlag = false;
        do {
            cantp_msg rx_msg;
            readFlag = false;
            status = CANTP_MsgDataAlloc_2016(&rx_msg, PCANTP_MSGTYPE_NONE);
            if (!CANTP_StatusIsOk_2016(status)) {
                debug("Failed to Allocate Msg");
            }

            status = CANTP_Read_2016(channel, &rx_msg, NULL, PCANTP_MSGTYPE_NONE);
            if (CANTP_StatusIsOk_2016(status, PCANTP_STATUS_OK, false)) {
                readFlag = true;
                if (!(rx_msg.msgdata.any->flags & PCANTP_MSGFLAG_LOOPBACK)) {
                    isotpReadLoopbackCheck(rx_msg);

                    if (CANTP_StatusIsOk_2016(status)) {
                        readCallback(rx_msg);
                    }
                }
            } // write에도 loopback을 통해 송신 체크를 위해 Read를 하는데 write 후 check 중일 때는 ReadHandle에서 Read를 하지 않음
        } while ( readFlag );
        // 메시지가 남아있지 않을 때까지 계속 Read 하여 체크
    }
}

void PCANComm::Write(const char *pData, int nSize, bool isFD)
{
    cantp_msg tx_msg;

    cantp_can_msgtype msgtype = PCANTP_CAN_MSGTYPE_EXTENDED;
    msgtype |= PCANTP_CAN_MSGTYPE_FD;
    msgtype |= PCANTP_CAN_MSGTYPE_BRS;
    // 29 bit 주소 형식 & CAN-FD, BRS 사용

    if ( isFD ) {
        status = CANTP_MsgDataAlloc_2016(&tx_msg, PCANTP_MSGTYPE_CANFD);
        if (!CANTP_StatusIsOk_2016(status)) {
            debug("Failed to Allocate tx msg");
            return;
        }

        status = CANTP_MsgDataInit_2016(&tx_msg, writecanID, msgtype, nSize, pData, NULL);
        // ISO-TP의 경우 CAN-ID는 전달받은 cantp_netaddrinfo를 보고 Mapping 상태에서 가져옴
    } else {

        cantp_netaddrinfo isotp_nai = {};
        isotp_nai.source_addr = N_SA;
        isotp_nai.target_addr = N_TA_PHYS;
        isotp_nai.target_type = PCANTP_ISOTP_ADDRESSING_PHYSICAL;
        isotp_nai.msgtype = PCANTP_ISOTP_MSGTYPE_DIAGNOSTIC;
        isotp_nai.format = PCANTP_ISOTP_FORMAT_NORMAL;

        status = CANTP_MsgDataAlloc_2016(&tx_msg, PCANTP_MSGTYPE_ISOTP);
        if (!CANTP_StatusIsOk_2016(status)) {
            debug("Failed to Allocate tx msg");
            return;
        }

        status = CANTP_MsgDataInit_2016(&tx_msg, PCANTP_MAPPING_FLOW_CTRL_NONE, msgtype, nSize, pData, &isotp_nai);
        // ISO-TP의 경우 CAN-ID는 전달받은 cantp_netaddrinfo를 보고 Mapping 상태에서 가져옴
    }


    if (!CANTP_StatusIsOk_2016(status)) {
        debug("Failed to initialize tx msg");
        return;
    }

    status = CANTP_Write_2016(channel, &tx_msg);
    if (!CANTP_StatusIsOk_2016(status)) {
        debug("Failed to write");
        return;
    }

    if ( !isFD ) {
        isotpWriteLoopbackCheck(tx_msg);
    }

    status = CANTP_MsgDataFree_2016(&tx_msg);
}

void PCANComm::readCallback(cantp_msg &rx_msg)
{
    readMsgQueue->enqueue(rx_msg);
}

void PCANComm::isotpReadLoopbackCheck(cantp_msg &rx_msg)
{
    cantp_msgprogress progress; // ISO-TP 메시지가 다 들어왔는지 체크하기 위함

    if (((PCANTP_MSGTYPE_ISOTP & rx_msg.type) == PCANTP_MSGTYPE_ISOTP)
        && ((rx_msg.msgdata.isotp->netaddrinfo.msgtype & PCANTP_ISOTP_MSGTYPE_FLAG_INDICATION_RX) == PCANTP_ISOTP_MSGTYPE_FLAG_INDICATION_RX)) {
        // The message is being received, wait and show progress
        memset(&progress, 0, sizeof(cantp_msgprogress));
        do {
            status = CANTP_GetMsgProgress_2016(channel, &rx_msg, PCANTP_MSGDIRECTION_RX, &progress);
            Sleep(10);
        } while (progress.state == PCANTP_MSGPROGRESS_STATE_PROCESSING);

        // The message is received, read it
        status = CANTP_Read_2016(channel, &rx_msg, NULL, PCANTP_MSGTYPE_NONE);
    } // 잔여 데이터가 있을 경우 계속 체크 후 다 들어왔다면 다시 한번 Read
}

void PCANComm::isotpWriteLoopbackCheck(cantp_msg &tx_msg)
{
    if ( tx_msg.msgdata.any->length < 30 ) {
        return ;
    } // ISOTP Header를 제외한 30바이트보다 작게 쓸 경우에는 Single Frame을 쏘기 때문에 ISOTP Loopback Check가 불필요

    cantp_msg loopback_msg;
    cantp_msgprogress progress;
    cantp_status result;

    memset(&loopback_msg, 0, sizeof(loopback_msg));
    memset(&progress, 0, sizeof(progress));

    result = CANTP_Read_2016(channel, &loopback_msg, NULL, PCANTP_MSGTYPE_ANY);

    if (CANTP_StatusIsOk_2016(result, PCANTP_STATUS_OK, false)) {
        if ( loopback_msg.can_info.can_id == readcanID ) {
            isotpReadLoopbackCheck(loopback_msg);
            readCallback(loopback_msg);
        } // Read 주소로 등록된 데이터가 위 Read에서 체크된 경우에는 Read 처리

        if (((PCANTP_MSGTYPE_ISOTP & loopback_msg.type) == PCANTP_MSGTYPE_ISOTP) && (loopback_msg.msgdata.isotp->flags == PCANTP_MSGFLAG_LOOPBACK)
            && ((loopback_msg.msgdata.isotp->netaddrinfo.msgtype & PCANTP_ISOTP_MSGTYPE_FLAG_INDICATION_TX) == PCANTP_ISOTP_MSGTYPE_FLAG_INDICATION_TX)) {
            do {
                result = CANTP_GetMsgProgress_2016(channel, &loopback_msg, PCANTP_MSGDIRECTION_TX, &progress);
            } while (progress.state == PCANTP_MSGPROGRESS_STATE_PROCESSING);
            // The message is being received, wait and show progress
        }
    }

    CANTP_MsgDataFree_2016(&loopback_msg);
}
