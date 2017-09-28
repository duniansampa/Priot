QT      -= core gui
CONFIG  -= qt shared
CONFIG  += shared
TARGET   = priot
TEMPLATE = lib
VERSION  = 1.0.0    # major.minor.patch
QMAKE_CFLAGS += -fno-strict-aliasing -g -O2 -Werror=implicit-function-declaration

DEFINES += PRIOT_PRIOTLIB_LIBRARY

#DESTDIR = "/home/dsampa/Documents/workspace/pessoal/lib"

INCLUDEPATH +=  $$PWD/../Modlib/

unix:!macx: LIBS += -L$$OUT_PWD/../Corelib/ -lcore

INCLUDEPATH += $$PWD/../Corelib
DEPENDPATH += $$PWD/../Corelib

HEADERS += \
    Agent.h \
    AgentHandler.h \
    AgentRegistry.h \
    VarStruct.h \
    Vars.h \
    AgentIndex.h \
    PriotSettings.h \
    DsAgent.h \
    AgentReadConfig.h \
    AgentCallbacks.h \
    SysORTable.h \
    Trap.h \
    AllHelpers.h \
    BabySteps.h \
    BulkToNext.h \
    CacheHandler.h \
    DebugHandler.h \
    Instance.h \
    Watcher.h \
    ModeEndCall.h \
    Multiplexer.h \
    Null.h \
    OldApi.h \
    ReadOnly.h \
    RowMerge.h \
    Scalar.h \
    ScalarGroup.h \
    Serialize.h \
    GetStatistic.h \
    MibModules.h \
    ModuleInits.h \
    StashCache.h \
    StashToNext.h \
    Table.h \
    TableArray.h \
    TableContainer.h \
    TableData.h \
    TableDataset.h \
    TableIterator.h \
    TableRow.h \
    TableTdata.h \
    ../Modlib/Utilities/Iquery.h \
    ../Modlib/Utilities/ExecuteCmd.h \
    ../Modlib/Struct.h \
    ../Modlib/V3/UsmConf.h \
    ../Modlib/MibII/VacmConf.h \
    ../Modlib/Agentx/Protocol.h \
    ../Modlib/Agentx/Protocol.h \
    ../Modlib/Agentx/Subagent.h \
    ../Modlib/Agentx/AgentxConfig.h \
    ../Modlib/Agentx/XClient.h \
    ../Modlib/Agentx/Master.h \
    ../Modlib/Agentx/MasterAdmin.h


SOURCES += \
    Agent.c \
    AgentHandler.c \
    AgentRegistry.c \
    AgentIndex.c \
    SysORTable.c \
    Trap.c \
    AllHelpers.c \
    BabySteps.c \
    AgentReadConfig.c \
    BulkToNext.c \
    CacheHandler.c \
    DebugHandler.c \
    Watcher.c \
    Null.c \
    OldApi.c \
    Multiplexer.c \
    ModeEndCall.c \
    Instance.c \
    RowMerge.c \
    ReadOnly.c \
    Scalar.c \
    ScalarGroup.c \
    GetStatistic.c \
    Vars.c \
    MibModules.c \
    Serialize.c \
    StashCache.c \
    StashToNext.c \
    Table.c \
    TableArray.c \
    TableContainer.c \
    TableData.c \
    TableDataset.c \
    TableIterator.c \
    TableRow.c \
    TableTdata.c \
    ../Modlib/Utilities/Iquery.c \
    ../Modlib/Utilities/ExecuteCmd.c \
    ../Modlib/V3/UsmConf.c \
    ../Modlib/MibII/VacmConf.c \
    ../Modlib/Agentx/Protocol.c \
    ../Modlib/Agentx/AgentxConfig.c \
    ../Modlib/Agentx/XClient.c \
    ../Modlib/Agentx/Subagent.c \
    ../Modlib/Agentx/Master.c \
    ../Modlib/Agentx/MasterAdmin.c
