/*
 * DisMan Event MIB:
 *     Implementation of the mteTriggerTable MIB interface
 * See 'mteTrigger.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include "mteTriggerTable.h"
#include "CheckVarbind.h"
#include "Client.h"
#include "Debug.h"
#include "Table.h"
#include "Tc.h"
#include "mteTrigger.h"
#include "utilities/Iquery.h"

static TableRegistrationInfo* table_info;

/** Initializes the mteTriggerTable module */
void init_mteTriggerTable( void )
{
    static oid mteTriggerTable_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 2 };
    size_t mteTriggerTable_oid_len = ASN01_OID_LENGTH( mteTriggerTable_oid );
    HandlerRegistration* reg;

    /*
     * Ensure the (combined) table container is available...
     */
    init_trigger_table_data();

    /*
     * ... then set up the MIB interface to the mteTriggerTable slice
     */
    reg = AgentHandler_createHandlerRegistration( "mteTriggerTable",
        mteTriggerTable_handler,
        mteTriggerTable_oid,
        mteTriggerTable_oid_len,
        HANDLER_CAN_RWRITE );

    table_info = TOOLS_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( table_info,
        ASN01_OCTET_STR, /* index: mteOwner       */
        /* index: mteTriggerName */
        ASN01_PRIV_IMPLIED_OCTET_STR,
        0 );

    table_info->min_column = COLUMN_MTETRIGGERCOMMENT;
    table_info->max_column = COLUMN_MTETRIGGERENTRYSTATUS;

    /* Register this using the (common) trigger_table_data container */
    TableTdata_register( reg, trigger_table_data, table_info );
    DEBUG_MSGTL( ( "disman:event:init", "Trigger Table\n" ) );
}

void shutdown_mteTriggerTable( void )
{
    if ( table_info ) {
        Table_registrationInfoFree( table_info );
        table_info = NULL;
    }
}

/** handles requests for the mteTriggerTable table */
int mteTriggerTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRequestInfo* tinfo;
    TdataRow* row;
    struct mteTrigger* entry;
    char mteOwner[ MTE_STR1_LEN + 1 ];
    char mteTName[ MTE_STR1_LEN + 1 ];
    long ret;

    DEBUG_MSGTL( ( "disman:event:mib", "Trigger Table handler (%d)\n",
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

            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGERCOMMENT:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->mteTriggerComment,
                    strlen( entry->mteTriggerComment ) );
                break;
            case COLUMN_MTETRIGGERTEST:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    &entry->mteTriggerTest, 1 );
                break;
            case COLUMN_MTETRIGGERSAMPLETYPE:
                ret = ( entry->flags & MTE_TRIGGER_FLAG_DELTA ) ? MTE_SAMPLE_DELTA : MTE_SAMPLE_ABSOLUTE;
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            case COLUMN_MTETRIGGERVALUEID:
                Client_setVarTypedValue( request->requestvb, ASN01_OBJECT_ID,
                    ( u_char* )entry->mteTriggerValueID,
                    entry->mteTriggerValueID_len * sizeof( oid ) );
                break;
            case COLUMN_MTETRIGGERVALUEIDWILDCARD:
                ret = ( entry->flags & MTE_TRIGGER_FLAG_VWILD ) ? TC_TV_TRUE : TC_TV_FALSE;
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            case COLUMN_MTETRIGGERTARGETTAG:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->mteTriggerTarget,
                    strlen( entry->mteTriggerTarget ) );
                break;
            case COLUMN_MTETRIGGERCONTEXTNAME:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->mteTriggerContext,
                    strlen( entry->mteTriggerContext ) );
                break;
            case COLUMN_MTETRIGGERCONTEXTNAMEWILDCARD:
                ret = ( entry->flags & MTE_TRIGGER_FLAG_CWILD ) ? TC_TV_TRUE : TC_TV_FALSE;
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            case COLUMN_MTETRIGGERFREQUENCY:
                Client_setVarTypedInteger( request->requestvb, ASN01_UNSIGNED,
                    entry->mteTriggerFrequency );
                break;
            case COLUMN_MTETRIGGEROBJECTSOWNER:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->mteTriggerOOwner,
                    strlen( entry->mteTriggerOOwner ) );
                break;
            case COLUMN_MTETRIGGEROBJECTS:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->mteTriggerObjects,
                    strlen( entry->mteTriggerObjects ) );
                break;
            case COLUMN_MTETRIGGERENABLED:
                ret = ( entry->flags & MTE_TRIGGER_FLAG_ENABLED ) ? TC_TV_TRUE : TC_TV_FALSE;
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            case COLUMN_MTETRIGGERENTRYSTATUS:
                ret = ( entry->flags & MTE_TRIGGER_FLAG_ACTIVE ) ? TC_RS_ACTIVE : TC_RS_NOTINSERVICE;
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

            entry = ( struct mteTrigger* )TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGERCOMMENT:
            case COLUMN_MTETRIGGERTARGETTAG:
            case COLUMN_MTETRIGGERCONTEXTNAME:
                ret = CheckVarbind_typeAndMaxSize(
                    request->requestvb, ASN01_OCTET_STR, MTE_STR2_LEN );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERTEST:
                ret = CheckVarbind_typeAndSize(
                    request->requestvb, ASN01_OCTET_STR, 1 );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERSAMPLETYPE:
                ret = CheckVarbind_intRange( request->requestvb,
                    MTE_SAMPLE_ABSOLUTE, MTE_SAMPLE_DELTA );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERVALUEID:
                ret = CheckVarbind_oid( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERVALUEIDWILDCARD:
            case COLUMN_MTETRIGGERCONTEXTNAMEWILDCARD:
            case COLUMN_MTETRIGGERENABLED:
                ret = CheckVarbind_truthValue( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;

            case COLUMN_MTETRIGGERFREQUENCY:
                ret = CheckVarbind_uint( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGEROBJECTSOWNER:
            case COLUMN_MTETRIGGEROBJECTS:
                ret = CheckVarbind_typeAndMaxSize(
                    request->requestvb, ASN01_OCTET_STR, MTE_STR1_LEN );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTETRIGGERENTRYSTATUS:
                ret = CheckVarbind_rowStatus( request->requestvb,
                    ( entry ? TC_RS_ACTIVE : TC_RS_NONEXISTENT ) );
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
             * Once a row has been made active, it cannot be
             *   modified except to delete it.  There's no good
             *   reason for this, but that's what the MIB says.
             *
             * The published version of the Event MIB even forbids
             *   enabling (or disabling) an active row, which
             *   would make this object completely pointless!
             * Fortunately this ludicrous decision has since been corrected.
             */
            if ( entry && entry->flags & MTE_TRIGGER_FLAG_ACTIVE ) {
                /* check for the acceptable assignments */
                if ( ( tinfo->colnum == COLUMN_MTETRIGGERENABLED ) || ( tinfo->colnum == COLUMN_MTETRIGGERENTRYSTATUS && *request->requestvb->val.integer != TC_RS_NOTINSERVICE ) )
                    continue;

                /* Otherwise, reject this request */
                Agent_setRequestError( reqinfo, request,
                    PRIOT_ERR_INCONSISTENTVALUE );
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
            case COLUMN_MTETRIGGERENTRYSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    /*
                     * Create an (empty) new row structure
                     */
                    memset( mteOwner, 0, sizeof( mteOwner ) );
                    memcpy( mteOwner, tinfo->indexes->val.string,
                        tinfo->indexes->valLen );
                    memset( mteTName, 0, sizeof( mteTName ) );
                    memcpy( mteTName,
                        tinfo->indexes->nextVariable->val.string,
                        tinfo->indexes->nextVariable->valLen );

                    row = mteTrigger_createEntry( mteOwner, mteTName, 0 );
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
            case COLUMN_MTETRIGGERENTRYSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    /*
                     * Tidy up after a failed row creation request
                     */
                    entry = ( struct mteTrigger* )
                        TableTdata_extractEntry( request );
                    if ( entry && !( entry->flags & MTE_TRIGGER_FLAG_VALID ) ) {
                        row = ( TdataRow* )
                            TableTdata_extractRow( request );
                        mteTrigger_removeEntry( row );
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
            entry = ( struct mteTrigger* )TableTdata_extractEntry( request );
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

            entry = ( struct mteTrigger* )TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_MTETRIGGERCOMMENT:
                memset( entry->mteTriggerComment, 0,
                    sizeof( entry->mteTriggerComment ) );
                memcpy( entry->mteTriggerComment,
                    request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERTEST:
                entry->mteTriggerTest = request->requestvb->val.string[ 0 ];
                break;
            case COLUMN_MTETRIGGERSAMPLETYPE:
                if ( *request->requestvb->val.integer == MTE_SAMPLE_DELTA )
                    entry->flags |= MTE_TRIGGER_FLAG_DELTA;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_DELTA;
                break;
            case COLUMN_MTETRIGGERVALUEID:
                memset( entry->mteTriggerValueID, 0,
                    sizeof( entry->mteTriggerValueID ) );
                memcpy( entry->mteTriggerValueID,
                    request->requestvb->val.string,
                    request->requestvb->valLen );
                entry->mteTriggerValueID_len = request->requestvb->valLen / sizeof( oid );
                break;
            case COLUMN_MTETRIGGERVALUEIDWILDCARD:
                if ( *request->requestvb->val.integer == TC_TV_TRUE )
                    entry->flags |= MTE_TRIGGER_FLAG_VWILD;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_VWILD;
                break;
            case COLUMN_MTETRIGGERTARGETTAG:
                memset( entry->mteTriggerTarget, 0,
                    sizeof( entry->mteTriggerTarget ) );
                memcpy( entry->mteTriggerTarget,
                    request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERCONTEXTNAME:
                memset( entry->mteTriggerContext, 0,
                    sizeof( entry->mteTriggerContext ) );
                memcpy( entry->mteTriggerContext,
                    request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERCONTEXTNAMEWILDCARD:
                if ( *request->requestvb->val.integer == TC_TV_TRUE )
                    entry->flags |= MTE_TRIGGER_FLAG_CWILD;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_CWILD;
                break;
            case COLUMN_MTETRIGGERFREQUENCY:
                entry->mteTriggerFrequency = *request->requestvb->val.integer;
                break;
            case COLUMN_MTETRIGGEROBJECTSOWNER:
                memset( entry->mteTriggerOOwner, 0,
                    sizeof( entry->mteTriggerOOwner ) );
                memcpy( entry->mteTriggerOOwner,
                    request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGEROBJECTS:
                memset( entry->mteTriggerObjects, 0,
                    sizeof( entry->mteTriggerObjects ) );
                memcpy( entry->mteTriggerObjects,
                    request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_MTETRIGGERENABLED:
                if ( *request->requestvb->val.integer == TC_TV_TRUE )
                    entry->flags |= MTE_TRIGGER_FLAG_ENABLED;
                else
                    entry->flags &= ~MTE_TRIGGER_FLAG_ENABLED;
                break;
            case COLUMN_MTETRIGGERENTRYSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_ACTIVE:
                    entry->flags |= MTE_TRIGGER_FLAG_ACTIVE;
                    mteTrigger_enable( entry );
                    break;
                case TC_RS_CREATEANDGO:
                    entry->flags |= MTE_TRIGGER_FLAG_ACTIVE;
                    entry->flags |= MTE_TRIGGER_FLAG_VALID;
                    entry->session = Iquery_pduSession( reqinfo->asp->pdu );
                    mteTrigger_enable( entry );
                    break;
                case TC_RS_CREATEANDWAIT:
                    entry->flags |= MTE_TRIGGER_FLAG_VALID;
                    entry->session = Iquery_pduSession( reqinfo->asp->pdu );
                    break;

                case TC_RS_DESTROY:
                    row = ( TdataRow* )
                        TableTdata_extractRow( request );
                    mteTrigger_removeEntry( row );
                }
                break;
            }
        }

        /** set up to save persistent store */
        Api_storeNeeded( NULL );

        break;
    }
    return PRIOT_ERR_NOERROR;
}
