/*
 * DisMan Event MIB:
 *     Implementation of the mteObjectsTable MIB interface
 * See 'mteObjects.c' for active behaviour of this table.
 *
 * (based on mib2c.table_data.conf output)
 */

#include "mteObjectsTable.h"
#include "CheckVarbind.h"
#include "Client.h"
#include "Debug.h"
#include "Table.h"
#include "Tc.h"
#include "mteObjects.h"

static TableRegistrationInfo* table_info;

/** Initializes the mteObjectsTable module */
void init_mteObjectsTable( void )

{
    static oid mteObjectsTable_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 3, 1 };
    size_t mteObjectsTable_oid_len = ASN01_OID_LENGTH( mteObjectsTable_oid );
    HandlerRegistration* reg;

    /*
     * Ensure the object table container is available...
     */
    init_objects_table_data();

    /*
     * ... then set up the MIB interface to this table
     */
    reg = AgentHandler_createHandlerRegistration( "mteObjectsTable",
        mteObjectsTable_handler,
        mteObjectsTable_oid,
        mteObjectsTable_oid_len,
        HANDLER_CAN_RWRITE );

    table_info = TOOLS_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( table_info,
        ASN01_OCTET_STR, /* index: mteOwner */
        ASN01_OCTET_STR, /* index: mteObjectsName */
        ASN01_UNSIGNED, /* index: mteObjectsIndex */
        0 );

    table_info->min_column = COLUMN_MTEOBJECTSID;
    table_info->max_column = COLUMN_MTEOBJECTSENTRYSTATUS;

    TableTdata_register( reg, objects_table_data, table_info );
}

void shutdown_mteObjectsTable( void )
{
    if ( table_info ) {
        Table_registrationInfoFree( table_info );
        table_info = NULL;
    }
}

/** handles requests for the mteObjectsTable table */
int mteObjectsTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRequestInfo* tinfo;
    TdataRow* row;
    struct mteObject* entry;
    char mteOwner[ MTE_STR1_LEN + 1 ];
    char mteOName[ MTE_STR1_LEN + 1 ];
    long ret;

    DEBUG_MSGTL( ( "disman:event:mib", "ObjTable handler (%d)\n", reqinfo->mode ) );

    switch ( reqinfo->mode ) {
    /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            entry = ( struct mteObject* )TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            if ( !entry ) {
                Agent_setRequestError( reqinfo, request, PRIOT_NOSUCHINSTANCE );
                continue;
            }
            switch ( tinfo->colnum ) {
            case COLUMN_MTEOBJECTSID:
                Client_setVarTypedValue( request->requestvb, ASN01_OBJECT_ID,
                    ( u_char* )entry->mteObjectID,
                    entry->mteObjectID_len * sizeof( oid ) );
                break;
            case COLUMN_MTEOBJECTSIDWILDCARD:
                ret = ( entry->flags & MTE_OBJECT_FLAG_WILD ) ? TC_TV_TRUE : TC_TV_FALSE;
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER, ret );
                break;
            case COLUMN_MTEOBJECTSENTRYSTATUS:
                ret = ( entry->flags & MTE_OBJECT_FLAG_ACTIVE ) ? TC_RS_ACTIVE : TC_RS_NOTINSERVICE;
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

            entry = ( struct mteObject* )TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_MTEOBJECTSID:
                ret = CheckVarbind_oid( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                /*
                 * Can't modify the OID of an active row
                 *  (an unnecessary restriction, IMO)
                 */
                if ( entry && entry->flags & MTE_OBJECT_FLAG_ACTIVE ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_INCONSISTENTVALUE );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEOBJECTSIDWILDCARD:
                ret = CheckVarbind_truthValue( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                /*
                 * Can't modify the wildcarding of an active row
                 *  (an unnecessary restriction, IMO)
                 */
                if ( entry && entry->flags & MTE_OBJECT_FLAG_ACTIVE ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_INCONSISTENTVALUE );
                    return PRIOT_ERR_NOERROR;
                }
                break;
            case COLUMN_MTEOBJECTSENTRYSTATUS:
                ret = CheckVarbind_rowStatus( request->requestvb,
                    ( entry ? TC_RS_ACTIVE : TC_RS_NONEXISTENT ) );
                if ( ret != PRIOT_ERR_NOERROR ) {
                    Agent_setRequestError( reqinfo, request, ret );
                    return PRIOT_ERR_NOERROR;
                }
                /* An active row can only be deleted */
                if ( entry && entry->flags & MTE_OBJECT_FLAG_ACTIVE && *request->requestvb->val.integer == TC_RS_NOTINSERVICE ) {
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
            case COLUMN_MTEOBJECTSENTRYSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    /*
                     * Create an (empty) new row structure
                     */
                    memset( mteOwner, 0, sizeof( mteOwner ) );
                    memcpy( mteOwner, tinfo->indexes->val.string,
                        tinfo->indexes->valLen );
                    memset( mteOName, 0, sizeof( mteOName ) );
                    memcpy( mteOName,
                        tinfo->indexes->nextVariable->val.string,
                        tinfo->indexes->nextVariable->valLen );
                    ret = *tinfo->indexes->nextVariable->nextVariable->val.integer;

                    row = mteObjects_createEntry( mteOwner, mteOName, ret, 0 );
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
            case COLUMN_MTEOBJECTSENTRYSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    /*
                     * Tidy up after a failed row creation request
                     */
                    entry = ( struct mteObject* )
                        TableTdata_extractEntry( request );
                    if ( entry && !( entry->flags & MTE_OBJECT_FLAG_VALID ) ) {
                        row = ( TdataRow* )
                            TableTdata_extractRow( request );
                        mteObjects_removeEntry( row );
                    }
                }
            }
        }
        break;

    case MODE_SET_ACTION:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;

            entry = ( struct mteObject* )TableTdata_extractEntry( request );
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

            entry = ( struct mteObject* )TableTdata_extractEntry( request );
            tinfo = Table_extractTableInfo( request );

            switch ( tinfo->colnum ) {
            case COLUMN_MTEOBJECTSID:
                memset( entry->mteObjectID, 0, sizeof( entry->mteObjectID ) );
                memcpy( entry->mteObjectID, request->requestvb->val.objid,
                    request->requestvb->valLen );
                entry->mteObjectID_len = request->requestvb->valLen / sizeof( oid );
                break;

            case COLUMN_MTEOBJECTSIDWILDCARD:
                if ( *request->requestvb->val.integer == TC_TV_TRUE )
                    entry->flags |= MTE_OBJECT_FLAG_WILD;
                else
                    entry->flags &= ~MTE_OBJECT_FLAG_WILD;
                break;

            case COLUMN_MTEOBJECTSENTRYSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_ACTIVE:
                    entry->flags |= MTE_OBJECT_FLAG_ACTIVE;
                    break;
                case TC_RS_CREATEANDGO:
                    entry->flags |= MTE_OBJECT_FLAG_VALID;
                    entry->flags |= MTE_OBJECT_FLAG_ACTIVE;
                    break;
                case TC_RS_CREATEANDWAIT:
                    entry->flags |= MTE_OBJECT_FLAG_VALID;
                    break;

                case TC_RS_DESTROY:
                    row = ( TdataRow* )
                        TableTdata_extractRow( request );
                    mteObjects_removeEntry( row );
                }
            }
        }

        /** set up to save persistent store */
        Api_storeNeeded( NULL );

        break;
    }
    return PRIOT_ERR_NOERROR;
}
