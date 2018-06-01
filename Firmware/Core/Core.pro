QT      -= core gui
CONFIG  -= qt shared
CONFIG  += shared
TARGET   = core
TEMPLATE = lib
LIBS    += -lcrypto

VERSION  = 1.0.0    # major.minor.patch
QMAKE_CFLAGS += -Werror=implicit-function-declaration
DEFINES += PRIOT_CORELIB_LIBRARY

INCLUDEPATH += $$PWD/../Plugin/
DEPENDPATH += $$PWD/../Plugin/

#DESTDIR = $$PWD/../../bin/lib

SOURCES += \
    System/Numerics/Double.c \
    System/Numerics/Float.c \
    System/Convert.c \
    System/Version.c \
    System/Containers/Map.c \
    System/Containers/Container.c \
    System/Containers/ContainerBinaryArray.c \
    System/Containers/ContainerIterator.c \
    System/Containers/ContainerListSsll.c \
    System/Containers/ContainerNull.c \
    System/Containers/MapList.c \
    System/Util/Logger.c \
    System/Util/Utilities.c \
    System/Util/Alarm.c \
    System/Util/Callback.c \
    System/Util/DefaultStore.c \
    Transports/AliasDomain.c \
    Transports/IPv4BaseDomain.c \
    Transports/SocketBaseDomain.c \
    Transports/TCPBaseDomain.c \
    Transports/TCPDomain.c \
    Transports/UDPBaseDomain.c \
    Transports/UDPDomain.c \
    Transports/UDPIPv4BaseDomain.c \
    Transports/UnixDomain.c \
    Transports/CallbackDomain.c \
    System/String.c \
    Api.c \
    Asn01.c \
    Client.c \
    Keytools.c \
    System/Util/LargeFdSet.c \
    Md5.c \
    Mib.c \
    OidStash.c \
    Parse.c \
    ParseArgs.c \
    Priot.c \
    ReadConfig.c \
    Scapi.c \
    Secmod.c \
    Service.c \
    Session.c \
    System/Util/System.c \
    Transport.c \
    Usm.c \
    V3.c \
    Vacm.c \
    FdEventManager.c \
    System/Util/Directory.c \
    System/Util/File.c \
    System/Numerics/Integer.c \
    System/Util/Memory.c \
    System/Util/Time.c \
    System/Numerics/Integer64.c \
    System/Task/Mutex.c \
    System/Containers/List.c \
    System/Util/VariableList.c \
    System/Util/FileParser.c \
    TextualConvention.c \
    System/Util/Trace.c \
    EngineTime.c

HEADERS += \
    Config.h \
    Generals.h \
    System/String.h \
    System/Version.h \
    System/Numerics/Double.h \
    System/Numerics/Float.h \
    System/Containers/Map.h \
    System/Containers/Container.h \
    System/Containers/ContainerBinaryArray.h \
    System/Containers/ContainerIterator.h \
    System/Containers/ContainerListSsll.h \
    System/Containers/ContainerNull.h \
    System/Containers/MapList.h \
    System/Util/Logger.h \
    System/Util/Utilities.h \
    System/Util/Assert.h \
    System/Util/Directory.h \
    System/Util/File.h \
    System/Util/Memory.h \
    System/Util/Time.h \
    System/Util/Alarm.h \
    System/Util/Callback.h \
    System/Util/DefaultStore.h \
    System/Numerics/Integer64.h \
    Transports/IPv4BaseDomain.h \
    Transports/SocketBaseDomain.h \
    Transports/TCPBaseDomain.h \
    Transports/UDPBaseDomain.h \
    Transports/UDPIPv4BaseDomain.h \
    Transports/TCPDomain.h \
    Transports/UDPDomain.h \
    Transports/UnixDomain.h \
    Transports/AliasDomain.h \
    Transports/CallbackDomain.h \
    DataType.h \
    Transport.h \
    System/Util/System.h \
    Types.h \
    Priot.h \
    System/Util/LargeFdSet.h \
    Keytools.h \
    Md5.h \
    Mib.h \
    Asn01.h \
    Parse.h \
    OidStash.h \
    ReadConfig.h \
    Scapi.h \
    Api.h \
    ParseArgs.h \
    Secmod.h \
    Service.h \
    Usm.h \
    V3.h \
    Vacm.h \
    Client.h \
    Impl.h \
    Session.h \
    ../Plugin/Agentx/Protocol.h \
    System/Convert.h \
    FdEventManager.h \
    System/Numerics/Integer.h \
    System/Task/Mutex.h \
    System/Containers/List.h \
    System/Util/VariableList.h \
    System/Util/FileParser.h \
    TextualConvention.h \
    System/Util/Trace.h \
    EngineTime.h



