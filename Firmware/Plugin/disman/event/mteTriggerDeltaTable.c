/*
 * DisMan Event MIB:
 *     Implementation of the mteTriggerDeltaTable MIB interface
 * See 'mteTrigger.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include "mteTriggerDeltaTable.h"
#include "System/Util/VariableList.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "Table.h"
#include "TextualConvention.h"
#include "mteTrigger.h"

/** Initializes the mteTriggerDeltaTable module */
void init_mteTriggerDeltaTable( void )
{
    static oid mteTDeltaTable_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 3 };
    size_t mteTDeltaTable_oid_len = asnOID_LENGTH( mteTDeltaTable_oid );
    HandlerRegistration* reg;

    TableRegistrationInfo* table_info;
    int rc;

    /*
     * Ensure the (combined) table container is available...
     */
    init_trigger_table_data();

    /*
     * ... then set up the MIB interface to the mteTriggerDeltaTable slice
     */
    reg = AgentHandler_createHandlerRegistration( "mteTriggerDeltaTable",
        mteTriggerDeltaTable_handler,
        mteTDeltaTable_oid,
        mteTDeltaTable_oid_len,
        HANDLER_CAN_RWRITE );

    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( table_info,
        asnOCTET_STR, /* index: mteOwner       */
        /* index: mteTriggerName */
        asnPRIV_IMPLIED_OCTET_STR,
        0 );

    table_info->min_column = COLUMN_MTETRIGGERDELTADISCONTINUITYID;
    table_info->max_column = COLUMN_MTETRIGGERDELTADISCONTINUITYIDTYPE;

    /* Register this using the (common) trigger_table_data container */
    rc = TableTdata_register( reg, trigger_table_data, table_info );
    if ( rc != ErrorCode_SUCCESS )
        return;
    Table_handlerOwnsTableInfo( reg->handler->next );
    DEBUG_MSGTL( ( "disman:event:init", "Trigger Delta Table\n" ) );
}

/** handles requests for the mteTriggerDeltaTable table */
int mteTriggerDeltaTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRequestInfo* tinfo;
    struct mteTrigger* entry;
    int ret;

    DEBUG_MSGTL( ( "disman:event:mib", "Delta Table handler (%d)\n",
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
             * The mteTriggerBooleanTable should only contains entries for
             *   rows where the mteTriggerSampleType is 'deltaValue(2)'
             * So skip entries where this isn't the case.
             */
            if ( !entry || !( entry->flags & MTE_TRIGGER_FLAG_DELTA ) ) {
                Agent_requestSetError( request, PRIOT_NOSUCHINSTANCE );
                continue;
            }

            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGERDELTADISCONTINUITYID:
                Client_setVarTypedValue( request->requestvb, asnOBJECT_ID,
                    ( u_char* )entry->mteDeltaDiscontID,
                    entry->mteDeltaDiscontID_len * sizeof( oid ) );
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDWILDCARD:
                ret = ( entry->flags & MTE_TRIGGER_FLAG_DWILD ) ? tcTRUE : tcFALSE;
                Client_setVarTypedInteger( request->requestvb, asnINTEGER, ret );
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDTYPE:
                Client_setVarTypedInteger( request->requestvb, asnINTEGER,
                    entry->mteDeltaDiscontIDType );
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
             * Since the mteTriggerDeltaTable only contains entries for
             *   rows where mteTriggerSampleType is 'deltaValue(2)',
             *   strictly speaking we should reject assignments where
             *   this isn't the case.
             * But SET requests that include an assignment of
             *   'deltaValue(2)' at the same time are valid, so would
             *    need to be accepted. Unfortunately, this assignment
             *   is only applied in the IMPL_COMMIT pass, so it's difficult
             *   to detect whether this holds or not.
             *
             * Let's fudge things for now, by processing
             *   assignments even if this value isn't set.
             */
            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGERDELTADISCONTINUITYID:
                ret = VariableList_checkOidMaxLength( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDWILDCARD:
                ret = VariableList_checkBoolLengthAndValue( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDTYPE:
                ret = VariableList_checkIntLengthAndRange( request->requestvb,
                    MTE_DELTAD_TTICKS,
                    MTE_DELTAD_DATETIME );
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
            case COLUMN_MTETRIGGERDELTADISCONTINUITYID:
                if ( Api_oidCompare(
                         request->requestvb->value.objectId,
                         request->requestvb->valueLength / sizeof( oid ),
                         _sysUpTime_instance, _sysUpTime_inst_len )
                    != 0 ) {
                    memset( entry->mteDeltaDiscontID, 0,
                        sizeof( entry->mteDeltaDiscontID ) );
                    memcpy( entry->mteDeltaDiscontID,
                        request->requestvb->value.string,
                        request->requestvb->valueLength );
                    entry->mteDeltaDiscontID_len = request->requestvb->valueLength / sizeof( oid );
                    entry->flags &= ~MTE_TRIGGER_FLAG_SYSUPT;
                }
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDWILDCARD:
                if ( *request->requestvb->value.integer == tcTRUE )
                    entry->flags |= MTE_TRIGGER_FLAG_DWILD;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_DWILD;
                break;
            case COLUMN_MTETRIGGERDELTADISCONTINUITYIDTYPE:
                entry->mteDeltaDiscontIDType = *request->requestvb->value.integer;
                break;
            }
        }
        break;
    }
    return PRIOT_ERR_NOERROR;
}
