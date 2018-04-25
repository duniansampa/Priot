/*
 * DisMan Event MIB:
 *     Implementation of the mteEventTable MIB interface
 * See 'mteEvent.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include "mteEventTable.h"
#include "CheckVarbind.h"
#include "Client.h"
#include "Debug.h"
#include "Table.h"
#include "Tc.h"
#include "mteEvent.h"
#include "utilities/Iquery.h"

static TableRegistrationInfo* table_info;

/* Initializes the mteEventTable module */
void init_mteEventTable( void )
{
    static oid mteEventTable_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 4, 2 };
    size_t mteEventTable_oid_len = ASN01_OID_LENGTH( mteEventTable_oid );
    HandlerRegistration* reg;

    /*
     * Ensure the (combined) table container is available...
     */
    init_event_table_data();

    /*
     * ... then set up the MIB interface to the mteEventTable slice
     */
    reg = AgentHandler_createHandlerRegistration( "mteEventTable",
        mteEventTable_handler,
        mteEventTable_oid,
        mteEventTable_oid_len,
        HANDLER_CAN_RWRITE );

    table_info = TOOLS_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( table_info,
        ASN01_OCTET_STR, /* index: mteOwner */
        /* index: mteEventName */
        ASN01_PRIV_IMPLIED_OCTET_STR,
        0 );

    table_info->min_column = COLUMN_MTEEVENTCOMMENT;
    table_info->max_column = COLUMN_MTEEVENTENTRYSTATUS;

    /* Register this using the (common) event_table_data container */
    TableTdata_register( reg, event_table_data, table_info );
    DEBUG_MSGTL( ( "disman:event:init", "Event Table container (%p)\n",
        event_table_data ) );
}

void shutdown_mteEventTable( void )
{
    if ( table_info ) {
        Table_registrationInfoFree( table_info );
        table_info = NULL;
    }
}

/** handles requests for the mteEventTable table */
int mteEventTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRequestInfo* tinfo;
    TdataRow* row;
    struct mteEvent* entry;
    char mteOwner[ MTE_STR1_LEN + 1 ];
    char mteEName[ MTE_STR1_LEN + 1 ];
    long ret;

    DEBUG_MSGTL( ( "disman:event:mib", "Event Table handler (%d)\n",
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
            if ( !entry || !( entry->flags & MTE_EVENT_FLAG_VALID ) )
                continue;

            switch ( tinfo->colnum ) {
            case COLUMN_MTEEVENTCOMMENT:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->mteEventComment,
                    strlen( entry->mteEventComment ) );
                break;
            case COLUMN_MTEEVENTACTIONS:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    &entry->mteEventActions, 1 );
                break;
            case COLUMN_MTEEVENTENABLED:
                ret = ( entry->flags & MTE_EVENT_FLAG_ENABLED ) ? TC_TV_TRUE : TC_TV_FALSE;
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            case COLUMN_MTEEVENTENTRYSTATUS:
                ret = ( entry->flags & MTE_EVENT_FLAG_ACTIVE ) ? TC_RS_ACTIVE : TC_RS_NOTINSERVICE;
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

            entry = ( struct mteEvent* )TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_MTEEVENTCOMMENT:
                ret = CheckVarbind_typeAndMaxSize(
                    request->requestvb, ASN01_OCTET_STR, MTE_STR1_LEN );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                /*
                 * Can't modify the comment of an active row
                 *   (No good reason for this, but that's what the MIB says!)
                 */
                if ( entry && entry->flags & MTE_EVENT_FLAG_ACTIVE ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_INCONSISTENTVALUE );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEEVENTACTIONS:
                ret = CheckVarbind_typeAndSize(
                    request->requestvb, ASN01_OCTET_STR, 1 );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                /*
                 * Can't modify the event types of an active row
                 *   (A little more understandable perhaps,
                 *    but still an unnecessary restriction IMO)
                 */
                if ( entry && entry->flags & MTE_EVENT_FLAG_ACTIVE ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_INCONSISTENTVALUE );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEEVENTENABLED:
                ret = CheckVarbind_truthValue( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                /*
                 * The published version of the Event MIB forbids
                 *   enabling (or disabling) an active row, which
                 *   would make this object completely pointless!
                 * Fortunately this ludicrous decision has since been corrected.
                 */
                break;

            case COLUMN_MTEEVENTENTRYSTATUS:
                ret = CheckVarbind_rowStatus( request->requestvb,
                    ( entry ? TC_RS_ACTIVE : TC_RS_NONEXISTENT ) );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                /* An active row can only be deleted */
                if ( entry && entry->flags & MTE_EVENT_FLAG_ACTIVE && *request->requestvb->val.integer == TC_RS_NOTINSERVICE ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_INCONSISTENTVALUE );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            default:
                Agent_setRequestError( reqinfo, request,
                    PRIOT_ERR_NOTWRITABLE );
                return PRIOT_ERR_NOERROR;
            }
        }
        break;

    case MODE_SET_RESERVE2:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_MTEEVENTENTRYSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    /*
                     * Create an (empty) new row structure
                     */
                    memset( mteOwner, 0, sizeof( mteOwner ) );
                    memcpy( mteOwner, tinfo->indexes->val.string,
                        tinfo->indexes->valLen );
                    memset( mteEName, 0, sizeof( mteEName ) );
                    memcpy( mteEName,
                        tinfo->indexes->nextVariable->val.string,
                        tinfo->indexes->nextVariable->valLen );

                    row = mteEvent_createEntry( mteOwner, mteEName, 0 );
                    if ( !row ) {
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_RESOURCEUNAVAILABLE );
                        return PRIOT_ERR_NOERROR;
                    }
                    TableTdata_insertTdataRow( request, row );
                }
            }
        }
        break;

    case MODE_SET_FREE:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_MTEEVENTENTRYSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    /*
                     * Tidy up after a failed row creation request
                     */
                    entry = ( struct mteEvent* )
                        TableTdata_extractEntry( request );
                    if ( entry && !( entry->flags & MTE_EVENT_FLAG_VALID ) ) {
                        row = ( TdataRow* )
                            TableTdata_extractRow( request );
                        mteEvent_removeEntry( row );
                    }
                }
            }
        }
        break;

    case MODE_SET_ACTION:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            tinfo = Table_extractTableInfo( request );
            entry = ( struct mteEvent* )TableTdata_extractEntry( request );
            if ( !entry ) {
                /*
                 * New rows must be created via the RowStatus column
                 */
                Agent_setRequestError( reqinfo, request,
                    PRIOT_ERR_NOCREATION );
                /* or inconsistentName? */
                return PRIOT_ERR_NOERROR;
            }
        }
        break;

    case MODE_SET_UNDO:
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
            case COLUMN_MTEEVENTCOMMENT:
                memset( entry->mteEventComment, 0,
                    sizeof( entry->mteEventComment ) );
                memcpy( entry->mteEventComment,
                    request->requestvb->val.string,
                    request->requestvb->valLen );
                break;

            case COLUMN_MTEEVENTACTIONS:
                entry->mteEventActions = request->requestvb->val.string[ 0 ];
                break;

            case COLUMN_MTEEVENTENABLED:
                if ( *request->requestvb->val.integer == TC_TV_TRUE )
                    entry->flags |= MTE_EVENT_FLAG_ENABLED;
                else
                    entry->flags &= ~MTE_EVENT_FLAG_ENABLED;
                break;

            case COLUMN_MTEEVENTENTRYSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_ACTIVE:
                    entry->flags |= MTE_EVENT_FLAG_ACTIVE;
                    break;
                case TC_RS_CREATEANDGO:
                    entry->flags |= MTE_EVENT_FLAG_ACTIVE;
                /* fall-through */
                case TC_RS_CREATEANDWAIT:
                    entry->flags |= MTE_EVENT_FLAG_VALID;
                    entry->session = Iquery_pduSession( reqinfo->asp->pdu );
                    break;

                case TC_RS_DESTROY:
                    row = ( TdataRow* )
                        TableTdata_extractRow( request );
                    mteEvent_removeEntry( row );
                }
            }
        }
        break;
    }
    DEBUG_MSGTL( ( "disman:event:mib", "Table handler, done\n" ) );
    return PRIOT_ERR_NOERROR;
}
