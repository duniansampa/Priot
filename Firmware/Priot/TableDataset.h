#ifndef TABLEDATASET_H
#define TABLEDATASET_H

#include "AgentHandler.h"
#include "OidStash.h"
#include "TableData.h"
#include "Api.h"

/*
 * This helper is designed to completely automate the task of storing
 * tables of data within the agent that are not tied to external data
 * sources (like the kernel, hardware, or other processes, etc).  IE,
 * all rows within a table are expected to be added manually using
 * functions found below.
 */

void
TableDataset_initTableDataset( void );

#define TABLE_DATA_SET_NAME "priotTableDataSet"

/*
 * return SNMP_ERR_NOERROR or some SNMP specific protocol error id
 */
typedef int
(ValueChangeOkFT) ( char   * old_value,
                    size_t   old_value_len,
                    char   * new_value,
                    size_t   new_value_len,
                    void   * mydata );

/*
 * stored within a given row
 */
typedef struct TableDataSetStorage_s {
    unsigned int    column;

    /*
     * info about it?
     */
    char            writable;
    ValueChangeOkFT *change_ok_fn;
    void           *my_change_data;

    /*
     * data actually stored
     */
    u_char              type;
    union {                 /* value of variable */
        void           *voidp;
        long           *integer;
        u_char         *string;
        oid            *objid;
        u_char         *bitstring;
        struct counter64 *counter64;
        float          *floatVal;
        double         *doubleVal;
    } data;
    u_long          data_len;

    struct TableDataSetStorage_s * next;
} TableDataSetStorage;

typedef struct TableDataSet_s {
    TableData           * table;
    TableDataSetStorage * default_row;
    int                   allow_creation; /* set to 1 to allow creation of new rows */
    unsigned int          rowstatus_column;
} TableDataSet;


/* ============================
* DataSet API: Table maintenance
* ============================ */

TableDataSet *
TableDataset_createTableDataSet( const char * );

TableRow *
TableDataset_cloneRow( TableRow * row );

void
TableDataset_deleteAllData( TableDataSetStorage * data );

void
TableDataset_deleteRow( TableRow * row );

void
TableDataset_addRow( TableDataSet * table,
                     TableRow     * row );
void
TableDataset_replaceRow( TableDataSet * table,
                         TableRow     * origrow,
                         TableRow     * newrow );
void
TableDataset_removeRow( TableDataSet * table,
                        TableRow     * row );
void
TableDataset_removeAndDeleteRow( TableDataSet * table,
                                 TableRow     * row );

void
TableDataset_deleteTableDataSet( TableDataSet * table_set );

/* ============================
* DataSet API: Default row operations
* ============================ */

/*
 * to set, add column, type, (writable) ? 1 : 0
 */
/*
 * default value, if not NULL, is the default value used in row
 * creation.  It is copied into the storage template (free your
 * calling argument).
 */
int
TableDataset_addDefaultRow( TableDataSet *,
                               unsigned int ,
                               int,
                               int,
                               void         * default_value,
                               size_t         default_value_len );

void
TableDataset_multiAddDefaultRow( TableDataSet *,  ... );


/* ============================
* DataSet API: MIB maintenance
* ============================ */

MibHandler *
TableDataset_getTableDataSetHandler( TableDataSet * );

NodeHandlerFT
TableDataset_helperHandler;

int
TableDataset_registerTableDataSet( HandlerRegistration   *,
                                   TableDataSet          *,
                                   TableRegistrationInfo * );

TableDataSet*
TableDataset_extractTableDataSet( RequestInfo * request );

TableDataSetStorage *
TableDataset_extractTableDataSetColumn( RequestInfo *,
                                        unsigned int );
OidStash_Node **
TableDataset_getOrCreateStash( AgentRequestInfo * ari,
                               TableDataSet     * tds,
                               TableRequestInfo * tri );
TableRow *
TableDataset_getNewrow( RequestInfo       * request,
                        AgentRequestInfo  * reqinfo,
                        int                 rootoid_len,
                        TableDataSet      * datatable,
                        TableRequestInfo  * table_info);


/* ============================
* DataSet API: Config-based operations
* ============================ */

void
TableDataset_registerAutoDataTable( TableDataSet * table_set,
                                    char         * registration_name);

void
TableDataset_unregisterAutoDataTable( TableDataSet * table_set,
                                      char         * registration_name);

void
TableDataset_configParseTableSet( const char * token,
                                  char       * line );

void
TableDataset_configParseAddRow( const char * token,
                                char       * line );


/* ============================
* DataSet API: Row operations
* ============================ */

TableRow *
TableDataset_getFirstRow( TableDataSet * table );

TableRow *
TableDataset_getNextRow( TableDataSet * table,
                         TableRow     * row );

int
TableDataset_numRows( TableDataSet * table );


/* ============================
* DataSet API: Column operations
* ============================ */

TableDataSetStorage *
TableDataset_findColumn( TableDataSetStorage *,
                         unsigned int );

int
TableDataset_markRowColumnWritable( TableRow * row,
                                    int        column,
                                    int        writable );

int
TableDataset_setRowColumn( TableRow      *,
                           unsigned int   ,
                           int            ,
                           const void    *,
                           size_t );

/* ============================
* DataSet API: Index operations
* ============================ */

void
TableDataset_addIndex( TableDataSet * table,
                       u_char         type );

void
TableDataset_addIndexes( TableDataSet * tset, ... );

#define TableDataset_addColumn( row, type, value, value_len) \
        Api_varlistAddVariable(&row->indexes, NULL, 0, type, (u_char *) value, value_len)


#endif // TABLEDATASET_H
