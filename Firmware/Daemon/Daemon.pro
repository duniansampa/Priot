TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

QMAKE_CFLAGS += -Werror=implicit-function-declaration


TARGET = daemon

SOURCES += \
    ../Plugin/UtilFuncs/Restart.c \
    Daemon.c


HEADERS += \
    M2M.h \
    ../Plugin/UtilFuncs/Restart.h \
    Daemon.h


INCLUDEPATH += $$PWD/../Plugin
DEPENDPATH += $$PWD/../Plugin

unix:!macx: LIBS += -L$$OUT_PWD/../Core/ -lcore

INCLUDEPATH += $$PWD/../Core
DEPENDPATH += $$PWD/../Core

unix:!macx: LIBS += -L$$OUT_PWD/../Priot/ -lpriot

INCLUDEPATH += $$PWD/../Priot
DEPENDPATH += $$PWD/../Priot
