
#include "nsLogging.h"
#include "Agent.h"
#include "Client.h"
#include "System/Util/Logger.h"
#include "Table.h"
#include "Tc.h"

/*
 * OID and columns for the logging table.
 */

#define NSLOGGING_TYPE 3
#define NSLOGGING_MAXLEVEL 4
#define NSLOGGING_STATUS 5

void init_nsLogging( void )
{
    TableRegistrationInfo* table_info;
    IteratorInfo* iinfo;

    const oid nsLoggingTable_oid[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 7, 2, 1 };

    /*
     * Register the table.
     * We need to define the column structure and indexing....
     */

    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    if ( !table_info ) {
        return;
    }
    Table_helperAddIndexes( table_info, ASN01_INTEGER,
        ASN01_PRIV_IMPLIED_OCTET_STR, 0 );
    table_info->min_column = NSLOGGING_TYPE;
    table_info->max_column = NSLOGGING_STATUS;

    /*
     * .... and the iteration information ....
     */
    iinfo = MEMORY_MALLOC_TYPEDEF( IteratorInfo );
    if ( !iinfo ) {
        return;
    }
    iinfo->get_first_data_point = get_first_logging_entry;
    iinfo->get_next_data_point = get_next_logging_entry;
    iinfo->table_reginfo = table_info;

    /*
     * .... and register the table with the agent.
     */
    TableIterator_registerTableIterator2(
        AgentHandler_createHandlerRegistration(
            "tzLoggingTable", handle_nsLoggingTable,
            nsLoggingTable_oid, ASN01_OID_LENGTH( nsLoggingTable_oid ),
            HANDLER_CAN_RWRITE ),
        iinfo );
}

/*
 * nsLoggingTable handling
 */

Types_VariableList*
get_first_logging_entry( void** loop_context, void** data_context,
    Types_VariableList* index,
    IteratorInfo* data )
{
    long temp;
    Logger_LogHandler* logh_head = Logger_getLogHandlerHead();
    if ( !logh_head )
        return NULL;

    temp = logh_head->priority;
    Client_setVarValue( index, ( u_char* )&temp,
        sizeof( temp ) );
    if ( logh_head->token )
        Client_setVarValue( index->nextVariable, ( const u_char* )logh_head->token,
            strlen( logh_head->token ) );
    else
        Client_setVarValue( index->nextVariable, NULL, 0 );
    *loop_context = ( void* )logh_head;
    *data_context = ( void* )logh_head;
    return index;
}

Types_VariableList*
get_next_logging_entry( void** loop_context, void** data_context,
    Types_VariableList* index,
    IteratorInfo* data )
{
    long temp;
    Logger_LogHandler* logh = ( Logger_LogHandler* )*loop_context;
    logh = logh->next;

    if ( !logh )
        return NULL;

    temp = logh->priority;
    Client_setVarValue( index, ( u_char* )&temp,
        sizeof( temp ) );
    if ( logh->token )
        Client_setVarValue( index->nextVariable, ( const u_char* )logh->token,
            strlen( logh->token ) );
    else
        Client_setVarValue( index->nextVariable, NULL, 0 );
    *loop_context = ( void* )logh;
    *data_context = ( void* )logh;
    return index;
}

int handle_nsLoggingTable( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    long temp;
    RequestInfo* request = NULL;
    TableRequestInfo* table_info = NULL;
    Logger_LogHandler* logh = NULL;
    Types_VariableList* idx = NULL;

    switch ( reqinfo->mode ) {

    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            logh = ( Logger_LogHandler* )TableIterator_extractIteratorContext( request );
            table_info = Table_extractTableInfo( request );

            switch ( table_info->colnum ) {
            case NSLOGGING_TYPE:
                if ( !logh ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHINSTANCE );
                    continue;
                }

                temp = logh->type;
                Client_setVarTypedValue( request->requestvb, ASN01_INTEGER,
                    ( u_char* )&temp,
                    sizeof( temp ) );
                break;

            case NSLOGGING_MAXLEVEL:
                if ( !logh ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHINSTANCE );
                    continue;
                }
                temp = logh->pri_max;
                Client_setVarTypedValue( request->requestvb, ASN01_INTEGER,
                    ( u_char* )&temp,
                    sizeof( temp ) );
                break;

            case NSLOGGING_STATUS:
                if ( !logh ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHINSTANCE );
                    continue;
                }
                temp = ( logh->type ? ( logh->enabled ? TC_RS_ACTIVE : TC_RS_NOTINSERVICE ) : TC_RS_NOTREADY );
                Client_setVarTypedValue( request->requestvb, ASN01_INTEGER,
                    ( u_char* )&temp, sizeof( temp ) );
                break;

            default:
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHOBJECT );
                continue;
            }
        }
        break;

    case MODE_SET_RESERVE1:
        for ( request = requests; request; request = request->next ) {
            if ( request->status != 0 ) {
                return PRIOT_ERR_NOERROR; /* Already got an error */
            }
            logh = ( Logger_LogHandler* )TableIterator_extractIteratorContext( request );
            table_info = Table_extractTableInfo( request );

            switch ( table_info->colnum ) {
            case NSLOGGING_TYPE:
                if ( request->requestvb->type != ASN01_INTEGER ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_WRONGTYPE;
                }
                if ( *request->requestvb->val.integer < 0 ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGVALUE );
                    return PRIOT_ERR_WRONGVALUE;
                }
                /*
		 * It's OK to create a new logging entry
		 *  (either in one go, or built up using createAndWait)
		 *  but it's not possible to change the type of an entry
		 *  once it's been created.
		 */
                if ( logh && logh->type ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_NOTWRITABLE );
                    return PRIOT_ERR_NOTWRITABLE;
                }
                break;

            case NSLOGGING_MAXLEVEL:
                if ( request->requestvb->type != ASN01_INTEGER ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_WRONGTYPE;
                }
                if ( *request->requestvb->val.integer < 0 || *request->requestvb->val.integer > 7 ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGVALUE );
                    return PRIOT_ERR_WRONGVALUE;
                }
                break;

            case NSLOGGING_STATUS:
                if ( request->requestvb->type != ASN01_INTEGER ) {
                    Agent_setRequestError( reqinfo, request, PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_WRONGTYPE;
                }
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_ACTIVE:
                case TC_RS_NOTINSERVICE:
                    /*
		     * Can only work on existing rows
		     */
                    if ( !logh ) {
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_INCONSISTENTVALUE );
                        return PRIOT_ERR_INCONSISTENTVALUE;
                    }
                    break;

                case TC_RS_CREATEANDWAIT:
                case TC_RS_CREATEANDGO:
                    /*
		     * Can only work with new rows
		     */
                    if ( logh ) {
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_INCONSISTENTVALUE );
                        return PRIOT_ERR_INCONSISTENTVALUE;
                    }

                    /*
                     *  Normally, we'd create the row at a later stage
                     *   (probably during the IMPL_RESERVE2 or IMPL_ACTION passes)
                     *
                     *  But we need to check that the values are
                     *   consistent during the IMPL_ACTION pass (which is the
                     *   latest that an error can be safely handled),
                     *   so the values all need to be set up before this
                     *      (i.e. during the IMPL_RESERVE2 pass)
                     *  So the new row needs to be created before that
                     *   in order to have somewhere to put them.
                     *
                     *  That's why we're doing this here.
                     */
                    idx = table_info->indexes;
                    logh = Logger_registerLoghandler(
                        /* not really, but we need a valid type */
                        LOGGER_LOG_TO_STDOUT,
                        *idx->val.integer );
                    if ( !logh ) {
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_GENERR ); /* ??? */
                        return PRIOT_ERR_GENERR;
                    }
                    idx = idx->nextVariable;
                    logh->type = 0;
                    logh->token = strdup( ( char* )idx->val.string );
                    TableIterator_insertIteratorContext( request, ( void* )logh );
                    break;

                case TC_RS_DESTROY:
                    /*
		     * Can work with new or existing rows
		     */
                    break;

                case TC_RS_NOTREADY:
                default:
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGVALUE );
                    return PRIOT_ERR_WRONGVALUE;
                }
                break;

            default:
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHOBJECT );
                return PRIOT_NOSUCHOBJECT;
            }
        }
        break;

    case MODE_SET_RESERVE2:
        for ( request = requests; request; request = request->next ) {
            if ( request->status != 0 ) {
                return PRIOT_ERR_NOERROR; /* Already got an error */
            }
            logh = ( Logger_LogHandler* )TableIterator_extractIteratorContext( request );
            table_info = Table_extractTableInfo( request );

            switch ( table_info->colnum ) {
            case NSLOGGING_TYPE:
                /*
		 * If we're creating a row using createAndGo,
		 * we need to set the type early, so that we
         * can validate it in the IMPL_ACTION pass.
		 *
		 * Remember that we need to be able to reverse this
		 */
                if ( logh )
                    logh->type = *request->requestvb->val.integer;
                break;
                /*
	     * Don't need to handle nsLogToken or nsLogStatus in this pass
	     */
            }
        }
        break;

    case MODE_SET_ACTION:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            if ( request->status != 0 ) {
                return PRIOT_ERR_NOERROR; /* Already got an error */
            }
            logh = ( Logger_LogHandler* )TableIterator_extractIteratorContext( request );
            table_info = Table_extractTableInfo( request );

            switch ( table_info->colnum ) {
            case NSLOGGING_STATUS:
                /*
		 * This is where we can check the internal consistency
		 * of the request.  Basically, for a row to be marked
		 * 'active', then there needs to be a valid type value.
		 */
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_ACTIVE:
                case TC_RS_CREATEANDGO:
                    if ( !logh || !logh->type ) {
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_INCONSISTENTVALUE );
                        return PRIOT_ERR_INCONSISTENTVALUE;
                    }
                    break;
                }
                break;
                /*
	     * Don't need to handle nsLogToken or nsLogType in this pass
	     */
            }
        }
        break;

    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        /*
         * If any resources were allocated in either of the
         *  two RESERVE passes, they need to be released here,
         *  and any assignments (in IMPL_RESERVE2) reversed.
         *
         * Nothing additional will have been done during IMPL_ACTION
         *  so this same code can do for IMPL_UNDO as well.
         */
        for ( request = requests; request; request = request->next ) {
            if ( request->processed != 0 )
                continue;
            logh = ( Logger_LogHandler* )TableIterator_extractIteratorContext( request );
            table_info = Table_extractTableInfo( request );

            switch ( table_info->colnum ) {
            case NSLOGGING_TYPE:
                /*
		 * If we've been setting the type, and the request
		 * has failed, then revert to an unset type.
		 *
		 * We need to be careful here - if the reason it failed is
		 *  that the type was already set, then we shouldn't "undo"
		 *  the assignment (since it won't actually have been made).
		 *
		 * Check the current value against the 'new' one.  If they're
		 * the same, then this is probably a successful assignment,
		 * and the failure was elsewhere, so we need to undo it.
		 *  (Or else there was an attempt to write the same value!)
		 */
                if ( logh && logh->type == *request->requestvb->val.integer )
                    logh->type = 0;
                break;

            case NSLOGGING_STATUS:
                temp = *request->requestvb->val.integer;
                if ( logh && ( temp == TC_RS_CREATEANDGO || temp == TC_RS_CREATEANDWAIT ) ) {
                    Logger_removeLoghandler( logh );
                }
                break;
                /*
	     * Don't need to handle nsLogToken in this pass
	     */
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
            logh = ( Logger_LogHandler* )TableIterator_extractIteratorContext( request );
            if ( !logh ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_COMMITFAILED );
                return PRIOT_ERR_COMMITFAILED; /* Shouldn't happen! */
            }
            table_info = Table_extractTableInfo( request );

            switch ( table_info->colnum ) {
            case NSLOGGING_MAXLEVEL:
                logh->pri_max = *request->requestvb->val.integer;
                break;

            case NSLOGGING_STATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_ACTIVE:
                case TC_RS_CREATEANDGO:
                    Logger_enableThisLoghandler( logh );
                    break;
                case TC_RS_NOTINSERVICE:
                case TC_RS_CREATEANDWAIT:
                    Logger_disableThisLoghandler( logh );
                    break;
                case TC_RS_DESTROY:
                    Logger_removeLoghandler( logh );
                    break;
                }
                break;
            }
        }
        break;
    }

    return PRIOT_ERR_NOERROR;
}
