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
#define ENTERPRISE_OID			8072

/* Set if snmpgets should block and never timeout */
/* The original CMU code had this hardcoded as = 1 */
#define PRIOT_SNMPBLOCK 1

/* this is the location of the net-snmp mib tree.  It shouldn't be
   changed, as the places it is used are expected to be constant
   values or are directly tied to the UCD-SNMP-MIB. */
#define NET_OID		    8072
#define NET_MIB		    1,3,6,1,4,1,8072
#define NET_DOT_MIB		1.3.6.1.4.1.8072
#define NET_DOT_MIB_LENGTH	7

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
#define DEFAULT_MIBDIRS "$HOME/.priot/mibs:/usr/local/share/priot/mibs"


/* umask permissions to set up persistent files with */
#define PERSISTENT_MASK 077


/* Define to the full name of this package. */
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "PRIOT"
#endif

/* Define to the full name and version of this package. */
#ifndef PACKAGE_STRING
#define PACKAGE_STRING "PRIOT 5.7.3"
#endif

/* Define to the one symbol short name of this package. */
#ifndef PACKAGE_TARNAME
#define PACKAGE_TARNAME "priot"
#endif

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "5.7.3"
#endif


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
#define USE_REVERSE_ASNENCODING       1
#define DEFAULT_ASNENCODING_DIRECTION 1 /* 1 = reverse, 0 = forwards */


/* PERSISTENT_DIRECTORY: If defined, the library is capabile of saving
   persisant information to this directory in the form of configuration
   lines: PERSISTENT_DIRECTORY/NAME.persistent.conf */
#define PERSISTENT_DIRECTORY "/var/priot"

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
#define NETSNMP_TEMP_FILE_PATTERN "/tmp/snmpdXXXXXX"

/* net-snmp's major path names */
#define SNMPLIBPATH "/usr/local/lib/snmp"
#define SNMPSHAREPATH "/usr/local/share/snmp"
#define SNMPCONFPATH "/usr/local/etc/snmp"
#define SNMPDLMODPATH "/usr/local/lib/snmp/dlmod"

/* PRIOT_LOGFILE:  If defined it closes stdout/err/in and opens this in
   out/err's place.  (stdin is closed so that sh scripts won't wait for it) */
#define LOGFILE "/var/log/priotd.log"

/* default system contact */
#define SYS_CONTACT "duniansampa@outlook.com"

/* system location */
#define SYS_LOC "São José dos Campos"

/* default list of mibs to load */
#define PRIOT_DEFAULT_MIBS ":SNMPv2-MIB:IF-MIB:IP-MIB:TCP-MIB:UDP-MIB:HOST-RESOURCES-MIB:NOTIFICATION-LOG-MIB:DISMAN-EVENT-MIB:DISMAN-SCHEDULE-MIB:UCD-SNMP-MIB:UCD-DEMO-MIB:SNMP-TARGET-MIB:NET-SNMP-AGENT-MIB:HOST-RESOURCES-TYPES:SNMP-MPD-MIB:SNMP-USER-BASED-SM-MIB:SNMP-FRAMEWORK-MIB:SNMP-VIEW-BASED-ACM-MIB:SNMP-COMMUNITY-MIB:IP-FORWARD-MIB:NET-SNMP-PASS-MIB:NET-SNMP-EXTEND-MIB:UCD-DLMOD-MIB:SNMP-NOTIFICATION-MIB:SNMPv2-TM:NET-SNMP-VACM-MIB"


#define CLASS_NAME(n) #n
#define SHOW(a)  std::cout.precision(10); std::cout << #a << ": " << a << std::endl

#define NULL    0
#define FALSE	0
#define TRUE	1
#define ONE_SEC         1000000L
//============= these defines will be changed ==========//


#define DSAGENT_INTERNAL_SECNAME  3    /* used by disman/mteTriggerTable. */


#endif // PRIO_CONFIG_H
