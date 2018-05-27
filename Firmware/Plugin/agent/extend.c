#include "extend.h"
#include "AgentCallbacks.h"
#include "AgentReadConfig.h"
#include "AgentRegistry.h"
#include "System/Util/Assert.h"
#include "Client.h"
#include "System/Containers/MapList.h"
#include "Impl.h"
#include "System/Util/Logger.h"
#include "Mib.h"
#include "ReadConfig.h"
#include "TableData.h"
#include "TextualConvention.h"
#include "Watcher.h"
#include "mibdefs.h"
#include "struct.h"
#include "utilities/ExecuteCmd.h"
#include "utilities/header_simple_table.h"

#define SHELLCOMMAND 3

oid ns_extend_oid[]
    = { 1, 3, 6, 1, 4, 1, 8072, 1, 3, 2 };
oid extend_count_oid[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 3, 2, 1 };
oid extend_config_oid[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 3, 2, 2 };
oid extend_out1_oid[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 3, 2, 3 };
oid extend_out2_oid[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 3, 2, 4 };

typedef struct extend_registration_block_s {
    TableData* dinfo;
    oid* root_oid;
    size_t oid_len;
    long num_entries;
    netsnmp_extend* ehead;
    HandlerRegistration* reg[ 4 ];
    struct extend_registration_block_s* next;
} extend_registration_block;
extend_registration_block* ereg_head = NULL;

typedef struct netsnmp_old_extend_s {
    unsigned int idx;
    netsnmp_extend* exec_entry;
    netsnmp_extend* efix_entry;
} netsnmp_old_extend;

unsigned int num_compatability_entries = 0;
unsigned int max_compatability_entries = 50;
netsnmp_old_extend* compatability_entries;

char* cmdlinebuf;
size_t cmdlinesize;

WriteMethodFT fixExec2Error;
FindVarMethodFT var_extensible_old;
oid old_extensible_variables_oid[] = { NETSNMP_UCDAVIS_MIB, NETSNMP_SHELLMIBNUM, 1 };
struct Variable2_s old_extensible_variables[] = {
    { MIBINDEX, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_extensible_old, 1, { MIBINDEX } },
    { ERRORNAME, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
        var_extensible_old, 1, { ERRORNAME } },
    { SHELLCOMMAND, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
        var_extensible_old, 1, { SHELLCOMMAND } },
    { ERRORFLAG, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_extensible_old, 1, { ERRORFLAG } },
    { ERRORMSG, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
        var_extensible_old, 1, { ERRORMSG } },
    { ERRORFIX, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
        var_extensible_old, 1, { ERRORFIX } },
    { ERRORFIXCMD, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
        var_extensible_old, 1, { ERRORFIXCMD } }
};

/*************************
         *
         *  Main initialisation routine
         *
         *************************/

extend_registration_block*
_find_extension_block( oid* name, size_t name_len )
{
    extend_registration_block* eptr;
    size_t len;
    for ( eptr = ereg_head; eptr; eptr = eptr->next ) {
        len = UTILITIES_MIN_VALUE( name_len, eptr->oid_len );
        if ( !Api_oidCompare( name, len, eptr->root_oid, eptr->oid_len ) )
            return eptr;
    }
    return NULL;
}

extend_registration_block*
_register_extend( oid* base, size_t len )
{
    extend_registration_block* eptr;
    oid oid_buf[ ASN01_MAX_OID_LEN ];

    TableData* dinfo;
    TableRegistrationInfo* tinfo;
    WatcherInfo* winfo;
    HandlerRegistration* reg = NULL;
    int rc;

    for ( eptr = ereg_head; eptr; eptr = eptr->next ) {
        if ( !Api_oidCompare( base, len, eptr->root_oid, eptr->oid_len ) )
            return eptr;
    }
    if ( !eptr ) {
        eptr = MEMORY_MALLOC_TYPEDEF( extend_registration_block );
        if ( !eptr )
            return NULL;
        eptr->root_oid = Api_duplicateObjid( base, len );
        eptr->oid_len = len;
        eptr->num_entries = 0;
        eptr->ehead = NULL;
        eptr->dinfo = TableData_createTableData( "nsExtendTable" );
        eptr->next = ereg_head;
        ereg_head = eptr;
    }

    dinfo = eptr->dinfo;
    memcpy( oid_buf, base, len * sizeof( oid ) );

    /*
         * Register the configuration table
         */
    tinfo = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( tinfo, ASN01_OCTET_STR, 0 );
    tinfo->min_column = COLUMN_EXTCFG_FIRST_COLUMN;
    tinfo->max_column = COLUMN_EXTCFG_LAST_COLUMN;
    oid_buf[ len ] = 2;
    reg = AgentHandler_createHandlerRegistration(
        "nsExtendConfigTable", handle_nsExtendConfigTable,
        oid_buf, len + 1, HANDLER_CAN_RWRITE );

    rc = TableData_registerTableData( reg, dinfo, tinfo );
    if ( rc != ErrorCode_SUCCESS ) {
        goto bail;
    }
    Table_handlerOwnsTableInfo( reg->handler->next );
    eptr->reg[ 0 ] = reg;

    /*
         * Register the main output table
         *   using the same table_data handle.
         * This is sufficient to link the two tables,
         *   and implement the AUGMENTS behaviour
         */
    tinfo = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( tinfo, ASN01_OCTET_STR, 0 );
    tinfo->min_column = COLUMN_EXTOUT1_FIRST_COLUMN;
    tinfo->max_column = COLUMN_EXTOUT1_LAST_COLUMN;
    oid_buf[ len ] = 3;
    reg = AgentHandler_createHandlerRegistration(
        "nsExtendOut1Table", handle_nsExtendOutput1Table,
        oid_buf, len + 1, HANDLER_CAN_RONLY );
    rc = TableData_registerTableData( reg, dinfo, tinfo );
    if ( rc != ErrorCode_SUCCESS )
        goto bail;
    Table_handlerOwnsTableInfo( reg->handler->next );
    eptr->reg[ 1 ] = reg;

    /*
         * Register the multi-line output table
         *   using a simple table helper.
         * This handles extracting the indexes from
         *   the request OID, but leaves most of
         *   the work to our handler routine.
         * Still, it was nice while it lasted...
         */
    tinfo = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( tinfo, ASN01_OCTET_STR, ASN01_INTEGER, 0 );
    tinfo->min_column = COLUMN_EXTOUT2_FIRST_COLUMN;
    tinfo->max_column = COLUMN_EXTOUT2_LAST_COLUMN;
    oid_buf[ len ] = 4;
    reg = AgentHandler_createHandlerRegistration(
        "nsExtendOut2Table", handle_nsExtendOutput2Table,
        oid_buf, len + 1, HANDLER_CAN_RONLY );
    rc = Table_registerTable( reg, tinfo );
    if ( rc != ErrorCode_SUCCESS )
        goto bail;
    Table_handlerOwnsTableInfo( reg->handler->next );
    eptr->reg[ 2 ] = reg;

    /*
         * Register a watched scalar to keep track of the number of entries
         */
    oid_buf[ len ] = 1;
    reg = AgentHandler_createHandlerRegistration(
        "nsExtendNumEntries", NULL,
        oid_buf, len + 1, HANDLER_CAN_RONLY );
    winfo = Watcher_createWatcherInfo(
        &( eptr->num_entries ), sizeof( eptr->num_entries ),
        ASN01_INTEGER, WATCHER_FIXED_SIZE );
    rc = Watcher_registerWatchedScalar2( reg, winfo );
    if ( rc != ErrorCode_SUCCESS )
        goto bail;
    eptr->reg[ 3 ] = reg;

    return eptr;

bail:
    if ( eptr->reg[ 3 ] )
        AgentHandler_unregisterHandler( eptr->reg[ 3 ] );
    if ( eptr->reg[ 2 ] )
        AgentHandler_unregisterHandler( eptr->reg[ 2 ] );
    if ( eptr->reg[ 1 ] )
        AgentHandler_unregisterHandler( eptr->reg[ 1 ] );
    if ( eptr->reg[ 0 ] )
        AgentHandler_unregisterHandler( eptr->reg[ 0 ] );
    return NULL;
}

static void
_unregister_extend( extend_registration_block* eptr )
{
    extend_registration_block* prev;

    Assert_assert( eptr );
    for ( prev = ereg_head; prev && prev->next != eptr; prev = prev->next )
        ;
    if ( prev ) {
        Assert_assert( eptr == prev->next );
        prev->next = eptr->next;
    } else {
        Assert_assert( eptr == ereg_head );
        ereg_head = eptr->next;
    }

    TableData_deleteTable( eptr->dinfo );
    free( eptr->root_oid );
    free( eptr );
}

int extend_clear_callback( int majorID, int minorID,
    void* serverarg, void* clientarg )
{
    extend_registration_block *eptr, *enext = NULL;

    for ( eptr = ereg_head; eptr; eptr = enext ) {
        enext = eptr->next;
        AgentHandler_unregisterHandler( eptr->reg[ 0 ] );
        AgentHandler_unregisterHandler( eptr->reg[ 1 ] );
        AgentHandler_unregisterHandler( eptr->reg[ 2 ] );
        AgentHandler_unregisterHandler( eptr->reg[ 3 ] );
        MEMORY_FREE( eptr );
    }
    ereg_head = NULL;
    return 0;
}

void init_extend( void )
{
    AgentReadConfig_priotdRegisterConfigHandler( "extend", extend_parse_config, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "extend-sh", extend_parse_config, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "extendfix", extend_parse_config, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "exec2", extend_parse_config, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "sh2", extend_parse_config, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "execFix2", extend_parse_config, NULL, NULL );
    ( void )_register_extend( ns_extend_oid, ASN01_OID_LENGTH( ns_extend_oid ) );

    AgentReadConfig_priotdRegisterConfigHandler( "exec", extend_parse_config, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "sh", extend_parse_config, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "execFix", extend_parse_config, NULL, NULL );
    compatability_entries = ( netsnmp_old_extend* )
        calloc( max_compatability_entries, sizeof( netsnmp_old_extend ) );
    REGISTER_MIB( "ucd-extensible", old_extensible_variables,
        Variable2_s, old_extensible_variables_oid );

    Callback_register( CallbackMajor_APPLICATION,
        PriotdCallback_PRE_UPDATE_CONFIG,
        extend_clear_callback, NULL );
}

void shutdown_extend( void )
{
    free( compatability_entries );
    compatability_entries = NULL;
    while ( ereg_head )
        _unregister_extend( ereg_head );
}

/*************************
         *
         *  Cached-data hooks
         *  see 'cache_handler' helper
         *
         *************************/

int extend_load_cache( Cache* cache, void* magic )
{

    int out_len = 1024 * 100;
    char out_buf[ 1024 * 100 ];
    int cmd_len = 255 * 2 + 2; /* 2 * DisplayStrings */
    char cmd_buf[ 255 * 2 + 2 ];
    int ret;
    char* cp;
    char* line_buf[ 1024 ];
    netsnmp_extend* extension = ( netsnmp_extend* )magic;

    if ( !magic )
        return -1;
    DEBUG_MSGTL( ( "nsExtendTable:cache", "load %s", extension->token ) );
    if ( extension->args )
        snprintf( cmd_buf, cmd_len, "%s %s", extension->command, extension->args );
    else
        snprintf( cmd_buf, cmd_len, "%s", extension->command );
    if ( extension->flags & NS_EXTEND_FLAGS_SHELL )
        ret = ExecuteCmd_runShellCommand( cmd_buf, extension->input, out_buf, &out_len );
    else
        ret = ExecuteCmd_runExecCommand( cmd_buf, extension->input, out_buf, &out_len );
    DEBUG_MSG( ( "nsExtendTable:cache", ": %s : %d\n", cmd_buf, ret ) );
    if ( ret >= 0 ) {
        if ( out_buf[ out_len - 1 ] == '\n' )
            out_buf[ --out_len ] = '\0'; /* Stomp on trailing newline */
        extension->output = strdup( out_buf );
        extension->out_len = out_len;
        /*
         * Now we need to pick the output apart into separate lines.
         * Start by counting how many lines we've got, and keeping
         * track of where each line starts in a static buffer
         */
        extension->numlines = 1;
        line_buf[ 0 ] = extension->output;
        for ( cp = extension->output; *cp; cp++ ) {
            if ( *cp == '\n' ) {
                line_buf[ extension->numlines++ ] = cp + 1;
            }
        }
        if ( extension->numlines > 1 ) {
            extension->lines = ( char** )calloc( sizeof( char* ), extension->numlines );
            memcpy( extension->lines, line_buf,
                sizeof( char* ) * extension->numlines );
        } else {
            extension->lines = &extension->output;
        }
    }
    extension->result = ret;
    return ret;
}

void extend_free_cache( Cache* cache, void* magic )
{
    netsnmp_extend* extension = ( netsnmp_extend* )magic;
    if ( !magic )
        return;

    DEBUG_MSGTL( ( "nsExtendTable:cache", "free %s\n", extension->token ) );
    if ( extension->output ) {
        MEMORY_FREE( extension->output );
        extension->output = NULL;
    }
    if ( extension->numlines > 1 ) {
        MEMORY_FREE( extension->lines );
    }
    extension->lines = NULL;
    extension->out_len = 0;
    extension->numlines = 0;
}

/*************************
         *
         *  Utility routines for setting up a new entry
         *  (either via SET requests, or the config file)
         *
         *************************/

void _free_extension( netsnmp_extend* extension, extend_registration_block* ereg )
{
    netsnmp_extend* eptr = NULL;
    netsnmp_extend* eprev = NULL;

    if ( !extension )
        return;

    if ( ereg ) {
        /* Unlink from 'ehead' list */
        for ( eptr = ereg->ehead; eptr; eptr = eptr->next ) {
            if ( eptr == extension )
                break;
            eprev = eptr;
        }
        if ( !eptr ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "extend: fell off end of list before finding extension\n" );
            return;
        }
        if ( eprev )
            eprev->next = eptr->next;
        else
            ereg->ehead = eptr->next;
        TableData_removeAndDeleteRow( ereg->dinfo, extension->row );
    }

    MEMORY_FREE( extension->token );
    MEMORY_FREE( extension->cache );
    MEMORY_FREE( extension->command );
    MEMORY_FREE( extension->args );
    MEMORY_FREE( extension->input );
    MEMORY_FREE( extension );
    return;
}

netsnmp_extend*
_new_extension( char* exec_name, int exec_flags, extend_registration_block* ereg )
{
    netsnmp_extend* extension;
    TableRow* row;
    netsnmp_extend *eptr1, *eptr2;
    TableData* dinfo = ereg->dinfo;

    if ( !exec_name )
        return NULL;
    extension = MEMORY_MALLOC_TYPEDEF( netsnmp_extend );
    if ( !extension )
        return NULL;
    extension->token = strdup( exec_name );
    extension->flags = exec_flags;
    extension->cache = CacheHandler_create( 0, extend_load_cache,
        extend_free_cache, NULL, 0 );
    if ( extension->cache )
        extension->cache->magic = extension;

    row = TableData_createTableDataRow();
    if ( !row || !extension->cache ) {
        _free_extension( extension, ereg );
        MEMORY_FREE( row );
        return NULL;
    }
    row->data = ( void* )extension;
    extension->row = row;
    TableData_rowAddIndex( row, ASN01_OCTET_STR,
        exec_name, strlen( exec_name ) );
    if ( TableData_addRow( dinfo, row ) != ErrorCode_SUCCESS ) {
        /* _free_extension( extension, ereg ); */
        MEMORY_FREE( extension ); /* Probably not sufficient */
        MEMORY_FREE( row );
        return NULL;
    }

    ereg->num_entries++;
    /*
         *  Now add this structure to a private linked list.
         *  We don't need this for the main tables - the
         *   'table_data' helper will take care of those.
         *  But it's probably easier to handle the multi-line
         *  output table ourselves, for which we need access
         *  to the underlying data.
         *   So we'll keep a list internally as well.
         */
    for ( eptr1 = ereg->ehead, eptr2 = NULL;
          eptr1;
          eptr2 = eptr1, eptr1 = eptr1->next ) {

        if ( strlen( eptr1->token ) > strlen( exec_name ) )
            break;
        if ( strlen( eptr1->token ) == strlen( exec_name ) && strcmp( eptr1->token, exec_name ) > 0 )
            break;
    }
    if ( eptr2 )
        eptr2->next = extension;
    else
        ereg->ehead = extension;
    extension->next = eptr1;
    return extension;
}

void extend_parse_config( const char* token, char* cptr )
{
    netsnmp_extend* extension;
    char exec_name[ STRMAX ];
    char exec_name2[ STRMAX ]; /* For use with UCD execFix directive */
    char exec_command[ STRMAX ];
    oid oid_buf[ ASN01_MAX_OID_LEN ];
    size_t oid_len;
    extend_registration_block* eptr;
    int flags;

    cptr = ReadConfig_copyNword( cptr, exec_name, sizeof( exec_name ) );
    if ( *exec_name == '.' ) {
        oid_len = ASN01_MAX_OID_LEN - 2;
        if ( 0 == Mib_readObjid( exec_name, oid_buf, &oid_len ) ) {
            ReadConfig_configPerror( "ERROR: Unrecognised OID" );
            return;
        }
        cptr = ReadConfig_copyNword( cptr, exec_name, sizeof( exec_name ) );
        if ( !strcmp( token, "sh" ) || !strcmp( token, "exec" ) ) {
            ReadConfig_configPerror( "ERROR: This output format has been deprecated - Please use the 'extend' directive instead" );
            return;
        }
    } else {
        memcpy( oid_buf, ns_extend_oid, sizeof( ns_extend_oid ) );
        oid_len = ASN01_OID_LENGTH( ns_extend_oid );
    }
    cptr = ReadConfig_copyNword( cptr, exec_command, sizeof( exec_command ) );
    /* XXX - check 'exec_command' exists & is executable */
    flags = ( NS_EXTEND_FLAGS_ACTIVE | NS_EXTEND_FLAGS_CONFIG );
    if ( !strcmp( token, "sh" ) || !strcmp( token, "extend-sh" ) || !strcmp( token, "sh2" ) )
        flags |= NS_EXTEND_FLAGS_SHELL;
    if ( !strcmp( token, "execFix" ) || !strcmp( token, "extendfix" ) || !strcmp( token, "execFix2" ) ) {
        strcpy( exec_name2, exec_name );
        strcat( exec_name, "Fix" );
        flags |= NS_EXTEND_FLAGS_WRITEABLE;
        /* XXX - Check for shell... */
    }

    eptr = _register_extend( oid_buf, oid_len );
    if ( !eptr ) {
        Logger_log( LOGGER_PRIORITY_ERR, "Failed to register extend entry '%s' - possibly duplicate name.\n", exec_name );
        return;
    }
    extension = _new_extension( exec_name, flags, eptr );
    if ( extension ) {
        extension->command = strdup( exec_command );
        if ( cptr )
            extension->args = strdup( cptr );
    } else {
        Logger_log( LOGGER_PRIORITY_ERR, "Failed to register extend entry '%s' - possibly duplicate name.\n", exec_name );
        return;
    }

    /*
     *  Compatability with the UCD extTable
     */
    if ( !strcmp( token, "execFix" ) ) {
        int i;
        for ( i = 0; i < num_compatability_entries; i++ ) {
            if ( !strcmp( exec_name2,
                     compatability_entries[ i ].exec_entry->token ) )
                break;
        }
        if ( i == num_compatability_entries )
            ReadConfig_configPerror( "No matching exec entry" );
        else
            compatability_entries[ i ].efix_entry = extension;

    } else if ( !strcmp( token, "sh" ) || !strcmp( token, "exec" ) ) {
        if ( num_compatability_entries == max_compatability_entries ) {
            /* XXX - should really use dynamic allocation */
            netsnmp_old_extend* new_compatability_entries;
            new_compatability_entries = ( netsnmp_old_extend* )realloc( compatability_entries,
                max_compatability_entries * 2 * sizeof( netsnmp_old_extend ) );
            if ( !new_compatability_entries )
                ReadConfig_configPerror( "No further UCD-compatible entries" );
            else {
                memset( new_compatability_entries + num_compatability_entries, 0,
                    sizeof( netsnmp_old_extend ) * max_compatability_entries );
                max_compatability_entries *= 2;
                compatability_entries = new_compatability_entries;
            }
        }
        if ( num_compatability_entries != max_compatability_entries )
            compatability_entries[ num_compatability_entries++ ].exec_entry = extension;
    }
}

/*************************
         *
         *  Main table handlers
         *  Most of the work is handled
         *   by the 'table_data' helper.
         *
         *************************/

int handle_nsExtendConfigTable( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    TableRequestInfo* table_info;
    netsnmp_extend* extension;
    extend_registration_block* eptr;
    int i;
    int need_to_validate = 0;

    for ( request = requests; request; request = request->next ) {
        if ( request->processed )
            continue;
        table_info = Table_extractTableInfo( request );
        extension = ( netsnmp_extend* )TableData_extractTableRowData( request );

        DEBUG_MSGTL( ( "nsExtendTable:config", "varbind: " ) );
        DEBUG_MSGOID( ( "nsExtendTable:config", request->requestvb->name,
            request->requestvb->nameLength ) );
        DEBUG_MSG( ( "nsExtendTable:config", " (%s)\n",
            MapList_findLabel( "agentMode", reqinfo->mode ) ) );

        switch ( reqinfo->mode ) {
        case MODE_GET:
            switch ( table_info->colnum ) {
            case COLUMN_EXTCFG_COMMAND:
                Client_setVarTypedValue(
                    request->requestvb, ASN01_OCTET_STR,
                    extension->command,
                    ( extension->command ) ? strlen( extension->command ) : 0 );
                break;
            case COLUMN_EXTCFG_ARGS:
                Client_setVarTypedValue(
                    request->requestvb, ASN01_OCTET_STR,
                    extension->args,
                    ( extension->args ) ? strlen( extension->args ) : 0 );
                break;
            case COLUMN_EXTCFG_INPUT:
                Client_setVarTypedValue(
                    request->requestvb, ASN01_OCTET_STR,
                    extension->input,
                    ( extension->input ) ? strlen( extension->input ) : 0 );
                break;
            case COLUMN_EXTCFG_CACHETIME:
                Client_setVarTypedValue(
                    request->requestvb, ASN01_INTEGER,
                    ( u_char* )&extension->cache->timeout, sizeof( int ) );
                break;
            case COLUMN_EXTCFG_EXECTYPE:
                i = ( ( extension->flags & NS_EXTEND_FLAGS_SHELL ) ? NS_EXTEND_ETYPE_SHELL : NS_EXTEND_ETYPE_EXEC );
                Client_setVarTypedValue(
                    request->requestvb, ASN01_INTEGER,
                    ( u_char* )&i, sizeof( i ) );
                break;
            case COLUMN_EXTCFG_RUNTYPE:
                i = ( ( extension->flags & NS_EXTEND_FLAGS_WRITEABLE ) ? NS_EXTEND_RTYPE_RWRITE : NS_EXTEND_RTYPE_RONLY );
                Client_setVarTypedValue(
                    request->requestvb, ASN01_INTEGER,
                    ( u_char* )&i, sizeof( i ) );
                break;

            case COLUMN_EXTCFG_STORAGE:
                i = ( ( extension->flags & NS_EXTEND_FLAGS_CONFIG ) ? TC_ST_PERMANENT : TC_ST_VOLATILE );
                Client_setVarTypedValue(
                    request->requestvb, ASN01_INTEGER,
                    ( u_char* )&i, sizeof( i ) );
                break;
            case COLUMN_EXTCFG_STATUS:
                i = ( ( extension->flags & NS_EXTEND_FLAGS_ACTIVE ) ? TC_RS_ACTIVE : TC_RS_NOTINSERVICE );
                Client_setVarTypedValue(
                    request->requestvb, ASN01_INTEGER,
                    ( u_char* )&i, sizeof( i ) );
                break;

            default:
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHOBJECT );
                continue;
            }
            break;

        /**********
         *
         * Start of SET handling
         *
         *   All config objects are potentially writable except
         *     nsExtendStorage which is fixed as either 'permanent'
         *     (if read from a config file) or 'volatile' (if set via SNMP)
         *   The string-based settings of a 'permanent' entry cannot 
         *     be changed - neither can the execution or run type.
         *   Such entries can be (temporarily) marked as inactive,
         *     and the cache timeout adjusted, but these changes are
         *     not persistent.
         *
         **********/

        case MODE_SET_RESERVE1:
            /*
             * Validate the new assignments
             */
            switch ( table_info->colnum ) {
            case COLUMN_EXTCFG_COMMAND:
                if ( request->requestvb->type != ASN01_OCTET_STR ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_WRONGTYPE;
                }
                /*
                 * Must have a full path to the command
                 * XXX - Assumes Unix-style paths
                 */
                if ( request->requestvb->valueLength == 0 || request->requestvb->value.string[ 0 ] != '/' ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGVALUE );
                    return PRIOT_ERR_WRONGVALUE;
                }
                /*
                 * XXX - need to check this file exists
                 *       (and is executable)
                 */

                if ( extension && extension->flags & NS_EXTEND_FLAGS_CONFIG ) {
                    /*
                     * config entries are "permanent" so can't be changed
                     */
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_NOTWRITABLE );
                    return PRIOT_ERR_NOTWRITABLE;
                }
                break;

            case COLUMN_EXTCFG_ARGS:
            case COLUMN_EXTCFG_INPUT:
                if ( request->requestvb->type != ASN01_OCTET_STR ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_WRONGTYPE;
                }

                if ( extension && extension->flags & NS_EXTEND_FLAGS_CONFIG ) {
                    /*
                     * config entries are "permanent" so can't be changed
                     */
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_NOTWRITABLE );
                    return PRIOT_ERR_NOTWRITABLE;
                }
                break;

            case COLUMN_EXTCFG_CACHETIME:
                if ( request->requestvb->type != ASN01_INTEGER ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_WRONGTYPE;
                }
                i = *request->requestvb->value.integer;
                /*
                 * -1 is a special value indicating "don't cache"
                 *    [[ XXX - should this be 0 ?? ]]
                 * Otherwise, cache times must be non-negative
                 */
                if ( i < -1 ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGVALUE );
                    return PRIOT_ERR_WRONGVALUE;
                }
                break;

            case COLUMN_EXTCFG_EXECTYPE:
                if ( request->requestvb->type != ASN01_INTEGER ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_WRONGTYPE;
                }
                i = *request->requestvb->value.integer;
                if ( i < 1 || i > 2 ) { /* 'exec(1)' or 'shell(2)' only */
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGVALUE );
                    return PRIOT_ERR_WRONGVALUE;
                }
                if ( extension && extension->flags & NS_EXTEND_FLAGS_CONFIG ) {
                    /*
                     * config entries are "permanent" so can't be changed
                     */
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_NOTWRITABLE );
                    return PRIOT_ERR_NOTWRITABLE;
                }
                break;

            case COLUMN_EXTCFG_RUNTYPE:
                if ( request->requestvb->type != ASN01_INTEGER ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_WRONGTYPE;
                }
                /*
                 * 'run-on-read(1)', 'run-on-set(2)'
                 *  or 'run-command(3)' only
                 */
                i = *request->requestvb->value.integer;
                if ( i < 1 || i > 3 ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGVALUE );
                    return PRIOT_ERR_WRONGVALUE;
                }
                /*
                 * 'run-command(3)' can only be used with
                 *  a pre-existing 'run-on-set(2)' entry.
                 */
                if ( i == 3 && !( extension && ( extension->flags & NS_EXTEND_FLAGS_WRITEABLE ) ) ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_INCONSISTENTVALUE );
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
                /*
                 * 'run-command(3)' is the only valid assignment
                 *  for permanent (i.e. config) entries
                 */
                if ( ( extension && extension->flags & NS_EXTEND_FLAGS_CONFIG )
                    && i != 3 ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_INCONSISTENTVALUE );
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
                break;

            case COLUMN_EXTCFG_STATUS:
                if ( request->requestvb->type != ASN01_INTEGER ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_WRONGTYPE;
                }
                i = *request->requestvb->value.integer;
                switch ( i ) {
                case TC_RS_ACTIVE:
                case TC_RS_NOTINSERVICE:
                    if ( !extension ) {
                        /* Must be used with existing rows */
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_INCONSISTENTVALUE );
                        return PRIOT_ERR_INCONSISTENTVALUE;
                    }
                    break; /* OK */
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    if ( extension ) {
                        /* Can only be used to create new rows */
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_INCONSISTENTVALUE );
                        return PRIOT_ERR_INCONSISTENTVALUE;
                    }
                    break;
                case TC_RS_DESTROY:
                    break;
                default:
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGVALUE );
                    return PRIOT_ERR_WRONGVALUE;
                }
                break;

            default:
                Agent_setRequestError( reqinfo, request,
                    PRIOT_ERR_NOTWRITABLE );
                return PRIOT_ERR_NOTWRITABLE;
            }
            break;

        case MODE_SET_RESERVE2:
            switch ( table_info->colnum ) {
            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->value.integer;
                switch ( i ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    eptr = _find_extension_block( request->requestvb->name,
                        request->requestvb->nameLength );
                    extension = _new_extension( ( char* )table_info->indexes->value.string,
                        0, eptr );
                    if ( !extension ) { /* failed */
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_RESOURCEUNAVAILABLE );
                        return PRIOT_ERR_RESOURCEUNAVAILABLE;
                    }
                    TableData_insertTableRow( request, extension->row );
                }
            }
            break;

        case MODE_SET_FREE:
            switch ( table_info->colnum ) {
            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->value.integer;
                switch ( i ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    eptr = _find_extension_block( request->requestvb->name,
                        request->requestvb->nameLength );
                    _free_extension( extension, eptr );
                }
            }
            break;

        case MODE_SET_ACTION:
            switch ( table_info->colnum ) {
            case COLUMN_EXTCFG_COMMAND:
                extension->old_command = extension->command;
                extension->command = Memory_strdupAndNull(
                    request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_EXTCFG_ARGS:
                extension->old_args = extension->args;
                extension->args = Memory_strdupAndNull(
                    request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_EXTCFG_INPUT:
                extension->old_input = extension->input;
                extension->input = Memory_strdupAndNull(
                    request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->value.integer;
                switch ( i ) {
                case TC_RS_ACTIVE:
                case TC_RS_CREATEANDGO:
                    need_to_validate = 1;
                }
                break;
            }
            break;

        case MODE_SET_UNDO:
            switch ( table_info->colnum ) {
            case COLUMN_EXTCFG_COMMAND:
                if ( extension && extension->old_command ) {
                    MEMORY_FREE( extension->command );
                    extension->command = extension->old_command;
                    extension->old_command = NULL;
                }
                break;
            case COLUMN_EXTCFG_ARGS:
                if ( extension && extension->old_args ) {
                    MEMORY_FREE( extension->args );
                    extension->args = extension->old_args;
                    extension->old_args = NULL;
                }
                break;
            case COLUMN_EXTCFG_INPUT:
                if ( extension && extension->old_input ) {
                    MEMORY_FREE( extension->input );
                    extension->input = extension->old_input;
                    extension->old_input = NULL;
                }
                break;
            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->value.integer;
                switch ( i ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    eptr = _find_extension_block( request->requestvb->name,
                        request->requestvb->nameLength );
                    _free_extension( extension, eptr );
                }
                break;
            }
            break;

        case MODE_SET_COMMIT:
            switch ( table_info->colnum ) {
            case COLUMN_EXTCFG_CACHETIME:
                i = *request->requestvb->value.integer;
                extension->cache->timeout = i;
                break;

            case COLUMN_EXTCFG_RUNTYPE:
                i = *request->requestvb->value.integer;
                switch ( i ) {
                case 1:
                    extension->flags &= ~NS_EXTEND_FLAGS_WRITEABLE;
                    break;
                case 2:
                    extension->flags |= NS_EXTEND_FLAGS_WRITEABLE;
                    break;
                case 3:
                    ( void )CacheHandler_checkAndReload( extension->cache );
                    break;
                }
                break;

            case COLUMN_EXTCFG_EXECTYPE:
                i = *request->requestvb->value.integer;
                if ( i == NS_EXTEND_ETYPE_SHELL )
                    extension->flags |= NS_EXTEND_FLAGS_SHELL;
                else
                    extension->flags &= ~NS_EXTEND_FLAGS_SHELL;
                break;

            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->value.integer;
                switch ( i ) {
                case TC_RS_ACTIVE:
                case TC_RS_CREATEANDGO:
                    extension->flags |= NS_EXTEND_FLAGS_ACTIVE;
                    break;
                case TC_RS_NOTINSERVICE:
                case TC_RS_CREATEANDWAIT:
                    extension->flags &= ~NS_EXTEND_FLAGS_ACTIVE;
                    break;
                case TC_RS_DESTROY:
                    eptr = _find_extension_block( request->requestvb->name,
                        request->requestvb->nameLength );
                    _free_extension( extension, eptr );
                    break;
                }
            }
            break;

        default:
            Agent_setRequestError( reqinfo, request, PRIOT_ERR_GENERR );
            return PRIOT_ERR_GENERR;
        }
    }

    /*
     * If we're marking a given row as active,
     *  then we need to check that it's ready.
     */
    if ( need_to_validate ) {
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;
            table_info = Table_extractTableInfo( request );
            extension = ( netsnmp_extend* )TableData_extractTableRowData( request );
            switch ( table_info->colnum ) {
            case COLUMN_EXTCFG_STATUS:
                i = *request->requestvb->value.integer;
                if ( ( i == TC_RS_ACTIVE || i == TC_RS_CREATEANDGO ) && !( extension && extension->command && extension->command[ 0 ] == '/' /* &&
                      is_executable(extension->command) */ ) ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_INCONSISTENTVALUE );
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
            }
        }
    }

    return PRIOT_ERR_NOERROR;
}

int handle_nsExtendOutput1Table( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    TableRequestInfo* table_info;
    netsnmp_extend* extension;
    int len;

    for ( request = requests; request; request = request->next ) {
        if ( request->processed )
            continue;
        table_info = Table_extractTableInfo( request );
        extension = ( netsnmp_extend* )TableData_extractTableRowData( request );

        DEBUG_MSGTL( ( "nsExtendTable:output1", "varbind: " ) );
        DEBUG_MSGOID( ( "nsExtendTable:output1", request->requestvb->name,
            request->requestvb->nameLength ) );
        DEBUG_MSG( ( "nsExtendTable:output1", "\n" ) );

        switch ( reqinfo->mode ) {
        case MODE_GET:
            if ( !extension || !( extension->flags & NS_EXTEND_FLAGS_ACTIVE ) ) {
                /*
                 * If this row is inactive, then skip it.
                 */
                Agent_setRequestError( reqinfo, request,
                    PRIOT_NOSUCHINSTANCE );
                continue;
            }
            if ( !( extension->flags & NS_EXTEND_FLAGS_WRITEABLE ) && ( CacheHandler_checkAndReload( extension->cache ) < 0 ) ) {
                /*
                 * If reloading the output cache of a 'run-on-read'
                 * entry fails, then skip it.
                 */
                Agent_setRequestError( reqinfo, request,
                    PRIOT_NOSUCHINSTANCE );
                continue;
            }
            if ( ( extension->flags & NS_EXTEND_FLAGS_WRITEABLE ) && ( CacheHandler_checkExpired( extension->cache ) == 1 ) ) {
                /*
                 * If the output cache of a 'run-on-write'
                 * entry has expired, then skip it.
                 */
                Agent_setRequestError( reqinfo, request,
                    PRIOT_NOSUCHINSTANCE );
                continue;
            }

            switch ( table_info->colnum ) {
            case COLUMN_EXTOUT1_OUTLEN:
                Client_setVarTypedValue(
                    request->requestvb, ASN01_INTEGER,
                    ( u_char* )&extension->out_len, sizeof( int ) );
                break;
            case COLUMN_EXTOUT1_OUTPUT1:
                /* 
                 * If we've got more than one line,
                 * find the length of the first one.
                 * Otherwise find the length of the whole string.
                 */
                if ( extension->numlines > 1 ) {
                    len = ( extension->lines[ 1 ] ) - ( extension->output ) - 1;
                } else if ( extension->output ) {
                    len = strlen( extension->output );
                } else {
                    len = 0;
                }
                Client_setVarTypedValue(
                    request->requestvb, ASN01_OCTET_STR,
                    extension->output, len );
                break;
            case COLUMN_EXTOUT1_OUTPUT2:
                Client_setVarTypedValue(
                    request->requestvb, ASN01_OCTET_STR,
                    extension->output,
                    ( extension->output ) ? extension->out_len : 0 );
                break;
            case COLUMN_EXTOUT1_NUMLINES:
                Client_setVarTypedValue(
                    request->requestvb, ASN01_INTEGER,
                    ( u_char* )&extension->numlines, sizeof( int ) );
                break;
            case COLUMN_EXTOUT1_RESULT:
                Client_setVarTypedValue(
                    request->requestvb, ASN01_INTEGER,
                    ( u_char* )&extension->result, sizeof( int ) );
                break;
            default:
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHOBJECT );
                continue;
            }
            break;
        default:
            Agent_setRequestError( reqinfo, request, PRIOT_ERR_GENERR );
            return PRIOT_ERR_GENERR;
        }
    }
    return PRIOT_ERR_NOERROR;
}

/*************************
         *
         *  Multi-line output table handler
         *  Most of the work is handled here.
         *
         *************************/

/*
 *  Locate the appropriate entry for a given request
 */
netsnmp_extend*
_extend_find_entry( RequestInfo* request,
    TableRequestInfo* table_info,
    int mode )
{
    netsnmp_extend* eptr;
    extend_registration_block* ereg;
    unsigned int line_idx;
    oid oid_buf[ ASN01_MAX_OID_LEN ];
    int oid_len;
    int i;
    char* token;
    size_t token_len;

    if ( !request || !table_info || !table_info->indexes
        || !table_info->indexes->next ) {
        DEBUG_MSGTL( ( "nsExtendTable:output2", "invalid invocation\n" ) );
        return NULL;
    }

    ereg = _find_extension_block( request->requestvb->name,
        request->requestvb->nameLength );

    /***
     *  GET handling - find the exact entry being requested
     ***/
    if ( mode == MODE_GET ) {
        DEBUG_MSGTL( ( "nsExtendTable:output2", "GET: %s / %ld\n ",
            table_info->indexes->value.string,
            *table_info->indexes->next->value.integer ) );
        for ( eptr = ereg->ehead; eptr; eptr = eptr->next ) {
            if ( !strcmp( eptr->token, ( char* )table_info->indexes->value.string ) )
                break;
        }

        if ( eptr ) {
            /*
             * Ensure the output is available...
             */
            if ( !( eptr->flags & NS_EXTEND_FLAGS_ACTIVE ) || ( CacheHandler_checkAndReload( eptr->cache ) < 0 ) )
                return NULL;

            /*
             * ...and check the line requested is valid
             */
            line_idx = *table_info->indexes->next->value.integer;
            if ( line_idx < 1 || line_idx > eptr->numlines )
                return NULL;
        }
    }

    /***
         *  GETNEXT handling - find the first suitable entry
         ***/
    else {
        if ( !table_info->indexes->valueLength ) {
            DEBUG_MSGTL( ( "nsExtendTable:output2", "GETNEXT: first entry\n" ) );
            /*
             * Beginning of the table - find the first active
             *  (and successful) entry, and use the first line of it
             */
            for ( eptr = ereg->ehead; eptr; eptr = eptr->next ) {
                if ( ( eptr->flags & NS_EXTEND_FLAGS_ACTIVE ) && ( CacheHandler_checkAndReload( eptr->cache ) >= 0 ) ) {
                    line_idx = 1;
                    break;
                }
            }
        } else {
            token = ( char* )table_info->indexes->value.string;
            token_len = table_info->indexes->valueLength;
            line_idx = *table_info->indexes->next->value.integer;
            DEBUG_MSGTL( ( "nsExtendTable:output2", "GETNEXT: %s / %d\n ",
                token, line_idx ) );
            /*
             * Otherwise, find the first entry not earlier
             * than the requested token...
             */
            for ( eptr = ereg->ehead; eptr; eptr = eptr->next ) {
                if ( strlen( eptr->token ) > token_len )
                    break;
                if ( strlen( eptr->token ) == token_len && strcmp( eptr->token, token ) >= 0 )
                    break;
            }
            if ( !eptr )
                return NULL; /* (assuming there is one) */

            /*
             * ... and make sure it's active & the output is available
             * (or use the first following entry that is)
             */
            for ( ; eptr; eptr = eptr->next ) {
                if ( ( eptr->flags & NS_EXTEND_FLAGS_ACTIVE ) && ( CacheHandler_checkAndReload( eptr->cache ) >= 0 ) ) {
                    break;
                }
                line_idx = 1;
            }

            if ( !eptr )
                return NULL; /* (assuming there is one) */

            /*
             *  If we're working with the same entry that was requested,
             *  see whether we've reached the end of the output...
             */
            if ( !strcmp( eptr->token, token ) ) {
                if ( eptr->numlines <= line_idx ) {
                    /*
                     * ... and if so, move on to the first line
                     * of the next (active and successful) entry.
                     */
                    line_idx = 1;
                    for ( eptr = eptr->next; eptr; eptr = eptr->next ) {
                        if ( ( eptr->flags & NS_EXTEND_FLAGS_ACTIVE ) && ( CacheHandler_checkAndReload( eptr->cache ) >= 0 ) ) {
                            break;
                        }
                    }
                } else {
                    /*
                     * Otherwise just use the next line of this entry.
                     */
                    line_idx++;
                }
            } else {
                /*
                 * If this is not the same entry that was requested,
                 * then we should return the first line.
                 */
                line_idx = 1;
            }
        }
        if ( eptr ) {
            DEBUG_MSGTL( ( "nsExtendTable:output2", "GETNEXT -> %s / %d\n ",
                eptr->token, line_idx ) );
            /*
             * Since we're processing a GETNEXT request,
             * now we've found the appropriate entry (and line),
             * we need to update the varbind OID ...
             */
            memset( oid_buf, 0, sizeof( oid_buf ) );
            oid_len = ereg->oid_len;
            memcpy( oid_buf, ereg->root_oid, oid_len * sizeof( oid ) );
            oid_buf[ oid_len++ ] = 4; /* nsExtendOutput2Table */
            oid_buf[ oid_len++ ] = 1; /* nsExtendOutput2Entry */
            oid_buf[ oid_len++ ] = COLUMN_EXTOUT2_OUTLINE;
            /* string token index */
            oid_buf[ oid_len++ ] = strlen( eptr->token );
            for ( i = 0; i < ( int )strlen( eptr->token ); i++ )
                oid_buf[ oid_len + i ] = eptr->token[ i ];
            oid_len += strlen( eptr->token );
            /* plus line number */
            oid_buf[ oid_len++ ] = line_idx;
            Client_setVarObjid( request->requestvb, oid_buf, oid_len );
            /*
             * ... and index values to match.
             */
            Client_setVarValue( table_info->indexes,
                eptr->token, strlen( eptr->token ) );
            Client_setVarValue( table_info->indexes->next,
                ( const u_char* )&line_idx, sizeof( line_idx ) );
        }
    }
    return eptr; /* Finally, signal success */
}

/*
 *  Multi-line output handler
 *  Locate the appropriate entry (using _extend_find_entry)
 *  and return the appropriate output line
 */
int handle_nsExtendOutput2Table( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    TableRequestInfo* table_info;
    netsnmp_extend* extension;
    char* cp;
    unsigned int line_idx;
    int len;

    for ( request = requests; request; request = request->next ) {
        if ( request->processed )
            continue;

        table_info = Table_extractTableInfo( request );
        extension = _extend_find_entry( request, table_info, reqinfo->mode );

        DEBUG_MSGTL( ( "nsExtendTable:output2", "varbind: " ) );
        DEBUG_MSGOID( ( "nsExtendTable:output2", request->requestvb->name,
            request->requestvb->nameLength ) );
        DEBUG_MSG( ( "nsExtendTable:output2", " (%s)\n",
            ( extension ) ? extension->token : "[none]" ) );

        if ( !extension ) {
            if ( reqinfo->mode == MODE_GET )
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHINSTANCE );
            else
                Agent_setRequestError( reqinfo, request, PRIOT_ENDOFMIBVIEW );
            continue;
        }

        switch ( reqinfo->mode ) {
        case MODE_GET:
        case MODE_GETNEXT:
            switch ( table_info->colnum ) {
            case COLUMN_EXTOUT2_OUTLINE:
                /* 
                 * Determine which line we've been asked for....
                 */
                line_idx = *table_info->indexes->next->value.integer;
                if ( line_idx < 1 || line_idx > extension->numlines ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHINSTANCE );
                    continue;
                }
                cp = extension->lines[ line_idx - 1 ];

                /* 
                 * ... and how long it is.
                 */
                if ( extension->numlines > line_idx )
                    len = ( extension->lines[ line_idx ] ) - cp - 1;
                else if ( cp )
                    len = strlen( cp );
                else
                    len = 0;

                Client_setVarTypedValue( request->requestvb,
                    ASN01_OCTET_STR, cp, len );
                break;
            default:
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHOBJECT );
                continue;
            }
            break;
        default:
            Agent_setRequestError( reqinfo, request, PRIOT_ERR_GENERR );
            return PRIOT_ERR_GENERR;
        }
    }
    return PRIOT_ERR_NOERROR;
}

/*************************
         *
         *  Compatability with the UCD extTable
         *
         *************************/

char* _get_cmdline( netsnmp_extend* extend )
{
    size_t size;
    char* newbuf;
    const char* args = extend->args;

    if ( args == NULL )
        /* Use empty string for processes without arguments. */
        args = "";

    size = strlen( extend->command ) + strlen( args ) + 2;
    if ( size > cmdlinesize ) {
        newbuf = ( char* )realloc( cmdlinebuf, size );
        if ( !newbuf ) {
            free( cmdlinebuf );
            cmdlinebuf = NULL;
            cmdlinesize = 0;
            return NULL;
        }
        cmdlinebuf = newbuf;
        cmdlinesize = size;
    }
    sprintf( cmdlinebuf, "%s %s", extend->command, args );
    return cmdlinebuf;
}

u_char*
var_extensible_old( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact,
    size_t* var_len, WriteMethodFT** write_method )
{
    netsnmp_old_extend* exten = NULL;
    static long long_ret;
    unsigned int idx;
    char* cmdline;

    if ( header_simple_table( vp, name, length, exact, var_len, write_method, num_compatability_entries ) )
        return ( NULL );

    idx = name[ *length - 1 ] - 1;
    if ( idx > max_compatability_entries )
        return NULL;
    exten = &compatability_entries[ idx ];
    if ( exten ) {
        switch ( vp->magic ) {
        case MIBINDEX:
            long_ret = name[ *length - 1 ];
            return ( ( u_char* )( &long_ret ) );
        case ERRORNAME: /* name defined in config file */
            *var_len = strlen( exten->exec_entry->token );
            return ( ( u_char* )( exten->exec_entry->token ) );
        case SHELLCOMMAND:
            cmdline = _get_cmdline( exten->exec_entry );
            if ( cmdline )
                *var_len = strlen( cmdline );
            return ( ( u_char* )cmdline );
        case ERRORFLAG: /* return code from the process */
            CacheHandler_checkAndReload( exten->exec_entry->cache );
            long_ret = exten->exec_entry->result;
            return ( ( u_char* )( &long_ret ) );
        case ERRORMSG: /* first line of text returned from the process */
            CacheHandler_checkAndReload( exten->exec_entry->cache );
            if ( exten->exec_entry->numlines > 1 ) {
                *var_len = ( exten->exec_entry->lines[ 1 ] ) - ( exten->exec_entry->output ) - 1;
            } else if ( exten->exec_entry->output ) {
                *var_len = strlen( exten->exec_entry->output );
            } else {
                *var_len = 0;
            }
            return ( ( u_char* )( exten->exec_entry->output ) );
        case ERRORFIX:
            *write_method = fixExec2Error;
            vars_longReturn = 0;
            return ( ( u_char* )&vars_longReturn );

        case ERRORFIXCMD:
            if ( exten->efix_entry ) {
                cmdline = _get_cmdline( exten->efix_entry );
                if ( cmdline )
                    *var_len = strlen( cmdline );
                return ( ( u_char* )cmdline );
            } else {
                *var_len = 0;
                return ( ( u_char* )&vars_longReturn ); /* Just needs to be non-null! */
            }
        }
        return NULL;
    }
    return NULL;
}

int fixExec2Error( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    netsnmp_old_extend* exten = NULL;
    unsigned int idx;

    idx = name[ name_len - 1 ] - 1;
    exten = &compatability_entries[ idx ];

    switch ( action ) {
    case MODE_SET_RESERVE1:
        if ( var_val_type != ASN01_INTEGER ) {
            Logger_log( LOGGER_PRIORITY_ERR, "Wrong type != int\n" );
            return PRIOT_ERR_WRONGTYPE;
        }
        idx = *( ( long* )var_val );
        if ( idx != 1 ) {
            Logger_log( LOGGER_PRIORITY_ERR, "Wrong value != 1\n" );
            return PRIOT_ERR_WRONGVALUE;
        }
        if ( !exten || !exten->efix_entry ) {
            Logger_log( LOGGER_PRIORITY_ERR, "No command to run\n" );
            return PRIOT_ERR_GENERR;
        }
        return PRIOT_ERR_NOERROR;

    case MODE_SET_COMMIT:
        CacheHandler_checkAndReload( exten->efix_entry->cache );
    }
    return PRIOT_ERR_NOERROR;
}
