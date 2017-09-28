#include "TableRow.h"
#include "Logger.h"
#include "Table.h"
#include "Api.h"
#include "Mib.h"
#include "Assert.h"
#include "Debug.h"
#include "Enum.h"

#define TABLE_ROW_DATA  "table_row"

/*
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_BEGIN        -1
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_RESERVE1     0
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_RESERVE2     1
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_ACTION       2
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_COMMIT       3
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_FREE         4
 * snmp.h:#define SNMP_MSG_INTERNAL_SET_UNDO         5
 */

/** @defgroup table_row table_row
 *  Helps you implement a table shared across two or more subagents,
 *  or otherwise split into individual row slices.
 *  @ingroup table
 *
 * @{
 */

static NodeHandlerFT _TableRow_handler;
static NodeHandlerFT _TableRow_defaultHandler;

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
 * Table Row API: Table maintenance
 *
 * This helper doesn't operate with the complete
 *   table, so these routines are not relevant.
 *
 * ================================== */


/* ==================================
 *
 * Table Row API: MIB maintenance
 *
 * ================================== */

/** returns a MibHandler object for the table_container helper */
MibHandler *
TableRow_handlerGet(void *row)
{
    MibHandler *handler;

    handler = AgentHandler_createHandler("table_row",
                                     _TableRow_handler);
    if(NULL == handler) {
        Logger_log(LOGGER_PRIORITY_ERR,
                 "malloc failure in TableRow_register\n");
        return NULL;
    }

    handler->myvoid = (void*)row;
    handler->flags |= MIB_HANDLER_INSTANCE;
 /* handler->flags |= MIB_HANDLER_AUTO_NEXT;  ??? */

    return handler;
}

int
TableRow_register(HandlerRegistration *reginfo,
                           TableRegistrationInfo *tabreg,
                           void *row, Types_VariableList *index)
{
    HandlerRegistration *reg2;
    MibHandler *handler;
    oid    row_oid[ASN01_MAX_OID_LEN];
    size_t row_oid_len, len;
    char   tmp[TOOLS_MAXBUF_MEDIUM];

    if ((NULL == reginfo) || (NULL == reginfo->handler) || (NULL == tabreg)) {
        Logger_log(LOGGER_PRIORITY_ERR, "bad param in TableRow_register\n");
        return ErrorCode_GENERR;
    }

        /*
         *   The first table_row invoked for a particular table should
         * register the full table as well, with a default handler to
         * process requests for non-existent (or incomplete) rows.
         *
         *   Subsequent table_row registrations attempting to set up
         * this default handler would fail - preferably silently!
         */
    snprintf(tmp, sizeof(tmp), "%s_table", reginfo->handlerName);
    reg2 = AgentHandler_createHandlerRegistration(
              tmp,     _TableRow_defaultHandler,
              reginfo->rootoid, reginfo->rootoid_len,
              reginfo->modes);
    Table_registerTable(reg2, tabreg);  /* Ignore return value */

        /*
         * Adjust the OID being registered, to take account
         * of the indexes and column range provided....
         */
    row_oid_len = reginfo->rootoid_len;
    memcpy( row_oid, (u_char *) reginfo->rootoid, row_oid_len * sizeof(oid));
    row_oid[row_oid_len++] = 1;   /* tableEntry */
    row_oid[row_oid_len++] = tabreg->min_column;
    reginfo->range_ubound  = tabreg->max_column;
    reginfo->range_subid   = row_oid_len-1;
    Mib_buildOidNoalloc(&row_oid[row_oid_len],
                      ASN01_MAX_OID_LEN-row_oid_len, &len, NULL, 0, index);
    row_oid_len += len;
    free(reginfo->rootoid);
    reginfo->rootoid = Api_duplicateObjid(row_oid, row_oid_len);
    reginfo->rootoid_len = row_oid_len;


        /*
         * ... insert a minimal handler ...
         */
    handler = TableRow_handlerGet(row);
    AgentHandler_injectHandler(reginfo, handler );

        /*
         * ... and register the row
         */
    return AgentHandler_registerHandler(reginfo);
}


/** return the row data structure supplied to the table_row helper */
void *
TableRow_extract(RequestInfo *request)
{
    return AgentHandler_requestGetListData(request, TABLE_ROW_DATA);
}
/** @cond */

/**********************************************************************
 **********************************************************************
 *                                                                    *
 *                                                                    *
 * netsnmp_table_row_helper_handler()                           *
 *                                                                    *
 *                                                                    *
 **********************************************************************
 **********************************************************************/

static int
_TableRow_handler(MibHandler          *handler,
                   HandlerRegistration *reginfo,
                   AgentRequestInfo   *reqinfo,
                   RequestInfo         *requests)
{
    int             rc = PRIOT_ERR_NOERROR;
    RequestInfo *req;
    void                 *row;

    /** sanity checks */
    Assert_assert((NULL != handler) && (NULL != handler->myvoid));
    Assert_assert((NULL != reginfo) && (NULL != reqinfo));

    DEBUG_MSGTL(("table_row", "Mode %s, Got request:\n",
                Enum_seFindLabelInSlist("agent_mode",reqinfo->mode)));

    /*
     * First off, get our pointer from the handler.
     * This contains the row that was actually registered.
     * Make this available for each of the requests passed in.
     */
    row = handler->myvoid;
    for (req = requests; req; req=req->next)
        AgentHandler_requestAddListData(req,
                DataList_create(TABLE_ROW_DATA, row, NULL));

    /*
     * Then call the next handler, to actually process the request
     */
    rc = AgentHandler_callNextHandler(handler, reginfo, reqinfo, requests);
    if (rc != PRIOT_ERR_NOERROR) {
        DEBUG_MSGTL(("table_row", "next handler returned %d\n", rc));
    }

    return rc;
}

static int
_TableRow_defaultHandler( MibHandler  *handler,
                          HandlerRegistration *reginfo,
                          AgentRequestInfo   *reqinfo,
                          RequestInfo         *requests)
{
    RequestInfo       *req;
    TableRequestInfo *table_info;
    TableRegistrationInfo *tabreg;

    tabreg = Table_findTableRegistrationInfo(reginfo);
    for ( req=requests; req; req=req->next ) {
        table_info = Table_extractTableInfo( req );
        if (( table_info->colnum >= tabreg->min_column ) ||
            ( table_info->colnum <= tabreg->max_column )) {
            Agent_setRequestError( reqinfo, req, PRIOT_NOSUCHINSTANCE );
        } else {
            Agent_setRequestError( reqinfo, req, PRIOT_NOSUCHOBJECT );
        }
    }
    return PRIOT_ERR_NOERROR;
}
/** @endcond */


/* ==================================
 *
 * Table Row API: Row operations
 *
 * This helper doesn't operate with the complete
 *   table, so these routines are not relevant.
 *
 * ================================== */


/* ==================================
 *
 * Table Row API: Index operations
 *
 * This helper doesn't operate with the complete
 *   table, so these routines are not relevant.
 *
 * ================================== */
