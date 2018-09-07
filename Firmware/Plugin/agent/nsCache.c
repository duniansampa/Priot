#include "nsCache.h"
#include "System/Util/Alarm.h"
#include "CacheHandler.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "System/Util/DefaultStore.h"
#include "DsAgent.h"
#include "Scalar.h"

#define nsCache 1, 3, 6, 1, 4, 1, 8072, 1, 5

/*
 * OIDs for the cacheging control scalar objects
 *
 * Note that these we're registering the full object rather
 *  than the (sole) valid instance in each case, in order
 *  to handle requests for invalid instances properly.
 */

/*
 * ... and for the cache table.
 */

#define NSCACHE_TIMEOUT 2
#define NSCACHE_STATUS 3

#define NSCACHE_STATUS_ENABLED 1
#define NSCACHE_STATUS_DISABLED 2
#define NSCACHE_STATUS_EMPTY 3
#define NSCACHE_STATUS_ACTIVE 4
#define NSCACHE_STATUS_EXPIRED 5

void init_nsCache( void )
{
    const oid nsCacheTimeout_oid[] = { nsCache, 1 };
    const oid nsCacheEnabled_oid[] = { nsCache, 2 };
    const oid nsCacheTable_oid[] = { nsCache, 3 };

    TableRegistrationInfo* table_info;
    IteratorInfo* iinfo;

    /*
     * Register the scalar objects...
     */
    DEBUG_MSGTL( ( "nsCacheScalars", "Initializing\n" ) );
    Scalar_registerScalar(
        AgentHandler_createHandlerRegistration(
            "nsCacheTimeout", handle_nsCacheTimeout,
            nsCacheTimeout_oid, asnOID_LENGTH( nsCacheTimeout_oid ),
            HANDLER_CAN_RWRITE ) );
    Scalar_registerScalar(
        AgentHandler_createHandlerRegistration(
            "nsCacheEnabled", handle_nsCacheEnabled,
            nsCacheEnabled_oid, asnOID_LENGTH( nsCacheEnabled_oid ),
            HANDLER_CAN_RWRITE ) );

    /*
     * ... and the table.
     * We need to define the column structure and indexing....
     */

    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    if ( !table_info ) {
        return;
    }
    Table_helperAddIndexes( table_info, asnPRIV_IMPLIED_OBJECT_ID, 0 );
    table_info->min_column = NSCACHE_TIMEOUT;
    table_info->max_column = NSCACHE_STATUS;

    /*
     * .... and the iteration information ....
     */
    iinfo = MEMORY_MALLOC_TYPEDEF( IteratorInfo );
    if ( !iinfo ) {
        return;
    }
    iinfo->get_first_data_point = get_first_cache_entry;
    iinfo->get_next_data_point = get_next_cache_entry;
    iinfo->table_reginfo = table_info;

    /*
     * .... and register the table with the agent.
     */
    TableIterator_registerTableIterator2(
        AgentHandler_createHandlerRegistration(
            "tzCacheTable", handle_nsCacheTable,
            nsCacheTable_oid, asnOID_LENGTH( nsCacheTable_oid ),
            HANDLER_CAN_RWRITE ),
        iinfo );
}

/*
 * nsCache scalar handling
 */

int handle_nsCacheTimeout( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    long cache_default_timeout = DefaultStore_getInt( DsStore_APPLICATION_ID,
        DsAgentInterger_CACHE_TIMEOUT );
    RequestInfo* request = NULL;

    switch ( reqinfo->mode ) {

    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            Client_setVarTypedValue( request->requestvb, asnINTEGER,
                ( u_char* )&cache_default_timeout,
                sizeof( cache_default_timeout ) );
        }
        break;

    case MODE_SET_RESERVE1:
        for ( request = requests; request; request = request->next ) {
            if ( request->status != 0 ) {
                return PRIOT_ERR_NOERROR; /* Already got an error */
            }
            if ( request->requestvb->type != asnINTEGER ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGTYPE );
                return PRIOT_ERR_WRONGTYPE;
            }
            if ( *request->requestvb->value.integer < 0 ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGVALUE );
                return PRIOT_ERR_WRONGVALUE;
            }
        }
        break;

    case MODE_SET_COMMIT:
        DefaultStore_setInt( DsStore_APPLICATION_ID,
            DsAgentInterger_CACHE_TIMEOUT,
            *requests->requestvb->value.integer );
        break;
    }

    return PRIOT_ERR_NOERROR;
}

int handle_nsCacheEnabled( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    long enabled;
    RequestInfo* request = NULL;

    switch ( reqinfo->mode ) {

    case MODE_GET:
        enabled = ( DefaultStore_getBoolean( DsStore_APPLICATION_ID,
                        DsAgentBoolean_NO_CACHING )
                ? NSCACHE_STATUS_ENABLED /* Actually True/False */
                : NSCACHE_STATUS_DISABLED );
        for ( request = requests; request; request = request->next ) {
            Client_setVarTypedValue( request->requestvb, asnINTEGER,
                ( u_char* )&enabled, sizeof( enabled ) );
        }
        break;

    case MODE_SET_RESERVE1:
        for ( request = requests; request; request = request->next ) {
            if ( request->status != 0 ) {
                return PRIOT_ERR_NOERROR; /* Already got an error */
            }
            if ( request->requestvb->type != asnINTEGER ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGTYPE );
                return PRIOT_ERR_WRONGTYPE;
            }
            if ( ( *request->requestvb->value.integer != NSCACHE_STATUS_ENABLED ) && ( *request->requestvb->value.integer != NSCACHE_STATUS_DISABLED ) ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGVALUE );
                return PRIOT_ERR_WRONGVALUE;
            }
        }
        break;

    case MODE_SET_COMMIT:
        enabled = *requests->requestvb->value.integer;
        if ( enabled == NSCACHE_STATUS_DISABLED )
            enabled = 0;
        DefaultStore_setBoolean( DsStore_APPLICATION_ID,
            DsAgentBoolean_NO_CACHING, enabled );
        break;
    }

    return PRIOT_ERR_NOERROR;
}

/*
 * nsCacheTable handling
 */

VariableList*
get_first_cache_entry( void** loop_context, void** data_context,
    VariableList* index,
    IteratorInfo* data )
{
    Cache* cache_head = CacheHandler_getHead();

    if ( !cache_head )
        return NULL;

    Client_setVarValue( index, ( u_char* )cache_head->rootoid,
        sizeof( oid ) * cache_head->rootoid_len );
    *loop_context = ( void* )cache_head;
    *data_context = ( void* )cache_head;
    return index;
}

VariableList*
get_next_cache_entry( void** loop_context, void** data_context,
    VariableList* index,
    IteratorInfo* data )
{
    Cache* cache = ( Cache* )*loop_context;
    cache = cache->next;

    if ( !cache )
        return NULL;

    Client_setVarValue( index, ( u_char* )cache->rootoid,
        sizeof( oid ) * cache->rootoid_len );
    *loop_context = ( void* )cache;
    *data_context = ( void* )cache;
    return index;
}

int handle_nsCacheTable( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    long status;
    RequestInfo* request = NULL;
    TableRequestInfo* table_info = NULL;
    Cache* cache_entry = NULL;

    switch ( reqinfo->mode ) {

    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;

            cache_entry = ( Cache* )TableIterator_extractIteratorContext( request );
            table_info = Table_extractTableInfo( request );

            switch ( table_info->colnum ) {
            case NSCACHE_TIMEOUT:
                if ( !cache_entry ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHINSTANCE );
                    continue;
                }
                status = cache_entry->timeout;
                Client_setVarTypedValue( request->requestvb, asnINTEGER,
                    ( u_char* )&status, sizeof( status ) );
                break;

            case NSCACHE_STATUS:
                if ( !cache_entry ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHINSTANCE );
                    continue;
                }
                status = ( cache_entry->enabled ? ( cache_entry->timestampM ? ( cache_entry->timeout >= 0 && !Time_readyMonotonic( cache_entry->timestampM, 1000 * cache_entry->timeout ) ? NSCACHE_STATUS_ACTIVE : NSCACHE_STATUS_EXPIRED ) : NSCACHE_STATUS_EMPTY ) : NSCACHE_STATUS_DISABLED );
                Client_setVarTypedValue( request->requestvb, asnINTEGER,
                    ( u_char* )&status, sizeof( status ) );
                break;

            default:
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHOBJECT );
                continue;
            }
        }
        break;

    case MODE_SET_RESERVE1:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            if ( request->status != 0 ) {
                return PRIOT_ERR_NOERROR; /* Already got an error */
            }
            cache_entry = ( Cache* )TableIterator_extractIteratorContext( request );
            table_info = Table_extractTableInfo( request );

            switch ( table_info->colnum ) {
            case NSCACHE_TIMEOUT:
                if ( !cache_entry ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_NOCREATION );
                    return PRIOT_ERR_NOCREATION;
                }
                if ( request->requestvb->type != asnINTEGER ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_WRONGTYPE;
                }
                if ( *request->requestvb->value.integer < 0 ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGVALUE );
                    return PRIOT_ERR_WRONGVALUE;
                }
                break;

            case NSCACHE_STATUS:
                if ( !cache_entry ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_NOCREATION );
                    return PRIOT_ERR_NOCREATION;
                }
                if ( request->requestvb->type != asnINTEGER ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_WRONGTYPE;
                }
                status = *request->requestvb->value.integer;
                if ( !( ( status == NSCACHE_STATUS_ENABLED ) || ( status == NSCACHE_STATUS_DISABLED ) || ( status == NSCACHE_STATUS_EMPTY ) ) ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGVALUE );
                    return PRIOT_ERR_WRONGVALUE;
                }
                break;

            default:
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_NOCREATION );
                return PRIOT_ERR_NOCREATION; /* XXX - is this right ? */
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
            cache_entry = ( Cache* )TableIterator_extractIteratorContext( request );
            if ( !cache_entry ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_COMMITFAILED );
                return PRIOT_ERR_COMMITFAILED; /* Shouldn't happen! */
            }
            table_info = Table_extractTableInfo( request );

            switch ( table_info->colnum ) {
            case NSCACHE_TIMEOUT:
                cache_entry->timeout = *request->requestvb->value.integer;
                /*
                 * check for auto repeat
                 */
                if ( cache_entry->timer_id ) {
                    struct Alarm_s* sa = Alarm_findSpecific( cache_entry->timer_id );
                    if ( NULL != sa )
                        sa->timeInterval.tv_sec = cache_entry->timeout;
                }
                break;

            case NSCACHE_STATUS:
                switch ( *request->requestvb->value.integer ) {
                case NSCACHE_STATUS_ENABLED:
                    cache_entry->enabled = 1;
                    break;
                case NSCACHE_STATUS_DISABLED:
                    cache_entry->enabled = 0;
                    break;
                case NSCACHE_STATUS_EMPTY:
                    cache_entry->free_cache( cache_entry, cache_entry->magic );
                    free( cache_entry->timestampM );
                    cache_entry->timestampM = NULL;
                    break;
                }
                break;
            }
        }
        break;
    }

    return PRIOT_ERR_NOERROR;
}
