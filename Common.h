#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <PCAN-ISO-TP_2016.h>

#define CAN_TX_DL 0x0D
#define N_SA		((BYTE)0xF1)
#define N_TA_PHYS	((BYTE)0x13)

constexpr const char* bitrate = "f_clock_mhz=40,nom_brp=5,nom_tseg1=10,nom_tseg2=5,nom_sjw=4,data_brp=2,data_tseg1=6,data_tseg2=3,data_sjw=3";

const QString saveFileName = "setting.ini";

namespace Key {
    static QString Bitrate = "Bitrate";
    static QString BlockSize = "BlockSize";
    static QString STmin = "STmin";
    static QString CANID = "CANID";
    static QString FlowID = "FlowID";
    static QString CANType = "CANType";
};

typedef struct _CANSettingParam {
    QString     sBitrate;
    uint32_t    CANID;
    uint32_t    FlowID;
    uint16_t    STmin;
    uint16_t    BlockSize;

    _CANSettingParam() { memset(this, 0x00, sizeof(_CANSettingParam)); }
} CANSettingParam;

#endif // COMMON_H
