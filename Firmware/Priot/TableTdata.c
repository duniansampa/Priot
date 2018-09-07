#include "TableTdata.h"
#include "Mib.h"
#include "Client.h"
#include "Api.h"
#include "System/Util/Logger.h"
#include "System/Util/Trace.h"
#include "System/Containers/Container.h"
#include "System/Util/Assert.h"
#include "TableContainer.h"

/** @defgroup tdata tdata
 *  Implement a table with datamatted storage.
 *  @ingroup table
 *
 *  This helper helps you implement a table where all the rows are
 *  expected to be stored within the agent itself and not in some
 *  external storage location.  It can be used to store a list of
 *  rows, where a row consists of the indexes to the table and a
 *  generic data pointer.  You can then implement a subhandler which
 *  is passed the exact row definition and data it must return data
 *  for or accept data for.  Complex GETNEXT handling is greatly
 *  simplified in this case.
 *
 *  @{
 */

/* ==================================
 *
 * TData API: Table maintenance
 *
 * ================================== */

/*
 * generates the index portion of an table oid from a varlist.
 */
void
TableTdata_generateIndexOid(TdataRow *row)
{
    Mib_buildOid(&row->oid_index.oids, &row->oid_index.len, NULL, 0, row->indexes);
}

/** creates and returns a 'tdata' table data structure */
Tdata *
TableTdata_createTable(const char *name, long flags)
{
    Tdata *table = MEMORY_MALLOC_TYPEDEF(Tdata);
    if ( !table )
        return NULL;

    table->flags = flags;
    if (name)
        table->name = strdup(name);

    if (!(table->flags & TDATA_FLAG_NO_CONTAINER)) {
        table->container = Container_find( name );
        if (!table->container)
            table->container = Container_find( "tableContainer" );
        if (table->container)
            table->container->containerName = strdup(name);
    }
    return table;
}

/** creates and returns a 'tdata' table data structure */
void
TableTdata_deleteTable(Tdata *table)
{
    if (!table)
       return;

    if (table->name)
       free(table->name);
    if (table->container)
       CONTAINER_FREE(table->container);

    MEMORY_FREE(table);
    return;
}

/** creates and returns a pointer to new row data structure */
TdataRow *
TableTdata_createRow(void)
{
    TdataRow *row = MEMORY_MALLOC_TYPEDEF(TdataRow);
    return row;
}


TdataRow *
TableTdata_cloneRow(TdataRow *row)
{
    TdataRow *newrow = NULL;
    if (!row)
        return NULL;

    newrow = (TdataRow *)Memory_memdup(row, sizeof(TdataRow));
    if (!newrow)
        return NULL;

    if (row->indexes) {
        newrow->indexes = Client_cloneVarbind(newrow->indexes);
        if (!newrow->indexes) {
            MEMORY_FREE(newrow);
            return NULL;
        }
    }

    if (row->oid_index.oids) {
        newrow->oid_index.oids =
            Api_duplicateObjid(row->oid_index.oids, row->oid_index.len);
        if (!newrow->oid_index.oids) {
            if (newrow->indexes)
                Api_freeVarbind(newrow->indexes);
            MEMORY_FREE(newrow);
            return NULL;
        }
    }

    return newrow;
}

int
TableTdata_copyRow(TdataRow *dst_row, TdataRow *src_row)
{
     if ( !src_row || !dst_row )
         return -1;

    memcpy((u_char *) dst_row, (u_char *) src_row,
           sizeof(TdataRow));
    if (src_row->indexes) {
        dst_row->indexes = Client_cloneVarbind(src_row->indexes);
        if (!dst_row->indexes)
            return -1;
    }

    if (src_row->oid_index.oids) {
        dst_row->oid_index.oids = Api_duplicateObjid(src_row->oid_index.oids,
                                                       src_row->oid_index.len);
        if (!dst_row->oid_index.oids)
            return -1;
    }

    return 0;
}

/** deletes the memory used by the specified row
 *  returns the table-specific entry data
 *  (that it doesn't know how to delete) */
void           *
TableTdata_deleteRow(TdataRow *row)
{
    void           *data;

    if (!row)
        return NULL;

    /*
     * free the memory we can
     */
    if (row->indexes)
        Api_freeVarbind(row->indexes);
    MEMORY_FREE(row->oid_index.oids);
    data = row->data;
    free(row);

    /*
     * return the void * pointer
     */
    return data;
}

/**
 * Adds a row to the given table (stored in proper lexographical order).
 *
 * returns ErrorCode_SUCCESS on successful addition.
 *      or ErrorCode_GENERR  on failure (E.G., indexes already existed)
 */
int
TableTdata_addRow(Tdata     *table,
                      TdataRow *row)
{
    if (!row || !table)
        return ErrorCode_GENERR;

    if (row->indexes)
        TableTdata_generateIndexOid(row);

    if (!row->oid_index.oids) {
        Logger_log(LOGGER_PRIORITY_ERR,
                 "illegal data attempted to be added to table %s (no index)\n",
                 table->name);
        return ErrorCode_GENERR;
    }

    /*
     * The individual index values probably won't be needed,
     *    so this memory can be released.
     * Note that this is purely internal to the helper.
     * The calling application can set this flag as
     *    a hint to the helper that these values aren't
     *    required, but it's up to the helper as to
     *    whether it takes any notice or not!
     */
    if (table->flags & TDATA_FLAG_NO_STORE_INDEXES) {
        Api_freeVarbind(row->indexes);
        row->indexes = NULL;
    }

    /*
     * add this row to the stored table
     */
    if (CONTAINER_INSERT( table->container, row ) != 0)
        return ErrorCode_GENERR;

    DEBUG_MSGTL(("tdata_add_row", "added row (%p)\n", row));

    return ErrorCode_SUCCESS;
}

void
TableTdata_replaceRow(Tdata *table,
                               TdataRow *origrow,
                               TdataRow *newrow)
{
    TableTdata_removeRow(table, origrow);
    TableTdata_addRow(table, newrow);
}

/**
 * removes a row from the given table and returns it (no free's called)
 *
 * returns the row pointer itself on successful removing.
 *      or NULL on failure (bad arguments)
 */
TdataRow *
TableTdata_removeRow( Tdata *table,
                          TdataRow *row)
{
    if (!row || !table)
        return NULL;

    CONTAINER_REMOVE( table->container, row );
    return row;
}

/**
 * removes and frees a row of the given table and
 *  returns the table-specific entry data
 *
 * returns the void * pointer on successful deletion.
 *      or NULL on failure (bad arguments)
 */
void           *
TableTdata_removeAndDeleteRow(Tdata     *table,
                                    TdataRow *row)
{
    if (!row || !table)
        return NULL;

    /*
     * remove it from the list
     */
    TableTdata_removeRow(table, row);
    return TableTdata_deleteRow(row);
}


/* ==================================
 *
 * TData API: MIB maintenance
 *
 * ================================== */

NodeHandlerFT TableTdata_helperHandler;

/** Creates a tdata handler and returns it */
MibHandler *
TableTdata_getTdataHandler(Tdata *table)
{
    MibHandler *ret = NULL;

    if (!table) {
        Logger_log(LOGGER_PRIORITY_INFO,
                 "TableTdata_getTdataHandler(NULL) called\n");
        return NULL;
    }

    ret = AgentHandler_createHandler(TABLE_TDATA_NAME,
                               TableTdata_helperHandler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
        ret->myvoid = (void *) table;
    }
    return ret;
}

/*
 * The helper handler that takes care of passing a specific row of
 * data down to the lower handler(s).  The table_container helper
 * has already taken care of identifying the appropriate row of the
 * table (and converting GETNEXT requests into an equivalent GET request)
 * So all we need to do here is make sure that the row is accessible
 * using tdata-style retrieval techniques as well.
 */
int
TableTdata_helperHandler(MibHandler *handler,
                                  HandlerRegistration *reginfo,
                                  AgentRequestInfo *reqinfo,
                                  RequestInfo *requests)
{
    Tdata *table = (Tdata *) handler->myvoid;
    RequestInfo       *request;
    TableRequestInfo *table_info;
    TdataRow          *row;
    int                         need_processing = 1;

    switch ( reqinfo->mode ) {
    case MODE_GET:
        need_processing = 0; /* only need processing if some vars found */
        /** Fall through */

    case MODE_SET_RESERVE1:

        for (request = requests; request; request = request->next) {
            if (request->processed)
                continue;

            table_info = Table_extractTableInfo(request);
            if (!table_info) {
                Assert_assert(table_info); /* yes, this will always hit */
                Agent_setRequestError(reqinfo, request, PRIOT_ERR_GENERR);
                continue;           /* eek */
            }
            row = (TdataRow*)TableContainer_rowExtract( request );
            if (!row && (reqinfo->mode == MODE_GET)) {
                Assert_assert(row); /* yes, this will always hit */
                Agent_setRequestError(reqinfo, request, PRIOT_ERR_GENERR);
                continue;           /* eek */
            }
            ++need_processing;
            AgentHandler_requestAddListData(request,
                                      Map_newElement(
                                          TABLE_TDATA_TABLE, table, NULL));
            AgentHandler_requestAddListData(request,
                                      Map_newElement(
                                          TABLE_TDATA_ROW,   row,   NULL));
        }
        /** skip next handler if processing not needed */
        if (!need_processing)
            handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
    }

    /* next handler called automatically - 'AUTO_NEXT' */
    return PRIOT_ERR_NOERROR;
}


/** registers a tdata-based MIB table */
int
TableTdata_register(HandlerRegistration    *reginfo,
                       Tdata                   *table,
                       TableRegistrationInfo *table_info)
{
    AgentHandler_injectHandler(reginfo, TableTdata_getTdataHandler(table));
    return TableContainer_register(reginfo, table_info,
                  table->container, TABLE_CONTAINER_KEY_NETSNMP_INDEX);
}


int
TableTdata_unregister(HandlerRegistration    *reginfo)
{
    /* free table; */
    return TableContainer_unregister(reginfo);
}

/** extracts the tdata table from the request structure */
Tdata *
TableTdata_extractTable(RequestInfo *request)
{
    return (Tdata *) AgentHandler_requestGetListData(request,
                                                           TABLE_TDATA_TABLE);
}

/** extracts the tdata container from the request structure */
Container_Container *
TableTdata_extractContainer(RequestInfo *request)
{
    Tdata *tdata = (Tdata*)
        AgentHandler_requestGetListData(request, TABLE_TDATA_TABLE);
    return ( tdata ? tdata->container : NULL );
}

/** extracts the tdata row being accessed from the request structure */
TdataRow *
TableTdata_extractRow(RequestInfo *request)
{
    return (TdataRow *) TableContainer_rowExtract(request);
}

/** extracts the (table-specific) entry being accessed from the
 *  request structure */
void           *
TableTdata_extractEntry(RequestInfo *request)
{
    TdataRow *row =
        (TdataRow *) TableTdata_extractRow(request);
    if (row)
        return row->data;
    else
        return NULL;
}

/** inserts a newly created tdata row into a request */
 void
TableTdata_insertTdataRow(RequestInfo *request,
                         TdataRow *row)
{
    TableContainer_rowInsert(request, (Types_Index *)row);
}

/** inserts a newly created tdata row into a request */
 void
TableTdata_removeTdataRow(RequestInfo *request,
                         TdataRow *row)
{
    TableContainer_rowRemove(request, (Types_Index *)row);
}


/* ==================================
 *
 * Generic API: Row operations
 *
 * ================================== */

/** returns the (table-specific) entry data for a given row */
void *
TableTdata_rowEntry( TdataRow *row )
{
    if (row)
        return row->data;
    else
        return NULL;
}

/** returns the first row in the table */
TdataRow *
TableTdata_rowFirst(Tdata *table)
{
    return (TdataRow *)CONTAINER_FIRST( table->container );
}

/** finds a row in the 'tdata' table given another row */
TdataRow *
TableTdata_rowGet(  Tdata     *table,
                        TdataRow *row)
{
    return (TdataRow*)CONTAINER_FIND( table->container, row );
}

/** returns the next row in the table */
TdataRow *
TableTdata_rowNext( Tdata      *table,
                        TdataRow  *row)
{
    return (TdataRow *)CONTAINER_NEXT( table->container, row  );
}

/** finds a row in the 'tdata' table given the index values */
TdataRow *
TableTdata_rowGetByidx(Tdata         *table,
                            VariableList *indexes)
{
    oid             searchfor[      asnMAX_OID_LEN];
    size_t          searchfor_len = asnMAX_OID_LEN;

    Mib_buildOidNoalloc(searchfor, asnMAX_OID_LEN, &searchfor_len, NULL, 0,
                      indexes);
    return TableTdata_rowGetByoid(table, searchfor, searchfor_len);
}

/** finds a row in the 'tdata' table given the index OID */
TdataRow *
TableTdata_rowGetByoid(Tdata *table,
                            oid * searchfor, size_t searchfor_len)
{
    Types_Index index;
    if (!table)
        return NULL;

    index.oids = searchfor;
    index.len  = searchfor_len;
    return (TdataRow*)CONTAINER_FIND( table->container, &index );
}

/** finds the lexically next row in the 'tdata' table
    given the index values */
TdataRow *
TableTdata_rowNextByidx(Tdata         *table,
                             VariableList *indexes)
{
    oid             searchfor[      asnMAX_OID_LEN];
    size_t          searchfor_len = asnMAX_OID_LEN;

    Mib_buildOidNoalloc(searchfor, asnMAX_OID_LEN, &searchfor_len, NULL, 0,
                      indexes);
    return TableTdata_rowNextByoid(table, searchfor, searchfor_len);
}

/** finds the lexically next row in the 'tdata' table
    given the index OID */
TdataRow *
TableTdata_rowNextByoid(Tdata *table,
                             oid * searchfor, size_t searchfor_len)
{
    Types_Index index;
    if (!table)
        return NULL;

    index.oids = searchfor;
    index.len  = searchfor_len;
    return (TdataRow*)CONTAINER_NEXT( table->container, &index );
}

int
TableTdata_rowCount(Tdata *table)
{
    if (!table)
        return 0;
    return CONTAINER_SIZE( table->container );
}

/* ==================================
 *
 * Generic API: Index operations on a 'tdata' table
 *
 * ================================== */


/** compare a row with the given index values */
int
TableTdata_compareIdx(TdataRow     *row,
                          VariableList *indexes)
{
    oid             searchfor[      asnMAX_OID_LEN];
    size_t          searchfor_len = asnMAX_OID_LEN;

    Mib_buildOidNoalloc(searchfor, asnMAX_OID_LEN, &searchfor_len, NULL, 0,
                      indexes);
    return TableTdata_compareOid(row, searchfor, searchfor_len);
}

/** compare a row with the given index OID */
int
TableTdata_compareOid(TdataRow     *row,
                          oid * compareto, size_t compareto_len)
{
    Types_Index *index = (Types_Index *)row;
    return Api_oidCompare( index->oids, index->len,
                             compareto,   compareto_len);
}

int
TableTdata_compareSubtreeIdx(TdataRow     *row,
                                  VariableList *indexes)
{
    oid             searchfor[      asnMAX_OID_LEN];
    size_t          searchfor_len = asnMAX_OID_LEN;

    Mib_buildOidNoalloc(searchfor, asnMAX_OID_LEN, &searchfor_len, NULL, 0,
                      indexes);
    return TableTdata_compareSubtreeOid(row, searchfor, searchfor_len);
}

int
TableTdata_compareSubtreeOid(TdataRow     *row,
                                  oid * compareto, size_t compareto_len)
{
    Types_Index *index = (Types_Index *)row;
    return Api_oidtreeCompare( index->oids, index->len,
                                 compareto,   compareto_len);
}
