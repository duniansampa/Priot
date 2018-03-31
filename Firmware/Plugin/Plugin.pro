QT      -= core gui
CONFIG  -= qt shared
CONFIG  += shared
TARGET   = puglin
TEMPLATE = lib

QMAKE_CFLAGS += -Werror=implicit-function-declaration

#DESTDIR = $$PWD/../../bin/lib

HEADERS += \
    PluginModules.h \
    ModuleIncludes.h \
    ModuleConfig.h \
    ModuleInits.h \
    ModuleShutdown.h

SOURCES += \
    PluginModules.c

SOURCES +=

unix:!macx: LIBS += -L$$OUT_PWD/../Core/ -lcore

INCLUDEPATH += $$PWD/../Core
DEPENDPATH += $$PWD/../Core
