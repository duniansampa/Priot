#ifndef TABLE_H
#define TABLE_H

#include "Types.h"
#include "AgentHandler.h"
#include "Asn01.h"
#include "OidStash.h"

/**
 * The table helper is designed to simplify the task of writing a
 * table handler for the net-snmp agent.  You should create a normal
 * handler and register it using the Table_registerTable() function
 * instead of the netsnmp_register_handler() function.
 */

/**
 * Notes:
 *
 *   1) illegal indexes automatically get handled for get/set cases.
 *      Simply check to make sure the value is type ASN01_NULL before
 *      you answer a request.
 */

/**
 * used as an index to parent_data lookups
 */
#define TABLE_HANDLER_NAME "table"

/** @typedef struct ColumnInfoT_s ColumnInfo
 * Typedefs the ColumnInfoT_s struct into ColumnInfo */

/**
 * @struct ColumnInfoT_s
 * column info struct.  OVERLAPPING RANGES ARE NOT SUPPORTED.
 */
typedef struct ColumnInfoT_s {
    char            isRange;
/** only useful if isRange == 0 */
    char            list_count;

    union {
        unsigned int    range[2];
        unsigned int   *list;
    } details;

    struct ColumnInfoT_s *next;

} ColumnInfo;

/** @typedef struct TableRegistrationInfo_s TableRegistrationInfo
* Typedefs the TableRegistrationInfo_s  struct into
* TableRegistrationInfo */

/**
* @struct TableRegistrationInfo_s
* Table registration structure.
*/
typedef struct TableRegistrationInfo_s {
/** list of varbinds with only 'type' set */
    VariableList *indexes;
/** calculated automatically */
    unsigned int    number_indexes;

   /**
    * the minimum columns number. If there are columns
    * in-between which are not valid, use valid_columns to get
    * automatic column range checking.
    */
    unsigned int    min_column;
/** the maximum columns number */
    unsigned int    max_column;

/** more details on columns */
    ColumnInfo *valid_columns;

} TableRegistrationInfo;

/** @typedef struct TableRequestInfo_s TableRequestInfo
* Typedefs the TableRequestInfo_s  struct into
* TableRequestInfo */

/**
* @struct TableRequestInfo_s
* The table request info structure.
*/
typedef struct TableRequestInfo_s {
/** 0 if OID not long enough */
    unsigned int    colnum;
    /** 0 if failure to parse any */
    unsigned int    number_indexes;
/** contents freed by helper upon exit */
    VariableList *indexes;

    oid    index_oid[asnMAX_OID_LEN];
    size_t index_oid_len;
    TableRegistrationInfo *reg_info;
} TableRequestInfo;

MibHandler *
Table_getTableHandler( TableRegistrationInfo* tabreq );

void
Table_handlerOwnsTableInfo( MibHandler* handler );

void
Table_registrationOwnsTableInfo( HandlerRegistration* reg );

int
Table_registerTable( HandlerRegistration*   reginfo,
                     TableRegistrationInfo* tabreq );

int
Table_unregisterTable(HandlerRegistration    *reginfo);

int
Table_buildOid( HandlerRegistration    *reginfo,
                RequestInfo            *reqinfo,
                TableRequestInfo   *table_info);
int
Table_buildOidFromIndex( HandlerRegistration *reginfo,
                         RequestInfo *reqinfo,
                         TableRequestInfo *table_info );

int
Table_buildResult( HandlerRegistration  *reginfo,
                   RequestInfo *reqinfo,
                   TableRequestInfo *table_info,
                   u_char type,
                   u_char * result,
                   size_t result_len );
int
Table_updateVariableListFromIndex( TableRequestInfo* );

int
Table_updateIndexesFromVariableList( TableRequestInfo* tri );

TableRegistrationInfo *
Table_findTableRegistrationInfo( HandlerRegistration* reginfo );

TableRegistrationInfo *
Table_registrationInfoClone( TableRegistrationInfo *tri );

void
Table_registrationInfoFree( TableRegistrationInfo * );


unsigned int
Table_closestColumn( unsigned int current,
                     ColumnInfo*  valid_columns );

NodeHandlerFT Table_helperHandler;

#define Table_helperAddIndex(tinfo, type) Api_varlistAddVariable(&tinfo->indexes, NULL, 0, (u_char)type, NULL, 0);

void
Table_helperAddIndexes( TableRegistrationInfo *tinfo, ...);

int Table_checkGetnextReply( RequestInfo *request,
                             oid * prefix,
                             size_t prefix_len,
                             VariableList * newvar,
                             VariableList ** outvar );

TableRequestInfo*
Table_extractTableInfo( RequestInfo * );

OidStashNode_t**
Table_getOrCreateRowStash( AgentRequestInfo *reqinfo,
                                const u_char * storage_name);
unsigned int
Table_nextColumn( TableRequestInfo* table_info );


int
Table_sparseTableRegister( HandlerRegistration *reginfo,
                           TableRegistrationInfo *tabreq);

MibHandler*
Table_sparseTableHandlerGet(void);

#endif // TABLE_H
