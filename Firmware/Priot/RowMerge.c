#include "RowMerge.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "System/Containers/Map.h"
#include "System/Util/Assert.h"

/** @defgroup row_merge row_merge
 *  Calls sub handlers with request for one row at a time.
 *  @ingroup utilities
 *  This helper splits a whole bunch of requests into chunks based on the row
 *  index that they refer to, and passes all requests for a given row to the lower handlers.
 *  This is useful for handlers that don't want to process multiple rows at the
 *  same time, but are happy to iterate through the request list for a single row.
 *  @{
 */

/** returns a row_merge handler that can be injected into a given
 *  handler chain.
 */
MibHandler*
RowMerge_getRowMergeHandler(int prefix_len)
{
    MibHandler *ret = NULL;
    ret = AgentHandler_createHandler("rowMerge",
                                  RowMerge_helperHandler);
    if (ret) {
        ret->myvoid = (void *)(intptr_t)prefix_len;
    }
    return ret;
}

/** functionally the same as calling AgentHandler_registerHandler() but also
 * injects a row_merge handler at the same time for you. */

int
RowMerge_registerRowMerge(HandlerRegistration *reginfo)
{
    AgentHandler_injectHandler(reginfo,
            RowMerge_getRowMergeHandler(reginfo->rootoid_len+1));
    return AgentHandler_registerHandler(reginfo);
}

static void
_RowMerge_rmStatusFree(void *mem)
{
    RowMergeStatus *rm_status = (RowMergeStatus*)mem;

    if (NULL != rm_status->saved_requests)
        free(rm_status->saved_requests);

    if (NULL != rm_status->saved_status)
        free(rm_status->saved_status);

    free(mem);
}


/** retrieve row_merge_status
 */
RowMergeStatus *
RowMerge_statusGet(HandlerRegistration *reginfo,
                             AgentRequestInfo *reqinfo,
                             int create_missing)
{
    RowMergeStatus *rm_status;
    char buf[64];
    int rc;

    /*
     * see if we've already been here
     */
    rc = snprintf(buf, sizeof(buf), "rowMerge:%p", reginfo);
    if ((-1 == rc) || ((size_t)rc >= sizeof(buf))) {
        Logger_log(LOGGER_PRIORITY_ERR,"error creating key\n");
        return NULL;
    }

    rm_status = (RowMergeStatus*)Agent_getListData(reqinfo, buf);
    if ((NULL == rm_status) && create_missing) {
        Map *data_list;

        rm_status = MEMORY_MALLOC_TYPEDEF(RowMergeStatus);
        if (NULL == rm_status) {
            Logger_log(LOGGER_PRIORITY_ERR,"error allocating memory\n");
            return NULL;
        }
        data_list = Map_newElement(buf, rm_status,
                                             _RowMerge_rmStatusFree);
        if (NULL == data_list) {
            free(rm_status);
            return NULL;
        }
        Agent_addListData(reqinfo, data_list);
    }

    return rm_status;
}

/** Determine if this is the first row
 *
 * returns 1 if this is the first row for this pass of the handler.
 */
int
RowMerge_statusFirst(HandlerRegistration *reginfo,
                               AgentRequestInfo *reqinfo)
{
    RowMergeStatus *rm_status;

    /*
     * find status
     */
    rm_status = RowMerge_statusGet(reginfo, reqinfo, 0);
    if (NULL == rm_status)
        return 0;

    return (rm_status->count == 1) ? 1 : (rm_status->current == 1);
}

/** Determine if this is the last row
 *
 * returns 1 if this is the last row for this pass of the handler.
 */
int
RowMerge_statusLast(HandlerRegistration *reginfo,
                              AgentRequestInfo *reqinfo)
{
    RowMergeStatus *rm_status;

    /*
     * find status
     */
    rm_status = RowMerge_statusGet(reginfo, reqinfo, 0);
    if (NULL == rm_status)
        return 0;

    return (rm_status->count == 1) ? 1 :
        (rm_status->current == rm_status->rows);
}


#define ROW_MERGE_WAITING 0
#define ROW_MERGE_ACTIVE  1
#define ROW_MERGE_DONE    2
#define ROW_MERGE_HEAD    3

/** Implements the row_merge handler */
int
RowMerge_helperHandler( MibHandler*          handler,
                        HandlerRegistration* reginfo,
                        AgentRequestInfo*    reqinfo,
                        RequestInfo*         requests )
{
    RequestInfo *request, **saved_requests;
    char *saved_status;
    RowMergeStatus *rm_status;
    int i, j, ret, tail, count, final_rc = PRIOT_ERR_NOERROR;

    /*
     * Use the prefix length as supplied during registration, rather
     *  than trying to second-guess what the MIB implementer wanted.
     */
    int SKIP_OID = (int)(intptr_t)handler->myvoid;

    DEBUG_MSGTL(("helper:row_merge", "Got request (%d): ", SKIP_OID));
    DEBUG_MSGOID(("helper:row_merge", reginfo->rootoid, reginfo->rootoid_len));
    DEBUG_MSG(("helper:row_merge", "\n"));

    /*
     * find or create status
     */
    rm_status = RowMerge_statusGet(reginfo, reqinfo, 1);

    /*
     * Count the requests, and set up an array to keep
     *  track of the original order.
     */
    for (count = 0, request = requests; request; request = request->next) {
        DEBUG_IF("helper:row_merge") {
            DEBUG_MSGTL(("helper:row_merge", "  got varbind: "));
            DEBUG_MSGOID(("helper:row_merge", request->requestvb->name,
                         request->requestvb->nameLength));
            DEBUG_MSG(("helper:row_merge", "\n"));
        }
        count++;
    }

    /*
     * Optimization: skip all this if there is just one request
     */
    if(count == 1) {
        rm_status->count = count;
        if (requests->processed)
            return PRIOT_ERR_NOERROR;
        return AgentHandler_callNextHandler(handler, reginfo, reqinfo, requests);
    }

    /*
     * we really should only have to do this once, instead of every pass.
     * as a precaution, we'll do it every time, but put in some asserts
     * to see if we have to.
     */
    /*
     * if the count changed, re-do everything
     */
    if ((0 != rm_status->count) && (rm_status->count != count)) {
        /*
         * ok, i know next/bulk can cause this condition. Probably
         * GET, too. need to rethink this mode counting. maybe
         * add the mode to the rm_status structure? xxx-rks
         */
        if ((reqinfo->mode != MODE_GET) &&
            (reqinfo->mode != MODE_GETNEXT) &&
            (reqinfo->mode != MODE_GETBULK)) {
            Assert_assert((NULL != rm_status->saved_requests) &&
                           (NULL != rm_status->saved_status));
        }
        DEBUG_MSGTL(("helper:row_merge", "count changed! do over...\n"));

        MEMORY_FREE(rm_status->saved_requests);
        MEMORY_FREE(rm_status->saved_status);

        rm_status->count = 0;
        rm_status->rows = 0;
    }

    if (0 == rm_status->count) {
        /*
         * allocate memory for saved structure
         */
        rm_status->saved_requests =
            (RequestInfo**)calloc(count+1,
                                           sizeof(RequestInfo*));
        rm_status->saved_status = (char*)calloc(count,sizeof(char));
    }

    saved_status = rm_status->saved_status;
    saved_requests = rm_status->saved_requests;

    /*
     * set up saved requests, and set any processed requests to done
     */
    i = 0;
    for (request = requests; request; request = request->next, i++) {
        if (request->processed) {
            saved_status[i] = ROW_MERGE_DONE;
            DEBUG_MSGTL(("helper:row_merge", "  skipping processed oid: "));
            DEBUG_MSGOID(("helper:row_merge", request->requestvb->name,
                         request->requestvb->nameLength));
            DEBUG_MSG(("helper:row_merge", "\n"));
        }
        else
            saved_status[i] = ROW_MERGE_WAITING;
        if (0 != rm_status->count)
            Assert_assert(saved_requests[i] == request);
        saved_requests[i] = request;
        saved_requests[i]->prev = NULL;
    }
    saved_requests[i] = NULL;

    /*
     * Note that saved_requests[count] is valid
     *    (because of the 'count+1' in the calloc above),
     * but NULL (since it's past the end of the list).
     * This simplifies the re-linking later.
     */

    /*
     * Work through the (unprocessed) requests in order.
     * For each of these, search the rest of the list for any
     *   matching indexes, and link them into a new list.
     */
    for (i=0; i<count; i++) {
    if (saved_status[i] != ROW_MERGE_WAITING)
        continue;

        if (0 == rm_status->count)
            rm_status->rows++;
        DEBUG_MSGTL(("helper:row_merge", " row %d oid[%d]: ", rm_status->rows, i));
        DEBUG_MSGOID(("helper:row_merge", saved_requests[i]->requestvb->name,
                     saved_requests[i]->requestvb->nameLength));
        DEBUG_MSG(("helper:row_merge", "\n"));

    saved_requests[i]->next = NULL;
    saved_status[i] = ROW_MERGE_HEAD;
    tail = i;
        for (j=i+1; j<count; j++) {
        if (saved_status[j] != ROW_MERGE_WAITING)
            continue;

            DEBUG_MSGTL(("helper:row_merge", "? oid[%d]: ", j));
            DEBUG_MSGOID(("helper:row_merge",
                         saved_requests[j]->requestvb->name,
                         saved_requests[j]->requestvb->nameLength));
            if (!Api_oidCompare(
                    saved_requests[i]->requestvb->name+SKIP_OID,
                    saved_requests[i]->requestvb->nameLength-SKIP_OID,
                    saved_requests[j]->requestvb->name+SKIP_OID,
                    saved_requests[j]->requestvb->nameLength-SKIP_OID)) {
                DEBUG_MSG(("helper:row_merge", " match\n"));
                saved_requests[tail]->next = saved_requests[j];
                saved_requests[j]->next    = NULL;
                saved_requests[j]->prev = saved_requests[tail];
            saved_status[j] = ROW_MERGE_ACTIVE;
            tail = j;
            }
            else
                DEBUG_MSG(("helper:row_merge", " no match\n"));
        }
    }

    /*
     * not that we have a list for each row, call next handler...
     */
    if (0 == rm_status->count)
        rm_status->count = count;
    rm_status->current = 0;
    for (i=0; i<count; i++) {
    if (saved_status[i] != ROW_MERGE_HEAD)
        continue;

        /*
         * found the head of a new row,
         * call the next handler with this list
         */
        rm_status->current++;
        ret = AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                            saved_requests[i]);
        if (ret != PRIOT_ERR_NOERROR) {
            Logger_log(LOGGER_PRIORITY_WARNING,
                     "bad rc (%d) from next handler in row_merge\n", ret);
            if (PRIOT_ERR_NOERROR == final_rc)
                final_rc = ret;
        }
    }

    /*
     * restore original linked list
     */
    for (i=0; i<count; i++) {
    saved_requests[i]->next = saved_requests[i+1];
        if (i>0)
        saved_requests[i]->prev = saved_requests[i-1];
    }

    return final_rc;
}

/**
 *  initializes the row_merge helper which then registers a row_merge
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
void
RowMerge_initRowMerge(void)
{
    AgentHandler_registerHandlerByName("rowMerge",
                                     RowMerge_getRowMergeHandler(-1));
}

