/*
 * DisMan Event MIB:
 *     Implementation of the mteTriggerBooleanTable MIB interface
 * See 'mteTrigger.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include "mteTriggerBooleanTable.h"
#include "System/Util/VariableList.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "Table.h"
#include "TextualConvention.h"
#include "mteTrigger.h"

static TableRegistrationInfo* table_info;

/** Initializes the mteTriggerBooleanTable module */
void init_mteTriggerBooleanTable( void )
{
    static oid mteTBoolTable_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 5 };
    size_t mteTBoolTable_oid_len = ASN01_OID_LENGTH( mteTBoolTable_oid );
    HandlerRegistration* reg;

    /*
     * Ensure the (combined) table container is available...
     */
    init_trigger_table_data();

    /*
     * ... then set up the MIB interface to the mteTriggerBooleanTable slice
     */
    reg = AgentHandler_createHandlerRegistration( "mteTriggerBooleanTable",
        mteTriggerBooleanTable_handler,
        mteTBoolTable_oid,
        mteTBoolTable_oid_len,
        HANDLER_CAN_RWRITE );

    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( table_info,
        ASN01_OCTET_STR, /* index: mteOwner       */
        /* index: mteTriggerName */
        ASN01_PRIV_IMPLIED_OCTET_STR,
        0 );

    table_info->min_column = COLUMN_MTETRIGGERBOOLEANCOMPARISON;
    table_info->max_column = COLUMN_MTETRIGGERBOOLEANEVENT;

    /* Register this using the (common) trigger_table_data container */
    TableTdata_register( reg, trigger_table_data, table_info );
    DEBUG_MSGTL( ( "disman:event:init", "Trigger Bool Table\n" ) );
}

void shutdown_mteTriggerBooleanTable( void )
{
    if ( table_info ) {
        Table_registrationInfoFree( table_info );
        table_info = NULL;
    }
}

/** handles requests for the mteTriggerBooleanTable table */
int mteTriggerBooleanTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRequestInfo* tinfo;
    struct mteTrigger* entry;
    int ret;

    DEBUG_MSGTL( ( "disman:event:mib", "Boolean Table handler (%d)\n",
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
             *   rows where the mteTriggerTest 'boolean(1)' bit is set.
             * So skip entries where this isn't the case.
             */
            if ( !entry || !( entry->mteTriggerTest & MTE_TRIGGER_BOOLEAN ) ) {
                Agent_requestSetError( request, PRIOT_NOSUCHINSTANCE );
                continue;
            }

            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGERBOOLEANCOMPARISON:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->mteTBoolComparison );
                break;
            case COLUMN_MTETRIGGERBOOLEANVALUE:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->mteTBoolValue );
                break;
            case COLUMN_MTETRIGGERBOOLEANSTARTUP:
                ret = ( entry->flags & MTE_TRIGGER_FLAG_BSTART ) ? TC_TV_TRUE : TC_TV_FALSE;
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            case COLUMN_MTETRIGGERBOOLEANOBJECTSOWNER:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTBoolObjOwner,
                    strlen( entry->mteTBoolObjOwner ) );
                break;
            case COLUMN_MTETRIGGERBOOLEANOBJECTS:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTBoolObjects,
                    strlen( entry->mteTBoolObjects ) );
                break;
            case COLUMN_MTETRIGGERBOOLEANEVENTOWNER:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTBoolEvOwner,
                    strlen( entry->mteTBoolEvOwner ) );
                break;
            case COLUMN_MTETRIGGERBOOLEANEVENT:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->mteTBoolEvent,
                    strlen( entry->mteTBoolEvent ) );
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
             * Since the mteTriggerBooleanTable only contains entries for
             *   rows where the mteTriggerTest 'boolean(1)' bit is set,
             *   strictly speaking we should reject assignments where
             *   this isn't the case.
             * But SET requests that include an assignment of the
             *   'boolean(1)' bit at the same time are valid, so would
             *    need to be accepted. Unfortunately, this assignment
             *   is only applied in the IMPL_COMMIT pass, so it's difficult
             *   to detect whether this holds or not.
             *
             * Let's fudge things for now, by processing assignments
             *   even if the 'boolean(1)' bit isn't set.
             */
            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGERBOOLEANCOMPARISON:
                ret = VariableList_checkIntLengthAndRange( request->requestvb,
                    MTE_BOOL_UNEQUAL, MTE_BOOL_GREATEREQUAL );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERBOOLEANVALUE:
                ret = VariableList_checkIntLength( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERBOOLEANSTARTUP:
                ret = VariableList_checkBoolLengthAndValue( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERBOOLEANOBJECTSOWNER:
            case COLUMN_MTETRIGGERBOOLEANOBJECTS:
            case COLUMN_MTETRIGGERBOOLEANEVENTOWNER:
            case COLUMN_MTETRIGGERBOOLEANEVENT:
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
            case COLUMN_MTETRIGGERBOOLEANCOMPARISON:
                entry->mteTBoolComparison = *request->requestvb->value.integer;
                break;
            case COLUMN_MTETRIGGERBOOLEANVALUE:
                entry->mteTBoolValue = *request->requestvb->value.integer;
                break;
            case COLUMN_MTETRIGGERBOOLEANSTARTUP:
                if ( *request->requestvb->value.integer == TC_TV_TRUE )
                    entry->flags |= MTE_TRIGGER_FLAG_BSTART;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_BSTART;
                break;
            case COLUMN_MTETRIGGERBOOLEANOBJECTSOWNER:
                memset( entry->mteTBoolObjOwner, 0, sizeof( entry->mteTBoolObjOwner ) );
                memcpy( entry->mteTBoolObjOwner, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_MTETRIGGERBOOLEANOBJECTS:
                memset( entry->mteTBoolObjects, 0, sizeof( entry->mteTBoolObjects ) );
                memcpy( entry->mteTBoolObjects, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_MTETRIGGERBOOLEANEVENTOWNER:
                memset( entry->mteTBoolEvOwner, 0, sizeof( entry->mteTBoolEvOwner ) );
                memcpy( entry->mteTBoolEvOwner, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            case COLUMN_MTETRIGGERBOOLEANEVENT:
                memset( entry->mteTBoolEvent, 0, sizeof( entry->mteTBoolEvent ) );
                memcpy( entry->mteTBoolEvent, request->requestvb->value.string,
                    request->requestvb->valueLength );
                break;
            }
        }
        break;
    }
    return PRIOT_ERR_NOERROR;
}
