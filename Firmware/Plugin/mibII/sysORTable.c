#include "sysORTable.h"
#include "AgentHandler.h"
#include "Client.h"
#include "System/Containers/Container.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "SysORTable.h"
#include "Table.h"
#include "TableContainer.h"
#include "System/Util/Utilities.h"
#include "Watcher.h"

/** Typical data structure for a row entry */
typedef struct SysORTableEntry_s {
    Types_Index oid_index;
    oid sysORIndex;
    const struct SysORTable_s* data;
} SysORTableEntry;

/*
 * column number definitions for table sysORTable
 */
#define COLUMN_SYSORINDEX 1
#define COLUMN_SYSORID 2
#define COLUMN_SYSORDESCR 3
#define COLUMN_SYSORUPTIME 4

static Container_Container* table = NULL;
static u_long sysORLastChange;
static oid sysORNextIndex = 1;

/** create a new row in the table */
static void register_foreach( const struct SysORTable_s* data, void* dummy )
{
    SysORTableEntry* entry;

    sysORLastChange = data->OR_uptime;

    entry = MEMORY_MALLOC_TYPEDEF( SysORTableEntry );
    if ( !entry ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "could not allocate storage, SysORTable_s is inconsistent\n" );
    } else {
        const oid firstNext = sysORNextIndex;
        Container_Iterator* it = CONTAINER_ITERATOR( table );

        do {
            const SysORTableEntry* value;
            const oid cur = sysORNextIndex;

            if ( sysORNextIndex == UTILITIES_MIN_VALUE( TYPES_OID_MAX_SUBID, 2147483647UL ) )
                sysORNextIndex = 1;
            else
                ++sysORNextIndex;

            for ( value = ( SysORTableEntry* )it->curr( it );
                  value && value->sysORIndex < cur;
                  value = ( SysORTableEntry* )CONTAINER_ITERATOR_NEXT( it ) ) {
            }

            if ( value && value->sysORIndex == cur ) {
                if ( sysORNextIndex < cur )
                    it->reset( it );
            } else {
                entry->sysORIndex = cur;
                break;
            }
        } while ( firstNext != sysORNextIndex );

        CONTAINER_ITERATOR_RELEASE( it );

        if ( firstNext == sysORNextIndex ) {
            Logger_log( LOGGER_PRIORITY_ERR, "Failed to locate a free index in SysORTable_s\n" );
            free( entry );
        } else {
            entry->data = data;
            entry->oid_index.len = 1;
            entry->oid_index.oids = &entry->sysORIndex;

            CONTAINER_INSERT( table, entry );
        }
    }
}

static int register_cb( int major, int minor, void* serv, void* client )
{
    DEBUG_MSGTL( ( "mibII/sysORTable/register_cb", "register_cb(%d, %d, %p, %p)\n",
        major, minor, serv, client ) );
    register_foreach( ( struct SysORTable_s* )serv, NULL );
    return PRIOT_ERR_NOERROR;
}

/** remove a row from the table */
static int unregister_cb( int major, int minor, void* serv, void* client )
{
    SysORTableEntry* value;
    Container_Iterator* it = CONTAINER_ITERATOR( table );

    DEBUG_MSGTL( ( "mibII/sysORTable/unregister_cb",
        "unregister_cb(%d, %d, %p, %p)\n", major, minor, serv,
        client ) );
    sysORLastChange = ( ( struct SysORTable_s* )( serv ) )->OR_uptime;

    while ( ( value = ( SysORTableEntry* )CONTAINER_ITERATOR_NEXT( it ) ) && value->data != serv )
        ;
    CONTAINER_ITERATOR_RELEASE( it );
    if ( value ) {
        CONTAINER_REMOVE( table, value );
        free( value );
    }
    return PRIOT_ERR_NOERROR;
}

/** handles requests for the SysORTable_s table */
static int sysORTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;

    DEBUG_MSGTL( ( "mibII/sysORTable/sysORTable_handler",
        "sysORTable_handler called\n" ) );

    if ( reqinfo->mode != MODE_GET ) {
        Logger_log( LOGGER_PRIORITY_ERR, "Got unexpected operation for sysORTable\n" );
        return PRIOT_ERR_GENERR;
    }

    /*
   * Read-support (also covers GetNext requests)
   */
    request = requests;
    while ( request && request->processed )
        request = request->next;
    while ( request ) {
        SysORTableEntry* table_entry;
        TableRequestInfo* table_info;

        if ( NULL == ( table_info = Table_extractTableInfo( request ) ) ) {
            Logger_log( LOGGER_PRIORITY_ERR, "could not extract table info for sysORTable\n" );
            Client_setVarTypedValue( request->requestvb, PRIOT_ERR_GENERR, NULL, 0 );
        } else if ( NULL == ( table_entry = ( SysORTableEntry* )
                                    TableContainer_extractContext( request ) ) ) {
            switch ( table_info->colnum ) {
            case COLUMN_SYSORID:
            case COLUMN_SYSORDESCR:
            case COLUMN_SYSORUPTIME:
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHINSTANCE );
                break;
            default:
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHOBJECT );
                break;
            }
        } else {
            switch ( table_info->colnum ) {
            case COLUMN_SYSORID:
                Client_setVarTypedValue( request->requestvb, ASN01_OBJECT_ID,
                    ( const u_char* )table_entry->data->OR_oid,
                    table_entry->data->OR_oidlen * sizeof( oid ) );
                break;
            case COLUMN_SYSORDESCR:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( const u_char* )table_entry->data->OR_descr,
                    strlen( table_entry->data->OR_descr ) );
                break;
            case COLUMN_SYSORUPTIME:
                Client_setVarTypedInteger( request->requestvb, ASN01_TIMETICKS,
                    table_entry->data->OR_uptime );
                break;
            default:
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHOBJECT );
                break;
            }
        }
        do {
            request = request->next;
        } while ( request && request->processed );
    }
    return PRIOT_ERR_NOERROR;
}

extern oid systemMib_systemModuleOid[];
extern int systemMib_systemModuleOidLen;
extern int systemMib_systemModuleCount;

static HandlerRegistration* sysORLastChange_reg;
static WatcherInfo sysORLastChange_winfo;
static HandlerRegistration* sysORTable_reg;
static TableRegistrationInfo* sysORTable_table_info;

/** Initializes the SysORTable_s module */
void init_sysORTable( void )
{
    const oid sysORLastChange_oid[] = { 1, 3, 6, 1, 2, 1, 1, 8 };
    const oid sysORTable_oid[] = { 1, 3, 6, 1, 2, 1, 1, 9 };

    sysORTable_table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );

    table = Container_find( "sysORTable:tableContainer" );

    if ( sysORTable_table_info == NULL || table == NULL ) {
        MEMORY_FREE( sysORTable_table_info );
        CONTAINER_FREE( table );
        return;
    }
    table->containerName = strdup( "sysORTable" );

    Table_helperAddIndexes( sysORTable_table_info,
        ASN01_INTEGER, /** index: sysORIndex */
        0 );
    sysORTable_table_info->min_column = COLUMN_SYSORID;
    sysORTable_table_info->max_column = COLUMN_SYSORUPTIME;

    sysORLastChange_reg = AgentHandler_createHandlerRegistration(
        "mibII/sysORLastChange", NULL, sysORLastChange_oid,
        ASN01_OID_LENGTH( sysORLastChange_oid ), HANDLER_CAN_RONLY );
    Watcher_initWatcherInfo( &sysORLastChange_winfo, &sysORLastChange,
        sizeof( u_long ), ASN01_TIMETICKS,
        WATCHER_FIXED_SIZE );
    Watcher_registerWatchedScalar( sysORLastChange_reg,
        &sysORLastChange_winfo );

    sysORTable_reg = AgentHandler_createHandlerRegistration(
        "mibII/sysORTable", sysORTable_handler, sysORTable_oid,
        ASN01_OID_LENGTH( sysORTable_oid ), HANDLER_CAN_RONLY );
    TableContainer_register( sysORTable_reg, sysORTable_table_info,
        table, TABLE_CONTAINER_KEY_NETSNMP_INDEX );

    sysORLastChange = Agent_getAgentUptime();

    /*
   * Initialise the contents of the table here
   */
    SysORTable_foreach( &register_foreach, NULL );

    /*
   * Register callbacks
   */
    Callback_registerCallback( CALLBACK_APPLICATION, PriotdCallback_REG_SYSOR,
        register_cb, NULL );
    Callback_registerCallback( CALLBACK_APPLICATION, PriotdCallback_UNREG_SYSOR,
        unregister_cb, NULL );

    if ( ++systemMib_systemModuleCount == 3 )
        REGISTER_SYSOR_TABLE( systemMib_systemModuleOid, systemMib_systemModuleOidLen,
            "The MIB module for SNMPv2 entities" );
}

void shutdown_sysORTable( void )
{
    if ( systemMib_systemModuleCount-- == 3 )
        UNREGISTER_SYSOR_TABLE( systemMib_systemModuleOid, systemMib_systemModuleOidLen );

    Callback_unregisterCallback( CALLBACK_APPLICATION,
        PriotdCallback_UNREG_SYSOR, unregister_cb, NULL,
        1 );
    Callback_unregisterCallback( CALLBACK_APPLICATION, PriotdCallback_REG_SYSOR,
        register_cb, NULL, 1 );

    if ( table )
        CONTAINER_CLEAR( table, Container_simpleFree, NULL );
    TableContainer_unregister( sysORTable_reg );
    sysORTable_reg = NULL;
    table = NULL;
    Table_registrationInfoFree( sysORTable_table_info );
    sysORTable_table_info = NULL;
    AgentHandler_unregisterHandler( sysORLastChange_reg );
    sysORLastChange_reg = NULL;
}
