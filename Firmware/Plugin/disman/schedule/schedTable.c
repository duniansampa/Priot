/*
 * DisMan Schedule MIB:
 *     Core implementation of the schedTable MIB interface.
 * See 'schedCore.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include "schedTable.h"
#include "Assert.h"
#include "CheckVarbind.h"
#include "Client.h"
#include "Debug.h"
#include "Table.h"
#include "Tc.h"
#include "schedCore.h"
#include "utilities/Iquery.h"

static TableRegistrationInfo* table_info;

/** Initializes the schedTable module */
void init_schedTable( void )
{
    static oid schedTable_oid[] = { 1, 3, 6, 1, 2, 1, 63, 1, 2 };
    size_t schedTable_oid_len = ASN01_OID_LENGTH( schedTable_oid );
    HandlerRegistration* reg;

    DEBUG_MSGTL( ( "disman:schedule:init", "Initializing table\n" ) );
    /*
     * Ensure the schedule table container is available...
     */
    init_schedule_container();

    /*
     * ... then set up the MIB interface.
     */
    reg = AgentHandler_createHandlerRegistration( "schedTable",
        schedTable_handler,
        schedTable_oid,
        schedTable_oid_len,
        HANDLER_CAN_RWRITE );

    table_info = TOOLS_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( table_info,
        ASN01_OCTET_STR, /* index: schedOwner */
        ASN01_OCTET_STR, /* index: schedName  */
        0 );
    table_info->min_column = COLUMN_SCHEDDESCR;
    table_info->max_column = COLUMN_SCHEDTRIGGERS;

    TableTdata_register( reg, schedule_table, table_info );
}

void shutdown_schedTable( void )
{
    if ( table_info ) {
        Table_registrationInfoFree( table_info );
        table_info = NULL;
    }
}

/** handles requests for the schedTable table */
int schedTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRequestInfo* tinfo;
    TdataRow* row;
    struct schedTable_entry* entry;
    int recalculate = 0;
    size_t len;
    char* cp;
    char owner[ SCHED_STR1_LEN + 1 ];
    char name[ SCHED_STR1_LEN + 1 ];
    int ret;

    DEBUG_MSGTL( ( "disman:schedule:mib", "Schedule handler (%d)\n",
        reqinfo->mode ) );
    switch ( reqinfo->mode ) {
    /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            entry = ( struct schedTable_entry* )
                TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_SCHEDDESCR:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->schedDescr,
                    strlen( entry->schedDescr ) );
                break;
            case COLUMN_SCHEDINTERVAL:
                Client_setVarTypedInteger( request->requestvb, ASN01_UNSIGNED,
                    entry->schedInterval );
                break;
            case COLUMN_SCHEDWEEKDAY:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    &entry->schedWeekDay,
                    sizeof( entry->schedWeekDay ) );
                break;
            case COLUMN_SCHEDMONTH:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->schedMonth,
                    sizeof( entry->schedMonth ) );
                break;
            case COLUMN_SCHEDDAY:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->schedDay,
                    sizeof( entry->schedDay ) );
                break;
            case COLUMN_SCHEDHOUR:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->schedHour,
                    sizeof( entry->schedHour ) );
                break;
            case COLUMN_SCHEDMINUTE:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->schedMinute,
                    sizeof( entry->schedMinute ) );
                break;
            case COLUMN_SCHEDCONTEXTNAME:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    entry->schedContextName,
                    strlen( entry->schedContextName ) );
                break;
            case COLUMN_SCHEDVARIABLE:
                Client_setVarTypedValue( request->requestvb, ASN01_OBJECT_ID,
                    ( u_char* )entry->schedVariable,
                    entry->schedVariable_len * sizeof( oid ) );
                break;
            case COLUMN_SCHEDVALUE:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->schedValue );
                break;
            case COLUMN_SCHEDTYPE:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->schedType );
                break;
            case COLUMN_SCHEDADMINSTATUS:
                ret = ( entry->flags & SCHEDULE_FLAG_ENABLED ) ? TC_TV_TRUE : TC_TV_FALSE;
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            case COLUMN_SCHEDOPERSTATUS:
                ret = ( entry->flags & SCHEDULE_FLAG_ENABLED ) ? TC_TV_TRUE : TC_TV_FALSE;
                /*
                 * Check for one-shot entries that have already fired
                 */
                if ( ( entry->schedType == SCHED_TYPE_ONESHOT ) && ( entry->schedLastRun != 0 ) )
                    ret = 3; /* finished(3) */
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            case COLUMN_SCHEDFAILURES:
                Client_setVarTypedInteger( request->requestvb, ASN01_COUNTER,
                    entry->schedFailures );
                break;
            case COLUMN_SCHEDLASTFAILURE:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->schedLastFailure );
                break;
            case COLUMN_SCHEDLASTFAILED:
                /*
                 * Convert 'schedLastFailed' timestamp
                 *   into DateAndTime string
                 */
                cp = ( char* )Tc_dateNTime( &entry->schedLastFailed, &len );
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    cp, len );
                break;
            case COLUMN_SCHEDSTORAGETYPE:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->schedStorageType );
                break;
            case COLUMN_SCHEDROWSTATUS:
                ret = ( entry->flags & SCHEDULE_FLAG_ACTIVE ) ? TC_TV_TRUE : TC_TV_FALSE;
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            case COLUMN_SCHEDTRIGGERS:
                Client_setVarTypedInteger( request->requestvb, ASN01_COUNTER,
                    entry->schedTriggers );
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

            entry = ( struct schedTable_entry* )
                TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_SCHEDDESCR:
                ret = CheckVarbind_typeAndMaxSize(
                    request->requestvb, ASN01_OCTET_STR, SCHED_STR2_LEN );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDINTERVAL:
                ret = CheckVarbind_uint( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDWEEKDAY:
                ret = CheckVarbind_typeAndSize(
                    request->requestvb, ASN01_OCTET_STR, 1 );
                /* XXX - check for bit(7) set */
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDMONTH:
                ret = CheckVarbind_typeAndSize( /* max_size ?? */
                    request->requestvb, ASN01_OCTET_STR, 2 );
                /* XXX - check for bit(12)-bit(15) set */
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDDAY:
                ret = CheckVarbind_typeAndSize( /* max_size ?? */
                    request->requestvb, ASN01_OCTET_STR, 4 + 4 );
                /* XXX - check for bit(62) or bit(63) set */
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDHOUR:
                ret = CheckVarbind_typeAndSize( /* max_size ?? */
                    request->requestvb, ASN01_OCTET_STR, 3 );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDMINUTE:
                ret = CheckVarbind_typeAndSize( /* max_size ?? */
                    request->requestvb, ASN01_OCTET_STR, 8 );
                /* XXX - check for bit(60)-bit(63) set */
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDCONTEXTNAME:
                ret = CheckVarbind_typeAndMaxSize(
                    request->requestvb, ASN01_OCTET_STR, SCHED_STR1_LEN );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDVARIABLE:
                ret = CheckVarbind_oid( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDVALUE:
                ret = CheckVarbind_int( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDTYPE:
                ret = CheckVarbind_intRange( request->requestvb,
                    SCHED_TYPE_PERIODIC, SCHED_TYPE_ONESHOT );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDADMINSTATUS:
                ret = CheckVarbind_truthValue( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGTYPE );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDSTORAGETYPE:
                ret = CheckVarbind_intRange( request->requestvb,
                    TC_ST_NONE, TC_ST_READONLY );
                /* XXX - check valid/consistent assignments */
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_SCHEDROWSTATUS:
                ret = CheckVarbind_rowStatus( request->requestvb,
                    ( entry ? TC_RS_ACTIVE : TC_RS_NONEXISTENT ) );
                /* XXX - check consistency assignments */
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
        }
        break;

    case MODE_SET_RESERVE2:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_SCHEDROWSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    /*
                     * Create an (empty) new row structure
                     */
                    memset( owner, 0, SCHED_STR1_LEN + 1 );
                    memset( name, 0, SCHED_STR1_LEN + 1 );
                    memcpy( owner, tinfo->indexes->val.string,
                        tinfo->indexes->valLen );
                    memcpy( name, tinfo->indexes->nextVariable->val.string,
                        tinfo->indexes->nextVariable->valLen );
                    row = schedTable_createEntry( owner, name );
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
            case COLUMN_SCHEDROWSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    /*
                     * Tidy up after a failed row creation request
                     */
                    entry = ( struct schedTable_entry* )
                        TableTdata_extractEntry( request );
                    if ( entry && !( entry->flags & SCHEDULE_FLAG_VALID ) ) {
                        row = ( TdataRow* )
                            TableTdata_extractRow( request );
                        schedTable_removeEntry( row );
                    }
                }
            }
        }
        break;

    case MODE_SET_ACTION:
        for ( request = requests; request; request = request->next ) {
            entry = ( struct schedTable_entry* )
                TableTdata_extractEntry( request );
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
        entry = NULL;
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            entry = ( struct schedTable_entry* )
                TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_SCHEDDESCR:
                memset( entry->schedDescr, 0, sizeof( entry->schedDescr ) );
                memcpy( entry->schedDescr, request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_SCHEDINTERVAL:
                entry->schedInterval = *request->requestvb->val.integer;
                recalculate = 1;
                break;
            case COLUMN_SCHEDWEEKDAY:
                entry->schedWeekDay = request->requestvb->val.string[ 0 ];
                recalculate = 1;
                break;
            case COLUMN_SCHEDMONTH:
                entry->schedMonth[ 0 ] = request->requestvb->val.string[ 0 ];
                entry->schedMonth[ 1 ] = request->requestvb->val.string[ 1 ];
                recalculate = 1;
                break;
            case COLUMN_SCHEDDAY:
                memset( entry->schedDay, 0, sizeof( entry->schedDay ) );
                memcpy( entry->schedDay, request->requestvb->val.string,
                    request->requestvb->valLen );
                recalculate = 1;
                break;
            case COLUMN_SCHEDHOUR:
                entry->schedHour[ 0 ] = request->requestvb->val.string[ 0 ];
                entry->schedHour[ 1 ] = request->requestvb->val.string[ 1 ];
                entry->schedHour[ 2 ] = request->requestvb->val.string[ 2 ];
                recalculate = 1;
                break;
            case COLUMN_SCHEDMINUTE:
                memset( entry->schedMinute, 0, sizeof( entry->schedMinute ) );
                memcpy( entry->schedMinute, request->requestvb->val.string,
                    request->requestvb->valLen );
                recalculate = 1;
                break;
            case COLUMN_SCHEDCONTEXTNAME:
                memset( entry->schedContextName, 0, sizeof( entry->schedContextName ) );
                memcpy( entry->schedContextName,
                    request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_SCHEDVARIABLE:
                memset( entry->schedVariable, 0, sizeof( entry->schedVariable ) );
                memcpy( entry->schedVariable,
                    request->requestvb->val.string,
                    request->requestvb->valLen );
                entry->schedVariable_len = request->requestvb->valLen / sizeof( oid );
                break;
            case COLUMN_SCHEDVALUE:
                entry->schedValue = *request->requestvb->val.integer;
                break;
            case COLUMN_SCHEDTYPE:
                entry->schedType = *request->requestvb->val.integer;
                break;
            case COLUMN_SCHEDADMINSTATUS:
                if ( *request->requestvb->val.integer == TC_TV_TRUE )
                    entry->flags |= SCHEDULE_FLAG_ENABLED;
                else
                    entry->flags &= ~SCHEDULE_FLAG_ENABLED;
                break;
            case COLUMN_SCHEDSTORAGETYPE:
                entry->schedStorageType = *request->requestvb->val.integer;
                break;
            case COLUMN_SCHEDROWSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_ACTIVE:
                    entry->flags |= SCHEDULE_FLAG_ACTIVE;
                    break;
                case TC_RS_CREATEANDGO:
                    entry->flags |= SCHEDULE_FLAG_ACTIVE;
                    entry->flags |= SCHEDULE_FLAG_VALID;
                    entry->session = Iquery_pduSession( reqinfo->asp->pdu );
                    break;
                case TC_RS_CREATEANDWAIT:
                    entry->flags |= SCHEDULE_FLAG_VALID;
                    entry->session = Iquery_pduSession( reqinfo->asp->pdu );
                    break;

                case TC_RS_DESTROY:
                    row = ( TdataRow* )
                        TableTdata_extractRow( request );
                    schedTable_removeEntry( row );
                }
                recalculate = 1;
                break;
            }
        }
        if ( recalculate ) {
            Assert_assert( entry );
            sched_nextTime( entry );
        }
        break;
    }
    return PRIOT_ERR_NOERROR;
}
