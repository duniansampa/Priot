 TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

TARGET = daemon

SOURCES += main.cpp \
#    siglogd.cpp \
    Options.cpp \
    Signals.cpp

HEADERS += \
#    siglogd.h \
    Options.h \
    Signals.h






unix:!macx: LIBS += -L$$OUT_PWD/../Core/ -lcore

INCLUDEPATH += $$PWD/../Core
DEPENDPATH += $$PWD/../Core
