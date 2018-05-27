/*
 *  System MIB group implementation - system.c
 *
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright � 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include "system_mib.h"
#include "Agent.h"
#include "AgentReadConfig.h"
#include "Client.h"
#include "Mib.h"
#include "Priot.h"
#include "ReadConfig.h"
#include "Scalar.h"
#include "SysORTable.h"
#include "System/Util/Logger.h"
#include "Watcher.h"
#include "updates.h"
#include "util_funcs.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

/*********************
     *
     *  Kernel & interface information,
     *   and internal forward declarations
     *
     *********************/

#define SYS_STRING_LEN 256
static char version_descr[ SYS_STRING_LEN ]
    = NETSNMP_VERS_DESC;
static char sysContact[ SYS_STRING_LEN ] = SYS_CONTACT;
static char sysName[ SYS_STRING_LEN ] = NETSNMP_SYS_NAME;
static char sysLocation[ SYS_STRING_LEN ] = SYS_LOC;
static oid sysObjectID[ ASN01_MAX_OID_LEN ];
static size_t sysObjectIDByteLength;

static int sysServices = 72;
static int sysServicesConfiged = 0;

extern oid version_id[];
extern int version_id_len;

static int sysContactSet = 0, sysLocationSet = 0, sysNameSet = 0;

/*********************
     *
     *  priotd.conf config parsing
     *
     *********************/

static void
system_parse_config_string2( const char* token, char* cptr,
    char* value, size_t size )
{
    if ( strlen( cptr ) < size ) {
        strcpy( value, cptr );
    } else {
        ReadConfig_error( "%s token too long (must be < %lu):\n\t%s",
            token, ( unsigned long )size, cptr );
    }
}

static void
system_parse_config_string( const char* token, char* cptr,
    const char* name, char* value, size_t size,
    int* guard )
{
    if ( *token == 'p' ) {
        if ( *guard < 0 ) {
            /*
             * This is bogus (and shouldn't happen anyway) -- the value is
             * already configured read-only.
             */
            Logger_log( LOGGER_PRIORITY_WARNING,
                "ignoring attempted override of read-only %s.0\n", name );
            return;
        } else {
            *guard = 1;
        }
    } else {
        if ( *guard > 0 ) {
            /*
             * This is bogus (and shouldn't happen anyway) -- we already read a
             * persistent value which we should ignore in favour of this one.
             */
            Logger_log( LOGGER_PRIORITY_WARNING,
                "ignoring attempted override of read-only %s.0\n", name );
            /*
             * Fall through and copy in this value.
             */
        }
        *guard = -1;
    }

    system_parse_config_string2( token, cptr, value, size );
}

static void
system_parse_config_sysdescr( const char* token, char* cptr )
{
    system_parse_config_string2( token, cptr, version_descr,
        sizeof( version_descr ) );
}

static void
system_parse_config_sysloc( const char* token, char* cptr )
{
    system_parse_config_string( token, cptr, "sysLocation", sysLocation,
        sizeof( sysLocation ), &sysLocationSet );
}

static void
system_parse_config_syscon( const char* token, char* cptr )
{
    system_parse_config_string( token, cptr, "sysContact", sysContact,
        sizeof( sysContact ), &sysContactSet );
}

static void
system_parse_config_sysname( const char* token, char* cptr )
{
    system_parse_config_string( token, cptr, "sysName", sysName,
        sizeof( sysName ), &sysNameSet );
}

static void
system_parse_config_sysServices( const char* token, char* cptr )
{
    sysServices = atoi( cptr );
    sysServicesConfiged = 1;
}

static void
system_parse_config_sysObjectID( const char* token, char* cptr )
{
    size_t sysObjectIDLength = ASN01_MAX_OID_LEN;
    if ( !Mib_readObjid( cptr, sysObjectID, &sysObjectIDLength ) ) {
        ReadConfig_error( "sysobjectid token not a parsable OID:\n\t%s",
            cptr );
        sysObjectIDByteLength = agent_versionSysoidLen * sizeof( oid );
        memcpy( sysObjectID, agent_versionSysoid, sysObjectIDByteLength );
    } else

        sysObjectIDByteLength = sysObjectIDLength * sizeof( oid );
}

/*********************
     *
     *  Initialisation & common implementation functions
     *
     *********************/

oid systemMib_systemModuleOid[] = { PRIOT_OID_SNMPMODULES, 1 };
int systemMib_systemModuleOidLen = ASN01_OID_LENGTH( systemMib_systemModuleOid );
int systemMib_systemModuleCount = 0;

static int
system_store( int a, int b, void* c, void* d )
{
    char line[ UTILITIES_MAX_BUFFER_SMALL ];

    if ( sysLocationSet > 0 ) {
        snprintf( line, UTILITIES_MAX_BUFFER_SMALL, "psyslocation %s", sysLocation );
        AgentReadConfig_priotdStoreConfig( line );
    }
    if ( sysContactSet > 0 ) {
        snprintf( line, UTILITIES_MAX_BUFFER_SMALL, "psyscontact %s", sysContact );
        AgentReadConfig_priotdStoreConfig( line );
    }
    if ( sysNameSet > 0 ) {
        snprintf( line, UTILITIES_MAX_BUFFER_SMALL, "psysname %s", sysName );
        AgentReadConfig_priotdStoreConfig( line );
    }

    return 0;
}

static int
handle_sysServices( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    if ( reqinfo->mode == MODE_GET && !sysServicesConfiged )
        Agent_requestSetError( requests, PRIOT_NOSUCHINSTANCE );
    return PRIOT_ERR_NOERROR;
}

static int
handle_sysUpTime( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    Client_setVarTypedInteger( requests->requestvb, ASN01_TIMETICKS,
        Agent_getAgentUptime() );
    return PRIOT_ERR_NOERROR;
}

void init_system_mib( void )
{

    struct utsname utsName;

    uname( &utsName );
    snprintf( version_descr, sizeof( version_descr ),
        "%s %s %s %s %s", utsName.sysname,
        utsName.nodename, utsName.release, utsName.version,
        utsName.machine );
    version_descr[ sizeof( version_descr ) - 1 ] = 0;

    gethostname( sysName, sizeof( sysName ) );

    /* default sysObjectID */
    memcpy( sysObjectID, agent_versionSysoid, agent_versionSysoidLen * sizeof( oid ) );
    sysObjectIDByteLength = agent_versionSysoidLen * sizeof( oid );

    {
        const oid sysDescr_oid[] = { 1, 3, 6, 1, 2, 1, 1, 1 };
        static WatcherInfo sysDescr_winfo;
        Watcher_registerWatchedScalar(
            AgentHandler_createHandlerRegistration(
                "mibII/sysDescr", NULL, sysDescr_oid, ASN01_OID_LENGTH( sysDescr_oid ),
                HANDLER_CAN_RONLY ),
            Watcher_initWatcherInfo( &sysDescr_winfo, version_descr, 0,
                ASN01_OCTET_STR, WATCHER_SIZE_STRLEN ) );
    }
    {
        const oid sysObjectID_oid[] = { 1, 3, 6, 1, 2, 1, 1, 2 };
        static WatcherInfo sysObjectID_winfo;
        Watcher_registerWatchedScalar(
            AgentHandler_createHandlerRegistration(
                "mibII/sysObjectID", NULL,
                sysObjectID_oid, ASN01_OID_LENGTH( sysObjectID_oid ),
                HANDLER_CAN_RONLY ),
            Watcher_initWatcherInfo6(
                &sysObjectID_winfo, sysObjectID, 0, ASN01_OBJECT_ID,
                WATCHER_MAX_SIZE | WATCHER_SIZE_IS_PTR,
                ASN01_MAX_OID_LEN, &sysObjectIDByteLength ) );
    }
    {
        const oid sysUpTime_oid[] = { 1, 3, 6, 1, 2, 1, 1, 3 };
        Scalar_registerScalar(
            AgentHandler_createHandlerRegistration(
                "mibII/sysUpTime", handle_sysUpTime,
                sysUpTime_oid, ASN01_OID_LENGTH( sysUpTime_oid ),
                HANDLER_CAN_RONLY ) );
    }
    {
        const oid sysContact_oid[] = { 1, 3, 6, 1, 2, 1, 1, 4 };
        static WatcherInfo sysContact_winfo;
        Watcher_registerWatchedScalar(
            netsnmp_create_update_handler_registration(
                "mibII/sysContact", sysContact_oid, ASN01_OID_LENGTH( sysContact_oid ),
                HANDLER_CAN_RWRITE, &sysContactSet ),
            Watcher_initWatcherInfo(
                &sysContact_winfo, sysContact, SYS_STRING_LEN - 1,
                ASN01_OCTET_STR, WATCHER_MAX_SIZE | WATCHER_SIZE_STRLEN ) );
    }
    {
        const oid sysName_oid[] = { 1, 3, 6, 1, 2, 1, 1, 5 };
        static WatcherInfo sysName_winfo;
        Watcher_registerWatchedScalar(
            netsnmp_create_update_handler_registration(
                "mibII/sysName", sysName_oid, ASN01_OID_LENGTH( sysName_oid ),
                HANDLER_CAN_RWRITE, &sysNameSet ),
            Watcher_initWatcherInfo(
                &sysName_winfo, sysName, SYS_STRING_LEN - 1, ASN01_OCTET_STR,
                WATCHER_MAX_SIZE | WATCHER_SIZE_STRLEN ) );
    }
    {
        const oid sysLocation_oid[] = { 1, 3, 6, 1, 2, 1, 1, 6 };
        static WatcherInfo sysLocation_winfo;
        Watcher_registerWatchedScalar(
            netsnmp_create_update_handler_registration(
                "mibII/sysLocation", sysLocation_oid,
                ASN01_OID_LENGTH( sysLocation_oid ),
                HANDLER_CAN_RWRITE, &sysLocationSet ),
            Watcher_initWatcherInfo(
                &sysLocation_winfo, sysLocation, SYS_STRING_LEN - 1,
                ASN01_OCTET_STR, WATCHER_MAX_SIZE | WATCHER_SIZE_STRLEN ) );
    }
    {
        const oid sysServices_oid[] = { 1, 3, 6, 1, 2, 1, 1, 7 };
        Watcher_registerReadOnlyIntScalar(
            "mibII/sysServices", sysServices_oid, ASN01_OID_LENGTH( sysServices_oid ),
            &sysServices, handle_sysServices );
    }
    if ( ++systemMib_systemModuleCount == 3 )
        REGISTER_SYSOR_ENTRY( systemMib_systemModuleOid,
            "The MIB module for SNMPv2 entities" );

    sysContactSet = sysLocationSet = sysNameSet = 0;

    /*
     * register our config handlers
     */
    AgentReadConfig_priotdRegisterConfigHandler( "sysdescr",
        system_parse_config_sysdescr, NULL,
        "description" );
    AgentReadConfig_priotdRegisterConfigHandler( "syslocation",
        system_parse_config_sysloc, NULL,
        "location" );
    AgentReadConfig_priotdRegisterConfigHandler( "syscontact", system_parse_config_syscon,
        NULL, "contact-name" );
    AgentReadConfig_priotdRegisterConfigHandler( "sysname", system_parse_config_sysname,
        NULL, "node-name" );
    AgentReadConfig_priotdRegisterConfigHandler( "psyslocation",
        system_parse_config_sysloc, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "psyscontact",
        system_parse_config_syscon, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "psysname", system_parse_config_sysname,
        NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "sysservices",
        system_parse_config_sysServices, NULL,
        "NUMBER" );
    AgentReadConfig_priotdRegisterConfigHandler( "sysobjectid",
        system_parse_config_sysObjectID, NULL,
        "OID" );
    Callback_register( CallbackMajor_LIBRARY, CallbackMinor_STORE_DATA,
        system_store, NULL );
}
