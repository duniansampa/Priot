QT      -= core gui
CONFIG  -= qt shared
CONFIG  += shared
TARGET   = core
TEMPLATE = lib
LIBS    += -lcrypto

VERSION  = 1.0.0    # major.minor.patch
QMAKE_CFLAGS += -fno-strict-aliasing -g -O2 -Werror=implicit-function-declaration
DEFINES += PRIOT_CORELIB_LIBRARY
INCLUDEPATH += "$$PWD"
INCLUDEPATH +=  $$PWD/../Plugin/
#DESTDIR = "/home/dsampa/Documents/workspace/pessoal/lib"
#LIBS +=  -L$$DESTDIR

message("message: $$LIBS")

SOURCES += \
    DefaultStore.c \
    System/Numerics/BigLong.c \
    System/Numerics/Byte.c \
    System/Numerics/Double.c \
    System/Numerics/Float.c \
    System/Numerics/Integer.c \
    System/Numerics/Long.c \
    System/Numerics/Short.c \
    Transports/AliasDomain.c \
    Transports/IPv4BaseDomain.c \
    Transports/SocketBaseDomain.c \
    Transports/TCPBaseDomain.c \
    Transports/TCPDomain.c \
    Transports/UDPBaseDomain.c \
    Transports/UDPDomain.c \
    Transports/UDPIPv4BaseDomain.c \
    Transports/UnixDomain.c \
    System/String.c \
    Alarm.c \
    Api.c \
    Asn01.c \
    Callback.c \
    Client.c \
    Container.c \
    ContainerBinaryArray.c \
    ContainerIterator.c \
    ContainerListSsll.c \
    ContainerNull.c \
    DataList.c \
    Debug.c \
    DirUtils.c \
    Enum.c \
    Factory.c \
    FileUtils.c \
    Int64.c \
    Keytools.c \
    LargeFdSet.c \
    LcdTime.c \
    Logger.c \
    Md5.c \
    Mib.c \
    MtSupport.c \
    OidStash.c \
    Parse.c \
    ParseArgs.c \
    Priot.c \
    ReadConfig.c \
    Scapi.c \
    Secmod.c \
    Service.c \
    Session.c \
    Strlcat.c \
    Strlcpy.c \
    System.c \
    Tc.c \
    TextUtils.c \
    Tools.c \
    Transport.c \
    UcdCompat.c \
    Usm.c \
    V3.c \
    Vacm.c \
    Version.c \
    Transports/CallbackDomain.c \
    System/Convert.c

HEADERS += \
    System/String.h \
    BigLong.h \
    System/Numerics/Byte.h \
    System/Numerics/Double.h \
    System/Numerics/Float.h \
    System/Numerics/Integer.h \
    System/Numerics/Long.h \
    System/Numerics/Short.h \
    DataType.h \
    DefaultStore.h \
    Generals.h \
    Transports/IPv4BaseDomain.h \
    Transport.h \
    System.h \
    Transports/SocketBaseDomain.h \
    Transports/TCPBaseDomain.h \
    Transports/UDPBaseDomain.h \
    Transports/UDPIPv4BaseDomain.h \
    Transports/TCPDomain.h \
    Transports/UDPDomain.h \
    Transports/UnixDomain.h \
    Debug.h \
    Logger.h \
    Tools.h \
    Container.h \
    Types.h \
    Factory.h \
    ContainerBinaryArray.h \
    ContainerIterator.h \
    Priot.h \
    ContainerListSsll.h \
    ContainerNull.h \
    DataList.h \
    Callback.h \
    DirUtils.h \
    FileUtils.h \
    LargeFdSet.h \
    Int64.h \
    Keytools.h \
    LcdTime.h \
    Md5.h \
    Mib.h \
    Asn01.h \
    Parse.h \
    MtSupport.h \
    OidStash.h \
    ReadConfig.h \
    Scapi.h \
    Alarm.h \
    Tc.h \
    Api.h \
    Enum.h \
    ParseArgs.h \
    Secmod.h \
    Service.h \
    Version.h \
    Usm.h \
    V3.h \
    Strlcat.h \
    Strlcpy.h \
    TextUtils.h \
    UcdCompat.h \
    Vacm.h \
    Client.h \
    Impl.h \
    Assert.h \
    Session.h \
    Transports/AliasDomain.h \
    Settings.h \
    ../Plugin/Agentx/Protocol.h \
    System/Numerics/Integer \
    Transports/CallbackDomain.h \
    System/Convert.h
