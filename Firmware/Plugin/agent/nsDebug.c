
#include "nsDebug.h"
#include "Client.h"
#include "Debug.h"
#include "DefaultStore.h"
#include "Scalar.h"
#include "Tc.h"

#define nsConfigDebug 1, 3, 6, 1, 4, 1, 8072, 1, 7, 1

void init_nsDebug( void )
{
    /*
     * OIDs for the debugging control scalar objects
     *
     * Note that these we're registering the full object rather
     *  than the (sole) valid instance in each case, in order
     *  to handle requests for invalid instances properly.
     */
    const oid nsDebugEnabled_oid[] = { nsConfigDebug, 1 };
    const oid nsDebugOutputAll_oid[] = { nsConfigDebug, 2 };
    const oid nsDebugDumpPdu_oid[] = { nsConfigDebug, 3 };

/*
     * ... and for the token table.
     */

#define DBGTOKEN_PREFIX 2
#define DBGTOKEN_ENABLED 3
#define DBGTOKEN_STATUS 4
    const oid nsDebugTokenTable_oid[] = { nsConfigDebug, 4 };

    TableRegistrationInfo* table_info;
    IteratorInfo* iinfo;

    /*
     * Register the scalar objects...
     */
    DEBUG_MSGTL( ( "nsDebugScalars", "Initializing\n" ) );
    Scalar_registerScalar(
        AgentHandler_createHandlerRegistration(
            "nsDebugEnabled", handle_nsDebugEnabled,
            nsDebugEnabled_oid, ASN01_OID_LENGTH( nsDebugEnabled_oid ),
            HANDLER_CAN_RWRITE ) );
    Scalar_registerScalar(
        AgentHandler_createHandlerRegistration(
            "nsDebugOutputAll", handle_nsDebugOutputAll,
            nsDebugOutputAll_oid, ASN01_OID_LENGTH( nsDebugOutputAll_oid ),
            HANDLER_CAN_RWRITE ) );
    Scalar_registerScalar(
        AgentHandler_createHandlerRegistration(
            "nsDebugDumpPdu", handle_nsDebugDumpPdu,
            nsDebugDumpPdu_oid, ASN01_OID_LENGTH( nsDebugDumpPdu_oid ),
            HANDLER_CAN_RWRITE ) );

    /*
     * ... and the table.
     * We need to define the column structure and indexing....
     */

    table_info = TOOLS_MALLOC_TYPEDEF( TableRegistrationInfo );
    if ( !table_info ) {
        return;
    }
    Table_helperAddIndexes( table_info, ASN01_PRIV_IMPLIED_OCTET_STR, 0 );
    table_info->min_column = DBGTOKEN_STATUS;
    table_info->max_column = DBGTOKEN_STATUS;

    /*
     * .... and the iteration information ....
     */
    iinfo = TOOLS_MALLOC_TYPEDEF( IteratorInfo );
    if ( !iinfo ) {
        return;
    }
    iinfo->get_first_data_point = get_first_debug_entry;
    iinfo->get_next_data_point = get_next_debug_entry;
    iinfo->table_reginfo = table_info;

    /*
     * .... and register the table with the agent.
     */
    TableIterator_registerTableIterator2(
        AgentHandler_createHandlerRegistration(
            "tzDebugTable", handle_nsDebugTable,
            nsDebugTokenTable_oid, ASN01_OID_LENGTH( nsDebugTokenTable_oid ),
            HANDLER_CAN_RWRITE ),
        iinfo );
}

int handle_nsDebugEnabled( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    long enabled;
    RequestInfo* request = NULL;

    switch ( reqinfo->mode ) {

    case MODE_GET:
        enabled = Debug_getDoDebugging();
        if ( enabled == 0 )
            enabled = 2; /* false */
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            Client_setVarTypedValue( request->requestvb, ASN01_INTEGER,
                ( u_char* )&enabled, sizeof( enabled ) );
        }
        break;

    case MODE_SET_RESERVE1:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            if ( request->status != 0 ) {
                return PRIOT_ERR_NOERROR; /* Already got an error */
            }
            if ( request->requestvb->type != ASN01_INTEGER ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGTYPE );
                return PRIOT_ERR_WRONGTYPE;
            }
            if ( ( *request->requestvb->val.integer != 1 ) && ( *request->requestvb->val.integer != 2 ) ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGVALUE );
                return PRIOT_ERR_WRONGVALUE;
            }
        }
        break;

    case MODE_SET_COMMIT:
        enabled = *requests->requestvb->val.integer;
        if ( enabled == 2 ) /* false */
            enabled = 0;
        Debug_setDoDebugging( enabled );
        break;
    }

    return PRIOT_ERR_NOERROR;
}

int handle_nsDebugOutputAll( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    long enabled;
    RequestInfo* request = NULL;

    switch ( reqinfo->mode ) {

    case MODE_GET:
        enabled = Debug_getDoDebugging();
        if ( enabled == 0 )
            enabled = 2; /* false */
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            Client_setVarTypedValue( request->requestvb, ASN01_INTEGER,
                ( u_char* )&enabled, sizeof( enabled ) );
        }
        break;

    case MODE_SET_RESERVE1:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            if ( request->status != 0 ) {
                return PRIOT_ERR_NOERROR; /* Already got an error */
            }
            if ( request->requestvb->type != ASN01_INTEGER ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGTYPE );
                return PRIOT_ERR_WRONGTYPE;
            }
            if ( ( *request->requestvb->val.integer != 1 ) && ( *request->requestvb->val.integer != 2 ) ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGVALUE );
                return PRIOT_ERR_WRONGVALUE;
            }
        }
        break;

    case MODE_SET_COMMIT:
        enabled = *requests->requestvb->val.integer;
        if ( enabled == 2 ) /* false */
            enabled = 0;
        Debug_setDoDebugging( enabled );
        break;
    }

    return PRIOT_ERR_NOERROR;
}

int handle_nsDebugDumpPdu( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    long enabled;
    RequestInfo* request = NULL;

    switch ( reqinfo->mode ) {

    case MODE_GET:
        enabled = DefaultStore_getBoolean( DsStorage_LIBRARY_ID,
            DsBool_DUMP_PACKET );
        if ( enabled == 0 )
            enabled = 2; /* false */
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            Client_setVarTypedValue( request->requestvb, ASN01_INTEGER,
                ( u_char* )&enabled, sizeof( enabled ) );
        }
        break;

    case MODE_SET_RESERVE1:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            if ( request->status != 0 ) {
                return PRIOT_ERR_NOERROR; /* Already got an error */
            }
            if ( request->requestvb->type != ASN01_INTEGER ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGTYPE );
                return PRIOT_ERR_WRONGTYPE;
            }
            if ( ( *request->requestvb->val.integer != 1 ) && ( *request->requestvb->val.integer != 2 ) ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGVALUE );
                return PRIOT_ERR_WRONGVALUE;
            }
        }
        break;

    case MODE_SET_COMMIT:
        enabled = *requests->requestvb->val.integer;
        if ( enabled == 2 ) /* false */
            enabled = 0;
        DefaultStore_setBoolean( DsStorage_LIBRARY_ID,
            DsBool_DUMP_PACKET, enabled );
        break;
    }

    return PRIOT_ERR_NOERROR;
}

/*
 * var_tzIntTableFixed():
 *   Handle the tzIntTable as a fixed table of NUMBER_TZ_ENTRIES rows,
 *    with the timezone offset hardwired to be the same as the index.
 */

Types_VariableList*
get_first_debug_entry( void** loop_context, void** data_context,
    Types_VariableList* index,
    IteratorInfo* data )
{
    int i;

    for ( i = 0; i < debug_numTokens; i++ ) {
        /* skip excluded til mib is updated */
        if ( debug_tokens[ i ].tokenName && ( debug_tokens[ i ].enabled != 2 ) )
            break;
    }
    if ( i == debug_numTokens )
        return NULL;

    Client_setVarValue( index, debug_tokens[ i ].tokenName,
        strlen( debug_tokens[ i ].tokenName ) );
    *loop_context = ( void* )( intptr_t )i;
    *data_context = ( void* )&debug_tokens[ i ];
    return index;
}

Types_VariableList*
get_next_debug_entry( void** loop_context, void** data_context,
    Types_VariableList* index,
    IteratorInfo* data )
{
    int i = ( int )( intptr_t )*loop_context;

    for ( i++; i < debug_numTokens; i++ ) {
        /* skip excluded til mib is updated */
        if ( debug_tokens[ i ].tokenName && ( debug_tokens[ i ].enabled != 2 ) )
            break;
    }
    if ( i == debug_numTokens )
        return NULL;

    Client_setVarValue( index, debug_tokens[ i ].tokenName,
        strlen( debug_tokens[ i ].tokenName ) );
    *loop_context = ( void* )( intptr_t )i;
    *data_context = ( void* )&debug_tokens[ i ];
    return index;
}

int handle_nsDebugTable( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    long status;
    RequestInfo* request = NULL;
    TableRequestInfo* table_info = NULL;
    Debug_tokenDescr* debug_entry = NULL;

    switch ( reqinfo->mode ) {

    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            debug_entry = ( Debug_tokenDescr* )
                TableIterator_extractIteratorContext( request );
            if ( !debug_entry )
                continue;
            status = ( debug_entry->enabled ? TC_RS_ACTIVE : TC_RS_NOTINSERVICE );
            Client_setVarTypedValue( request->requestvb, ASN01_INTEGER,
                ( u_char* )&status, sizeof( status ) );
        }
        break;

    case MODE_SET_RESERVE1:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            if ( request->status != 0 ) {
                return PRIOT_ERR_NOERROR; /* Already got an error */
            }
            if ( request->requestvb->type != ASN01_INTEGER ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGTYPE );
                return PRIOT_ERR_WRONGTYPE;
            }

            debug_entry = ( Debug_tokenDescr* )
                TableIterator_extractIteratorContext( request );
            switch ( *request->requestvb->val.integer ) {
            case TC_RS_ACTIVE:
            case TC_RS_NOTINSERVICE:
                /*
		 * These operations require an existing row
		 */
                if ( !debug_entry ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_INCONSISTENTVALUE );
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
                break;

            case TC_RS_CREATEANDWAIT:
            case TC_RS_CREATEANDGO:
                /*
		 * These operations assume the row doesn't already exist
		 */
                if ( debug_entry ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_INCONSISTENTVALUE );
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
                break;

            case TC_RS_DESTROY:
                /*
		 * This operation can work regardless
		 */
                break;

            case TC_RS_NOTREADY:
            default:
                Agent_setRequestError( reqinfo, request,
                    PRIOT_ERR_WRONGVALUE );
                return PRIOT_ERR_WRONGVALUE;
            }
        }
        break;

    case MODE_SET_COMMIT:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            if ( request->status != 0 ) {
                return PRIOT_ERR_NOERROR; /* Already got an error */
            }

            switch ( *request->requestvb->val.integer ) {
            case TC_RS_ACTIVE:
            case TC_RS_NOTINSERVICE:
                /*
		 * Update the enabled field appropriately
		 */
                debug_entry = ( Debug_tokenDescr* )
                    TableIterator_extractIteratorContext( request );
                if ( debug_entry )
                    debug_entry->enabled = ( *request->requestvb->val.integer == TC_RS_ACTIVE );
                break;

            case TC_RS_CREATEANDWAIT:
            case TC_RS_CREATEANDGO:
                /*
		 * Create the entry, and set the enabled field appropriately
		 */
                table_info = Table_extractTableInfo( request );
                Debug_registerTokens( ( char* )table_info->indexes->val.string );

                break;

            case TC_RS_DESTROY:
                /*
		 * XXX - there's no "remove" API  :-(
		 */
                debug_entry = ( Debug_tokenDescr* )
                    TableIterator_extractIteratorContext( request );
                if ( debug_entry ) {
                    debug_entry->enabled = 0;
                    free( debug_entry->tokenName );
                    debug_entry->tokenName = NULL;
                }
                break;
            }
        }
        break;
    }

    return PRIOT_ERR_NOERROR;
}
