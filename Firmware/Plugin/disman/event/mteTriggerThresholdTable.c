/*
 * DisMan Event MIB:
 *     Implementation of the mteTriggerThresholdTable MIB interface
 * See 'mteTrigger.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include "mteTriggerThresholdTable.h"
#include "CheckVarbind.h"
#include "Client.h"
#include "Debug.h"
#include "Table.h"
#include "mteTrigger.h"

static TableRegistrationInfo* table_info;

/** Initializes the mteTriggerThresholdTable module */
void init_mteTriggerThresholdTable( void )
{
    static oid mteTThreshTable_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 6 };
    size_t mteTThreshTable_oid_len = ASN01_OID_LENGTH( mteTThreshTable_oid );
    HandlerRegistration* reg;

    /*
     * Ensure the (combined) table container is available...
     */
    init_trigger_table_data();

    /*
     * ... then set up the MIB interface to the mteTriggerThresholdTable slice
     */
    reg = AgentHandler_createHandlerRegistration( "mteTriggerThresholdTable",
        mteTriggerThresholdTable_handler,
        mteTThreshTable_oid,
        mteTThreshTable_oid_len,
        HANDLER_CAN_RWRITE );

    table_info = TOOLS_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( table_info,
        ASN01_OCTET_STR, /* index: mteOwner       */
        /* index: mteTriggerName */
        ASN01_PRIV_IMPLIED_OCTET_STR,
        0 );

    table_info->min_column = COLUMN_MTETRIGGERTHRESHOLDSTARTUP;
    table_info->max_column = COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENT;

    /* Register this using the (common) trigger_table_data container */
    TableTdata_register( reg, trigger_table_data, table_info );
    DEBUG_MSGTL( ( "disman:event:init", "Trigger Threshold Table\n" ) );
}

void shutdown_mteTriggerThresholdTable( void )
{
    if ( table_info ) {
        Table_registrationInfoFree( table_info );
        table_info = NULL;
    }
}

/** handles requests for the mteTriggerThresholdTable table */
int mteTriggerThresholdTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRequestInfo* tinfo;
    struct mteTrigger* entry;
    int ret;

    DEBUG_MSGTL( ( "disman:event:mib", "Threshold Table handler (%d)\n",
        reqinfo->mode ) );

    switch ( reqinfo->mode ) {
    /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            entry = ( struct mteTrigger* )TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            /*
             * The mteTriggerThresholdTable should only contains entries for
             *   rows where the mteTriggerTest 'threshold(2)' bit is set.
             * So skip entries where this isn't the case.
             */
            if ( !entry || !( entry->mteTriggerTest & MTE_TRIGGER_THRESHOLD ) ) {
                Agent_requestSetError( request, PRIOT_NOSUCHINSTANCE );
                continue;
            }

            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGERTHRESHOLDSTARTUP:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->mteTThStartup );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISING:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->mteTThRiseValue );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLING:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->mteTThFallValue );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISING:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->mteTThDRiseValue );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLING:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->mteTThDFallValue );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTSOWNER:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTThObjOwner,
                    strlen( entry->mteTThObjOwner ) );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTS:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTThObjects,
                    strlen( entry->mteTThObjects ) );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENTOWNER:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTThRiseOwner,
                    strlen( entry->mteTThRiseOwner ) );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENT:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTThRiseEvent,
                    strlen( entry->mteTThRiseEvent ) );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENTOWNER:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTThFallOwner,
                    strlen( entry->mteTThFallOwner ) );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENT:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTThFallEvent,
                    strlen( entry->mteTThFallEvent ) );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENTOWNER:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTThDRiseOwner,
                    strlen( entry->mteTThDRiseOwner ) );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENT:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTThDRiseEvent,
                    strlen( entry->mteTThDRiseEvent ) );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENTOWNER:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTThDFallOwner,
                    strlen( entry->mteTThDFallOwner ) );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENT:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTThDFallEvent,
                    strlen( entry->mteTThDFallEvent ) );
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

            entry = ( struct mteTrigger* )TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            /*
             * Since the mteTriggerThresholdTable only contains entries for
             *   rows where the mteTriggerTest 'threshold(2)' bit is set,
             *   strictly speaking we should reject assignments where
             *   this isn't the case.
             * But SET requests that include an assignment of the
             *   'threshold(2)' bit at the same time are valid, so would
             *    need to be accepted. Unfortunately, this assignment
             *   is only applied in the IMPL_COMMIT pass, so it's difficult
             *   to detect whether this holds or not.
             *
             * Let's fudge things for now, by processing assignments
             *   even if the 'threshold(2)' bit isn't set.
             */
            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGERTHRESHOLDSTARTUP:
                ret = CheckVarbind_intRange( request->requestvb,
                    MTE_THRESH_START_RISE,
                    MTE_THRESH_START_RISEFALL );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISING:
            case COLUMN_MTETRIGGERTHRESHOLDFALLING:
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISING:
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLING:
                ret = CheckVarbind_int( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTSOWNER:
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTS:
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENTOWNER:
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENT:
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENTOWNER:
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENT:
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENTOWNER:
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENT:
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENTOWNER:
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENT:
                ret = CheckVarbind_typeAndMaxSize(
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
             * The Event MIB is somewhat ambiguous as to whether the
             *  various trigger table entries can be modified once the
             *  main mteTriggerTable entry has been marked 'active'. 
             * But it's clear from discussion on the DisMan mailing
             *  list is that the intention is not.
             *
             * So check for whether this row is already active,
             *  and reject *all* SET requests if it is.
             */
            entry = ( struct mteTrigger* )TableTdata_extractEntry( request );
            if ( entry && entry->flags & MTE_TRIGGER_FLAG_ACTIVE ) {
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

            entry = ( struct mteTrigger* )TableTdata_extractEntry( request );
            if ( !entry ) {
                /*
                 * New rows must be created via the RowStatus column
                 *   (in the main mteTriggerTable)
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

            entry = ( struct mteTrigger* )TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGERTHRESHOLDSTARTUP:
                entry->mteTThStartup = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISING:
                entry->mteTThRiseValue = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLING:
                entry->mteTThFallValue = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISING:
                entry->mteTThDRiseValue = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLING:
                entry->mteTThDFallValue = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTSOWNER:
                memset( entry->mteTThObjOwner, 0, sizeof( entry->mteTThObjOwner ) );
                memcpy( entry->mteTThObjOwner, request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDOBJECTS:
                memset( entry->mteTThObjects, 0, sizeof( entry->mteTThObjects ) );
                memcpy( entry->mteTThObjects, request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENTOWNER:
                memset( entry->mteTThRiseOwner, 0, sizeof( entry->mteTThRiseOwner ) );
                memcpy( entry->mteTThRiseOwner, request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDRISINGEVENT:
                memset( entry->mteTThRiseEvent, 0, sizeof( entry->mteTThRiseEvent ) );
                memcpy( entry->mteTThRiseEvent, request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENTOWNER:
                memset( entry->mteTThFallOwner, 0, sizeof( entry->mteTThFallOwner ) );
                memcpy( entry->mteTThFallOwner, request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDFALLINGEVENT:
                memset( entry->mteTThFallEvent, 0, sizeof( entry->mteTThFallEvent ) );
                memcpy( entry->mteTThFallEvent, request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENTOWNER:
                memset( entry->mteTThDRiseOwner, 0, sizeof( entry->mteTThDRiseOwner ) );
                memcpy( entry->mteTThDRiseOwner, request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTARISINGEVENT:
                memset( entry->mteTThDRiseEvent, 0, sizeof( entry->mteTThDRiseEvent ) );
                memcpy( entry->mteTThDRiseEvent, request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENTOWNER:
                memset( entry->mteTThDFallOwner, 0, sizeof( entry->mteTThDFallOwner ) );
                memcpy( entry->mteTThDFallOwner, request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERTHRESHOLDDELTAFALLINGEVENT:
                memset( entry->mteTThDFallEvent, 0, sizeof( entry->mteTThDFallEvent ) );
                memcpy( entry->mteTThDFallEvent, request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            }
        }
        break;
    }
    return PRIOT_ERR_NOERROR;
}
