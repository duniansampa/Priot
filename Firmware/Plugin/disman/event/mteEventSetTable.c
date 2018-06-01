/*
 * DisMan Event MIB:
 *     Implementation of the mteEventSetTable MIB interface
 * See 'mteEvent.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include "mteEventSetTable.h"
#include "System/Util/VariableList.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "Table.h"
#include "TextualConvention.h"
#include "mteEvent.h"

static TableRegistrationInfo* table_info;

/* Initializes the mteEventSetTable module */
void init_mteEventSetTable( void )
{
    static oid mteEventSetTable_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 4, 4 };
    size_t mteEventSetTable_oid_len = ASN01_OID_LENGTH( mteEventSetTable_oid );
    HandlerRegistration* reg;

    /*
     * Ensure the (combined) table container is available...
     */
    init_event_table_data();

    /*
     * ... then set up the MIB interface to the mteEventSetTable slice
     */
    reg = AgentHandler_createHandlerRegistration( "mteEventSetTable",
        mteEventSetTable_handler,
        mteEventSetTable_oid,
        mteEventSetTable_oid_len,
        HANDLER_CAN_RWRITE );

    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( table_info,
        ASN01_OCTET_STR, /* index: mteOwner     */
        /* index: mteEventName */
        ASN01_PRIV_IMPLIED_OCTET_STR,
        0 );

    table_info->min_column = COLUMN_MTEEVENTSETOBJECT;
    table_info->max_column = COLUMN_MTEEVENTSETCONTEXTNAMEWILDCARD;

    /* Register this using the (common) event_table_data container */
    TableTdata_register( reg, event_table_data, table_info );
    DEBUG_MSGTL( ( "disman:event:init", "Event Set Table container (%p)\n",
        event_table_data ) );
}

void shutdown_mteEventSetTable( void )
{
    if ( table_info ) {
        Table_registrationInfoFree( table_info );
        table_info = NULL;
    }
}

/** handles requests for the mteEventSetTable table */
int mteEventSetTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRequestInfo* tinfo;
    struct mteEvent* entry;
    int ret;

    DEBUG_MSGTL( ( "disman:event:mib", "Set Table handler (%d)\n",
        reqinfo->mode ) );

    switch ( reqinfo->mode ) {
    /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            entry = ( struct mteEvent* )TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            /*
             * The mteEventSetTable should only contains entries for
             *   rows where the mteEventActions 'set(1)' bit is set.
             * So skip entries where this isn't the case.
             */
            if ( !entry || !( entry->mteEventActions & MTE_EVENT_SET ) )
                continue;

            switch ( tinfo->colnum ) {
            case COLUMN_MTEEVENTSETOBJECT:
                Client_setVarTypedValue( request->requestvb, ASN01_OBJECT_ID,
                    ( u_char* )entry->mteSetOID,
                    entry->mteSetOID_len * sizeof( oid ) );
                break;
            case COLUMN_MTEEVENTSETOBJECTWILDCARD:
                ret = ( entry->flags & MTE_SET_FLAG_OBJWILD ) ? tcTRUE : tcFALSE;
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            case COLUMN_MTEEVENTSETVALUE:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->mteSetValue );
                break;
            case COLUMN_MTEEVENTSETTARGETTAG:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteSetTarget,
                    strlen( entry->mteSetTarget ) );
                break;
            case COLUMN_MTEEVENTSETCONTEXTNAME:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteSetContext,
                    strlen( entry->mteSetContext ) );
                break;
            case COLUMN_MTEEVENTSETCONTEXTNAMEWILDCARD:
                ret = ( entry->flags & MTE_SET_FLAG_CTXWILD ) ? tcTRUE : tcFALSE;
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            }
        }
        break;

    /*
         * Write-support
         */
    case MODE_SET_RESERVE1:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            tinfo = Table_extractTableInfo( request );

            /*
             * Since the mteEventSetTable only contains entries for
             *   rows where the mteEventActions 'set(1)' bit is set,
             *   strictly speaking we should reject assignments where
             *   this isn't the case.
             * But SET requests that include an assignment of the
             *   'set(1)' bit at the same time are valid, so would
             *    need to be accepted. Unfortunately, this assignment
             *   is only applied in the IMPL_COMMIT pass, so it's difficult
             *   to detect whether this holds or not.
             *
             * Let's fudge things for now, by processing assignments
             *   even if the 'set(1)' bit isn't set.
             */
            switch ( tinfo->colnum ) {
            case COLUMN_MTEEVENTSETOBJECT:
                ret = VariableList_checkOidMaxLength( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEEVENTSETOBJECTWILDCARD:
            case COLUMN_MTEEVENTSETCONTEXTNAMEWILDCARD:
                ret = VariableList_checkBoolLengthAndValue( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEEVENTSETVALUE:
                ret = VariableList_checkIntLength( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEEVENTSETTARGETTAG:
            case COLUMN_MTEEVENTSETCONTEXTNAME:
                ret = VariableList_checkTypeAndMaxLength(
                    request->requestvb, ASN01_OCTET_STR, MTE_STR2_LEN );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            default:
                Agent_setRequestError( reqinfo, request,
                    PRIOT_ERR_NOTWRITABLE );
                return PRIOT_ERR_NOERROR;
            }

            /*
             * The Event MIB is somewhat ambiguous as to whether
             *  mteEventSetTable (and mteEventNotificationTable)
             *  entries can be modified once the main mteEventTable
             *  entry has been marked 'active'. 
             * But it's clear from discussion on the DisMan mailing
             *  list is that the intention is not.
             *
             * So check for whether this row is already active,
             *  and reject *all* SET requests if it is.
             */
            entry = ( struct mteEvent* )TableTdata_extractEntry( request );
            if ( entry && entry->flags & MTE_EVENT_FLAG_ACTIVE ) {
                Agent_setRequestError( reqinfo, request,
                    PRIOT_ERR_INCONSISTENTVALUE );
                return PRIOT_ERR_NOERROR;
            }
        }
        break;

    case MODE_SET_RESERVE2:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        break;

    case MODE_SET_ACTION:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            entry = ( struct mteEvent* )TableTdata_extractEntry( request );
            if ( !entry ) {
                /*
                 * New rows must be created via the RowStatus column
                 *   (in the main mteEventTable)
                 */
                Agent_setRequestError( reqinfo, request,
                    PRIOT_ERR_NOCREATION );
                /* or inconsistentName? */
                return PRIOT_ERR_NOERROR;
            }
        }
        break;

    case MODE_SET_COMMIT:
        /*
         * All these assignments are "unfailable", so it's
         *  (reasonably) safe to apply them in the Commit phase
         */
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            entry = ( struct mteEvent* )TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_MTEEVENTSETOBJECT:
                memset( entry->mteSetOID, 0, sizeof( entry->mteSetOID ) );
                memcpy( entry->mteSetOID, request->requestvb->value.objectId,
                    request->requestvb->valueLength );
                entry->mteSetOID_len = request->requestvb->valueLength / sizeof( oid );
                break;
            case COLUMN_MTEEVENTSETOBJECTWILDCARD:
                if ( *request->requestvb->value.integer == tcTRUE )
                    entry->flags |= MTE_SET_FLAG_OBJWILD;
                else
                    entry->flags &= ~MTE_SET_FLAG_OBJWILD;
                break;
            case COLUMN_MTEEVENTSETVALUE:
                entry->mteSetValue = *request->requestvb->value.integer;
                break;
            case COLUMN_MTEEVENTSETTARGETTAG:
                memset( entry->mteSetTarget, 0, sizeof( entry->mteSetTarget ) );
                memcpy( entry->mteSetTarget, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_MTEEVENTSETCONTEXTNAME:
                memset( entry->mteSetContext, 0, sizeof( entry->mteSetContext ) );
                memcpy( entry->mteSetContext, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_MTEEVENTSETCONTEXTNAMEWILDCARD:
                if ( *request->requestvb->value.integer == tcTRUE )
                    entry->flags |= MTE_SET_FLAG_CTXWILD;
                else
                    entry->flags &= ~MTE_SET_FLAG_CTXWILD;
                break;
            }
        }
        break;
    }
    return PRIOT_ERR_NOERROR;
}
