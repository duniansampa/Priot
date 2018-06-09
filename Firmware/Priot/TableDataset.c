#include "TableDataset.h"
#include "Api.h"
#include "Client.h"
#include "Mib.h"
#include "Parse.h"
#include "ReadConfig.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"
#include "TextualConvention.h"

static Map* _autoTables;

typedef struct DataSetTables_s {
    TableDataSet* table_set;
} DataSetTables;

typedef struct DataSetCache_s {
    void* data;
    size_t data_len;
} DataSetCache;

#define STATE_ACTION 1
#define STATE_COMMIT 2
#define STATE_UNDO 3
#define STATE_FREE 4

typedef struct NewrowStash_s {
    TableRow* newrow;
    int state;
    int created;
    int deleted;
} NewrowStash;

/** @defgroup table_dataset table_dataset
 *  Helps you implement a table with automatted storage.
 *  @ingroup table_data
 *
 *  This handler helps you implement a table where all the data is
 *  expected to be stored within the agent itself and not in some
 *  external storage location.  It handles all MIB requests including
 *  GETs, GETNEXTs and SETs.  It's possible to simply create a table
 *  without actually ever defining a handler to be called when SNMP
 *  requests come in.  To use the data, you can either attach a
 *  sub-handler that merely uses/manipulates the data further when
 *  requests come in, or you can loop through it externally when it's
 *  actually needed.  This handler is most useful in cases where a
 *  table is holding configuration data for something which gets
 *  triggered via another event.
 *
 *  NOTE NOTE NOTE: This helper isn't complete and is likely to change
 *  somewhat over time.  Specifically, the way it stores data
 *  internally may change drastically.
 *
 *  @{
 */

void TableDataset_initTableDataset( void )
{
    ReadConfig_registerAppConfigHandler( "table",
        TableDataset_configParseTableSet, NULL,
        "tableoid" );
    ReadConfig_registerAppConfigHandler( "addRow", TableDataset_configParseAddRow,
        NULL, "table_name indexes... values..." );
}

/* ==================================
 *
 * Data Set API: Table maintenance
 *
 * ================================== */

/** deletes a single dataset table data.
 *  returns the (possibly still good) next pointer of the deleted data object.
 */
static inline TableDataSetStorage*
_TableDataset_datasetDeleteData( TableDataSetStorage* data )
{
    TableDataSetStorage* nextPtr = NULL;
    if ( data ) {
        nextPtr = data->next;
        MEMORY_FREE( data->data.voidp );
    }
    MEMORY_FREE( data );
    return nextPtr;
}

/** deletes all the data from this node and beyond in the linked list */
void TableDataset_deleteAllData( TableDataSetStorage* data )
{

    while ( data ) {
        data = _TableDataset_datasetDeleteData( data );
    }
}

/** deletes all the data from this node and beyond in the linked list */
void TableDataset_deleteRow( TableRow* row )
{
    TableDataSetStorage* data;

    if ( !row )
        return;

    data = ( TableDataSetStorage* )TableData_deleteRow( row );
    TableDataset_deleteAllData( data );
}

/** adds a new row to a dataset table */
void TableDataset_addRow( TableDataSet* table,
    TableRow* row )
{
    if ( !table )
        return;
    TableData_addRow( table->table, row );
}

/** adds a new row to a dataset table */
void TableDataset_replaceRow( TableDataSet* table,
    TableRow* origrow,
    TableRow* newrow )
{
    if ( !table )
        return;
    TableData_replaceRow( table->table, origrow, newrow );
}

/** removes a row from the table, but doesn't delete/free the column values */
void TableDataset_removeRow( TableDataSet* table,
    TableRow* row )
{
    if ( !table )
        return;

    TableData_removeAndDeleteRow( table->table, row );
}

/** removes a row from the table and then deletes it (and all its data) */
void TableDataset_removeAndDeleteRow( TableDataSet* table,
    TableRow* row )
{
    TableDataSetStorage* data;

    if ( !table )
        return;

    data = ( TableDataSetStorage* )
        TableData_removeAndDeleteRow( table->table, row );

    TableDataset_deleteAllData( data );
}

/** Create a TableDataSet structure given a table_data definition */
TableDataSet*
TableDataset_createTableDataSet( const char* table_name )
{
    TableDataSet* table_set = MEMORY_MALLOC_TYPEDEF( TableDataSet );
    if ( !table_set )
        return NULL;
    table_set->table = TableData_createTableData( table_name );
    return table_set;
}

void TableDataset_deleteTableDataSet( TableDataSet* table_set )
{
    TableDataSetStorage *ptr, *next;
    TableRow *prow, *pnextrow;

    for ( ptr = table_set->default_row; ptr; ptr = next ) {
        next = ptr->next;
        free( ptr );
    }
    table_set->default_row = NULL;
    for ( prow = table_set->table->first_row; prow; prow = pnextrow ) {
        pnextrow = prow->next;
        TableDataset_removeAndDeleteRow( table_set, prow );
    }
    table_set->table->first_row = NULL;
    TableData_deleteTable( table_set->table );
    free( table_set );
}

/** clones a dataset row, including all data. */
TableRow*
TableDataset_cloneRow( TableRow* row )
{
    TableDataSetStorage *data, **newrowdata;
    TableRow* newrow;

    if ( !row )
        return NULL;

    newrow = TableData_cloneRow( row );
    if ( !newrow )
        return NULL;

    data = ( TableDataSetStorage* )row->data;

    if ( data ) {
        for ( newrowdata = ( TableDataSetStorage** )&( newrow->data ); data;
              newrowdata = &( ( *newrowdata )->next ), data = data->next ) {

            *newrowdata = ( TableDataSetStorage* )Memory_memdup( data,
                sizeof( TableDataSetStorage ) );
            if ( !*newrowdata ) {
                TableDataset_deleteRow( newrow );
                return NULL;
            }

            if ( data->data.voidp ) {
                ( *newrowdata )->data.voidp = Memory_memdup( data->data.voidp, data->data_len );
                if ( !( *newrowdata )->data.voidp ) {
                    TableDataset_deleteRow( newrow );
                    return NULL;
                }
            }
        }
    }
    return newrow;
}

/* ==================================
 *
 * Data Set API: Default row operations
 *
 * ================================== */

/** creates a new row from an existing defined default set */
TableRow*
TableDataset_createRowFromDefaults( TableDataSetStorage* defrow )
{
    TableRow* row;
    row = TableData_createTableDataRow();
    if ( !row )
        return NULL;
    for ( ; defrow; defrow = defrow->next ) {
        TableDataset_setRowColumn( row, defrow->column, defrow->type,
            defrow->data.voidp, defrow->data_len );
        if ( defrow->writable )
            TableDataset_markRowColumnWritable( row, defrow->column, 1 );
    }
    return row;
}

/** adds a new default row to a table_set.
 * Arguments should be the table_set, column number, variable type and
 * finally a 1 if it is allowed to be writable, or a 0 if not.  If the
 * default_value field is not NULL, it will be used to populate new
 * valuse in that column fro newly created rows. It is copied into the
 * storage template (free your calling argument).
 *
 * returns ErrorCode_SUCCESS or ErrorCode_FAILURE
 */
int TableDataset_addDefaultRow( TableDataSet* table_set,
    unsigned int column,
    int type, int writable,
    void* default_value,
    size_t default_value_len )
{
    TableDataSetStorage *new_col, *ptr, *pptr;

    if ( !table_set )
        return ErrorCode_GENERR;

    /*
     * double check
     */
    new_col = TableDataset_findColumn( table_set->default_row, column );
    if ( new_col != NULL ) {
        if ( new_col->type == type && new_col->writable == writable )
            return ErrorCode_SUCCESS;
        return ErrorCode_GENERR;
    }

    new_col = MEMORY_MALLOC_TYPEDEF( TableDataSetStorage );
    if ( new_col == NULL )
        return ErrorCode_GENERR;
    new_col->type = type;
    new_col->writable = writable;
    new_col->column = column;
    if ( default_value ) {
        new_col->data.voidp = Memory_memdup( default_value, default_value_len );
        new_col->data_len = default_value_len;
    }
    if ( table_set->default_row == NULL )
        table_set->default_row = new_col;
    else {
        /* sort in order just because (needed for add_row support) */
        for ( ptr = table_set->default_row, pptr = NULL;
              ptr;
              pptr = ptr, ptr = ptr->next ) {
            if ( ptr->column > column ) {
                new_col->next = ptr;
                if ( pptr )
                    pptr->next = new_col;
                else
                    table_set->default_row = new_col;
                return ErrorCode_SUCCESS;
            }
        }
        if ( pptr )
            pptr->next = new_col;
        else
            Logger_log( LOGGER_PRIORITY_ERR, "Shouldn't have gotten here: table_dataset/add_row" );
    }
    return ErrorCode_SUCCESS;
}

/** adds multiple data column definitions to each row.  Functionally,
 *  this is a wrapper around calling TableDataset_addDefaultRow
 *  repeatedly for you.
 */
void TableDataset_multiAddDefaultRow( TableDataSet* tset, ... )
{
    va_list debugargs;
    unsigned int column;
    int type, writable;
    void* data;
    size_t data_len;

    va_start( debugargs, tset );

    while ( ( column = va_arg( debugargs, unsigned int ) ) != 0 ) {
        type = va_arg( debugargs, int );
        writable = va_arg( debugargs, int );
        data = va_arg( debugargs, void* );
        data_len = va_arg( debugargs, size_t );
        TableDataset_addDefaultRow( tset, column, type, writable,
            data, data_len );
    }

    va_end( debugargs );
}

/* ==================================
 *
 * Data Set API: MIB maintenance
 *
 * ================================== */

/** Given a TableDataSet definition, create a handler for it */
MibHandler*
TableDataset_getTableDataSetHandler( TableDataSet* data_set )
{
    MibHandler* ret = NULL;

    if ( !data_set ) {
        Logger_log( LOGGER_PRIORITY_INFO,
            "TableDataset_getTableDataSetHandler(NULL) called\n" );
        return NULL;
    }

    ret = AgentHandler_createHandler( TABLE_DATA_SET_NAME,
        TableDataset_helperHandler );
    if ( ret ) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
        ret->myvoid = ( void* )data_set;
    }
    return ret;
}

/** register a given data_set at a given oid (specified in the
    HandlerRegistration pointer).  The
    reginfo->handler->access_method *may* be null if the call doesn't
    ever want to be called for SNMP operations.
*/
int TableDataset_registerTableDataSet( HandlerRegistration* reginfo,
    TableDataSet* data_set,
    TableRegistrationInfo* table_info )
{
    int ret;

    if ( NULL == table_info ) {
        /*
         * allocate the table if one wasn't allocated
         */
        table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
        if ( table_info == NULL )
            return PRIOT_ERR_GENERR;
    }

    if ( NULL == table_info->indexes && data_set->table->indexes_template ) {
        /*
         * copy the indexes in
         */
        table_info->indexes = Client_cloneVarbind( data_set->table->indexes_template );
    }

    if ( ( !table_info->min_column || !table_info->max_column ) && ( data_set->default_row ) ) {
        /*
         * determine min/max columns
         */
        unsigned int mincol = 0xffffffff, maxcol = 0;
        TableDataSetStorage* row;

        for ( row = data_set->default_row; row; row = row->next ) {
            mincol = UTILITIES_MIN_VALUE( mincol, row->column );
            maxcol = UTILITIES_MAX_VALUE( maxcol, row->column );
        }
        if ( !table_info->min_column )
            table_info->min_column = mincol;
        if ( !table_info->max_column )
            table_info->max_column = maxcol;
    }

    AgentHandler_injectHandler( reginfo,
        TableDataset_getTableDataSetHandler( data_set ) );
    ret = TableData_registerTableData( reginfo, data_set->table,
        table_info );
    if ( ret == ErrorCode_SUCCESS && reginfo->handler )
        Table_handlerOwnsTableInfo( reginfo->handler->next );
    return ret;
}

NewrowStash*
TableDataset_createNewrowstash( TableDataSet* datatable,
    TableRequestInfo* table_info )
{
    NewrowStash* newrowstash = NULL;
    TableRow* newrow = NULL;

    newrowstash = MEMORY_MALLOC_TYPEDEF( NewrowStash );

    if ( newrowstash != NULL ) {
        newrowstash->created = 1;
        newrow = TableDataset_createRowFromDefaults( datatable->default_row );
        newrow->indexes = Client_cloneVarbind( table_info->indexes );
        newrowstash->newrow = newrow;
    }

    return newrowstash;
}

/* implements the table data helper.  This is the routine that takes
 *  care of all SNMP requests coming into the table. */
int TableDataset_helperHandler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    TableDataSetStorage* data = NULL;
    NewrowStash* newrowstash = NULL;
    TableRow *row, *newrow = NULL;
    TableRequestInfo* table_info;
    RequestInfo* request;
    OidStashNode_t** stashp = NULL;

    if ( !handler )
        return ErrorCode_GENERR;

    DEBUG_MSGTL( ( "TableDataSet", "handler starting\n" ) );
    for ( request = requests; request; request = request->next ) {
        TableDataSet* datatable = ( TableDataSet* )handler->myvoid;
        const oid* const suffix = requests->requestvb->name + reginfo->rootoid_len + 2;
        const size_t suffix_len = requests->requestvb->nameLength - ( reginfo->rootoid_len + 2 );

        if ( request->processed )
            continue;

        /*
         * extract our stored data and table info
         */
        row = TableData_extractTableRow( request );
        table_info = Table_extractTableInfo( request );

        if ( MODE_IS_SET( reqinfo->mode ) ) {

            /*
             * use a cached copy of the row for modification
             */

            /*
             * cache location: may have been created already by other
             * SET requests in the same master request.
             */
            stashp = TableDataset_getOrCreateStash( reqinfo,
                datatable,
                table_info );
            if ( NULL == stashp ) {
                Agent_setRequestError( reqinfo, request, PRIOT_ERR_GENERR );
                continue;
            }

            newrowstash
                = ( NewrowStash* )OidStash_getData( *stashp, suffix, suffix_len );

            if ( !newrowstash ) {
                if ( !row ) {
                    if ( datatable->allow_creation ) {
                        /*
                         * entirely new row.  Create the row from the template
                         */
                        newrowstash = TableDataset_createNewrowstash(
                            datatable, table_info );
                        newrow = newrowstash->newrow;
                    } else if ( datatable->rowstatus_column == 0 ) {
                        /*
                         * A RowStatus object may be used to control the
                         *  creation of a new row.  But if this object
                         *  isn't declared (and the table isn't marked as
                         *  'auto-create'), then we can't create a new row.
                         */
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_NOCREATION );
                        continue;
                    }
                } else {
                    /*
                     * existing row that needs to be modified
                     */
                    newrowstash = MEMORY_MALLOC_TYPEDEF( NewrowStash );
                    if ( newrowstash == NULL ) {
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_GENERR );
                        continue;
                    }
                    newrow = TableDataset_cloneRow( row );
                    newrowstash->newrow = newrow;
                }
                OidStash_addData( stashp, suffix, suffix_len,
                    newrowstash );
            } else {
                newrow = newrowstash->newrow;
            }
            /*
             * all future SET data modification operations use this
             * temp pointer
             */
            if ( reqinfo->mode == MODE_SET_RESERVE1 || reqinfo->mode == MODE_SET_RESERVE2 )
                row = newrow;
        }

        if ( row )
            data = ( TableDataSetStorage* )row->data;

        if ( !row || !table_info || !data ) {
            if ( !table_info
                || !MODE_IS_SET( reqinfo->mode ) ) {
                Agent_setRequestError( reqinfo, request,
                    PRIOT_NOSUCHINSTANCE );
                continue;
            }
        }

        data = TableDataset_findColumn( data, table_info->colnum );

        switch ( reqinfo->mode ) {
        case MODE_GET:
        case MODE_GETNEXT:
        case MODE_GETBULK: /* XXXWWW */
            if ( !data || data->type == PRIOT_NOSUCHINSTANCE ) {
                Agent_setRequestError( reqinfo, request,
                    PRIOT_NOSUCHINSTANCE );
            } else {
                /*
                 * Note: data->data.voidp can be NULL, e.g. when a zero-length
                 * octet string has been stored in the table cache.
                 */
                TableData_buildResult( reginfo, reqinfo, request,
                    row,
                    table_info->colnum,
                    data->type,
                    ( u_char* )data->data.voidp,
                    data->data_len );
            }
            break;

        case MODE_SET_RESERVE1:
            if ( data ) {
                /*
                 * Can we modify the existing row?
                 */
                if ( !data->writable ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_NOTWRITABLE );
                } else if ( request->requestvb->type != data->type ) {
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGTYPE );
                }
            } else if ( datatable->rowstatus_column == table_info->colnum ) {
                /*
                 * Otherwise, this is where we create a new row using
                 * the RowStatus object (essentially duplicating the
                 * steps followed earlier in the 'allow_creation' case)
                 */
                switch ( *( request->requestvb->value.integer ) ) {
                case tcROW_STATUS_CREATEANDGO:
                case tcROW_STATUS_CREATEANDWAIT:
                    newrowstash = TableDataset_createNewrowstash(
                        datatable, table_info );
                    newrow = newrowstash->newrow;
                    row = newrow;
                    OidStash_addData( stashp, suffix, suffix_len,
                        newrowstash );
                }
            }
            break;

        case MODE_SET_RESERVE2:
            /*
             * If the agent receives a SET request for an object in a non-existant
             *  row, then the IMPL_RESERVE1 pass will create the row automatically.
             *
             * But since the row doesn't exist at that point, the test for whether
             *  the object is writable or not will be skipped.  So we need to check
             *  for this possibility again here.
             *
             * Similarly, if row creation is under the control of the RowStatus
             *  object (i.e. allow_creation == 0), but this particular request
             *  doesn't include such an object, then the row won't have been created,
             *  and the writable check will also have been skipped.  Again - check here.
             */
            if ( data && data->writable == 0 ) {
                Agent_setRequestError( reqinfo, request,
                    PRIOT_ERR_NOTWRITABLE );
                continue;
            }
            if ( datatable->rowstatus_column == table_info->colnum ) {
                switch ( *( request->requestvb->value.integer ) ) {
                case tcROW_STATUS_ACTIVE:
                case tcROW_STATUS_NOTINSERVICE:
                    /*
                     * Can only operate on pre-existing rows.
                     */
                    if ( !newrowstash || newrowstash->created ) {
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_INCONSISTENTVALUE );
                        continue;
                    }
                    break;

                case tcROW_STATUS_CREATEANDGO:
                case tcROW_STATUS_CREATEANDWAIT:
                    /*
                     * Can only operate on newly created rows.
                     */
                    if ( !( newrowstash && newrowstash->created ) ) {
                        Agent_setRequestError( reqinfo, request,
                            PRIOT_ERR_INCONSISTENTVALUE );
                        continue;
                    }
                    break;

                case tcROW_STATUS_DESTROY:
                    /*
                     * Can operate on new or pre-existing rows.
                     */
                    break;

                case tcROW_STATUS_NOTREADY:
                default:
                    /*
                     * Not a valid value to Set
                     */
                    Agent_setRequestError( reqinfo, request,
                        PRIOT_ERR_WRONGVALUE );
                    continue;
                }
            }
            if ( !data ) {
                Agent_setRequestError( reqinfo, request,
                    PRIOT_ERR_NOCREATION );
                continue;
            }

            /*
             * modify row and set new value
             */
            MEMORY_FREE( data->data.string );
            data->data.string = ( u_char* )
                Memory_strdupAndNull( request->requestvb->value.string,
                    request->requestvb->valueLength );
            if ( !data->data.string ) {
                Agent_setRequestError( reqinfo, requests,
                    PRIOT_ERR_RESOURCEUNAVAILABLE );
            }
            data->data_len = request->requestvb->valueLength;

            if ( datatable->rowstatus_column == table_info->colnum ) {
                switch ( *( request->requestvb->value.integer ) ) {
                case tcROW_STATUS_CREATEANDGO:
                    /*
                     * XXX: check legality
                     */
                    *( data->data.integer ) = tcROW_STATUS_ACTIVE;
                    break;

                case tcROW_STATUS_CREATEANDWAIT:
                    /*
                     * XXX: check legality
                     */
                    *( data->data.integer ) = tcROW_STATUS_NOTINSERVICE;
                    break;

                case tcROW_STATUS_DESTROY:
                    newrowstash->deleted = 1;
                    break;
                }
            }
            break;

        case MODE_SET_ACTION:

            /*
             * Install the new row into the stored table.
         * Do this only *once* per row ....
             */
            if ( newrowstash->state != STATE_ACTION ) {
                newrowstash->state = STATE_ACTION;
                if ( newrowstash->created ) {
                    TableDataset_addRow( datatable, newrow );
                } else {
                    TableDataset_replaceRow( datatable,
                        row, newrow );
                }
            }
            /*
             * ... but every (relevant) varbind in the request will
         * need to know about this new row, so update the
         * per-request row information regardless
             */
            if ( newrowstash->created ) {
                AgentHandler_requestAddListData( request,
                    Map_newElement( TABLE_DATA_NAME,
                                                     newrow, NULL ) );
            }
            break;

        case MODE_SET_UNDO:
            /*
             * extract the new row, replace with the old or delete
             */
            if ( newrowstash->state != STATE_UNDO ) {
                newrowstash->state = STATE_UNDO;
                if ( newrowstash->created ) {
                    TableDataset_removeAndDeleteRow( datatable, newrow );
                } else {
                    TableDataset_replaceRow( datatable,
                        newrow, row );
                    TableDataset_deleteRow( newrow );
                }
                newrow = NULL;
            }
            break;

        case MODE_SET_COMMIT:
            if ( newrowstash->state != STATE_COMMIT ) {
                newrowstash->state = STATE_COMMIT;
                if ( !newrowstash->created ) {
                    RequestInfo* req;
                    TableDataset_deleteRow( row );

                    /* Walk the request list to update the reference to the old row w/ th new one */
                    for ( req = requests; req; req = req->next ) {

                        /*
                         * For requests that have the old row values,
                         * so add the newly-created row information.
                         */
                        if ( ( TableRow* )TableData_extractTableRow( req ) == row ) {
                            AgentHandler_requestRemoveListData( req, TABLE_DATA_ROW );
                            AgentHandler_requestAddListData( req,
                                Map_newElement( TABLE_DATA_ROW, newrow, NULL ) );
                        }
                    }

                    row = NULL;
                }
                if ( newrowstash->deleted ) {
                    TableDataset_removeAndDeleteRow( datatable, newrow );
                    newrow = NULL;
                }
            }
            break;

        case MODE_SET_FREE:
            if ( newrowstash && newrowstash->state != STATE_FREE ) {
                newrowstash->state = STATE_FREE;
                TableDataset_deleteRow( newrow );
                newrow = NULL;
            }
            break;

        default:
            Logger_log( LOGGER_PRIORITY_ERR,
                "table_dataset: unknown mode passed into the handler\n" );
            return PRIOT_ERR_GENERR;
        }
    }

    /* next handler called automatically - 'AUTO_NEXT' */
    return PRIOT_ERR_NOERROR;
}

/**
 * extracts a TableDataSet pointer from a given request
 */
TableDataSet*
TableDataset_extractTableDataSet( RequestInfo* request )
{
    return ( TableDataSet* )
        AgentHandler_requestGetListData( request, TABLE_DATA_SET_NAME );
}

/**
 * extracts a TableDataSet pointer from a given request
 */
TableDataSetStorage*
TableDataset_extractTableDataSetColumn( RequestInfo* request,
    unsigned int column )
{
    TableDataSetStorage* data = ( TableDataSetStorage* )TableData_extractTableRowData( request );
    if ( data ) {
        data = TableDataset_findColumn( data, column );
    }
    return data;
}

/* ==================================
 *
 * Data Set API: Config-based operation
 *
 * ================================== */

/** registers a table_dataset so that the "addRow" priotd.conf token
  * can be used to add data to this table.  If registration_name is
  * NULL then the name used when the table was created will be used
  * instead.
  *
  * @todo create a properly free'ing registeration pointer for the
  * datalist, and get the datalist freed at shutdown.
  */
void TableDataset_registerAutoDataTable( TableDataSet* table_set,
    char* registration_name )
{
    DataSetTables* tables;
    tables = MEMORY_MALLOC_TYPEDEF( DataSetTables );
    if ( !tables )
        return;
    tables->table_set = table_set;
    if ( !registration_name ) {
        registration_name = table_set->table->name;
    }
    Map_insert( &_autoTables,
        Map_newElement( registration_name,
            tables, free ) ); /* XXX */
}

/** Undo the effect of TableDataset_registerAutoDataTable().
 */
void TableDataset_unregisterAutoDataTable( TableDataSet* table_set,
    char* registration_name )
{
    Map_erase( &_autoTables, registration_name
            ? registration_name
            : table_set->table->name );
}

static void
_TableDataset_addIndexes( TableDataSet* table_set, struct Parse_Tree_s* tp )
{
    oid name[ ASN01_MAX_OID_LEN ];
    size_t name_length = ASN01_MAX_OID_LEN;
    struct Parse_IndexList_s* index;
    struct Parse_Tree_s* indexnode;
    u_char type;
    int fixed_len = 0;

    /*
     * loop through indexes and add types
     */
    for ( index = tp->indexes; index; index = index->next ) {
        if ( !Mib_parseOid( index->ilabel, name, &name_length ) || ( NULL == ( indexnode = Mib_getTree( name, name_length, Mib_getTreeHead() ) ) ) ) {
            ReadConfig_configPwarn( "can't instatiate table since "
                                    "I don't know anything about one index" );
            Logger_log( LOGGER_PRIORITY_WARNING, "  index %s not found in tree\n",
                index->ilabel );
            return; /* xxx mem leak */
        }

        type = Mib_toAsnType( indexnode->type );
        if ( type == ( u_char )-1 ) {
            ReadConfig_configPwarn( "unknown index type" );
            return; /* xxx mem leak */
        }
        /*
         * if implied, mark it as such. also mark fixed length
         * octet strings as implied (ie no length prefix) as well.
         * */
        if ( ( PARSE_TYPE_OCTETSTR == indexnode->type ) && /* octet str */
            ( NULL != indexnode->ranges ) && /* & has range */
            ( NULL == indexnode->ranges->next ) && /*   but only one */
            ( indexnode->ranges->high == /*   & high==low */
                 indexnode->ranges->low ) ) {
            type |= ASN01_PRIVATE;
            fixed_len = indexnode->ranges->high;
        } else if ( index->isimplied )
            type |= ASN01_PRIVATE;

        DEBUG_MSGTL( ( "table_set_add_table",
            "adding default index of type %d\n", type ) );
        TableDataset_addIndex( table_set, type );

        /*
         * hack alert: for fixed length strings, save the
         * length for use during oid parsing.
         */
        if ( fixed_len ) {
            /*
             * find last (just added) index
             */
            VariableList* var = table_set->table->indexes_template;
            while ( NULL != var->next )
                var = var->next;
            var->valueLength = fixed_len;
        }
    }
}
/** @internal */
void TableDataset_configParseTableSet( const char* token, char* line )
{
    oid table_name[ ASN01_MAX_OID_LEN ];
    size_t table_name_length = ASN01_MAX_OID_LEN;
    struct Parse_Tree_s* tp;
    TableDataSet* table_set;
    DataSetTables* tables;
    unsigned int mincol = 0xffffff, maxcol = 0;
    char* pos;

    /*
     * instatiate a fake table based on MIB information
     */
    DEBUG_MSGTL( ( "9:table_set_add_table", "processing '%s'\n", line ) );
    if ( NULL != ( pos = strchr( line, ' ' ) ) ) {
        ReadConfig_configPwarn( "ignoring extra tokens on line" );
        Logger_log( LOGGER_PRIORITY_WARNING, "  ignoring '%s'\n", pos );
        *pos = '\0';
    }

    /*
     * check for duplicate table
     */
    tables = ( DataSetTables* )Map_at( _autoTables, line );
    if ( NULL != tables ) {
        ReadConfig_configPwarn( "duplicate table definition" );
        return;
    }

    /*
     * parse oid and find tree structure
     */
    if ( !Mib_parseOid( line, table_name, &table_name_length ) ) {
        ReadConfig_configPwarn( "can't instatiate table since I can't parse the table name" );
        return;
    }
    if ( NULL == ( tp = Mib_getTree( table_name, table_name_length,
                       Mib_getTreeHead() ) ) ) {
        ReadConfig_configPwarn( "can't instatiate table since "
                                "I can't find mib information about it" );
        return;
    }

    if ( NULL == ( tp = tp->child_list ) || NULL == tp->child_list ) {
        ReadConfig_configPwarn( "can't instatiate table since it doesn't appear to be "
                                "a proper table (no children)" );
        return;
    }

    table_set = TableDataset_createTableDataSet( line );

    /*
     * check for augments indexes
     */
    if ( NULL != tp->augments ) {
        oid name[ ASN01_MAX_OID_LEN ];
        size_t name_length = ASN01_MAX_OID_LEN;
        struct Parse_Tree_s* tp2;

        if ( !Mib_parseOid( tp->augments, name, &name_length ) ) {
            ReadConfig_configPwarn( "I can't parse the augment table name" );
            Logger_log( LOGGER_PRIORITY_WARNING, "  can't parse %s\n", tp->augments );
            MEMORY_FREE( table_set );
            return;
        }
        if ( NULL == ( tp2 = Mib_getTree( name, name_length, Mib_getTreeHead() ) ) ) {
            ReadConfig_configPwarn( "can't instatiate table since "
                                    "I can't find mib information about augment table" );
            Logger_log( LOGGER_PRIORITY_WARNING, "  table %s not found in tree\n",
                tp->augments );
            MEMORY_FREE( table_set );
            return;
        }
        _TableDataset_addIndexes( table_set, tp2 );
    }

    _TableDataset_addIndexes( table_set, tp );

    /*
     * loop through children and add each column info
     */
    for ( tp = tp->child_list; tp; tp = tp->next_peer ) {
        int canwrite = 0;
        u_char type;
        type = Mib_toAsnType( tp->type );
        if ( type == ( u_char )-1 ) {
            ReadConfig_configPwarn( "unknown column type" );
            MEMORY_FREE( table_set );
            return; /* xxx mem leak */
        }

        DEBUG_MSGTL( ( "table_set_add_table",
            "adding column %s(%ld) of type %d (access %d)\n",
            tp->label, tp->subid, type, tp->access ) );

        switch ( tp->access ) {
        case PARSE_MIB_ACCESS_CREATE:
            table_set->allow_creation = 1;
        /* fallthrough */
        case PARSE_MIB_ACCESS_READWRITE:
        case PARSE_MIB_ACCESS_WRITEONLY:
            canwrite = 1;
        /* fallthrough */
        case PARSE_MIB_ACCESS_READONLY:
            DEBUG_MSGTL( ( "table_set_add_table",
                "adding column %ld of type %d\n", tp->subid, type ) );
            TableDataset_addDefaultRow( table_set, tp->subid, type,
                canwrite, NULL, 0 );
            mincol = UTILITIES_MIN_VALUE( mincol, tp->subid );
            maxcol = UTILITIES_MAX_VALUE( maxcol, tp->subid );
            break;

        case PARSE_MIB_ACCESS_NOACCESS:
        case PARSE_MIB_ACCESS_NOTIFY:
            break;

        default:
            ReadConfig_configPwarn( "unknown column access type" );
            break;
        }
    }

    /*
     * register the table
     */
    TableDataset_registerTableDataSet( AgentHandler_createHandlerRegistration( line, NULL, table_name,
                                           table_name_length,
                                           HANDLER_CAN_RWRITE ),
        table_set, NULL );

    TableDataset_registerAutoDataTable( table_set, NULL );
}

/** @internal */
void TableDataset_configParseAddRow( const char* token, char* line )
{
    char buf[ UTILITIES_MAX_BUFFER_MEDIUM ];
    char tname[ UTILITIES_MAX_BUFFER_MEDIUM ];
    size_t buf_size;
    int rc;

    DataSetTables* tables;
    VariableList* vb; /* containing only types */
    TableRow* row;
    TableDataSetStorage* dr;

    line = ReadConfig_copyNword( line, tname, sizeof( tname ) );

    tables = ( DataSetTables* )Map_at( _autoTables, tname );
    if ( !tables ) {
        ReadConfig_configPwarn( "Unknown table trying to add a row" );
        return;
    }

    /*
     * do the indexes first
     */
    row = TableData_createTableDataRow();

    for ( vb = tables->table_set->table->indexes_template; vb;
          vb = vb->next ) {
        if ( !line ) {
            ReadConfig_configPwarn( "missing an index value" );
            MEMORY_FREE( row );
            return;
        }

        DEBUG_MSGTL( ( "table_set_add_row", "adding index of type %d\n",
            vb->type ) );
        buf_size = sizeof( buf );
        line = ReadConfig_readMemory( vb->type, line, buf, &buf_size );
        TableData_rowAddIndex( row, vb->type, buf, buf_size );
    }

    /*
     * then do the data
     */
    for ( dr = tables->table_set->default_row; dr; dr = dr->next ) {
        if ( !line ) {
            ReadConfig_configPwarn( "missing a data value. "
                                    "All columns must be specified." );
            Logger_log( LOGGER_PRIORITY_WARNING, "  can't find value for column %d\n",
                dr->column - 1 );
            MEMORY_FREE( row );
            return;
        }

        buf_size = sizeof( buf );
        line = ReadConfig_readMemory( dr->type, line, buf, &buf_size );
        DEBUG_MSGTL( ( "table_set_add_row",
            "adding data at column %d of type %d\n", dr->column,
            dr->type ) );
        TableDataset_setRowColumn( row, dr->column, dr->type, buf, buf_size );
        if ( dr->writable )
            TableDataset_markRowColumnWritable( row, dr->column, 1 ); /* make writable */
    }
    rc = TableData_addRow( tables->table_set->table, row );
    if ( ErrorCode_SUCCESS != rc ) {
        ReadConfig_configPwarn( "error adding table row" );
    }
    if ( NULL != line ) {
        ReadConfig_configPwarn( "extra data value. Too many columns specified." );
        Logger_log( LOGGER_PRIORITY_WARNING, "  extra data '%s'\n", line );
    }
}

OidStashNode_t**
TableDataset_getOrCreateStash( AgentRequestInfo* reqinfo,
    TableDataSet* datatable,
    TableRequestInfo* table_info )
{
    OidStashNode_t** stashp = NULL;
    char buf[ 256 ]; /* is this reasonable size?? */
    size_t len;
    int rc;

    rc = snprintf( buf, sizeof( buf ), "dataset_row_stash:%s:",
        datatable->table->name );
    if ( ( -1 == rc ) || ( ( size_t )rc >= sizeof( buf ) ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "%s handler name too long\n", datatable->table->name );
        return NULL;
    }

    len = sizeof( buf ) - rc;
    rc = Mib_snprintObjid( &buf[ rc ], len, table_info->index_oid,
        table_info->index_oid_len );
    if ( -1 == rc ) {
        Logger_log( LOGGER_PRIORITY_ERR, "%s oid or name too long\n", datatable->table->name );
        return NULL;
    }

    stashp = ( OidStashNode_t** )
        Table_getOrCreateRowStash( reqinfo, ( u_char* )buf );
    return stashp;
}

TableRow*
TableDataset_getNewrow( RequestInfo* request,
    AgentRequestInfo* reqinfo,
    int rootoid_len,
    TableDataSet* datatable,
    TableRequestInfo* table_info )
{
    oid* const suffix = request->requestvb->name + rootoid_len + 2;
    size_t suffix_len = request->requestvb->nameLength - ( rootoid_len + 2 );
    OidStashNode_t** stashp;
    NewrowStash* newrowstash;

    stashp = TableDataset_getOrCreateStash( reqinfo, datatable,
        table_info );
    if ( NULL == stashp )
        return NULL;

    newrowstash = ( NewrowStash* )OidStash_getData( *stashp, suffix, suffix_len );
    if ( NULL == newrowstash )
        return NULL;

    return newrowstash->newrow;
}

/* ==================================
 *
 * Data Set API: Row operations
 *
 * ================================== */

/** returns the first row in the table */
TableRow*
TableDataset_getFirstRow( TableDataSet* table )
{
    return TableData_getFirstRow( table->table );
}

/** returns the next row in the table */
TableRow*
TableDataset_getNextRow( TableDataSet* table,
    TableRow* row )
{
    return TableData_getNextRow( table->table, row );
}

int TableDataset_numRows( TableDataSet* table )
{
    if ( !table )
        return 0;
    return TableData_numRows( table->table );
}

/* ==================================
 *
 * Data Set API: Column operations
 *
 * ================================== */

/** Finds a column within a given storage set, given the pointer to
   the start of the storage set list.
*/
TableDataSetStorage*
TableDataset_findColumn( TableDataSetStorage* start,
    unsigned int column )
{
    while ( start && start->column != column )
        start = start->next;
    return start;
}

/**
 * marks a given column in a row as writable or not.
 */
int TableDataset_markRowColumnWritable( TableRow* row, int column,
    int writable )
{
    TableDataSetStorage* data;

    if ( !row )
        return ErrorCode_GENERR;

    data = ( TableDataSetStorage* )row->data;
    data = TableDataset_findColumn( data, column );

    if ( !data ) {
        /*
         * create it
         */
        data = MEMORY_MALLOC_TYPEDEF( TableDataSetStorage );
        if ( !data ) {
            Logger_log( LOGGER_PRIORITY_CRIT, "no memory in TableDataset_setRowColumn" );
            return ErrorCode_MALLOC;
        }
        data->column = column;
        data->writable = writable;
        data->next = ( struct TableDataSetStorage_s* )row->data;
        row->data = data;
    } else {
        data->writable = writable;
    }
    return ErrorCode_SUCCESS;
}

/**
 * Sets a given column in a row with data given a type, value,
 * and length. Data is memdup'ed by the function, at least if
 * type != PRIOT_NOSUCHINSTANCE and if value_len > 0.
 *
 * @param[in] row       Pointer to the row to be modified.
 * @param[in] column    Index of the column to be modified.
 * @param[in] type      Either the ASN type of the value to be set or
 *   PRIOT_NOSUCHINSTANCE.
 * @param[in] value     If type != PRIOT_NOSUCHINSTANCE, pointer to the
 *   new value. May be NULL if value_len == 0, e.g. when storing a
 *   zero-length octet string. Ignored when type == PRIOT_NOSUCHINSTANCE.
 * @param[in] value_len If type != PRIOT_NOSUCHINSTANCE, number of bytes
 *   occupied by *value. Ignored when type == PRIOT_NOSUCHINSTANCE.
 *
 * @return ErrorCode_SUCCESS upon success; ErrorCode_MALLOC when out of memory;
 *   or ErrorCode_GENERR when row == 0 or when type does not match the datatype
 *   of the data stored in *row.
 *
 */
int TableDataset_setRowColumn( TableRow* row, unsigned int column,
    int type, const void* value, size_t value_len )
{
    TableDataSetStorage* data;

    if ( !row )
        return ErrorCode_GENERR;

    data = ( TableDataSetStorage* )row->data;
    data = TableDataset_findColumn( data, column );

    if ( !data ) {
        /*
         * create it
         */
        data = MEMORY_MALLOC_TYPEDEF( TableDataSetStorage );
        if ( !data ) {
            Logger_log( LOGGER_PRIORITY_CRIT, "no memory in TableDataset_setRowColumn" );
            return ErrorCode_MALLOC;
        }

        data->column = column;
        data->type = type;
        data->next = ( struct TableDataSetStorage_s* )row->data;
        row->data = data;
    }

    /* Transitions from / to PRIOT_NOSUCHINSTANCE are allowed, but no other transitions. */
    if ( data->type != type && data->type != PRIOT_NOSUCHINSTANCE
        && type != PRIOT_NOSUCHINSTANCE )
        return ErrorCode_GENERR;

    /* Return now if neither the type nor the data itself has been modified. */
    if ( data->type == type && data->data_len == value_len
        && ( value == NULL || memcmp( &data->data.string, value, value_len ) == 0 ) )
        return ErrorCode_SUCCESS;

    /* Reallocate memory and store the new value. */
    data->data.voidp = realloc( data->data.voidp, value ? value_len : 0 );
    if ( value && value_len && !data->data.voidp ) {
        data->data_len = 0;
        data->type = PRIOT_NOSUCHINSTANCE;
        Logger_log( LOGGER_PRIORITY_CRIT, "no memory in TableDataset_setRowColumn" );
        return ErrorCode_MALLOC;
    }
    if ( value && value_len )
        memcpy( data->data.string, value, value_len );
    data->type = type;
    data->data_len = value_len;
    return ErrorCode_SUCCESS;
}

/* ==================================
 *
 * Data Set API: Index operations
 *
 * ================================== */

/** adds an index to the table.  Call this repeatly for each index. */
void TableDataset_addIndex( TableDataSet* table, u_char type )
{
    if ( !table )
        return;
    TableData_addIndex( table->table, type );
}

/** adds multiple indexes to a table_dataset helper object.
 *  To end the list, use a 0 after the list of ASN index types. */
void TableDataset_addIndexes( TableDataSet* tset,
    ... )
{
    va_list debugargs;
    int type;

    va_start( debugargs, tset );

    if ( tset )
        while ( ( type = va_arg( debugargs, int ) ) != 0 )
            TableData_addIndex( tset->table, ( u_char )type );

    va_end( debugargs );
}
