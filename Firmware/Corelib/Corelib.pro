QT      -= core gui
CONFIG  -= qt shared
CONFIG  += shared
TARGET   = core
TEMPLATE = lib
LIBS    += -lcrypto

VERSION  = 1.0.0    # major.minor.patch
QMAKE_CFLAGS += -fno-strict-aliasing -g -O2
DEFINES += PRIOT_CORELIB
INCLUDEPATH += "$$PWD"
#DESTDIR = $$PWD/../lib
#LIBS +=  -L$$DESTDIR


message("message: $$LIBS")

SOURCES += \
    String.cpp \
    Numbers/BigLong.cpp \
    Numbers/Byte.cpp \
    Numbers/Double.cpp \
    Numbers/Float.cpp \
    Numbers/Integer.cpp \
    Numbers/Long.cpp \
    Numbers/Short.cpp \
    DefaultStore.cpp \
    Transports/IPv4BaseDomain.cpp \
    Transport.cpp \
    System.cpp \
    Transports/SocketBaseDomain.cpp \
    Transports/TCPBaseDomain.cpp \
    Transports/UDPBaseDomain.cpp \
    Transports/UDPIPv4BaseDomain.cpp \
    Transports/TCPDomain.cpp \
    Transports/UDPDomain.cpp \
    Transports/UnixDomain.cpp \
    Debug.cpp \
    Logger.cpp \
    Tools.cpp \
    Container.cpp \
    Factory.cpp \
    ContainerBinaryArray.cpp \
    ContainerIterator.cpp \
    Priot.cpp \
    ContainerListSsll.cpp \
    ContainerNull.cpp \
    DataList.cpp \
    Callback.cpp \
    DirUtils.cpp \
    FileUtils.cpp \
    LargeFdSet.cpp \
    Int64.cpp \
    Keytools.cpp \
    LcdTime.cpp \
    Md5.cpp \
    Mib.cpp \
    Asn01.cpp \
    Parse.cpp \
    MtSupport.cpp \
    OidStash.cpp \
    ReadConfig.cpp \
    Scapi.cpp \
    Alarm.cpp \
    Tc.cpp \
    Api.cpp \
    Auth.cpp \
    Enum.cpp \
    ParseArgs.cpp \
    Secmod.cpp \
    Service.cpp \
    Version.cpp \
    Usm.cpp \
    V3.cpp \
    Strlcat.cpp \
    Strlcpy.cpp \
    TextUtils.cpp \
    UcdCompat.cpp \
    Vacm.cpp \
    Client.cpp \
    Session.cpp \
    Transports/AliasDomain.cpp

HEADERS += \
    String.h \
    Numbers/BigLong.h \
    Numbers/Byte.h \
    Numbers/Double.h \
    Numbers/Float.h \
    Numbers/Integer.h \
    Numbers/Long.h \
    Numbers/Short.h \
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
    Auth.h \
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
    Corelib.h \
    Session.h \
    Transports/AliasDomain.h \
    Settings.h \
    ../Modlib/Agentx/Protocol.h
