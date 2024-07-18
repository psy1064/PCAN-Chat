QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    CANSettingDialog.cpp \
    PCANComm.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    CANSettingDialog.h \
    Common.h \
    PCAN/PCAN-ISO-TP_2016.h \
    PCAN/PCANBasic.h \
    PCANComm.h \
    mainwindow.h

FORMS += \
    CANSettingDialog.ui \
    mainwindow.ui

INCLUDEPATH += $$PWD/PCAN
DEPENDPATH += $$PWD/PCAN

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32: LIBS += -L$$PWD/PCAN/ -lPCANBasic

win32:!win32-g++: PRE_TARGETDEPS += $$PWD/PCAN/PCANBasic.lib
else:win32-g++: PRE_TARGETDEPS += $$PWD/PCAN/libPCANBasic.a

win32: LIBS += -L$$PWD/PCAN/ -lPCAN-ISO-TP

win32:!win32-g++: PRE_TARGETDEPS += $$PWD/PCAN/PCAN-ISO-TP.lib
else:win32-g++: PRE_TARGETDEPS += $$PWD/PCAN/libPCAN-ISO-TP.a

DESTDIR = $$PWD

CONFIG(release, debug|release): {
    TARGET = CAN_Chatting
} else {
    TARGET = CAN_Chatting_d
}
