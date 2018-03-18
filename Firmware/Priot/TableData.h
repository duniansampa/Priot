#ifndef TABLEDATA_H
#define TABLEDATA_H

#include "AgentHandler.h"
#include "Table.h"

/*
 * This helper is designed to completely automate the task of storing
 * tables of data within the agent that are not tied to external data
 * sources (like the kernel, hardware, or other processes, etc).  IE,
 * all rows within a table are expected to be added manually using
 * functions found below.
 */

#define TABLE_DATA_NAME "tableData"
#define TABLE_DATA_ROW  "tableData"
#define TABLE_DATA_TABLE "tableDataTable"

typedef struct TableRow_s {
    Types_VariableList* indexes; /* stored permanently if store_indexes = 1 */
    oid*                index_oid;
    size_t              index_oid_len;
    void*               data;   /* the data to store */

    struct TableRow_s*  next;        /* if used in a list */
    struct TableRow_s*  prev;
} TableRow;

typedef struct TableData_s {
    Types_VariableList* indexes_template;        /* containing only types */
    char*               name;   /* if !NULL, it's registered globally */
    int                 flags;  /* not currently used */
    int                 store_indexes;
    TableRow*           first_row;
    TableRow*           last_row;
} TableData;

/* =================================
* Table Data API: Table maintenance
* ================================= */

void
TableData_generateIndexOid( TableRow* row);

TableData*
TableData_createTableData( const char* name );

TableRow*
TableData_createTableDataRow( void );

TableRow*
TableData_cloneRow( TableRow* row );

void*
TableData_deleteRow( TableRow* row );

int
TableData_addRow( TableData* table,
                  TableRow*  row );
void
TableData_replaceRow( TableData* table,
                       TableRow*  origrow,
                       TableRow*  newrow );

TableRow*
TableData_removeRow( TableData* table,
                     TableRow*  row );

void*
TableData_removeAndDeleteRow( TableData* table,
                              TableRow*  row );

void
TableData_deleteTable( TableData* table );

/* =================================
* Table Data API: MIB maintenance
* ================================= */

MibHandler*
TableData_getTableDataHandler( TableData* table );

int
TableData_registerTableData( HandlerRegistration*   reginfo,
                             TableData*             table,
                             TableRegistrationInfo* table_info);

int
TableData_registerReadOnlyTableData( HandlerRegistration*   reginfo,
                                     TableData*             table,
                                     TableRegistrationInfo* table_info );

NodeHandlerFT
TableData_helperHandler;

TableData*
TableData_extractTable( RequestInfo* );

TableRow*
TableData_extractTableRow( RequestInfo* );

void*
TableData_extractTableRowData( RequestInfo* );

void
TableData_insertTableRow( RequestInfo*,
                          TableRow* );

int
TableData_buildResult( HandlerRegistration* reginfo,
                       AgentRequestInfo*    reqinfo,
                       RequestInfo*         request,
                       TableRow*            row,
                       int                  column,
                       u_char               type,
                       u_char*              result_data,
                       size_t               result_data_len );

/* =================================
* Table Data API: Row operations
* ================================= */

TableRow*
TableData_getFirstRow( TableData* table );

TableRow*
TableData_getNextRow( TableData* table,
                      TableRow*  row );

TableRow*
TableData_get( TableData*          table,
               Types_VariableList* indexes);

TableRow*
TableData_getFromOid( TableData* table,
                      oid*       searchfor,
                      size_t     searchfor_len );

int
TableData_numRows( TableData* table );


/* =================================
* Table Data API: Index operations
* ================================= */

#define TableData_addIndex(thetable, type) Api_varlistAddVariable(&thetable->indexes_template, NULL, 0, type, NULL, 0)
#define TableData_rowAddIndex(row, type, value, value_len) Api_varlistAddVariable(&row->indexes, NULL, 0, type, (const u_char *) value, value_len)


#endif // TABLEDATA_H
