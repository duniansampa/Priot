#include "TableContainer.h"
#include "Logger.h"
#include "Api.h"
#include "AgentRegistry.h"
#include "Assert.h"
#include "Mib.h"
#include "Enum.h"

/*
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_BEGIN        -1
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_RESERVE1     0
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_RESERVE2     1
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_ACTION       2
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_COMMIT       3
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_FREE         4
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_UNDO         5
 */

/*
 * PRIVATE structure for holding important info for each table.
 */
typedef struct ContainerTableData_s {

    /** Number of handlers whose myvoid pointer points to this structure. */
    int refcnt;

   /** registration info for the table */
    TableRegistrationInfo *tblreg_info;

   /** container for the table rows */
    Container_Container          *table;

    /*
     * mutex_type                lock;
     */

   /* what type of key do we want? */
   char            key_type;

} ContainerTableData;

/** @defgroup table_container table_container
 *  Helps you implement a table when data can be found via a Container_Container.
 *  @ingroup table
 *
 *  The table_container handler is used (automatically) in conjuntion
 *  with the @link table table@endlink handler.
 *
 *  This handler will use the index information provided by
 *  the @link table @endlink handler to find the row needed to process
 *  the request.
 *
 *  The container must use one of 3 key types. It is the sub-handler's
 *  responsibility to ensure that the container and key type match (unless
 *  neither is specified, in which case a default will be used.)
 *
 *  The current key types are:
 *
 *    TABLE_CONTAINER_KEY_NETSNMP_INDEX
 *        The container should do comparisons based on a key that may be cast
 *        to a netsnmp index (Types_Index *). This index contains only the
 *        index portion of the OID, not the entire OID.
 *
 *    TABLE_CONTAINER_KEY_VARBIND_INDEX
 *        The container should do comparisons based on a key that may be cast
 *        to a netsnmp variable list (Types_VariableList *). This variable
 *        list will contain one varbind for each index component.
 *
 *    TABLE_CONTAINER_KEY_VARBIND_RAW    (NOTE: unimplemented)
 *        While not yet implemented, future plans include passing the request
 *        varbind with the full OID to a container.
 *
 *  If a key type is not specified at registration time, the default key type
 *  of TABLE_CONTAINER_KEY_NETSNMP_INDEX will be used. If a container is
 *  provided, or the handler name is aliased to a container type, the container
 *  must use a netsnmp index.
 *
 *  If no container is provided, a lookup will be made based on the
 *  sub-handler's name, or if that isn't found, "tableContainer". The
 *  table_container key type will be Types_Index.
 *
 *  The container must, at a minimum, implement find and find_next. If a NULL
 *  key is passed to the container, it must return the first item, if any.
 *  All containers provided by net-snmp fulfil this requirement.
 *
 *  This handler will only register to process 'data lookup' modes. In
 *  traditional net-snmp modes, that is any GET-like mode (GET, GET-NEXT,
 *  GET-BULK) or the first phase of a SET (RESERVE1). In the new baby-steps
 *  mode, DATA_LOOKUP is it's own mode, and is a pre-cursor to other modes.
 *
 *  When called, the handler will call the appropriate container method
 *  with the appropriate key type. If a row was not found, the result depends
 *  on the mode.
 *
 *  GET Processing
 *    An exact match must be found. If one is not, the error NOSUCHINSTANCE
 *    is set.
 *
 *  GET-NEXT / GET-BULK
 *    If no row is found, the column number will be increased (using any
 *    valid_columns structure that may have been provided), and the first row
 *    will be retrieved. If no first row is found, the processed flag will be
 *    set, so that the sub-handler can skip any processing related to the
 *    request. The agent will notice this unsatisfied request, and attempt to
 *    pass it to the next appropriate handler.
 *
 *  SET
 *    If the hander did not register with the HANDLER_CAN_NOT_CREATE flag
 *    set in the registration modes, it is assumed that this is a row
 *    creation request and a NULL row is added to the request's data list.
 *    The sub-handler is responsbile for dealing with any row creation
 *    contraints and inserting any newly created rows into the container
 *    and the request's data list.
 *
 *  If a row is found, it will be inserted into
 *  the request's data list. The sub-handler may retrieve it by calling
 *      netsnmp_container_table_extract_context(request); *
 *  NOTE NOTE NOTE:
 *
 *  This helper and it's API are still being tested and are subject to change.
 *
 * @{
 */

static int
_TableContainer_handler(MibHandler *handler,
                         HandlerRegistration *reginfo,
                         AgentRequestInfo *agtreq_info,
                         RequestInfo *requests);

static void *
_TableContainer_findNextRow(Container_Container *c,
               TableRequestInfo *tblreq,
               void * key);

/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * PUBLIC Registration functions                                      *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/

/* ==================================
 *
 * Container Table API: Table maintenance
 *
 * ================================== */

ContainerTableData *
TableContainer_createTable( const char *name,
                                 Container_Container *container, long flags )
{
    ContainerTableData *table;

    table = TOOLS_MALLOC_TYPEDEF(ContainerTableData);
    if (!table)
        return NULL;
    if (container)
        table->table = container;
    else {
        table->table = Container_find("tableContainer");
        if (!table->table) {
            TOOLS_FREE(table);
            return NULL;
        }
    }

    if (flags)
        table->key_type = (char)(flags & 0x03);  /* Use lowest two bits */
    else
        table->key_type = TABLE_CONTAINER_KEY_NETSNMP_INDEX;

    if (!table->table->compare)
         table->table->compare  = Container_compareIndex;
    if (!table->table->nCompare)
         table->table->nCompare = Container_nCompareIndex;

    return table;
}

void
TableContainer_deleteTable( ContainerTableData *table )
{
    if (!table)
       return;

    if (table->table)
       CONTAINER_FREE(table->table);

    TOOLS_FREE(table);
    return;
}

    /*
     * The various standalone row operation routines
     *    (create/clone/copy/delete)
     * will be specific to a particular table,
     *    so can't be implemented here.
     */

int
TableContainer_addRow( ContainerTableData *table, Types_Index *row )
{
    if (!table || !table->table || !row)
        return -1;
    CONTAINER_INSERT( table->table, row );
    return 0;
}

Types_Index *
TableContainer_removeRow( ContainerTableData *table, Types_Index *row )
{
    if (!table || !table->table || !row)
        return NULL;
    CONTAINER_REMOVE( table->table, row );
    return NULL;
}

int
TableContainer_replaceRow( ContainerTableData *table,
                                Types_Index *old_row, Types_Index *new_row )
{
    if (!table || !table->table || !old_row || !new_row)
        return -1;
    TableContainer_removeRow( table, old_row );
    TableContainer_addRow(    table, new_row );
    return 0;
}

    /* netsnmp_tcontainer_remove_delete_row() will be table-specific too */


/* ==================================
 *
 * Container Table API: MIB maintenance
 *
 * ================================== */

static ContainerTableData *
_TableContainer_dataClone(ContainerTableData *tad)
{
    ++tad->refcnt;
    return tad;
}

static void
_TableContainer_dataFree(ContainerTableData *tad)
{
    if (--tad->refcnt == 0)
    free(tad);
}

/** returns a MibHandler object for the table_container helper */
MibHandler *
TableContainer_handlerGet(TableRegistrationInfo *tabreg,
                                    Container_Container *container, char key_type)
{
    ContainerTableData *tad;
    MibHandler *handler;

    if (NULL == tabreg) {
        Logger_log(LOGGER_PRIORITY_ERR, "bad param in TableContainer_register\n");
        return NULL;
    }

    tad = TOOLS_MALLOC_TYPEDEF(ContainerTableData);
    handler = AgentHandler_createHandler("tableContainer",
                                     _TableContainer_handler);
    if((NULL == tad) || (NULL == handler)) {
        if(tad) free(tad); /* TOOLS_FREE wasted on locals */
        if(handler) free(handler); /* TOOLS_FREE wasted on locals */
        Logger_log(LOGGER_PRIORITY_ERR,
                 "malloc failure in TableContainer_register\n");
        return NULL;
    }

    tad->refcnt = 1;
    tad->tblreg_info = tabreg;  /* we need it too, but it really is not ours */
    if(key_type)
        tad->key_type = key_type;
    else
        tad->key_type = TABLE_CONTAINER_KEY_NETSNMP_INDEX;

    if(NULL == container)
        container = Container_find("tableContainer");
    tad->table = container;

    if (NULL==container->compare)
        container->compare = Container_compareIndex;
    if (NULL==container->nCompare)
        container->nCompare = Container_nCompareIndex;

    handler->myvoid = (void*)tad;
    handler->data_clone = (void *(*)(void *))_TableContainer_dataClone;
    handler->data_free = (void (*)(void *))_TableContainer_dataFree;
    handler->flags |= MIB_HANDLER_AUTO_NEXT;

    return handler;
}

int
TableContainer_register(HandlerRegistration *reginfo,
                                 TableRegistrationInfo *tabreg,
                                 Container_Container *container, char key_type )
{
    MibHandler *handler;

    if ((NULL == reginfo) || (NULL == reginfo->handler) || (NULL == tabreg)) {
        Logger_log(LOGGER_PRIORITY_ERR, "bad param in TableContainer_register\n");
        return ErrorCode_GENERR;
    }

    if (NULL==container)
        container = Container_find(reginfo->handlerName);

    handler = TableContainer_handlerGet(tabreg, container, key_type);
    AgentHandler_injectHandler(reginfo, handler );

    return Table_registerTable(reginfo, tabreg);
}

int
TableContainer_unregister(HandlerRegistration *reginfo)
{
    ContainerTableData *tad;

    if (!reginfo)
        return MIB_UNREGISTRATION_FAILED;
    tad = (ContainerTableData *)
        AgentHandler_findHandlerDataByName(reginfo, "tableContainer");
    if (tad) {
        CONTAINER_FREE( tad->table );
        tad->table = NULL;
    /*
     * Note: don't free the memory tad points at here - that is done
     * by TableContainer_dataFree().
     */
    }
    return Table_unregisterTable( reginfo );
}

/** retrieve the container used by the table_container helper */
Container_Container*
TableContainer_containerExtract(RequestInfo *request)
{
    return (Container_Container *)
         AgentHandler_requestGetListData(request, TABLE_CONTAINER_CONTAINER);
}


/** inserts a newly created table_container entry into a request list */
void
TableContainer_rowInsert(RequestInfo *request,
                                   Types_Index        *row)
{
    RequestInfo       *req;
    TableRequestInfo *table_info = NULL;
    Types_VariableList      *this_index = NULL;
    Types_VariableList      *that_index = NULL;
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
        if (req->processed)
            continue;

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
                DataList_create(TABLE_CONTAINER_ROW, row, NULL));
        }
    }
}

/** find the context data used by the table_container helper */

void *
TableContainer_rowExtract(RequestInfo *request)
{
    /*
     * NOTE: this function must match in table_container.c and table_container.h.
     *       if you change one, change them both!
     */
    return AgentHandler_requestGetListData(request, TABLE_CONTAINER_ROW);
}

void *
TableContainer_extractContext(RequestInfo *request)
{
    /*
     * NOTE: this function must match in table_container.c and table_container.h.
     *       if you change one, change them both!
     */
    return AgentHandler_requestGetListData(request, TABLE_CONTAINER_ROW);
}

/** removes a table_container entry from a request list */
void
TableContainer_rowRemove(RequestInfo *request,
                                   Types_Index        *row)
{
    RequestInfo       *req;
    TableRequestInfo *table_info = NULL;
    Types_VariableList      *this_index = NULL;
    Types_VariableList      *that_index = NULL;
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
        if (req->processed)
            continue;

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
            AgentHandler_requestRemoveListData(req, TABLE_CONTAINER_ROW);
        }
    }
}

/** @cond */
/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * DATA LOOKUP functions                                              *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/
static inline void
_TableContainer_setKey( ContainerTableData * tad, RequestInfo *request,
          TableRequestInfo *tblreq_info,
          void **key, Types_Index *index )
{
    if (TABLE_CONTAINER_KEY_NETSNMP_INDEX == tad->key_type) {
        index->oids = tblreq_info->index_oid;
        index->len = tblreq_info->index_oid_len;
        *key = index;
    }
    else if (TABLE_CONTAINER_KEY_VARBIND_INDEX == tad->key_type) {
        *key = tblreq_info->indexes;
    }
    else
        *key = NULL;
}


static inline void
_TableContainer_dataLookup(HandlerRegistration *reginfo,
            AgentRequestInfo *agtreq_info,
            RequestInfo *request, ContainerTableData * tad)
{
    Types_Index *row = NULL;
    TableRequestInfo *tblreq_info;
    Types_VariableList *var;
    Types_Index index;
    void *key;

    var = request->requestvb;

    DEBUG_IF("tableContainer") {
        DEBUG_MSGTL(("tableContainer", "  data_lookup oid:"));
        DEBUG_MSGOID(("tableContainer", var->name, var->nameLength));
        DEBUG_MSG(("tableContainer", "\n"));
    }

    /*
     * Get pointer to the table information for this request. This
     * information was saved by table_helper_handler.
     */
    tblreq_info = Table_extractTableInfo(request);
    /** the table_helper_handler should enforce column boundaries. */
    Assert_assert((NULL != tblreq_info) &&
                   (tblreq_info->colnum <= tad->tblreg_info->max_column));

    if ((agtreq_info->mode == MODE_GETNEXT) ||
        (agtreq_info->mode == MODE_GETBULK)) {
        /*
         * find the row. This will automatically move to the next
         * column, if necessary.
         */
        _TableContainer_setKey( tad, request, tblreq_info, &key, &index );
        row = (Types_Index*)_TableContainer_findNextRow(tad->table, tblreq_info, key);
        if (row) {
            /*
             * update indexes in tblreq_info (index & varbind),
             * then update request varbind oid
             */
            if(TABLE_CONTAINER_KEY_NETSNMP_INDEX == tad->key_type) {
                tblreq_info->index_oid_len = row->len;
                memcpy(tblreq_info->index_oid, row->oids,
                       row->len * sizeof(oid));
                Table_updateVariableListFromIndex(tblreq_info);
            }
            else if (TABLE_CONTAINER_KEY_VARBIND_INDEX == tad->key_type) {
                /** xxx-rks: shouldn't tblreq_info->indexes be updated
                    before we call this?? */
                Table_updateIndexesFromVariableList(tblreq_info);
            }

            if (TABLE_CONTAINER_KEY_VARBIND_RAW != tad->key_type) {
                Table_buildOidFromIndex(reginfo, request,
                                                   tblreq_info);
            }
        }
        else {
            /*
             * no results found. Flag the request so lower handlers will
             * ignore it, but it is not an error - getnext will move
             * on to another handler to process this request.
             */
            Agent_setRequestError(agtreq_info, request, PRIOT_ENDOFMIBVIEW);
            DEBUG_MSGTL(("tableContainer", "no row found\n"));
        }
    } /** GETNEXT/GETBULK */
    else {

        _TableContainer_setKey( tad, request, tblreq_info, &key, &index );
        row = (Types_Index*)CONTAINER_FIND(tad->table, key);
        if (NULL == row) {
            /*
             * not results found. For a get, that is an error
             */
            DEBUG_MSGTL(("tableContainer", "no row found\n"));
            if((agtreq_info->mode != MODE_SET_RESERVE1) || /* get */
               (reginfo->modes & HANDLER_CAN_NOT_CREATE)) { /* no create */
                Agent_setRequestError(agtreq_info, request,
                                          PRIOT_NOSUCHINSTANCE);
            }
        }
    } /** GET/SET */

    /*
     * save the data and table in the request.
     */
    if (PRIOT_ENDOFMIBVIEW != request->requestvb->type) {
        if (NULL != row)
            AgentHandler_requestAddListData(request,
                                          DataList_create
                                          (TABLE_CONTAINER_ROW,
                                           row, NULL));
        AgentHandler_requestAddListData(request,
                                      DataList_create
                                      (TABLE_CONTAINER_CONTAINER,
                                       tad->table, NULL));
    }
}

/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * netsnmp_table_container_helper_handler()                           *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/
static int
_TableContainer_handler(MibHandler *handler,
                         HandlerRegistration *reginfo,
                         AgentRequestInfo *agtreq_info,
                         RequestInfo *requests)
{
    int             rc = PRIOT_ERR_NOERROR;
    int             oldmode, need_processing = 0;
    ContainerTableData *tad;

    /** sanity checks */
    Assert_assert((NULL != handler) && (NULL != handler->myvoid));
    Assert_assert((NULL != reginfo) && (NULL != agtreq_info));

    DEBUG_MSGTL(("tableContainer", "Mode %s, Got request:\n",
                Enum_seFindLabelInSlist("agentMode",agtreq_info->mode)));

    /*
     * First off, get our pointer from the handler. This
     * lets us get to the table registration information we
     * saved in get_table_container_handler(), as well as the
     * container where the actual table data is stored.
     */
    tad = (ContainerTableData *)handler->myvoid;

    /*
     * only do data lookup for first pass
     *
     * xxx-rks: this should really be handled up one level. we should
     * be able to say what modes we want to be called for during table
     * registration.
     */
    oldmode = agtreq_info->mode;
    if(MODE_IS_GET(oldmode)
       || (MODE_SET_RESERVE1 == oldmode)
        ) {
        RequestInfo *curr_request;
        /*
         * Loop through each of the requests, and
         * try to find the appropriate row from the container.
         */
        for (curr_request = requests; curr_request; curr_request = curr_request->next) {
            /*
             * skip anything that doesn't need processing.
             */
            if (curr_request->processed != 0) {
                DEBUG_MSGTL(("tableContainer", "already processed\n"));
                continue;
            }

            /*
             * find data for this request
             */
            _TableContainer_dataLookup(reginfo, agtreq_info, curr_request, tad);

            if(curr_request->processed)
                continue;

            ++need_processing;
        } /** for ( ... requests ... ) */
    }

    /*
     * send GET instead of GETNEXT to sub-handlers
     * xxx-rks: again, this should be handled further up.
     */
    if ((oldmode == MODE_GETNEXT) && (handler->next)) {
        /*
         * tell agent handlder not to auto call next handler
         */
        handler->flags |= MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE;

        /*
         * if we found rows to process, pretend to be a get request
         * and call handler below us.
         */
        if(need_processing > 0) {
            agtreq_info->mode = MODE_GET;
            rc = AgentHandler_callNextHandler(handler, reginfo, agtreq_info,
                                           requests);
            if (rc != PRIOT_ERR_NOERROR) {
                DEBUG_MSGTL(("tableContainer",
                            "next handler returned %d\n", rc));
            }

            agtreq_info->mode = oldmode; /* restore saved mode */
        }
    }

    return rc;
}
/** @endcond */


/* ==================================
 *
 * Container Table API: Row operations
 *
 * ================================== */

static void *
_TableContainer_findNextRow(Container_Container *c,
               TableRequestInfo *tblreq,
               void * key)
{
    void *row = NULL;

    if (!c || !tblreq || !tblreq->reg_info ) {
        Logger_log(LOGGER_PRIORITY_ERR,"TableContainer_findNextRow param error\n");
        return NULL;
    }

    /*
     * table helper should have made sure we aren't below our minimum column
     */
    Assert_assert(tblreq->colnum >= tblreq->reg_info->min_column);

    /*
     * if no indexes then use first row.
     */
    if(tblreq->number_indexes == 0) {
        row = CONTAINER_FIRST(c);
    } else {

        if(NULL == key) {
            Types_Index index;
            index.oids = tblreq->index_oid;
            index.len = tblreq->index_oid_len;
            row = CONTAINER_NEXT(c, &index);
        }
        else
            row = CONTAINER_NEXT(c, key);

        /*
         * we don't have a row, but we might be at the end of a
         * column, so try the next column.
         */
        if (NULL == row) {
            /*
             * don't set tblreq next_col unless we know there is one,
             * so we don't mess up table handler sparse table processing.
             */
            oid next_col = Table_nextColumn(tblreq);
            if (0 != next_col) {
                tblreq->colnum = next_col;
                row = CONTAINER_FIRST(c);
            }
        }
    }

    return row;
}

/**
 * deprecated, backwards compatability only
 *
 * expected impact to remove: none
 *  - used between helpers, shouldn't have been used by end users
 *
 * replacement: none
 *  - never should have been a public method in the first place
 */
Types_Index *
TableContainer_indexFindNextRow(Container_Container *c,
                                  TableRequestInfo *tblreq)
{
    return (Types_Index*)_TableContainer_findNextRow(c, tblreq, NULL );
}

/* ==================================
 *
 * Container Table API: Index operations
 *
 * ================================== */
