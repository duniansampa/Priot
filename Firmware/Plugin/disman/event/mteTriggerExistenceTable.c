/*
 * DisMan Event MIB:
 *     Implementation of the mteTriggerExistenceTable MIB interface
 * See 'mteTrigger.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include "mteTriggerExistenceTable.h"
#include "System/Util/VariableList.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "Table.h"
#include "mteTrigger.h"

static TableRegistrationInfo* table_info;

/* Initializes the mteTriggerExistenceTable module */
void init_mteTriggerExistenceTable( void )
{
    static oid mteTExistTable_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 4 };
    size_t mteTExistTable_oid_len = asnOID_LENGTH( mteTExistTable_oid );
    HandlerRegistration* reg;
    int rc;

    /*
     * Ensure the (combined) table container is available...
     */
    init_trigger_table_data();

    /*
     * ... then set up the MIB interface to the mteTriggerExistenceTable slice
     */
    reg = AgentHandler_createHandlerRegistration( "mteTriggerExistenceTable",
        mteTriggerExistenceTable_handler,
        mteTExistTable_oid,
        mteTExistTable_oid_len,
        HANDLER_CAN_RWRITE );

    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( table_info,
        asnOCTET_STR, /* index: mteOwner       */
        /* index: mteTriggerName */
        asnPRIV_IMPLIED_OCTET_STR,
        0 );

    table_info->min_column = COLUMN_MTETRIGGEREXISTENCETEST;
    table_info->max_column = COLUMN_MTETRIGGEREXISTENCEEVENT;

    /* Register this using the (common) trigger_table_data container */
    rc = TableTdata_register( reg, trigger_table_data, table_info );
    if ( rc != ErrorCode_SUCCESS )
        return;

    Table_handlerOwnsTableInfo( reg->handler->next );
    DEBUG_MSGTL( ( "disman:event:init", "Trigger Exist Table\n" ) );
}

/** handles requests for the mteTriggerExistenceTable table */
int mteTriggerExistenceTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRequestInfo* tinfo;
    struct mteTrigger* entry;
    int ret;

    DEBUG_MSGTL( ( "disman:event:mib", "Exist Table handler (%d)\n",
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
             * The mteTriggerExistenceTable should only contains entries for
             *   rows where the mteTriggerTest 'existence(0)' bit is set.
             * So skip entries where this isn't the case.
             */
            if ( !entry || !( entry->mteTriggerTest & MTE_TRIGGER_EXISTENCE ) ) {
                Agent_requestSetError( request, PRIOT_NOSUCHINSTANCE );
                continue;
            }

            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGEREXISTENCETEST:
                Client_setVarTypedValue( request->requestvb, asnOCTET_STR,
                    ( u_char* )&entry->mteTExTest, 1 );
                break;
            case COLUMN_MTETRIGGEREXISTENCESTARTUP:
                Client_setVarTypedValue( request->requestvb, asnOCTET_STR,
                    ( u_char* )&entry->mteTExStartup, 1 );
                break;
            case COLUMN_MTETRIGGEREXISTENCEOBJECTSOWNER:
                Client_setVarTypedValue( request->requestvb, asnOCTET_STR,
                    ( u_char* )entry->mteTExObjOwner,
                    strlen( entry->mteTExObjOwner ) );
                break;
            case COLUMN_MTETRIGGEREXISTENCEOBJECTS:
                Client_setVarTypedValue( request->requestvb, asnOCTET_STR,
                    ( u_char* )entry->mteTExObjects,
                    strlen( entry->mteTExObjects ) );
                break;
            case COLUMN_MTETRIGGEREXISTENCEEVENTOWNER:
                Client_setVarTypedValue( request->requestvb, asnOCTET_STR,
                    ( u_char* )entry->mteTExEvOwner,
                    strlen( entry->mteTExEvOwner ) );
                break;
            case COLUMN_MTETRIGGEREXISTENCEEVENT:
                Client_setVarTypedValue( request->requestvb, asnOCTET_STR,
                    ( u_char* )entry->mteTExEvent,
                    strlen( entry->mteTExEvent ) );
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
             * Since the mteTriggerExistenceTable only contains entries for
             *   rows where the mteTriggerTest 'existence(0)' bit is set,
             *   strictly speaking we should reject assignments where
             *   this isn't the case.
             * But SET requests that include an assignment of the
             *   'existence(0)' bit at the same time are valid, so would
             *    need to be accepted. Unfortunately, this assignment
             *   is only applied in the IMPL_COMMIT pass, so it's difficult
             *   to detect whether this holds or not.
             *
             * Let's fudge things for now, by processing assignments
             *   even if the 'existence(0)' bit isn't set.
             */
            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGEREXISTENCETEST:
            case COLUMN_MTETRIGGEREXISTENCESTARTUP:
                ret = VariableList_checkTypeAndLength(
                    request->requestvb, asnOCTET_STR, 1 );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;

            case COLUMN_MTETRIGGEREXISTENCEOBJECTSOWNER:
            case COLUMN_MTETRIGGEREXISTENCEOBJECTS:
            case COLUMN_MTETRIGGEREXISTENCEEVENTOWNER:
            case COLUMN_MTETRIGGEREXISTENCEEVENT:
                ret = VariableList_checkTypeAndMaxLength(
                    request->requestvb, asnOCTET_STR, MTE_STR1_LEN );
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
            case COLUMN_MTETRIGGEREXISTENCETEST:
                entry->mteTExTest = request->requestvb->value.string[ 0 ];
                break;
            case COLUMN_MTETRIGGEREXISTENCESTARTUP:
                entry->mteTExStartup = request->requestvb->value.string[ 0 ];
                break;
            case COLUMN_MTETRIGGEREXISTENCEOBJECTSOWNER:
                memset( entry->mteTExObjOwner, 0, sizeof( entry->mteTExObjOwner ) );
                memcpy( entry->mteTExObjOwner, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_MTETRIGGEREXISTENCEOBJECTS:
                memset( entry->mteTExObjects, 0, sizeof( entry->mteTExObjects ) );
                memcpy( entry->mteTExObjects, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_MTETRIGGEREXISTENCEEVENTOWNER:
                memset( entry->mteTExEvOwner, 0, sizeof( entry->mteTExEvOwner ) );
                memcpy( entry->mteTExEvOwner, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_MTETRIGGEREXISTENCEEVENT:
                memset( entry->mteTExEvent, 0, sizeof( entry->mteTExEvent ) );
                memcpy( entry->mteTExEvent, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            }
        }
        break;
    }
    return PRIOT_ERR_NOERROR;
}
