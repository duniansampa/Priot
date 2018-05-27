/*
 * DisMan Event MIB:
 *     Implementation of the mteEventNotificationTable MIB interface
 * See 'mteEvent.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include "mteEventNotificationTable.h"
#include "System/Util/VariableList.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "Table.h"
#include "mteEvent.h"

static TableRegistrationInfo* table_info;

/* Initializes the mteEventNotificationTable module */
void init_mteEventNotificationTable( void )
{
    static oid mteEventNotificationTable_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 4, 3 };
    size_t mteEventNotificationTable_oid_len = ASN01_OID_LENGTH( mteEventNotificationTable_oid );
    HandlerRegistration* reg;

    /*
     * Ensure the (combined) table container is available...
     */
    init_event_table_data();

    /*
     * ... then set up the MIB interface to the mteEventNotificationTable slice
     */
    reg = AgentHandler_createHandlerRegistration( "mteEventNotificationTable",
        mteEventNotificationTable_handler,
        mteEventNotificationTable_oid,
        mteEventNotificationTable_oid_len,
        HANDLER_CAN_RWRITE );

    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( table_info,
        ASN01_OCTET_STR, /* index: mteOwner */
        /* index: mteEventName */
        ASN01_PRIV_IMPLIED_OCTET_STR,
        0 );

    table_info->min_column = COLUMN_MTEEVENTNOTIFICATION;
    table_info->max_column = COLUMN_MTEEVENTNOTIFICATIONOBJECTS;

    /* Register this using the (common) event_table_data container */
    TableTdata_register( reg, event_table_data, table_info );
    DEBUG_MSGTL( ( "disman:event:init", "Event Notify Table container (%p)\n",
        event_table_data ) );
}

void shutdown_mteEventNotificationTable( void )
{
    if ( table_info ) {
        Table_registrationInfoFree( table_info );
        table_info = NULL;
    }
}

/** handles requests for the mteEventNotificationTable table */
int mteEventNotificationTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRequestInfo* tinfo;
    struct mteEvent* entry;
    int ret;

    DEBUG_MSGTL( ( "disman:event:mib", "Notification Table handler (%d)\n", reqinfo->mode ) );

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
             * The mteEventNotificationTable should only contains entries
             *   for rows where the mteEventActions 'notification(0)' bit
             *   is set. So skip entries where this isn't the case.
             */
            if ( !entry || !( entry->mteEventActions & MTE_EVENT_NOTIFICATION ) )
                continue;

            switch ( tinfo->colnum ) {
            case COLUMN_MTEEVENTNOTIFICATION:
                Client_setVarTypedValue( request->requestvb, ASN01_OBJECT_ID,
                    ( u_char* )entry->mteNotification,
                    entry->mteNotification_len * sizeof( oid ) );
                break;
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTSOWNER:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteNotifyOwner,
                    strlen( entry->mteNotifyOwner ) );
                break;
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTS:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteNotifyObjects,
                    strlen( entry->mteNotifyObjects ) );
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
             * Since the mteEventNotificationTable only contains entries
             *   for rows where the mteEventActions 'notification(0)'
             *   bit is set, strictly speaking we should reject
             *   assignments where this isn't the case.
             * But SET requests that include an assignment of the
             *   'notification(0)' bit at the same time are valid,
             *   so would need to be accepted. Unfortunately, this
             *   assignment is only applied in the IMPL_COMMIT pass, so
             *   it's difficult to detect whether this holds or not.
             *
             * Let's fudge things for now, by processing assignments
             *   even if the 'notification(0)' bit isn't set.
             */

            switch ( tinfo->colnum ) {
            case COLUMN_MTEEVENTNOTIFICATION:
                ret = VariableList_checkOidMaxLength( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTSOWNER:
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTS:
                ret = VariableList_checkTypeAndMaxLength(
                    request->requestvb, ASN01_OCTET_STR, MTE_STR1_LEN );
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
             *  mteEventNotificationTable (and mteEventSetTable)
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
            case COLUMN_MTEEVENTNOTIFICATION:
                memset( entry->mteNotification, 0, sizeof( entry->mteNotification ) );
                memcpy( entry->mteNotification, request->requestvb->value.objectId,
                    request->requestvb->valueLength );
                entry->mteNotification_len = request->requestvb->valueLength / sizeof( oid );
                break;
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTSOWNER:
                memset( entry->mteNotifyOwner, 0, sizeof( entry->mteNotifyOwner ) );
                memcpy( entry->mteNotifyOwner, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_MTEEVENTNOTIFICATIONOBJECTS:
                memset( entry->mteNotifyObjects, 0, sizeof( entry->mteNotifyObjects ) );
                memcpy( entry->mteNotifyObjects, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            }
        }
        break;
    }
    return PRIOT_ERR_NOERROR;
}
