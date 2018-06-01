#include "TableArray.h"
#include "PriotSettings.h"
#include "System/Util/Logger.h"
#include "System/Util/Trace.h"
#include "TextualConvention.h"
#include "Client.h"
#include "System/Util/Assert.h"
#include "TableContainer.h"


/*
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_BEGIN        -1
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_RESERVE1     0
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_RESERVE2     1
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_ACTION       2
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_COMMIT       3
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_FREE         4
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_UNDO         5
 */

static const char * _tableArray_modeName[] = {
    "Reserve 1",
    "Reserve 2",
    "Action",
    "Commit",
    "Free",
    "Undo"
};

/*
 * PRIVATE structure for holding important info for each table.
 */
typedef struct TableContainerData_s {

   /** registration info for the table */
    TableRegistrationInfo *tblreg_info;

   /** container for the table rows */
   Container_Container          *table;

    /*
     * mutex_type                lock;
     */

   /** do we want to group rows with the same index
    * together when calling callbacks? */
    int             group_rows;

   /** callbacks for this table */
    TableArrayCallbacks *cb;

} TableContainerData;

/** @defgroup table_array table_array
 *  Helps you implement a table when data can be stored locally. The data is stored in a sorted array, using a binary search for lookups.
 *  @ingroup table
 *
 *  The table_array handler is used (automatically) in conjuntion
 *  with the @link table table@endlink handler. It is primarily
 *  intended to be used with the mib2c configuration file
 *  mib2c.array-user.conf.
 *
 *  The code generated by mib2c is useful when you have control of
 *  the data for each row. If you cannot control when rows are added
 *  and deleted (or at least be notified of changes to row data),
 *  then this handler is probably not for you.
 *
 *  This handler makes use of callbacks (function pointers) to
 *  handle various tasks. Code is generated for each callback,
 *  but will need to be reviewed and flushed out by the user.
 *
 *  NOTE NOTE NOTE: Once place where mib2c is somewhat lacking
 *  is with regards to tables with external indices. If your
 *  table makes use of one or more external indices, please
 *  review the generated code very carefully for comments
 *  regarding external indices.
 *
 *  NOTE NOTE NOTE: This helper, the API and callbacks are still
 *  being tested and may change.
 *
 *  The generated code will define a structure for storage of table
 *  related data. This structure must be used, as it contains the index
 *  OID for the row, which is used for keeping the array sorted. You can
 *  add addition fields or data to the structure for your own use.
 *
 *  The generated code will also have code to handle SNMP-SET processing.
 *  If your table does not support any SET operations, simply comment
 *  out the \#define \<PREFIX\>_SET_HANDLING (where \<PREFIX\> is your
 *  table name) in the header file.
 *
 *  SET processing modifies the row in-place. The duplicate_row
 *  callback will be called to save a copy of the original row.
 *  In the event of a failure before the commite phase, the
 *  row_copy callback will be called to restore the original row
 *  from the copy.
 *
 *  Code will be generated to handle row creation. This code may be
 *  disabled by commenting out the \#define \<PREFIX\>_ROW_CREATION
 *  in the header file.
 *
 *  If your table contains a RowStatus object, by default the
 *  code will not allow object in an active row to be modified.
 *  To allow active rows to be modified, remove the comment block
 *  around the \#define \<PREFIX\>_CAN_MODIFY_ACTIVE_ROW in the header
 *  file.
 *
 *  Code will be generated to maintain a secondary index for all
 *  rows, stored in a binary tree. This is very useful for finding
 *  rows by a key other than the OID index. By default, the functions
 *  for maintaining this tree will be based on a character string.
 *  NOTE: this will likely be made into a more generic mechanism,
 *  using new callback methods, in the near future.
 *
 *  The generated code contains many TODO comments. Make sure you
 *  check each one to see if it applies to your code. Examples include
 *  checking indices for syntax (ranges, etc), initializing default
 *  values in newly created rows, checking for row activation and
 *  deactivation requirements, etc.
 *
 * @{
 */

/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * PUBLIC Registration functions                                      *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/
/** register specified callbacks for the specified table/oid. If the
    group_rows parameter is set, the row related callbacks will be
    called once for each unique row index. Otherwise, each callback
    will be called only once, for all objects.
*/
int
TableArray_containerRegister(HandlerRegistration *reginfo,
                             TableRegistrationInfo *tabreg,
                             TableArrayCallbacks *cb,
                             Container_Container *container,
                             int group_rows)
{
    TableContainerData *tad = MEMORY_MALLOC_TYPEDEF(TableContainerData);
    if (!tad)
        return ErrorCode_GENERR;
    tad->tblreg_info = tabreg;  /* we need it too, but it really is not ours */

    if (!cb) {
        Logger_log(LOGGER_PRIORITY_ERR, "table_array registration with no callbacks\n" );
        free(tad); /* MEMORY_FREE is overkill for local var */
        return ErrorCode_GENERR;
    }
    /*
     * check for required callbacks
     */
    if ((cb->can_set &&
         ((NULL==cb->duplicate_row) || (NULL==cb->delete_row) ||
          (NULL==cb->row_copy)) )) {
        Logger_log(LOGGER_PRIORITY_ERR, "table_array registration with incomplete "
                 "callback structure.\n");
        free(tad); /* MEMORY_FREE is overkill for local var */
        return ErrorCode_GENERR;
    }

    if (NULL==container) {
        tad->table = Container_find("tableArray");
        Logger_log(LOGGER_PRIORITY_ERR, "table_array couldn't allocate container\n" );
        free(tad); /* MEMORY_FREE is overkill for local var */
        return ErrorCode_GENERR;
    } else
        tad->table = container;
    if (NULL==tad->table->compare)
        tad->table->compare = Container_compareIndex;
    if (NULL==tad->table->nCompare)
        tad->table->nCompare = Container_nCompareIndex;

    tad->cb = cb;

    reginfo->handler->myvoid = tad;

    return Table_registerTable(reginfo, tabreg);
}

int
TableArray_register(HandlerRegistration *reginfo,
                             TableRegistrationInfo *tabreg,
                             TableArrayCallbacks *cb,
                             Container_Container *container,
                             int group_rows)
{
    AgentHandler_injectHandler(reginfo,
                           AgentHandler_createHandler(reginfo->handlerName,
                               TableArray_helperHandler));
    return TableArray_containerRegister(reginfo, tabreg, cb,
                                            container, group_rows);
}

/** find the handler for the table_array helper. */
MibHandler *
TableArray_findTableArrayHandler(HandlerRegistration *reginfo)
{
    MibHandler *mh;
    if (!reginfo)
        return NULL;
    mh = reginfo->handler;
    while (mh) {
        if (mh->access_method == TableArray_helperHandler)
            break;
        mh = mh->next;
    }

    return mh;
}

/** find the context data used by the table_array helper */
Container_Container      *
TableArray_extractArrayContext(RequestInfo *request)
{
    return (Container_Container*)AgentHandler_requestGetListData(request, TABLE_ARRAY_NAME);
}

/** this function is called to validate RowStatus transitions. */
int
TableArray_checkRowStatus(TableArrayCallbacks *cb,
                                     RequestGroup *ag,
                                     long *rs_new, long *rs_old)
{
    Types_Index *row_ctx;
    Types_Index *undo_ctx;
    if (!ag || !cb)
        return ErrorCode_GENERR;
    row_ctx  = ag->existing_row;
    undo_ctx = ag->undo_info;

    /*
     * xxx-rks: revisit row delete scenario
     */
    if (row_ctx) {
        /*
         * either a new row, or change to old row
         */
        /*
         * is it set to active?
         */
        if (tcROW_STATUS_IS_GOING_ACTIVE(*rs_new)) {
            /*
             * is it ready to be active?
             */
            if ((NULL==cb->can_activate) ||
                cb->can_activate(undo_ctx, row_ctx, ag))
                *rs_new = tcROW_STATUS_ACTIVE;
            else
                return PRIOT_ERR_INCONSISTENTVALUE;
        } else {
            /*
             * not going active
             */
            if (undo_ctx) {
                /*
                 * change
                 */
                if (tcROW_STATUS_IS_ACTIVE(*rs_old)) {
                    /*
                     * check pre-reqs for deactivation
                     */
                    if (cb->can_deactivate &&
                        !cb->can_deactivate(undo_ctx, row_ctx, ag)) {
                        return PRIOT_ERR_INCONSISTENTVALUE;
                    }
                }
            } else {
                /*
                 * new row
                 */
            }

            if (*rs_new != tcROW_STATUS_DESTROY) {
                if ((NULL==cb->can_activate) ||
                    cb->can_activate(undo_ctx, row_ctx, ag))
                    *rs_new = tcROW_STATUS_NOTINSERVICE;
                else
                    *rs_new = tcROW_STATUS_NOTREADY;
            } else {
                if (cb->can_delete && !cb->can_delete(undo_ctx, row_ctx, ag)) {
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
                ag->row_deleted = 1;
            }
        }
    } else {
        /*
         * check pre-reqs for delete row
         */
        if (cb->can_delete && !cb->can_delete(undo_ctx, row_ctx, ag)) {
            return PRIOT_ERR_INCONSISTENTVALUE;
        }
    }

    return PRIOT_ERR_NOERROR;
}

/** @} */

/** @cond */
/**********************************************************************
 **********************************************************************
 **********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 *                                                                    *
 *                                                                    *
 * EVERYTHING BELOW THIS IS PRIVATE IMPLEMENTATION DETAILS.           *
 *                                                                    *
 *                                                                    *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************
 **********************************************************************
 **********************************************************************/

/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * Structures, Utility/convenience functions                          *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/
/*
 * context info for SET requests
 */
typedef struct SetContext_s {
    AgentRequestInfo *agtreq_info;
    TableContainerData *tad;
    int             status;
} SetContext;

void
TableArray_buildNewOid(HandlerRegistration *reginfo,
              TableRequestInfo *tblreq_info,
              Types_Index *row, RequestInfo *current)
{
    oid             coloid[ASN01_MAX_OID_LEN];

    if (!tblreq_info || !reginfo || !row || !current)
        return;

    memcpy(coloid, reginfo->rootoid, reginfo->rootoid_len * sizeof(oid));

    /** table.entry */
    coloid[reginfo->rootoid_len] = 1;

    /** table.entry.column */
    coloid[reginfo->rootoid_len + 1] = tblreq_info->colnum;

    /** table.entry.column.index */
    memcpy(&coloid[reginfo->rootoid_len + 2], row->oids,
           row->len * sizeof(oid));

    Client_setVarObjid(current->requestvb, coloid,
                       reginfo->rootoid_len + 2 + row->len);
}

/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * GET procession functions                                           *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/
int
TableArray_processGetRequests(HandlerRegistration *reginfo,
                     AgentRequestInfo *agtreq_info,
                     RequestInfo *requests,
                     TableContainerData * tad)
{
    int             rc = PRIOT_ERR_NOERROR;
    RequestInfo *current;
    Types_Index *row = NULL;
    TableRequestInfo *tblreq_info;
    VariableList *var;

    /*
     * Loop through each of the requests, and
     * try to find the appropriate row from the container.
     */
    for (current = requests; current; current = current->next) {

        var = current->requestvb;
        DEBUG_MSGTL(("table_array:get",
                    "  process_get_request oid:"));
        DEBUG_MSGOID(("table_array:get", var->name,
                     var->nameLength));
        DEBUG_MSG(("table_array:get", "\n"));

        /*
         * skip anything that doesn't need processing.
         */
        if (current->processed != 0) {
            DEBUG_MSGTL(("table_array:get", "already processed\n"));
            continue;
        }

        /*
         * Get pointer to the table information for this request. This
         * information was saved by table_helper_handler. When
         * debugging, we double check a few assumptions. For example,
         * the table_helper_handler should enforce column boundaries.
         */
        tblreq_info = Table_extractTableInfo(current);
        Assert_assert(tblreq_info->colnum <= tad->tblreg_info->max_column);

        if ((agtreq_info->mode == MODE_GETNEXT) ||
            (agtreq_info->mode == MODE_GETBULK)) {
            /*
             * find the row
             */
            row = TableContainer_indexFindNextRow(tad->table, tblreq_info);
            if (!row) {
                /*
                 * no results found.
                 *
                 * xxx-rks: how do we skip this entry for the next handler,
                 * but still allow it a chance to hit another handler?
                 */
                DEBUG_MSGTL(("table_array:get", "no row found\n"));
                Agent_setRequestError(agtreq_info, current,
                                          PRIOT_ENDOFMIBVIEW);
                continue;
            }

            /*
             * * if data was found, make sure it has the column we want
             */
/* xxx-rks: add suport for sparse tables */

            /*
             * build new oid
             */
            TableArray_buildNewOid(reginfo, tblreq_info, row, current);

        } /** GETNEXT/GETBULK */
        else {
            Types_Index index;
            index.oids = tblreq_info->index_oid;
            index.len = tblreq_info->index_oid_len;

            row = (Types_Index*)CONTAINER_FIND(tad->table, &index);
            if (!row) {
                DEBUG_MSGTL(("table_array:get", "no row found\n"));
                Agent_setRequestError(agtreq_info, current,
                                          PRIOT_NOSUCHINSTANCE);
                continue;
            }
        } /** GET */

        /*
         * get the data
         */
        rc = tad->cb->get_value(current, row, tblreq_info);

    } /** for ( ... requests ... ) */

    return rc;
}

/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * SET procession functions                                           *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/

void
TableArray_groupRequests(AgentRequestInfo *agtreq_info,
               RequestInfo *requests,
               Container_Container *request_group, TableContainerData * tad)
{
    TableRequestInfo *tblreq_info;
    Types_Index *row, *tmp, index;
    RequestInfo *current;
    RequestGroup *g;
    RequestGroupItem *i;

    for (current = requests; current; current = current->next) {
        /*
         * skip anything that doesn't need processing.
         */
        if (current->processed != 0) {
            DEBUG_MSGTL(("table_array:group",
                        "already processed\n"));
            continue;
        }

        /*
         * 3.2.1 Setup and paranoia
         * *
         * * Get pointer to the table information for this request. This
         * * information was saved by table_helper_handler. When
         * * debugging, we double check a few assumptions. For example,
         * * the table_helper_handler should enforce column boundaries.
         */
        row = NULL;
        tblreq_info = Table_extractTableInfo(current);
        Assert_assert(tblreq_info->colnum <= tad->tblreg_info->max_column);

        /*
         * search for index
         */
        index.oids = tblreq_info->index_oid;
        index.len = tblreq_info->index_oid_len;
        tmp = (Types_Index*)CONTAINER_FIND(request_group, &index);
        if (tmp) {
            DEBUG_MSGTL(("table_array:group",
                        "    existing group:"));
            DEBUG_MSGOID(("table_array:group", index.oids,
                         index.len));
            DEBUG_MSG(("table_array:group", "\n"));
            g = (RequestGroup *) tmp;
            i = MEMORY_MALLOC_TYPEDEF(RequestGroupItem);
            if (i == NULL)
                return;
            i->ri = current;
            i->tri = tblreq_info;
            i->next = g->list;
            g->list = i;

            /** xxx-rks: store map of colnum to request */
            continue;
        }

        DEBUG_MSGTL(("table_array:group", "    new group"));
        DEBUG_MSGOID(("table_array:group", index.oids,
                     index.len));
        DEBUG_MSG(("table_array:group", "\n"));
        g = MEMORY_MALLOC_TYPEDEF(RequestGroup);
        i = MEMORY_MALLOC_TYPEDEF(RequestGroupItem);
        if (i == NULL || g == NULL) {
            MEMORY_FREE(i);
            MEMORY_FREE(g);
            return;
        }
        g->list = i;
        g->table = tad->table;
        i->ri = current;
        i->tri = tblreq_info;
        /** xxx-rks: store map of colnum to request */

        /*
         * search for row. all changes are made to the original row,
         * later, we'll make a copy in undo_info before we start processing.
         */
        row = g->existing_row = (Types_Index*)CONTAINER_FIND(tad->table, &index);
        if (!g->existing_row) {
            if (!tad->cb->create_row) {
                if(MODE_IS_SET(agtreq_info->mode))
                    Agent_setRequestError(agtreq_info, current,
                                              PRIOT_ERR_NOTWRITABLE);
                else
                    Agent_setRequestError(agtreq_info, current,
                                              PRIOT_NOSUCHINSTANCE);
                free(g);
                free(i);
                continue;
            }
            /** use undo_info temporarily */
            row = g->existing_row = tad->cb->create_row(&index);
            if (!row) {
                /* xxx-rks : parameter to create_row to allow
                 * for better error reporting. */
                Agent_setRequestError(agtreq_info, current,
                                          PRIOT_ERR_GENERR);
                free(g);
                free(i);
                continue;
            }
            g->row_created = 1;
        }

        g->index.oids = row->oids;
        g->index.len = row->len;

        CONTAINER_INSERT(request_group, g);

    } /** for( current ... ) */
}

static void
_TableArray_releaseRequestGroup(Types_Index *g, void *v)
{
    RequestGroupItem *tmp;
    RequestGroup *group = (RequestGroup *) g;

    if (!g)
        return;
    while (group->list) {
        tmp = group->list;
        group->list = tmp->next;
        free(tmp);
    }

    free(group);
}

static void
_TableArray_releaseRequestGroups(void *vp)
{
    Container_Container *c = (Container_Container*)vp;
    CONTAINER_FOR_EACH(c, (Container_FuncObjFunc*)
                       _TableArray_releaseRequestGroup, NULL);
    CONTAINER_FREE(c);
}

static void
_TableArray_processSetGroup(Types_Index *o, void *c)
{
    /* xxx-rks: should we continue processing after an error?? */
    SetContext           *context = (SetContext *) c;
    RequestGroup *ag = (RequestGroup *) o;
    int                    rc = PRIOT_ERR_NOERROR;

    switch (context->agtreq_info->mode) {

    case MODE_SET_RESERVE1:/** -> SET_RESERVE2 || SET_FREE */

        /*
         * if not a new row, save undo info
         */
        if (ag->row_created == 0) {
            if (context->tad->cb->duplicate_row)
                ag->undo_info = context->tad->cb->duplicate_row(ag->existing_row);
            else
                ag->undo_info = NULL;
            if (NULL == ag->undo_info) {
                rc = PRIOT_ERR_RESOURCEUNAVAILABLE;
                break;
            }
        }

        if (context->tad->cb->set_reserve1)
            context->tad->cb->set_reserve1(ag);
        break;

    case MODE_SET_RESERVE2:/** -> SET_ACTION || SET_FREE */
        if (context->tad->cb->set_reserve2)
            context->tad->cb->set_reserve2(ag);
        break;

    case MODE_SET_ACTION:/** -> SET_COMMIT || SET_UNDO */
        if (context->tad->cb->set_action)
            context->tad->cb->set_action(ag);
        break;

    case MODE_SET_COMMIT:/** FINAL CHANCE ON SUCCESS */
        if (ag->row_created == 0) {
            /*
             * this is an existing row, has it been deleted?
             */
            if (ag->row_deleted == 1) {
                DEBUG_MSGT((TABLE_ARRAY_NAME, "action: deleting row\n"));
                if (CONTAINER_REMOVE(ag->table, ag->existing_row) != 0) {
                    rc = PRIOT_ERR_COMMITFAILED;
                    break;
                }
            }
        } else if (ag->row_deleted == 0) {
            /*
             * new row (that hasn't been deleted) should be inserted
             */
            DEBUG_MSGT((TABLE_ARRAY_NAME, "action: inserting row\n"));
            if (CONTAINER_INSERT(ag->table, ag->existing_row) != 0) {
                rc = PRIOT_ERR_COMMITFAILED;
                break;
            }
        }

        if (context->tad->cb->set_commit)
            context->tad->cb->set_commit(ag);

        /** no more use for undo_info, so free it */
        if (ag->undo_info) {
            context->tad->cb->delete_row(ag->undo_info);
            ag->undo_info = NULL;
        }

        if ((ag->row_created == 0) && (ag->row_deleted == 1)) {
            context->tad->cb->delete_row(ag->existing_row);
            ag->existing_row = NULL;
        }
        break;

    case MODE_SET_FREE:/** FINAL CHANCE ON FAILURE */
        if (context->tad->cb->set_free)
            context->tad->cb->set_free(ag);

        /** no more use for undo_info, so free it */
        if (ag->row_created == 1) {
            if (context->tad->cb->delete_row)
                context->tad->cb->delete_row(ag->existing_row);
            ag->existing_row = NULL;
        }
        else {
            if (context->tad->cb->delete_row)
                context->tad->cb->delete_row(ag->undo_info);
            ag->undo_info = NULL;
        }
        break;

    case MODE_SET_UNDO:/** FINAL CHANCE ON FAILURE */
        /*
         * status already set - don't change it now
         */
        if (context->tad->cb->set_undo)
            context->tad->cb->set_undo(ag);

        /*
         * no more use for undo_info, so free it
         */
        if (ag->row_created == 0) {
            /*
             * restore old values
             */
            context->tad->cb->row_copy(ag->existing_row, ag->undo_info);
            context->tad->cb->delete_row(ag->undo_info);
            ag->undo_info = NULL;
        }
        else {
            context->tad->cb->delete_row(ag->existing_row);
            ag->existing_row = NULL;
        }
        break;

    default:
        Logger_log(LOGGER_PRIORITY_ERR, "unknown mode processing SET for "
                 "TableArray_helperHandler\n");
        rc = PRIOT_ERR_GENERR;
        break;
    }

    if (rc)
        Agent_setRequestError(context->agtreq_info,
                                  ag->list->ri, rc);

}

int
TableArray_processSetRequests(AgentRequestInfo *agtreq_info,
                     RequestInfo *requests,
                     TableContainerData * tad, char *handler_name)
{
    SetContext         context;
    Container_Container  *request_group;

    /*
     * create and save structure for set info
     */
    request_group = (Container_Container*) Agent_getListData
        (agtreq_info, handler_name);
    if (request_group == NULL) {
        Map *tmp;
        request_group = Container_find("requestGroup:"
                                               "tableContainer");
        request_group->compare = Container_compareIndex;
        request_group->nCompare = Container_nCompareIndex;

        DEBUG_MSGTL(("tableArray", "Grouping requests by oid\n"));

        tmp = Map_newElement(handler_name,
                                       request_group,
                                       _TableArray_releaseRequestGroups);
        Agent_addListData(agtreq_info, tmp);
        /*
         * group requests.
         */
        TableArray_groupRequests(agtreq_info, requests, request_group, tad);
    }

    /*
     * process each group one at a time
     */
    context.agtreq_info = agtreq_info;
    context.tad = tad;
    context.status = PRIOT_ERR_NOERROR;
    CONTAINER_FOR_EACH(request_group,
                       (Container_FuncObjFunc*)_TableArray_processSetGroup,
                       &context);

    return context.status;
}


/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * TableArray_helperHandler()                               *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/
int
TableArray_helperHandler(MibHandler *handler,
                                   HandlerRegistration *reginfo,
                                   AgentRequestInfo *agtreq_info,
                                   RequestInfo *requests)
{

    /*
     * First off, get our pointer from the handler. This
     * lets us get to the table registration information we
     * saved in get_table_array_handler(), as well as the
     * container where the actual table data is stored.
     */
    int             rc = PRIOT_ERR_NOERROR;
    TableContainerData *tad = (TableContainerData *)handler->myvoid;

    if (agtreq_info->mode < 0 || agtreq_info->mode > 5) {
        DEBUG_MSGTL(("tableArray", "Mode %d, Got request:\n",
                    agtreq_info->mode));
    } else {
        DEBUG_MSGTL(("tableArray", "Mode %s, Got request:\n",
                    _tableArray_modeName[agtreq_info->mode]));
    }

    if (MODE_IS_SET(agtreq_info->mode)) {
        /*
         * netsnmp_mutex_lock(&tad->lock);
         */
        rc = TableArray_processSetRequests(agtreq_info, requests,
                                  tad, handler->handler_name);
        /*
         * netsnmp_mutex_unlock(&tad->lock);
         */
    } else
        rc = TableArray_processGetRequests(reginfo, agtreq_info, requests, tad);

    if (rc != PRIOT_ERR_NOERROR) {
        DEBUG_MSGTL(("tableArray", "processing returned rc %d\n", rc));
    }

    /*
     * Now we've done our processing. If there is another handler below us,
     * call them.
     */
    if (handler->next) {
        rc = AgentHandler_callNextHandler(handler, reginfo, agtreq_info, requests);
        if (rc != PRIOT_ERR_NOERROR) {
            DEBUG_MSGTL(("tableArray", "next handler returned rc %d\n", rc));
        }
    }

    return rc;
}
