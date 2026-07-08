QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    StreamPusher.cpp \
    main.cpp \
    mainwindow.cpp \
    test_push.cpp

HEADERS += \
    StreamPusher.h \
    mainwindow.h

FORMS += \
    mainwindow.ui


TARGET = RtmpClient
TEMPLATE = app
# FFmpeg 路径（根据你的实际路径修改）
FFMPEG_DIR = E:/ffmpeg-6.0-full_build-shared

INCLUDEPATH += $${FFMPEG_DIR}/include
LIBS += -L$${FFMPEG_DIR}/lib \
        -lavcodec \
        -lavformat \
        -lavdevice \
        -lavutil \
        -lswscale \
        -lswresample



# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=


# 包含子模块
include(src/capturer/capturer.pri)
include(src/encoder/encoder.pri)
include(src/output/output.pri)
include(src/converter/converter.pri)

