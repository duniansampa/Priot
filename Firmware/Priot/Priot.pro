QT      -= core gui
CONFIG  -= qt shared
CONFIG  += shared
TARGET   = priot
TEMPLATE = lib
VERSION  = 1.0.0    # major.minor.patch
QMAKE_CFLAGS += -fno-strict-aliasing -g -O2 -Werror=implicit-function-declaration

DEFINES += PRIOT_PRIOTLIB_LIBRARY

#DESTDIR = "/home/dsampa/Documents/workspace/pessoal/lib"

INCLUDEPATH +=  $$PWD/../Plugin/

unix:!macx: LIBS += -L$$OUT_PWD/../Core/ -lcore

INCLUDEPATH += $$PWD/../Core
DEPENDPATH += $$PWD/../Core

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
    ../Plugin/Utilities/Iquery.h \
    ../Plugin/Utilities/ExecuteCmd.h \
    ../Plugin/Struct.h \
    ../Plugin/V3/UsmConf.h \
    ../Plugin/MibII/VacmConf.h \
    ../Plugin/Agentx/Protocol.h \
    ../Plugin/Agentx/Protocol.h \
    ../Plugin/Agentx/Subagent.h \
    ../Plugin/Agentx/AgentxConfig.h \
    ../Plugin/Agentx/XClient.h \
    ../Plugin/Agentx/Master.h \
    ../Plugin/Agentx/MasterAdmin.h


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
    ../Plugin/Utilities/Iquery.c \
    ../Plugin/Utilities/ExecuteCmd.c \
    ../Plugin/V3/UsmConf.c \
    ../Plugin/MibII/VacmConf.c \
    ../Plugin/Agentx/Protocol.c \
    ../Plugin/Agentx/AgentxConfig.c \
    ../Plugin/Agentx/XClient.c \
    ../Plugin/Agentx/Subagent.c \
    ../Plugin/Agentx/Master.c \
    ../Plugin/Agentx/MasterAdmin.c
