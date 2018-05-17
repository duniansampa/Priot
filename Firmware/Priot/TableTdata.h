#ifndef TABLETDATA_H
#define TABLETDATA_H


#include "AgentHandler.h"
#include "System/Containers/Container.h"
#include "Table.h"

/*
 * This helper is designed to completely automate the task of storing
 * tables of data within the agent that are not tied to external data
 * sources (like the kernel, hardware, or other processes, etc).  IE,
 * all rows within a table are expected to be added manually using
 * functions found below.
 */

#define TABLE_TDATA_NAME  "tableTdata"
#define TABLE_TDATA_ROW   "tableTdata"
#define TABLE_TDATA_TABLE "tableTdataTable"

#define TDATA_FLAG_NO_STORE_INDEXES   0x01
#define TDATA_FLAG_NO_CONTAINER       0x02 /* user will provide container */

/*
 * The (table-independent) per-row data structure
 * This is a wrapper round the table-specific per-row data
 *   structure, which is referred to as a "table entry"
 *
 * It should be regarded as an opaque, private data structure,
 *   and shouldn't be accessed directly.
 */
typedef struct TdataRow_s {
    Types_Index   oid_index;      /* table_container index format */
    Types_VariableList *indexes; /* stored permanently if store_indexes = 1 */
    void           *data;   /* the data to store */
} TdataRow;

/*
 * The data structure to hold a complete table.
 *
 * This should be regarded as an opaque, private data structure,
 *   and shouldn't be accessed directly.
 */
typedef struct Tdata_s {
    Types_VariableList *indexes_template;        /* containing only types */
    char           *name;   /* if !NULL, it's registered globally */
    int             flags;  /* This field may legitimately be accessed by external code */
    Container_Container *container;
} Tdata;

/* Backwards compatability with the previous (poorly named) data structures */
typedef  struct TdataRow_s TableData2row;

typedef  struct Tdata_s     TableData2;


/* ============================
* TData API: Table maintenance
* ============================ */

Tdata *
TableTdata_createTable( const char * name,
                        long         flags );

void
TableTdata_deleteTable( Tdata *table );

TdataRow *
TableTdata_createRow( void );

TdataRow *
TableTdata_cloneRow( TdataRow * row );

int
TableTdata_copyRow(  TdataRow * dst_row,
                     TdataRow * src_row );
void *
TableTdata_deleteRow( TdataRow * row );

int
TableTdata_addRow( Tdata    * table,
                   TdataRow * row );
void
TableTdata_replaceRow(  Tdata    * table,
                        TdataRow * origrow,
                        TdataRow * newrow );

TdataRow *
TableTdata_removeRow( Tdata    * table,
                      TdataRow * row );

void *
TableTdata_removeAndDeleteRow( Tdata    * table,
                               TdataRow * row );


/* ============================
* TData API: MIB maintenance
* ============================ */

MibHandler *
TableTdata_getTdataHandler( Tdata * table );

int
TableTdata_register( HandlerRegistration   * reginfo,
                     Tdata                 * table,
                     TableRegistrationInfo * table_info );

int
TableTdata_unregister( HandlerRegistration * reginfo );

Tdata *
TableTdata_extractTable( RequestInfo * );

Container_Container *
TableTdata_extractContainer( RequestInfo * );

TdataRow  *
TableTdata_extractRow( RequestInfo * );

void *
TableTdata_extractEntry( RequestInfo * );

void
TableTdata_insertTdataRow( RequestInfo *, TdataRow *);

void
TableTdata_removeTdataRow( RequestInfo *, TdataRow *);


/* ============================
* TData API: Row operations
* ============================ */

void *
TableTdata_rowEntry( TdataRow * row );

TdataRow *
TableTdata_rowFirst( Tdata * table );

TdataRow *
TableTdata_rowGet(  Tdata    * table,
                    TdataRow * row );

TdataRow *
TableTdata_rowNext( Tdata    * table,
                    TdataRow * row );

TdataRow *
TableTdata_rowGetByidx( Tdata              * table,
                        Types_VariableList * indexes );
TdataRow *
TableTdata_rowGetByoid( Tdata  * table,
                        oid    * searchfor,
                        size_t   searchfor_len );

TdataRow *
TableTdata_rowNextByidx( Tdata              * table,
                         Types_VariableList * indexes );

TdataRow *
TableTdata_rowNextByoid( Tdata  * table,
                         oid    * searchfor,
                         size_t   searchfor_len );

int
TableTdata_rowCount( Tdata * table );


/* ============================
* TData API: Index operations
* ============================ */

#define TableTdata_addIndex(thetable, type) \
    Api_varlistAddVariable(&thetable->indexes_template, NULL, 0, type, NULL, 0)

#define TableTdata_rowAddIndex(row, type, value, value_len) \
    Api_varlistAddVariable(&row->indexes, NULL, 0, type, (const u_char *) value, value_len)

int
TableTdata_compareIdx( TdataRow           * row,
                       Types_VariableList * indexes );

int
TableTdata_compareOid( TdataRow * row,
                       oid      * compareto,
                       size_t     compareto_len );

int
TableTdata_compareSubtreeIdx( TdataRow           * row,
                              Types_VariableList * indexes);

int
TableTdata_compareSubtreeOid( TdataRow * row,
                              oid      * compareto,
                              size_t     compareto_len );



#endif // TABLETDATA_H
