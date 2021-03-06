#ifndef PRIO_CONFIG_H
#define PRIO_CONFIG_H

/* Portions of this file are subject to the following copyright(s).  See
 * the PRIOT's COPYING file for more details and other copyrights
 * that may apply:
 */

/* The enterprise number has been assigned by the IANA group.   */
/* Optionally, this may point to the location in the tree your  */
/* company/organization has been allocated.                     */
/* The assigned enterprise number for the NET_SNMP MIB modules. */
#define ENTERPRISE_OID 8072

/* Set if snmpgets should block and never timeout */
/* The original CMU code had this hardcoded as = 1 */
#define PRIOT_SNMPBLOCK 1

/* this is the location of the net-snmp mib tree.  It shouldn't be
   changed, as the places it is used are expected to be constant
   values or are directly tied to the UCD-SNMP-MIB. */
#define NET_OID 8072
#define NET_MIB 1, 3, 6, 1, 4, 1, 8072
#define NET_DOT_MIB 1.3.6.1.4.1.8072
#define NET_DOT_MIB_LENGTH 7

/* Default (SNMP) version number for the tools to use */
#define DEFAULT_PRIOT_VERSION 3

/* Environment separator character surrounded by double quotes. */
#define ENV_SEPARATOR ":"

/* Environment separator character surrounded by single quotes. */
#define ENV_SEPARATOR_CHAR ':'

/* debugging stuff */
/* if defined, we optimize the code to exclude all debugging calls. */
/* #undef NETSNMP_NO_DEBUGGING */
/* ignore the -D flag and always print debugging information */
#define DEBUG_ALWAYS_DEBUG 0

/* default location to look for mibs to load using the above tokens and/or
   those in the MIBS envrionment variable */
#define DEFAULT_MIBDIRS "/priot/mibs" /** "$HOME/.priot/mibs:/usr/local/share/priot/mibs" */

/* umask permissions to set up persistent files with */
#define PERSISTENT_MASK 077

/* Define to the full name of this package. */
#define PACKAGE_NAME "PRIOT"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "PRIOT 5.7.3"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "priot"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "5.7.3"

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `intmax_t', as computed by sizeof. */
#define SIZEOF_INTMAX_T 8

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 8

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of `short', as computed by sizeof. */
#define SIZEOF_SHORT 2

/* reverse encoding BER packets is both faster and more efficient in space. */
#define USE_REVERSE_ASNENCODING 1
#define DEFAULT_ASNENCODING_DIRECTION 1 /* 1 = reverse, 0 = forwards */

/* PERSISTENT_DIRECTORY: If defined, the library is capabile of saving
   persisant information to this directory in the form of configuration
   lines: PERSISTENT_DIRECTORY/NAME.persistent.conf */
#define PERSISTENT_DIRECTORY "/priot/var/priot" /** "/var/priot" */

/* AGENT_DIRECTORY_MODE: the mode the agents should use to create
   directories with. Since the data stored here is probably sensitive, it
   probably should be read-only by root/administrator. */
#define AGENT_DIRECTORY_MODE 0700

/* MAX_PERSISTENT_BACKUPS:
 *   The maximum number of persistent backups the library will try to
 *   read from the persistent cache directory.  If an application fails to
 *   close down successfully more than this number of times, data will be lost.
 */
#define MAX_PERSISTENT_BACKUPS 10

/* pattern for temporary file names */
#define NETSNMP_TEMP_FILE_PATTERN "/priot/tmp/priotdXXXXXX"

/* net-snmp's major path names */
#define SNMPLIBPATH "/priot/lib/priot"
#define SNMPSHAREPATH "/priot/share/priot"
#define SNMPCONFPATH "/priot/etc/priot"
#define SNMPDLMODPATH "/priot/lib/priot/dlmod"

/* LOGFILE:  If defined it closes stdout/err/in and opens this in
   out/err's place.  (stdin is closed so that sh scripts won't wait for it) */
#define LOGFILE "/priot/var/log/priotd.log"

/* default system contact */
#define SYS_CONTACT "duniansampa@outlook.com"

/* system location */
#define SYS_LOC "São José dos Campos"

/* default list of mibs to load */
#define PRIOT_DEFAULT_MIBS ":SNMPv2-MIB:IF-MIB:IP-MIB:TCP-MIB:UDP-MIB:HOST-RESOURCES-MIB:NOTIFICATION-LOG-MIB:DISMAN-EVENT-MIB:DISMAN-SCHEDULE-MIB:UCD-SNMP-MIB:UCD-DEMO-MIB:SNMP-TARGET-MIB:NET-SNMP-AGENT-MIB:HOST-RESOURCES-TYPES:SNMP-MPD-MIB:SNMP-USER-BASED-SM-MIB:SNMP-FRAMEWORK-MIB:SNMP-VIEW-BASED-ACM-MIB:SNMP-COMMUNITY-MIB:IP-FORWARD-MIB:NET-SNMP-PASS-MIB:NET-SNMP-EXTEND-MIB:UCD-DLMOD-MIB:SNMP-NOTIFICATION-MIB:SNMPv2-TM:NET-SNMP-VACM-MIB"

#define CLASS_NAME( n ) #n
#define SHOW( a )              \
    std::cout.precision( 10 ); \
    std::cout << #a << ": " << a << std::endl

#ifndef NULL
#define NULL 0
#endif

#define FALSE 0
#define TRUE 1
#define ONE_SEC 1000000L

/* location of mount table list */
#define ETC_MNTTAB "/etc/mtab"

/* location of UNIX kernel */
#define KERNEL_LOC "unknown"

/* Path to the lpstat command */
#define LPSTAT_PATH "/usr/bin/lpstat"

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Unix domain socket for AgentX master-subagent communication */
#define NETSNMP_AGENTX_SOCKET "/priot/var/agentx/master"

/* "Define if crytography support is possible" */
#define NETSNMP_CAN_DO_CRYPTO 1

/* configure options specified */
#define NETSNMP_CONFIGURE_OPTIONS " '--with-default-snmp-version=3' '--with-sys-contact=duniansampa@outlook.com' '--with-sys-location=São José dos Campos' '--with-logfile=/var/log/snmpd.log' '--with-persistent-directory=/var/net-snmp'"

/* define if you want to compile support for both authentication and privacy
   support. */
#define NETSNMP_ENABLE_SCAPI_AUTHPRIV 1

/* If you don't want the agent to report on variables it doesn't have data for
   */
#define NETSNMP_NO_DUMMY_VALUES 1

/* Size prefix to use to printf a size_t or ssize_t */
#define NETSNMP_PRIz "l"

/* Define this if you have lm_sensors v3 or later */
#define NETSNMP_USE_SENSORS_V3 1

/* Should we compile to use special opaque types: float, double, counter64,
   i64, ui64, union? */
#define NETSNMP_WITH_OPAQUE_SPECIAL_TYPES 1

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "net-snmp-coders@lists.sourceforge.net"

/* Command to generate ps output, the final column must be the process name
   withOUT arguments */
#define PSCMD "/usr/bin/ps -e"

/* The size of `sockaddr_un.sun_path', as computed by sizeof. */
#define SIZEOF_SOCKADDR_UN_SUN_PATH 108

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* define if SIOCGIFADDR exists in sys/ioctl.h */
#define SYS_IOCTL_H_HAS_SIOCGIFADDR 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Where is the uname command */
#define UNAMEPROG "/usr/bin/uname"

/* Enable extensions on AIX 3, Interix.  */
#define _ALL_SOURCE 1

/* Enable GNU extensions on systems that have them.  */
//#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
//#endif

/* Enable threading extensions on Solaris.  */
#define _POSIX_PTHREAD_SEMANTICS 1

/* Enable extensions on HP NonStop.  */
#define _TANDEM_SOURCE 1

/* Enable general extensions on Solaris.  */
#define __EXTENSIONS__ 1

/* Define if you have RPM 4.6 or newer to turn on legacy API */
#define _RPM_4_4_COMPAT /**/

#define NETSNMP_USE_OPENSSL 1

/* Default (SNMP) version number for the tools to use */
#define NETSNMP_DEFAULT_SNMP_VERSION 3

/* default list of mibs to load */
#define NETSNMP_DEFAULT_MIBS ":SNMPv2-MIB:IF-MIB:IP-MIB:TCP-MIB:UDP-MIB:HOST-RESOURCES-MIB:NOTIFICATION-LOG-MIB:DISMAN-EVENT-MIB:DISMAN-SCHEDULE-MIB:UCD-SNMP-MIB:UCD-DEMO-MIB:SNMP-TARGET-MIB:NET-SNMP-AGENT-MIB:HOST-RESOURCES-TYPES:SNMP-MPD-MIB:SNMP-USER-BASED-SM-MIB:SNMP-FRAMEWORK-MIB:SNMP-VIEW-BASED-ACM-MIB:SNMP-COMMUNITY-MIB:IP-FORWARD-MIB:NET-SNMP-PASS-MIB:NET-SNMP-EXTEND-MIB:UCD-DLMOD-MIB:SNMP-NOTIFICATION-MIB:SNMPv2-TM:NET-SNMP-VACM-MIB"

/* debugging stuff */
/* if defined, we optimize the code to exclude all debugging calls. */
/* #undef NETSNMP_NO_DEBUGGING */
/* ignore the -D flag and always print debugging information */
#define NETSNMP_ALWAYS_DEBUG 0

/* Mib-2 tree Info */
/* These are the system information variables. */

#define NETSNMP_VERS_DESC "unknown" /* overridden at run time */
#define NETSNMP_SYS_NAME "unknown" /* overridden at run time */

/* comment out the second define to turn off functionality for any of
   these: (See README for details) */

/*   proc PROCESSNAME [MAX] [MIN] */
#define NETSNMP_PROCMIBNUM 2

/*   exec/shell NAME COMMAND      */
#define NETSNMP_SHELLMIBNUM 8

/*   swap MIN                     */
#define NETSNMP_MEMMIBNUM 4

/*   disk DISK MINSIZE            */
#define NETSNMP_DISKMIBNUM 9

/*   load 1 5 15                  */
#define NETSNMP_LOADAVEMIBNUM 10

/* which version are you using? This mibloc will tell you */
#define NETSNMP_VERSIONMIBNUM 100

/* Reports errors the agent runs into */
/* (typically its "can't fork, no mem" problems) */
#define NETSNMP_ERRORMIBNUM 101

/* The sub id of EXTENSIBLEMIB returned to queries of
   .iso.org.dod.internet.mgmt.mib-2.system.sysObjectID.0 */
#define NETSNMP_AGENTID 250

/* This ID is returned after the AGENTID above.  IE, the resulting
   value returned by a query to sysObjectID is
   EXTENSIBLEMIB.AGENTID.???, where ??? is defined below by OSTYPE */

#define NETSNMP_LINUXID 10

#define NETSNMP_OSTYPE NETSNMP_LINUXID

#define NETSNMP_ENTERPRISE_MIB 1, 3, 6, 1, 4, 1, 8072
#define NETSNMP_ENTERPRISE_DOT_MIB 1.3.6.1.4.1.8072
#define NETSNMP_ENTERPRISE_DOT_MIB_LENGTH 7

/* The assigned enterprise number for sysObjectID. */
#define NETSNMP_SYSTEM_MIB 1, 3, 6, 1, 4, 1, 8072, 3, 2, NETSNMP_OSTYPE
#define NETSNMP_SYSTEM_DOT_MIB 1.3.6.1.4.1.8072.3.2.NETSNMP_OSTYPE
#define NETSNMP_SYSTEM_DOT_MIB_LENGTH 10

/* The assigned enterprise number for notifications. */
#define NETSNMP_NOTIFICATION_MIB 1, 3, 6, 1, 4, 1, 8072, 4
#define NETSNMP_NOTIFICATION_DOT_MIB 1.3.6.1.4.1.8072.4
#define NETSNMP_NOTIFICATION_DOT_MIB_LENGTH 8

/* this is the location of the ucdavis mib tree.  It shouldn't be
   changed, as the places it is used are expected to be constant
   values or are directly tied to the UCD-SNMP-MIB. */
#define NETSNMP_UCDAVIS_OID 2021
#define NETSNMP_UCDAVIS_MIB 1, 3, 6, 1, 4, 1, 2021
#define NETSNMP_UCDAVIS_DOT_MIB 1.3.6.1.4.1.2021
#define NETSNMP_UCDAVIS_DOT_MIB_LENGTH 7

/* how long to wait (seconds) for error querys before reseting the error trap.*/
#define NETSNMP_ERRORTIMELENGTH 600

#define NETSNMP_EXCACHETIME 30
#define NETSNMP_CACHEFILE ".priot-exec-cache"
#define NETSNMP_MAXCACHESIZE ( 1500 * 80 ) /* roughly 1500 lines max */

/* misc defaults */

/* default of 100 meg minimum if the minimum size is not specified in
   the config file */
#define NETSNMP_DEFDISKMINIMUMSPACE 100000

/* default maximum load average before error */
#define NETSNMP_DEFMAXLOADAVE 12.0

/* max times to loop reading output from execs. */
/* Because of sleep(1)s, this will also be time to wait (in seconds) for exec
   to finish */
#define NETSNMP_MAXREADCOUNT 100

/* Set if snmpgets should block and never timeout */
/* The original CMU code had this hardcoded as = 1 */
#define NETSNMP_SNMPBLOCK 1

/* How long to wait before restarting the agent after a snmpset to
   EXTENSIBLEMIB.VERSIONMIBNUM.VERRESTARTAGENT.  This is
   necessary to finish the snmpset reply before restarting. */
#define NETSNMP_RESTARTSLEEP 5

/* UNdefine to allow specifying zero-length community string */
/* #define NETSNMP_NO_ZEROLENGTH_COMMUNITY 1 */

/* Number of community strings to store */
#define NETSNMP_NUM_COMMUNITIES 5

/* internal define */
#define NETSNMP_LASTFIELD -1

/*  Pluggable transports.  */

/*  This is defined if support for the UDP/IP transport domain is
    available.   */
#define NETSNMP_TRANSPORT_UDP_DOMAIN 1

/*  This is defined if support for the "callback" transport domain is
    available.   */
#define NETSNMP_TRANSPORT_CALLBACK_DOMAIN 1

/*  This is defined if support for the TCP/IP transport domain is
    available.  */
#define NETSNMP_TRANSPORT_TCP_DOMAIN 1

/*  This is defined if support for the Unix transport domain
    (a.k.a. "local IPC") is available.  */
#define NETSNMP_TRANSPORT_UNIX_DOMAIN 1

/*  This is defined if support for the Alias transport domain is
    available.   */
#define NETSNMP_TRANSPORT_ALIAS_DOMAIN 1

/*  This is defined if support for the IPv4Base transport domain is available.   */
#define NETSNMP_TRANSPORT_IPV4BASE_DOMAIN 1

/* define this if the USM security module is available */
#define NETSNMP_SECMOD_USM 1

/* this is the location of the net-snmp mib tree.  It shouldn't be
   changed, as the places it is used are expected to be constant
   values or are directly tied to the UCD-SNMP-MIB. */
#define NETSNMP_OID 8072
#define NETSNMP_MIB 1, 3, 6, 1, 4, 1, 8072
#define NETSNMP_DOT_MIB 1.3.6.1.4.1.8072
#define NETSNMP_DOT_MIB_LENGTH 7

/* comment the next line if you are compiling with libsnmp.h
   and are not using the UC-Davis SNMP library. */
#define UCD_SNMP_LIBRARY 1

#define DONT_USE_NLIST 1 //NETSNMP_DONT_USE_NLIST

#define config_UNUSED( x ) ( void )( x )

#define TYPES_MAX_OID_LEN 128 /* max subid's in an oid */

/*
 * Error return values.
 *
 * API_ERR_SUCCESS is the non-PDU "success" code.
 *
 * XXX  These should be merged with PRIOT_ERR_* defines and confined
 *      to values < 0.  ???
 */

//Error codes (the value of the field error-status in PDUs)
enum ErrorCode_e {
    ErrorCode_TLS_NO_CERTIFICATE = -69,
    ErrorCode_TRANSPORT_CONFIG_ERROR,
    ErrorCode_TRANSPORT_NO_CONFIG,
    ErrorCode_JUST_A_CONTEXT_PROBE,
    ErrorCode_OID_NONINCREASING,
    ErrorCode_PROTOCOL,
    ErrorCode_KRB5,
    ErrorCode_MALLOC,
    ErrorCode_VAR_TYPE,
    ErrorCode_NO_VARS,
    ErrorCode_NULL_PDU,
    ErrorCode_UNKNOWN_OBJID,
    ErrorCode_VALUE,
    ErrorCode_BAD_NAME,
    ErrorCode_LONG_OID,
    ErrorCode_BAD_SUBID,
    ErrorCode_MAX_SUBID,
    ErrorCode_RANGE,
    ErrorCode_NOMIB,
    ErrorCode_USM_DECRYPTIONERROR,
    ErrorCode_USM_NOTINTIMEWINDOW,
    ErrorCode_USM_UNKNOWNENGINEID,
    ErrorCode_USM_PARSEERROR,
    ErrorCode_USM_AUTHENTICATIONFAILURE,
    ErrorCode_USM_ENCRYPTIONERROR,
    ErrorCode_USM_UNSUPPORTEDSECURITYLEVEL,
    ErrorCode_USM_UNKNOWNSECURITYNAME,
    ErrorCode_USM_GENERICERROR,
    ErrorCode_UNKNOWN_REPORT,
    ErrorCode_KT_NOT_AVAILABLE,
    ErrorCode_SC_NOT_CONFIGURED,
    ErrorCode_SC_GENERAL_FAILURE,
    ErrorCode_DECRYPTION_ERR,
    ErrorCode_NOT_IN_TIME_WINDOW,
    ErrorCode_AUTHENTICATION_FAILURE,
    ErrorCode_UNSUPPORTED_SEC_LEVEL,
    ErrorCode_UNKNOWN_USER_NAME,
    ErrorCode_UNKNOWN_ENG_ID,
    ErrorCode_INVALID_MSG,
    ErrorCode_UNKNOWN_SEC_MODEL,
    ErrorCode_ASN_PARSE_ERR,
    ErrorCode_BAD_SEC_LEVEL,
    ErrorCode_BAD_SEC_NAME,
    ErrorCode_BAD_ENG_ID,
    ErrorCode_BAD_RECVFROM,
    ErrorCode_TIMEOUT,
    ErrorCode_UNKNOWN_PDU,
    ErrorCode_ABORT,
    ErrorCode_BAD_PARTY,
    ErrorCode_BAD_ACL,
    ErrorCode_NOAUTH_DESPRIV,
    ErrorCode_BAD_COMMUNITY,
    ErrorCode_BAD_CONTEXT,
    ErrorCode_BAD_DST_PARTY,
    ErrorCode_BAD_SRC_PARTY,
    ErrorCode_BAD_VERSION,
    ErrorCode_BAD_PARSE,
    ErrorCode_BAD_SENDTO,
    ErrorCode_BAD_ASN1_BUILD,
    ErrorCode_BAD_REPETITIONS,
    ErrorCode_BAD_REPEATERS,
    ErrorCode_V1_IN_V2,
    ErrorCode_V2_IN_V1,
    ErrorCode_NO_SOCKET,
    ErrorCode_TOO_LONG,
    ErrorCode_BAD_SESSION,
    ErrorCode_BAD_ADDRESS,
    ErrorCode_BAD_LOCPORT,
    ErrorCode_GENERR,
    ErrorCode_SUCCESS = 0 // must be = 0
};

#define ErrorCode_MAX ( -69 )

#endif // PRIO_CONFIG_H
