#ifndef TABLECONTAINER_H
#define TABLECONTAINER_H

#include "AgentHandler.h"
#include "Table.h"
#include "System/Containers/Container.h"
/*
 * The table container helper is designed to simplify the task of
 * writing a table handler for the net-snmp agent when the data being
 * accessed is accessible via a Container_Container.
 *
 * Functionally, it is a specialized version of the more
 * generic table helper but easies the burden of GETNEXT processing by
 * retrieving the appropriate row for each index through
 * function calls which should be supplied by the module that wishes
 * help.  The module the table_container helps should, afterwards,
 * never be called for the case of "MODE_GETNEXT" and only for the GET
 * and SET related modes instead.
 */

#define TABLE_CONTAINER_ROW       "tableContainer:row"
#define TABLE_CONTAINER_CONTAINER "tableContainer:container"

#define TABLE_CONTAINER_KEY_NETSNMP_INDEX         1 /* default */
#define TABLE_CONTAINER_KEY_VARBIND_INDEX         2
#define TABLE_CONTAINER_KEY_VARBIND_RAW           3

/* ====================================
* Container Table API: MIB maintenance
* ==================================== */

/*
 * get an injectable container table handler
 */
MibHandler *
TableContainer_handlerGet(TableRegistrationInfo *tabreq,
                                    Container_Container *container,
                                    char key_type);
/*
 * register a container table
 */
int
TableContainer_register(HandlerRegistration *reginfo,
                                 TableRegistrationInfo *tabreq,
                                 Container_Container *container,
                                 char key_type);
int
TableContainer_unregister(HandlerRegistration *reginfo);

/** retrieve the container used by the table_container helper */
Container_Container*
TableContainer_containerExtract(RequestInfo *request);


void TableContainer_rowInsert( RequestInfo *request,
                               Types_Index *row);

void *
TableContainer_rowExtract(RequestInfo *request);

void *
TableContainer_extractContext(RequestInfo *request);

void TableContainer_rowRemove( RequestInfo *request,
                               Types_Index *row);

Types_Index *
TableContainer_indexFindNextRow(Container_Container *c,
                                  TableRequestInfo *tblreq);

/* ===================================
* Container Table API: Row operations
* =================================== */


#endif // TABLECONTAINER_H
