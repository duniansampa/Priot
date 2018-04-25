/*
 *  Host Resources MIB - storage group implementation - hrh_storage.c
 *
 */

#include "hrh_storage.h"
#include "AgentHandler.h"
#include "AgentReadConfig.h"
#include "AgentRegistry.h"
#include "Client.h"
#include "DefaultStore.h"
#include "DsAgent.h"
#include "Impl.h"
#include "Logger.h"
#include "ReadConfig.h"
#include "Scalar.h"
#include "Strlcpy.h"
#include "hrh_filesys.h"
#include "siglog/agent/hardware/fsys.h"
#include "siglog/agent/hardware/memory.h"

#include "PluginSettings.h"

#define HRSTORE_MONOTONICALLY_INCREASING

/*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

extern netsnmp_fsys_info* HRFS_entry;

static void parse_storage_config( const char*, char* );

/*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/
int Get_Next_HR_Store( void );
void Init_HR_Store( void );
int header_hrstore( struct Variable_s*, oid*, size_t*, int,
    size_t*, WriteMethodFT** );
void* header_hrstoreEntry( struct Variable_s*, oid*, size_t*,
    int, size_t*, WriteMethodFT** );
NodeHandlerFT handle_memsize;

#define HRSTORE_MEMSIZE 1
#define HRSTORE_INDEX 2
#define HRSTORE_TYPE 3
#define HRSTORE_DESCR 4
#define HRSTORE_UNITS 5
#define HRSTORE_SIZE 6
#define HRSTORE_USED 7
#define HRSTORE_FAILS 8

struct Variable2_s hrstore_variables[] = {
    { HRSTORE_INDEX, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_hrstore, 1, { 1 } },
    { HRSTORE_TYPE, ASN01_OBJECT_ID, IMPL_OLDAPI_RONLY,
        var_hrstore, 1, { 2 } },
    { HRSTORE_DESCR, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
        var_hrstore, 1, { 3 } },
    { HRSTORE_UNITS, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_hrstore, 1, { 4 } },
    { HRSTORE_SIZE, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_hrstore, 1, { 5 } },
    { HRSTORE_USED, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_hrstore, 1, { 6 } },
    { HRSTORE_FAILS, ASN01_COUNTER, IMPL_OLDAPI_RONLY,
        var_hrstore, 1, { 7 } }
};
oid hrstore_variables_oid[] = { 1, 3, 6, 1, 2, 1, 25, 2 };
oid hrMemorySize_oid[] = { 1, 3, 6, 1, 2, 1, 25, 2, 2 };
oid hrStorageTable_oid[] = { 1, 3, 6, 1, 2, 1, 25, 2, 3, 1 };

void init_hrh_storage( void )
{
    char* appname;

    Scalar_registerScalar(
        AgentHandler_createHandlerRegistration( "host/hrMemorySize", handle_memsize,
            hrMemorySize_oid, ASN01_OID_LENGTH( hrMemorySize_oid ),
            HANDLER_CAN_RONLY ) );
    REGISTER_MIB( "host/hr_storage", hrstore_variables, Variable2_s,
        hrStorageTable_oid );

    appname = DefaultStore_getString( DsStorage_LIBRARY_ID,
        DsStr_APPTYPE );
    DefaultStore_registerConfig( ASN01_BOOLEAN, appname, "skipNFSInHostResources",
        DsStorage_APPLICATION_ID,
        DsAgentBoolean_SKIPNFSINHOSTRESOURCES );

    DefaultStore_registerConfig( ASN01_BOOLEAN, appname, "realStorageUnits",
        DsStorage_APPLICATION_ID,
        DsAgentBoolean_REALSTORAGEUNITS );

    AgentReadConfig_priotdRegisterConfigHandler( "storageUseNFS", parse_storage_config, NULL,
        "1 | 2\t\t(1 = enable, 2 = disable)" );
}

static int storageUseNFS = 1; /* Default to reporting NFS mounts as NetworkDisk */

static void
parse_storage_config( const char* token, char* cptr )
{
    char* val;
    int ival;
    char* st;

    val = strtok_r( cptr, " \t", &st );
    if ( !val ) {
        ReadConfig_configPerror( "Missing FLAG parameter in storageUseNFS" );
        return;
    }
    ival = atoi( val );
    if ( ival < 1 || ival > 2 ) {
        ReadConfig_configPerror( "storageUseNFS must be 1 or 2" );
        return;
    }
    storageUseNFS = ( ival == 1 ) ? 1 : 0;
}

/*
 * header_hrstoreEntry(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */

void* header_hrstoreEntry( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact,
    size_t* var_len, WriteMethodFT** write_method )
{
#define HRSTORE_ENTRY_NAME_LENGTH 11
    oid newname[ ASN01_MAX_OID_LEN ];
    int storage_idx, LowIndex = -1;
    int result;
    int idx = -1;
    netsnmp_memory_info* mem = NULL;

    DEBUG_MSGTL( ( "host/hr_storage", "var_hrstoreEntry: request " ) );
    DEBUG_MSGOID( ( "host/hr_storage", name, *length ) );
    DEBUG_MSG( ( "host/hr_storage", " exact=%d\n", exact ) );

    memcpy( ( char* )newname, ( char* )vp->name,
        ( int )vp->namelen * sizeof( oid ) );
    result = Api_oidCompare( name, *length, vp->name, vp->namelen );

    DEBUG_MSGTL( ( "host/hr_storage", "var_hrstoreEntry: compare " ) );
    DEBUG_MSGOID( ( "host/hr_storage", vp->name, vp->namelen ) );
    DEBUG_MSG( ( "host/hr_storage", " => %d\n", result ) );

    if ( result < 0 || *length <= HRSTORE_ENTRY_NAME_LENGTH ) {
        /*
        * Requested OID too early or too short to refer
        *   to a valid row (for the current column object).
        * GET requests should fail, GETNEXT requests
        *   should use the first row.
        */
        if ( exact )
            return NULL;
        netsnmp_memory_load();
        mem = netsnmp_memory_get_first( 0 );
    } else {
        /*
         * Otherwise, retrieve the requested
         *  (or following) row as appropriate.
         */
        if ( exact && *length > HRSTORE_ENTRY_NAME_LENGTH + 1 )
            return NULL; /* Too long for a valid instance */
        idx = name[ HRSTORE_ENTRY_NAME_LENGTH ];
        if ( idx < NETSNMP_MEM_TYPE_MAX ) {
            netsnmp_memory_load();
            mem = ( exact ? netsnmp_memory_get_byIdx( idx, 0 ) : netsnmp_memory_get_next_byIdx( idx, 0 ) );
        }
    }

    /*
     * If this matched a memory-based entry, then
     *    update the OID parameter(s) for GETNEXT requests.
     */
    if ( mem ) {
        if ( !exact ) {
            newname[ HRSTORE_ENTRY_NAME_LENGTH ] = mem->idx;
            memcpy( ( char* )name, ( char* )newname,
                ( ( int )vp->namelen + 1 ) * sizeof( oid ) );
            *length = vp->namelen + 1;
        }
    }
    /*
     * If this didn't match a memory-based entry,
     *   then consider the disk-based storage.
     */
    else {
        Init_HR_Store();
        for ( ;; ) {
            storage_idx = Get_Next_HR_Store();
            DEBUG_MSG( ( "host/hr_storage", "(index %d ....", storage_idx ) );
            if ( storage_idx == -1 )
                break;
            newname[ HRSTORE_ENTRY_NAME_LENGTH ] = storage_idx;
            DEBUG_MSGOID( ( "host/hr_storage", newname, *length ) );
            DEBUG_MSG( ( "host/hr_storage", "\n" ) );
            result = Api_oidCompare( name, *length, newname, vp->namelen + 1 );
            if ( exact && ( result == 0 ) ) {
                LowIndex = storage_idx;
                /*
                 * Save storage status information 
                 */
                break;
            }
            if ( ( !exact && ( result < 0 ) ) && ( LowIndex == -1 || storage_idx < LowIndex ) ) {
                LowIndex = storage_idx;
                /*
                 * Save storage status information 
                 */
                break;
            }
        }
        if ( LowIndex != -1 ) {
            if ( !exact ) {
                newname[ HRSTORE_ENTRY_NAME_LENGTH ] = LowIndex;
                memcpy( ( char* )name, ( char* )newname,
                    ( ( int )vp->namelen + 1 ) * sizeof( oid ) );
                *length = vp->namelen + 1;
            }
            mem = ( netsnmp_memory_info* )0xffffffff; /* To indicate 'success' */
        }
    }

    *write_method = ( WriteMethodFT* )0;
    *var_len = sizeof( long ); /* default to 'long' results */

    /*
     *  ... and return the appropriate row
     */
    DEBUG_MSGTL( ( "host/hr_storage", "var_hrstoreEntry: process " ) );
    DEBUG_MSGOID( ( "host/hr_storage", name, *length ) );
    DEBUG_MSG( ( "host/hr_storage", " (%p)\n", mem ) );
    return ( void* )mem;
}

oid storage_type_id[] = { 1, 3, 6, 1, 2, 1, 25, 2, 1, 1 }; /* hrStorageOther */
int storage_type_len = sizeof( storage_type_id ) / sizeof( storage_type_id[ 0 ] );

/*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

int handle_memsize( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    netsnmp_memory_info* mem_info;
    int val;

    /*
     * We just need to handle valid GET requests, as invalid instances
     *   are rejected automatically, and (valid) GETNEXT requests are
     *   converted into the appropriate GET request.
     *
     * We also only ever receive one request at a time.
     */
    switch ( reqinfo->mode ) {
    case MODE_GET:
        netsnmp_memory_load();
        mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 0 );
        if ( !mem_info || mem_info->size == -1 || mem_info->units == -1 )
            Agent_setRequestError( reqinfo, requests, PRIOT_NOSUCHOBJECT );
        else {
            val = mem_info->size; /* memtotal */
            val *= ( mem_info->units / 1024 );
            Client_setVarTypedValue( requests->requestvb, ASN01_INTEGER,
                ( u_char* )&val, sizeof( val ) );
        }
        return PRIOT_ERR_NOERROR;

    default:
        /*
         * we should never get here, so this is a really bad error 
         */
        Logger_log( LOGGER_PRIORITY_ERR, "unknown mode (%d) in handle_memsize\n",
            reqinfo->mode );
        return PRIOT_ERR_GENERR;
    }

    return PRIOT_ERR_NOERROR;
}

u_char*
var_hrstore( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
    int store_idx = 0;
    static char string[ 1024 ];
    void* ptr;
    netsnmp_memory_info* mem = NULL;

really_try_next:
    ptr = header_hrstoreEntry( vp, name, length, exact, var_len,
        write_method );
    if ( ptr == NULL )
        return NULL;

    store_idx = name[ HRSTORE_ENTRY_NAME_LENGTH ];
    if ( HRFS_entry && store_idx > NETSNMP_MEM_TYPE_MAX && DefaultStore_getBoolean( DsStorage_APPLICATION_ID,
                                                               DsAgentBoolean_SKIPNFSINHOSTRESOURCES )
        && Check_HR_FileSys_NFS() )
        return NULL;
    if ( store_idx <= NETSNMP_MEM_TYPE_MAX ) {
        mem = ( netsnmp_memory_info* )ptr;
    }

    switch ( vp->magic ) {
    case HRSTORE_INDEX:
        vars_longReturn = store_idx;
        return ( u_char* )&vars_longReturn;
    case HRSTORE_TYPE:
        if ( store_idx > NETSNMP_MEM_TYPE_MAX )
            if ( HRFS_entry->flags & NETSNMP_FS_FLAG_REMOTE )
                storage_type_id[ storage_type_len - 1 ] = 10; /* Network Disk */
            else if ( HRFS_entry->flags & NETSNMP_FS_FLAG_REMOVE )
                storage_type_id[ storage_type_len - 1 ] = 5; /* Removable Disk */
            else
                storage_type_id[ storage_type_len - 1 ] = 4; /* Assume fixed */
        else
            switch ( store_idx ) {
            case NETSNMP_MEM_TYPE_PHYSMEM:
            case NETSNMP_MEM_TYPE_USERMEM:
                storage_type_id[ storage_type_len - 1 ] = 2; /* RAM */
                break;
            case NETSNMP_MEM_TYPE_VIRTMEM:
            case NETSNMP_MEM_TYPE_SWAP:
                storage_type_id[ storage_type_len - 1 ] = 3; /* Virtual Mem */
                break;
            default:
                storage_type_id[ storage_type_len - 1 ] = 1; /* Other */
                break;
            }
        *var_len = sizeof( storage_type_id );
        return ( u_char* )storage_type_id;
    case HRSTORE_DESCR:
        if ( store_idx > NETSNMP_MEM_TYPE_MAX ) {
            Strlcpy_strlcpy( string, HRFS_entry->path, sizeof( string ) );
            *var_len = strlen( string );
            return ( u_char* )string;
        } else {
            if ( !mem || !mem->descr )
                goto try_next;
            *var_len = strlen( mem->descr );
            return ( u_char* )mem->descr;
        }
    case HRSTORE_UNITS:
        if ( store_idx > NETSNMP_MEM_TYPE_MAX ) {
            if ( DefaultStore_getBoolean( DsStorage_APPLICATION_ID,
                     DsAgentBoolean_REALSTORAGEUNITS ) )
                vars_longReturn = HRFS_entry->units & 0xffffffff;
            else
                vars_longReturn = HRFS_entry->units_32;
        } else {
            if ( !mem || mem->units == -1 )
                goto try_next;
            vars_longReturn = mem->units;
        }
        return ( u_char* )&vars_longReturn;
    case HRSTORE_SIZE:
        if ( store_idx > NETSNMP_MEM_TYPE_MAX ) {
            if ( DefaultStore_getBoolean( DsStorage_APPLICATION_ID,
                     DsAgentBoolean_REALSTORAGEUNITS ) )
                vars_longReturn = HRFS_entry->size & 0xffffffff;
            else
                vars_longReturn = HRFS_entry->size_32;
        } else {
            if ( !mem || mem->size == -1 )
                goto try_next;
            vars_longReturn = mem->size;
        }
        return ( u_char* )&vars_longReturn;
    case HRSTORE_USED:
        if ( store_idx > NETSNMP_MEM_TYPE_MAX ) {
            if ( DefaultStore_getBoolean( DsStorage_APPLICATION_ID,
                     DsAgentBoolean_REALSTORAGEUNITS ) )
                vars_longReturn = HRFS_entry->used & 0xffffffff;
            else
                vars_longReturn = HRFS_entry->used_32;
        } else {
            if ( !mem || mem->size == -1 || mem->free == -1 )
                goto try_next;
            vars_longReturn = mem->size - mem->free;
        }
        return ( u_char* )&vars_longReturn;
    case HRSTORE_FAILS:
        if ( store_idx > NETSNMP_MEM_TYPE_MAX )
            goto try_next;

        else {
            if ( !mem || mem->other == -1 )
                goto try_next;
            vars_longReturn = mem->other;
        }
        return ( u_char* )&vars_longReturn;
    default:
        DEBUG_MSGTL( ( "snmpd", "unknown sub-id %d in var_hrstore\n",
            vp->magic ) );
    }
    return NULL;

try_next:
    if ( !exact )
        goto really_try_next;

    return NULL;
}

/*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

static int HRS_index;

void Init_HR_Store( void )
{
    HRS_index = 0;
    Init_HR_FileSys();
}

int Get_Next_HR_Store( void )
{
    /*
     * File-based storage 
     */
    for ( ;; ) {
        HRS_index = Get_Next_HR_FileSys();
        if ( HRS_index >= 0 ) {
            if ( !( DefaultStore_getBoolean( DsStorage_APPLICATION_ID,
                        DsAgentBoolean_SKIPNFSINHOSTRESOURCES )
                     && Check_HR_FileSys_NFS() ) ) {
                return HRS_index + NETSNMP_MEM_TYPE_MAX;
            }
        } else {
            return -1;
        }
    }
}
