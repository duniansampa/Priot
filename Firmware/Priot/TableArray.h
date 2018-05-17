#ifndef TABLEARRAY_H
#define TABLEARRAY_H

#include "AgentHandler.h"
#include "Table.h"
#include "System/Containers/Container.h"

/*
 * The table array helper is designed to simplify the task of
 * writing a table handler for the net-snmp agent when the data being
 * accessed is in an oid sorted form and must be accessed externally.
 *
 * Functionally, it is a specialized version of the more
 * generic table helper but easies the burden of GETNEXT processing by
 * retrieving the appropriate row for ead index through
 * function calls which should be supplied by the module that wishes
 * help.  The module the table_array helps should, afterwards,
 * never be called for the case of "MODE_GETNEXT" and only for the GET
 * and SET related modes instead.
 */

#define TABLE_ARRAY_NAME "tableArray"

/*
 * group_item is to allow us to keep a list of requests without
 * disrupting the actual RequestInfo list.
 */
typedef struct RequestGroupItem_s {
    RequestInfo *ri;
    TableRequestInfo *tri;
    struct RequestGroupItem_s *next;
} RequestGroupItem;

/*
 * structure to keep a list of requests for each unique index
 */
typedef struct RequestGroup_s {
   /*
    * index for this row. points to someone else's memory, so
    * don't free it!
    */
    Types_Index               index;

   /*
    * container in which rows belong
    */
    Container_Container           *table;

   /*
    * actual old and new rows
    */
    Types_Index               *existing_row;
    Types_Index               *undo_info;

   /*
    * flags
    */
   char                          row_created;
   char                          row_deleted;
   char                          fill1;
   char                          fill2;

   /*
    * requests for this row
    */
    RequestGroupItem  *list;

    int                          status;

    void                        *rg_void;

} RequestGroup;

typedef int
(UserRowOperationCFT) ( const void *lhs,
                        const void *rhs);
typedef int
(UserRowOperation) ( void *lhs,
                     void *rhs);

typedef int
(UserGetProcessor) ( RequestInfo *,
                     Types_Index *,
                     TableRequestInfo *);

typedef Types_Index
*(UserRowMethod) ( Types_Index * );

typedef int
(UserRowAction) ( Types_Index *,
                  Types_Index *,
                  RequestGroup * );

typedef void
(UserGroupMethod) ( RequestGroup * );

/*
 * structure for array callbacks
 */
typedef struct TableArrayCallbacks_s {

    UserRowOperation   *row_copy;
    UserRowOperationCFT *row_compare;

    UserGetProcessor *get_value;


    UserRowAction *can_activate;
    UserRowAction *activated;
    UserRowAction *can_deactivate;
    UserRowAction *deactivated;
    UserRowAction *can_delete;

    UserRowMethod  *create_row;
    UserRowMethod  *duplicate_row;
    UserRowMethod  *delete_row;    /* always returns NULL */

    UserGroupMethod *set_reserve1;
    UserGroupMethod *set_reserve2;
    UserGroupMethod *set_action;
    UserGroupMethod *set_commit;
    UserGroupMethod *set_free;
    UserGroupMethod *set_undo;

   /** not callbacks, but this is a useful place for them... */
   Container_Container* container;
   char can_set;

} TableArrayCallbacks;


int
TableArray_containerRegister( HandlerRegistration *reginfo,
                              TableRegistrationInfo *tabreq,
                              TableArrayCallbacks *cb,
                              Container_Container *container,
                              int group_rows );

int
TableArray_register( HandlerRegistration *reginfo,
                     TableRegistrationInfo *tabreq,
                     TableArrayCallbacks *cb,
                     Container_Container *container,
                     int group_rows );

Container_Container *
TableArray_extractArrayContext( RequestInfo * );

NodeHandlerFT
TableArray_helperHandler;

int
TableArray_checkRowStatus( TableArrayCallbacks *cb,
                           RequestGroup *ag,
                           long *rs_new,
                           long *rs_old );

#endif // TABLEARRAY_H
