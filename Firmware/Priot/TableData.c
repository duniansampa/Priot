#include "TableData.h"
#include "System/Util/Trace.h"
#include "Client.h"
#include "System/Util/Logger.h"
#include "ReadOnly.h"
#include "Mib.h"


/** @defgroup table_data table_data
 *  Helps you implement a table with datamatted storage.
 *  @ingroup table
 *
 *  This helper is obsolete.  If you are writing a new module, please
 *  consider using the table_tdata helper instead.
 *
 *  This helper helps you implement a table where all the indexes are
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
 * Table Data API: Table maintenance
 *
 * ================================== */

/*
 * generates the index portion of an table oid from a varlist.
 */
void
TableData_generateIndexOid(TableRow *row)
{
    Mib_buildOid(&row->index_oid, &row->index_oid_len, NULL, 0, row->indexes);
}

/** creates and returns a pointer to table data set */
TableData *
TableData_createTableData(const char *name)
{
    TableData *table = MEMORY_MALLOC_TYPEDEF(TableData);
    if (name && table)
        table->name = strdup(name);
    return table;
}

/** creates and returns a pointer to table data set */
TableRow *
TableData_createTableDataRow(void)
{
    TableRow *row = MEMORY_MALLOC_TYPEDEF(TableRow);
    return row;
}

/** clones a data row. DOES NOT CLONE THE CONTAINED DATA. */
TableRow *
TableData_cloneRow(TableRow *row)
{
    TableRow *newrow = NULL;
    if (!row)
        return NULL;

    newrow = (TableRow *) Memory_memdup(row, sizeof(TableRow));
    if (!newrow)
        return NULL;

    if (row->indexes) {
        newrow->indexes = Client_cloneVarbind(newrow->indexes);
        if (!newrow->indexes) {
            free(newrow);
            return NULL;
        }
    }

    if (row->index_oid) {
        newrow->index_oid =
            Api_duplicateObjid(row->index_oid, row->index_oid_len);
        if (!newrow->index_oid) {
            free(newrow->indexes);
            free(newrow);
            return NULL;
        }
    }

    return newrow;
}

/** deletes a row's memory.
 *  returns the void data that it doesn't know how to delete. */
void           *
TableData_deleteRow(TableRow *row)
{
    void           *data;

    if (!row)
        return NULL;

    /*
     * free the memory we can
     */
    if (row->indexes)
        Api_freeVarbind(row->indexes);
    MEMORY_FREE(row->index_oid);
    data = row->data;
    free(row);

    /*
     * return the void * pointer
     */
    return data;
}

/**
 * Adds a row of data to a given table (stored in proper lexographical order).
 *
 * returns ErrorCode_SUCCESS on successful addition.
 *      or ErrorCode_GENERR  on failure (E.G., indexes already existed)
 */
int
TableData_addRow(TableData *table,
                           TableRow *row)
{
    int rc, dup = 0;
    TableRow *nextrow = NULL, *prevrow;

    if (!row || !table)
        return ErrorCode_GENERR;

    if (row->indexes)
        TableData_generateIndexOid(row);

    /*
     * we don't store the index info as it
     * takes up memory.
     */
    if (!table->store_indexes) {
        Api_freeVarbind(row->indexes);
        row->indexes = NULL;
    }

    if (!row->index_oid) {
        Logger_log(LOGGER_PRIORITY_ERR,
                 "illegal data attempted to be added to table %s (no index)\n",
                 table->name);
        return ErrorCode_GENERR;
    }

    /*
     * check for simple append
     */
    if ((prevrow = table->last_row) != NULL) {
        rc = Api_oidCompare(prevrow->index_oid, prevrow->index_oid_len,
                              row->index_oid, row->index_oid_len);
        if (0 == rc)
            dup = 1;
    }
    else
        rc = 1;

    /*
     * if no last row, or newrow < last row, search the table and
     * insert it into the table in the proper oid-lexographical order
     */
    if (rc > 0) {
        for (nextrow = table->first_row, prevrow = NULL;
             nextrow != NULL; prevrow = nextrow, nextrow = nextrow->next) {
            if (NULL == nextrow->index_oid) {
                DEBUG_MSGT(("table_data_add_data", "row doesn't have index!\n"));
                /** xxx-rks: remove invalid row? */
                continue;
            }
            rc = Api_oidCompare(nextrow->index_oid, nextrow->index_oid_len,
                                  row->index_oid, row->index_oid_len);
            if(rc > 0)
                break;
            if (0 == rc) {
                dup = 1;
                break;
            }
        }
    }

    if (dup) {
        /*
         * exact match.  Duplicate entries illegal
         */
        Logger_log(LOGGER_PRIORITY_WARNING,
                 "duplicate table data attempted to be entered. row exists\n");
        return ErrorCode_GENERR;
    }

    /*
     * ok, we have the location of where it should go
     */
    /*
     * (after prevrow, and before nextrow)
     */
    row->next = nextrow;
    row->prev = prevrow;

    if (row->next)
        row->next->prev = row;

    if (row->prev)
        row->prev->next = row;

    if (NULL == row->prev)      /* it's the (new) first row */
        table->first_row = row;
    if (NULL == row->next)      /* it's the last row */
        table->last_row = row;

    DEBUG_MSGTL(("table_data_add_data", "added something...\n"));

    return ErrorCode_SUCCESS;
}

/** swaps out origrow with newrow.  This does *not* delete/free anything! */
void
TableData_replaceRow(TableData *table,
                               TableRow *origrow,
                               TableRow *newrow)
{
    TableData_removeRow(table, origrow);
    TableData_addRow(table, newrow);
}

/**
 * removes a row of data to a given table and returns it (no free's called)
 *
 * returns the row pointer itself on successful removing.
 *      or NULL on failure (bad arguments)
 */
TableRow *
TableData_removeRow(TableData *table,
                              TableRow *row)
{
    if (!row || !table)
        return NULL;

    if (row->prev)
        row->prev->next = row->next;
    else
        table->first_row = row->next;

    if (row->next)
        row->next->prev = row->prev;
    else
        table->last_row = row->prev;

    return row;
}

/**
 * removes and frees a row of data to a given table and returns the void *
 *
 * returns the void * data on successful deletion.
 *      or NULL on failure (bad arguments)
 */
void           *
TableData_removeAndDeleteRow(TableData *table,
                                         TableRow *row)
{
    if (!row || !table)
        return NULL;

    /*
     * remove it from the list
     */
    TableData_removeRow(table, row);
    return TableData_deleteRow(row);
}

    /* =====================================
     * Generic API - mostly renamed wrappers
     * ===================================== */

TableData *
TableData_createTable(const char *name, long flags)
{
    return TableData_createTableData( name );
}

void
TableData_deleteTable( TableData *table )
{
    TableRow *row, *nextrow;

    if (!table)
        return;

    Api_freeVarbind(table->indexes_template);
    table->indexes_template = NULL;

    for (row = table->first_row; row; row=nextrow) {
        nextrow   = row->next;
        row->next = NULL;
        TableData_deleteRow(row);
        /* Can't delete table-specific entry memory */
    }
    table->first_row = NULL;

    MEMORY_FREE(table->name);
    MEMORY_FREE(table);
    return;
}

TableRow *
TableData_createRow( void* entry )
{
    TableRow *row = MEMORY_MALLOC_TYPEDEF(TableRow);
    if (row)
        row->data = entry;
    return row;
}

    /* TableData_cloneRow() defined above */

int
TableData_copyRow( TableRow  *old_row,
                             TableRow  *new_row )
{
    if (!old_row || !new_row)
        return -1;

    memcpy(new_row, old_row, sizeof(TableRow));

    if (old_row->indexes)
        new_row->indexes = Client_cloneVarbind(old_row->indexes);
    if (old_row->index_oid)
        new_row->index_oid =
            Api_duplicateObjid(old_row->index_oid, old_row->index_oid_len);
    /* XXX - Doesn't copy table-specific row structure */
    return 0;
}

    /*
     * TableData_deleteRow()
     * TableData_addRow()
     * TableData_replaceRow()
     * TableData_removeRow()
     *     all defined above
     */

void *
TableData_removeDeleteRow(TableData *table,
                                     TableRow *row)
{
    return TableData_removeAndDeleteRow(table, row);
}


/* ==================================
 *
 * Table Data API: MIB maintenance
 *
 * ================================== */

/** Creates a table_data handler and returns it */
MibHandler *
TableData_getTableDataHandler(TableData *table)
{
    MibHandler *ret = NULL;

    if (!table) {
        Logger_log(LOGGER_PRIORITY_INFO,
                 "TableData_getTableDataHandler(NULL) called\n");
        return NULL;
    }

    ret =
        AgentHandler_createHandler(TABLE_DATA_NAME,
                               TableData_helperHandler);
    if (ret) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
        ret->myvoid = (void *) table;
    }
    return ret;
}

/** registers a handler as a data table.
 *  If table_info != NULL, it registers it as a normal table too. */
int
TableData_registerTableData(HandlerRegistration *reginfo,
                            TableData *table,
                            TableRegistrationInfo *table_info)
{
    AgentHandler_injectHandler(reginfo, TableData_getTableDataHandler(table));
    return Table_registerTable(reginfo, table_info);
}


/** registers a handler as a read-only data table
 *  If table_info != NULL, it registers it as a normal table too. */
int
TableData_registerReadOnlyTableData(HandlerRegistration *reginfo,
                                      TableData *table,
                                      TableRegistrationInfo *table_info)
{
    AgentHandler_injectHandler(reginfo, ReadOnly_getReadOnlyHandler());
    return TableData_registerTableData(reginfo, table, table_info);
}

int
TableData_unregisterTableData(HandlerRegistration *reginfo)
{
    /* free table; */
    return Table_unregisterTable(reginfo);
}

/*
 * The helper handler that takes care of passing a specific row of
 * data down to the lower handler(s).  It sets request->processed if
 * the request should not be handled.
 */
int
TableData_helperHandler(MibHandler *handler,
                                  HandlerRegistration *reginfo,
                                  AgentRequestInfo *reqinfo,
                                  RequestInfo *requests)
{
    TableData *table = (TableData *) handler->myvoid;
    RequestInfo *request;
    int             valid_request = 0;
    TableRow *row;
    TableRequestInfo *table_info;
    TableRegistrationInfo *table_reg_info =
        Table_findTableRegistrationInfo(reginfo);
    int             result, regresult;
    int             oldmode;

    for (request = requests; request; request = request->next) {
        if (request->processed)
            continue;

        table_info = Table_extractTableInfo(request);
        if (!table_info)
            continue;           /* ack */
        switch (reqinfo->mode) {
        case MODE_GET:
        case MODE_GETNEXT:
        case MODE_SET_RESERVE1:
            AgentHandler_requestAddListData(request,
                                      Map_newElement(
                                          TABLE_DATA_TABLE, table, NULL));
        }

        /*
         * find the row in question
         */
        switch (reqinfo->mode) {
        case MODE_GETNEXT:
        case MODE_GETBULK:     /* XXXWWW */
            if (request->requestvb->type != ASN01_NULL)
                continue;
            /*
             * loop through data till we find the next row
             */
            result = Api_oidCompare(request->requestvb->name,
                                      request->requestvb->nameLength,
                                      reginfo->rootoid,
                                      reginfo->rootoid_len);
            regresult = Api_oidCompare(request->requestvb->name,
                                         UTILITIES_MIN_VALUE(request->requestvb->
                                                  nameLength,
                                                  reginfo->rootoid_len),
                                         reginfo->rootoid,
                                         reginfo->rootoid_len);
            if (regresult == 0
                && request->requestvb->nameLength < reginfo->rootoid_len)
                regresult = -1;

            if (result < 0 || 0 == result) {
                /*
                 * before us entirely, return the first
                 */
                row = table->first_row;
                table_info->colnum = table_reg_info->min_column;
            } else if (regresult == 0 && request->requestvb->nameLength ==
                       reginfo->rootoid_len + 1 &&
                       /* entry node must be 1, but any column is ok */
                       request->requestvb->name[reginfo->rootoid_len] == 1) {
                /*
                 * exactly to the entry
                 */
                row = table->first_row;
                table_info->colnum = table_reg_info->min_column;
            } else if (regresult == 0 && request->requestvb->nameLength ==
                       reginfo->rootoid_len + 2 &&
                       /* entry node must be 1, but any column is ok */
                       request->requestvb->name[reginfo->rootoid_len] == 1) {
                /*
                 * exactly to the column
                 */
                row = table->first_row;
            } else {
                /*
                 * loop through all rows looking for the first one
                 * that is equal to the request or greater than it
                 */
                for (row = table->first_row; row; row = row->next) {
                    /*
                     * compare the index of the request to the row
                     */
                    result =
                        Api_oidCompare(row->index_oid,
                                         row->index_oid_len,
                                         request->requestvb->name + 2 +
                                         reginfo->rootoid_len,
                                         request->requestvb->nameLength -
                                         2 - reginfo->rootoid_len);
                    if (result == 0) {
                        /*
                         * equal match, return the next row
                         */
                        row = row->next;
                        break;
                    } else if (result > 0) {
                        /*
                         * the current row is greater than the
                         * request, use it
                         */
                        break;
                    }
                }
            }
            if (!row) {
                table_info->colnum++;
                if (table_info->colnum <= table_reg_info->max_column) {
                    row = table->first_row;
                }
            }
            if (row) {
                valid_request = 1;
                AgentHandler_requestAddListData(request,
                                              Map_newElement
                                              (TABLE_DATA_ROW, row,
                                               NULL));
                /*
                 * Set the name appropriately, so we can pass this
                 *  request on as a simple GET request
                 */
                TableData_buildResult(reginfo, reqinfo, request,
                                                row,
                                                table_info->colnum,
                                                ASN01_NULL, NULL, 0);
            } else {            /* no decent result found.  Give up. It's beyond us. */
                request->processed = 1;
            }
            break;

        case MODE_GET:
            if (request->requestvb->type != ASN01_NULL)
                continue;
            /*
             * find the row in question
             */
            if (request->requestvb->nameLength < (reginfo->rootoid_len + 3)) { /* table.entry.column... */
                /*
                 * request too short
                 */
                Agent_setRequestError(reqinfo, request,
                                          PRIOT_NOSUCHINSTANCE);
                break;
            } else if (NULL ==
                       (row =
                        TableData_getFromOid(table,
                                                        request->
                                                        requestvb->name +
                                                        reginfo->
                                                        rootoid_len + 2,
                                                        request->
                                                        requestvb->
                                                        nameLength -
                                                        reginfo->
                                                        rootoid_len -
                                                        2))) {
                /*
                 * no such row
                 */
                Agent_setRequestError(reqinfo, request,
                                          PRIOT_NOSUCHINSTANCE);
                break;
            } else {
                valid_request = 1;
                AgentHandler_requestAddListData(request,
                                              Map_newElement
                                              (TABLE_DATA_ROW, row,
                                               NULL));
            }
            break;

        case MODE_SET_RESERVE1:
            valid_request = 1;
            if (NULL !=
                (row =
                 TableData_getFromOid(table,
                                                 request->requestvb->name +
                                                 reginfo->rootoid_len + 2,
                                                 request->requestvb->
                                                 nameLength -
                                                 reginfo->rootoid_len -
                                                 2))) {
                AgentHandler_requestAddListData(request,
                                              Map_newElement
                                              (TABLE_DATA_ROW, row,
                                               NULL));
            }
            break;

        case MODE_SET_RESERVE2:
        case MODE_SET_ACTION:
        case MODE_SET_COMMIT:
        case MODE_SET_FREE:
        case MODE_SET_UNDO:
            valid_request = 1;

        }
    }

    if (valid_request &&
       (reqinfo->mode == MODE_GETNEXT || reqinfo->mode == MODE_GETBULK)) {
        /*
         * If this is a GetNext or GetBulk request, then we've identified
         *  the row that ought to include the appropriate next instance.
         *  Convert the request into a Get request, so that the lower-level
         *  handlers don't need to worry about skipping on, and call these
         *  handlers ourselves (so we can undo this again afterwards).
         */
        oldmode = reqinfo->mode;
        reqinfo->mode = MODE_GET;
        result = AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                         requests);
        reqinfo->mode = oldmode;
        handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;
        return result;
    }
    else
        /* next handler called automatically - 'AUTO_NEXT' */
        return PRIOT_ERR_NOERROR;
}

/** extracts the table being accessed passed from the table_data helper */
TableData *
TableData_extractTable(RequestInfo *request)
{
    return (TableData *)
                AgentHandler_requestGetListData(request, TABLE_DATA_TABLE);
}

/** extracts the row being accessed passed from the table_data helper */
TableRow *
TableData_extractTableRow(RequestInfo *request)
{
    return (TableRow *) AgentHandler_requestGetListData(request,
                                                               TABLE_DATA_ROW);
}

/** extracts the data from the row being accessed passed from the
 * table_data helper */
void           *
TableData_extractTableRowData(RequestInfo *request)
{
    TableRow *row;
    row = (TableRow *) TableData_extractTableRow(request);
    if (row)
        return row->data;
    else
        return NULL;
}

/** inserts a newly created table_data row into a request */
void
TableData_insertTableRow(RequestInfo *request,
                         TableRow *row)
{
    RequestInfo       *req;
    TableRequestInfo *table_info = NULL;
    VariableList      *this_index = NULL;
    VariableList      *that_index = NULL;
    oid      base_oid[] = {0, 0};	/* Make sure index OIDs are legal! */
    oid      this_oid[ASN01_MAX_OID_LEN];
    oid      that_oid[ASN01_MAX_OID_LEN];
    size_t   this_oid_len, that_oid_len;

    if (!request)
        return;

    /*
     * We'll add the new row information to any request
     * structure with the same index values as the request
     * passed in (which includes that one!).
     *
     * So construct an OID based on these index values.
     */

    table_info = Table_extractTableInfo(request);
    this_index = table_info->indexes;
    Mib_buildOidNoalloc(this_oid, ASN01_MAX_OID_LEN, &this_oid_len,
                      base_oid, 2, this_index);

    /*
     * We need to look through the whole of the request list
     * (as received by the current handler), as there's no
     * guarantee that this routine will be called by the first
     * varbind that refers to this row.
     *   In particular, a RowStatus controlled row creation
     * may easily occur later in the variable list.
     *
     * So first, we rewind to the head of the list....
     */
    for (req=request; req->prev; req=req->prev)
        ;

    /*
     * ... and then start looking for matching indexes
     * (by constructing OIDs from these index values)
     */
    for (; req; req=req->next) {
        table_info = Table_extractTableInfo(req);
        that_index = table_info->indexes;
        Mib_buildOidNoalloc(that_oid, ASN01_MAX_OID_LEN, &that_oid_len,
                          base_oid, 2, that_index);

        /*
         * This request has the same index values,
         * so add the newly-created row information.
         */
        if (Api_oidCompare(this_oid, this_oid_len,
                             that_oid, that_oid_len) == 0) {
            AgentHandler_requestAddListData(req,
                Map_newElement(TABLE_DATA_ROW, row, NULL));
        }
    }
}

/* builds a result given a row, a varbind to set and the data */
int
TableData_buildResult(HandlerRegistration *reginfo,
                                AgentRequestInfo *reqinfo,
                                RequestInfo *request,
                                TableRow *row,
                                int column,
                                u_char type,
                                u_char * result_data,
                                size_t result_data_len)
{
    oid             build_space[ASN01_MAX_OID_LEN];

    if (!reginfo || !reqinfo || !request)
        return ErrorCode_GENERR;

    if (reqinfo->mode == MODE_GETNEXT || reqinfo->mode == MODE_GETBULK) {
        /*
         * only need to do this for getnext type cases where oid is changing
         */
        memcpy(build_space, reginfo->rootoid,   /* registered oid */
               reginfo->rootoid_len * sizeof(oid));
        build_space[reginfo->rootoid_len] = 1;  /* entry */
        build_space[reginfo->rootoid_len + 1] = column; /* column */
        memcpy(build_space + reginfo->rootoid_len + 2,  /* index data */
               row->index_oid, row->index_oid_len * sizeof(oid));
        Client_setVarObjid(request->requestvb, build_space,
                           reginfo->rootoid_len + 2 + row->index_oid_len);
    }
    Client_setVarTypedValue(request->requestvb, type,
                             result_data, result_data_len);
    return ErrorCode_SUCCESS;     /* WWWXXX: check for bounds */
}


/* ==================================
 *
 * Table Data API: Row operations
 *     (table-independent rows)
 *
 * ================================== */

/** returns the first row in the table */
TableRow *
TableData_getFirstRow(TableData *table)
{
    if (!table)
        return NULL;
    return table->first_row;
}

/** returns the next row in the table */
TableRow *
TableData_getNextRow(TableData *table,
                                TableRow  *row)
{
    if (!row)
        return NULL;
    return row->next;
}

/** finds the data in "datalist" stored at "indexes" */
TableRow *
TableData_get(TableData *table,
                       VariableList * indexes)
{
    oid             searchfor[ASN01_MAX_OID_LEN];
    size_t          searchfor_len = ASN01_MAX_OID_LEN;

    Mib_buildOidNoalloc(searchfor, ASN01_MAX_OID_LEN, &searchfor_len, NULL, 0,
                      indexes);
    return TableData_getFromOid(table, searchfor,
                                           searchfor_len);
}

/** finds the data in "datalist" stored at the searchfor oid */
TableRow *
TableData_getFromOid(TableData *table,
                                oid * searchfor, size_t searchfor_len)
{
    TableRow *row;
    if (!table)
        return NULL;

    for (row = table->first_row; row != NULL; row = row->next) {
        if (row->index_oid &&
            Api_oidCompare(searchfor, searchfor_len,
                             row->index_oid, row->index_oid_len) == 0)
            return row;
    }
    return NULL;
}

int
TableData_numRows(TableData *table)
{
    int i=0;
    TableRow *row;
    if (!table)
        return 0;
    for (row = table->first_row; row; row = row->next) {
        i++;
    }
    return i;
}

    /* =====================================
     * Generic API - mostly renamed wrappers
     * ===================================== */

TableRow *
TableData_rowFirst(TableData *table)
{
    return TableData_getFirstRow(table);
}

TableRow *
TableData_rowGet(  TableData *table,
                             TableRow  *row)
{
    if (!table || !row)
        return NULL;
    return TableData_getFromOid(table, row->index_oid,
                                                  row->index_oid_len);
}

TableRow *
TableData_rowNext( TableData *table,
                             TableRow  *row)
{
    return TableData_getNextRow(table, row);
}

TableRow *
TableData_rowGetByoid( TableData *table,
                                  oid *instance, size_t len)
{
    return TableData_getFromOid(table, instance, len);
}

TableRow *
TableData_rowNextByoid(TableData *table,
                                  oid *instance, size_t len)
{
    TableRow *row;

    if (!table || !instance)
        return NULL;

    for (row = table->first_row; row; row = row->next) {
        if (Api_oidCompare(row->index_oid,
                             row->index_oid_len,
                             instance, len) > 0)
            return row;
    }
    return NULL;
}

TableRow *
TableData_rowGetByidx( TableData    *table,
                                  VariableList *indexes)
{
    return TableData_get(table, indexes);
}

TableRow *
TableData_rowNextByidx(TableData    *table,
                                  VariableList *indexes)
{
    oid    instance[ASN01_MAX_OID_LEN];
    size_t len    = ASN01_MAX_OID_LEN;

    if (!table || !indexes)
        return NULL;

    Mib_buildOidNoalloc(instance, ASN01_MAX_OID_LEN, &len, NULL, 0, indexes);
    return TableData_rowNextByoid(table, instance, len);
}

int
TableData_rowCount(TableData *table)
{
    return TableData_numRows(table);
}


/* ==================================
 *
 * Table Data API: Row operations
 *     (table-specific rows)
 *
 * ================================== */

void *
TableData_tableDataEntryFirst(TableData *table)
{
    TableRow *row =
        TableData_getFirstRow(table);
    return (row ? row->data : NULL );
}

void *
TableData_tableDataEntryGet(  TableData *table,
                               TableRow  *row)
{
    return (row ? row->data : NULL );
}

void *
TableData_tableDataEntryNext( TableData *table,
                               TableRow  *row)
{
    row =
        TableData_rowNext(table, row);
    return (row ? row->data : NULL );
}

void *
TableData_entryGetByidx( TableData    *table,
                                    VariableList *indexes)
{
    TableRow *row =
        TableData_rowGetByidx(table, indexes);
    return (row ? row->data : NULL );
}

void *
TableData_entryNextByidx(TableData    *table,
                                    VariableList *indexes)
{
    TableRow *row =
        TableData_rowNextByidx(table, indexes);
    return (row ? row->data : NULL );
}

void *
TableData_entryGetByoid( TableData *table,
                                    oid *instance, size_t len)
{
    TableRow *row =
        TableData_rowGetByoid(table, instance, len);
    return (row ? row->data : NULL );
}

void *
TableData_entryNextByoid(TableData *table,
                                    oid *instance, size_t len)
{
    TableRow *row =
        TableData_rowNextByoid(table, instance, len);
    return (row ? row->data : NULL );
}

    /* =====================================
     * Generic API - mostly renamed wrappers
     * ===================================== */

/* ==================================
 *
 * Table Data API: Index operations
 *
 * ================================== */

