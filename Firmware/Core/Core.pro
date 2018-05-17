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
    DefaultStore.c \
    System/Numerics/Double.c \
    System/Numerics/Float.c \
    System/Convert.c \
    System/Containers/Map.c \
    System/Containers/Container.c \
    System/Containers/ContainerBinaryArray.c \
    System/Containers/ContainerIterator.c \
    System/Containers/ContainerListSsll.c \
    System/Containers/ContainerNull.c \
    System/Containers/MapList.c \
    System/Util/Logger.c \
    System/Util/Debug.c \
    System/Util/Utilities.c \
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
    Callback.c \
    Client.c \
    Int64.c \
    Keytools.c \
    LargeFdSet.c \
    LcdTime.c \
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
    System.c \
    Tc.c \
    TextUtils.c \
    Transport.c \
    UcdCompat.c \
    Usm.c \
    V3.c \
    Vacm.c \
    Version.c \
    FdEventManager.c \
    Alarm.c \
    CheckVarbind.c \
    System/Util/Directory.c \
    System/Util/File.c \
    System/Numerics/Integer.c \
    System/Util/Memory.c \
    System/Util/Time.c \
    System/Task/Mutex.c

HEADERS += \
    Config.h \
    Generals.h \
    System/String.h \
    System/Numerics/Double.h \
    System/Numerics/Float.h \
    System/Containers/Map.h \
    System/Containers/Container.h \
    System/Containers/ContainerBinaryArray.h \
    System/Containers/ContainerIterator.h \
    System/Containers/ContainerListSsll.h \
    System/Containers/ContainerNull.h \
    System/Containers/MapList.h \
    System/Util/Debug.h \
    System/Util/Logger.h \
    System/Util/Utilities.h \
    System/Util/Assert.h \
    System/Util/Directory.h \
    System/Util/File.h \
    System/Util/Memory.h \
    System/Util/Time.h \
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
    DefaultStore.h \
    Transport.h \
    System.h \
    Types.h \
    Priot.h \
    Callback.h \
    LargeFdSet.h \
    Int64.h \
    Keytools.h \
    LcdTime.h \
    Md5.h \
    Mib.h \
    Asn01.h \
    Parse.h \
    OidStash.h \
    ReadConfig.h \
    Scapi.h \
    Alarm.h \
    Tc.h \
    Api.h \
    ParseArgs.h \
    Secmod.h \
    Service.h \
    Version.h \
    Usm.h \
    V3.h \
    TextUtils.h \
    UcdCompat.h \
    Vacm.h \
    Client.h \
    Impl.h \
    Session.h \
    ../Plugin/Agentx/Protocol.h \
    System/Convert.h \
    FdEventManager.h \
    CheckVarbind.h \
    System/Numerics/Integer.h \
    System/Task/Mutex.h



