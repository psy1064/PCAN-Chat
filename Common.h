#ifndef COMMON_H
#define COMMON_H

#include <QString>
#include <PCAN-ISO-TP_2016.h>

#define CAN_TX_DL 0x0D
#define N_SA		((BYTE)0xF1)
#define N_TA_PHYS	((BYTE)0x13)

constexpr const char* bitrate = "f_clock_mhz=40,nom_brp=5,nom_tseg1=10,nom_tseg2=5,nom_sjw=4,data_brp=2,data_tseg1=6,data_tseg2=3,data_sjw=3";

const QString saveFileName = "setting.ini";
const uint StandardAddrMax = 0x7FF;
const uint ExtendedAddrMax = 0x1FFFFFFF;

namespace Key {
    static QString Bitrate      = "Bitrate";
    static QString BlockSize    = "BlockSize";
    static QString STmin        = "STmin";
    static QString CANID        = "CANID";
    static QString FlowID       = "FlowID";
    static QString CANType      = "CANType";
    static QString CANDL        = "CANDL";
    static QString AddressType  = "AdressType";
};

typedef struct _CANSettingParam {
    QString     sBitrate;
    uint32_t    CANID;
    uint32_t    FlowID;
    uint16_t    STmin;
    uint16_t    BlockSize;
    uchar       CANDL;
    bool        addrStandard;

    _CANSettingParam() { memset(this, 0x00, sizeof(_CANSettingParam)); }
} CANSettingParam;

static int ConvertDLtoSize(int DL) {
    int nLength = 0;
    switch (DL)
    {
        case 9: nLength = 12; break;
        case 10: nLength = 16; break;
        case 11: nLength = 20; break;
        case 12: nLength = 24; break;
        case 13: nLength = 32; break;
        case 14: nLength = 48; break;
        case 15: nLength = 64; break;
        default: nLength = DL; break;
    }
    return nLength;
}

#endif // COMMON_H
