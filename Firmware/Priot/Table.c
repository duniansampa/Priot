#include "Table.h"
#include "System/Util/Assert.h"
#include "Client.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "System/Containers/MapList.h"
#include "Mib.h"
#include "System/Util/Utilities.h"

static void _Table_helperCleanup( AgentRequestInfo* reqinfo,
    RequestInfo* request,
    int status );
static void _Table_dataFreeFunc( void* data );
static int
_Table_sparseTableHelperHandler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests );

/** @defgroup table table
 *  Helps you implement a table.
 *  @ingroup handler
 *
 *  This handler helps you implement a table by doing some of the
 *  processing for you.
 *
 *  This handler truly shows the power of the new handler mechanism.
 *  By creating a table handler and injecting it into your calling
 *  chain, or by using the Table_registerTable() function to register your
 *  table, you get access to some pre-parsed information.
 *  Specifically, the table handler pulls out the column number and
 *  indexes from the request oid so that you don't have to do the
 *  complex work to do that parsing within your own code.
 *
 *  To do this, the table handler needs to know up front how your
 *  table is structured.  To inform it about this, you fill in a
 *  table_registeration_info structure that is passed to the table
 *  handler.  It contains the asn index types for the table as well as
 *  the minimum and maximum column that should be used.
 *
 *  @{
 */

/** Given a TableRegistrationInfo object, creates a table handler.
 *  You can use this table handler by injecting it into a calling
 *  chain.  When the handler gets called, it'll do processing and
 *  store it's information into the request->parent_data structure.
 *
 *  The table helper handler pulls out the column number and indexes from
 *  the request oid so that you don't have to do the complex work of
 *  parsing within your own code.
 *
 *  @param tabreq is a pointer to a TableRegistrationInfo struct.
 *	The table handler needs to know up front how your table is structured.
 *	A netsnmp_table_registeration_info structure that is
 *	passed to the table handler should contain the asn index types for the
 *	table as well as the minimum and maximum column that should be used.
 *
 *  @return Returns a pointer to a MibHandler struct which contains
 *	the handler's name and the access method
 *
 */
MibHandler*
Table_getTableHandler( TableRegistrationInfo* tabreq )
{
    MibHandler* ret = NULL;

    if ( !tabreq ) {
        Logger_log( LOGGER_PRIORITY_INFO, "Table_getTableHandler(NULL) called\n" );
        return NULL;
    }

    ret = AgentHandler_createHandler( TABLE_HANDLER_NAME, Table_helperHandler );
    if ( ret ) {
        ret->myvoid = ( void* )tabreq;
        tabreq->number_indexes = Client_countVarbinds( tabreq->indexes );
    }
    return ret;
}

/** Configures a handler such that table registration information is freed by
 *  AgentHandler_handlerFree(). Should only be called if handler->myvoid points to
 *  an object of type TableRegistrationInfo.
 */
void Table_handlerOwnsTableInfo( MibHandler* handler )
{
    Assert_assert( handler );
    Assert_assert( handler->myvoid );
    handler->data_clone
        = ( void* ( * )( void* ))Table_registrationInfoClone;
    handler->data_free
        = ( void ( * )( void* ) )Table_registrationInfoFree;
}

/** Configures a handler such that table registration information is freed by
 *  AgentHandler_handlerFree(). Should only be called if reg->handler->myvoid
 *  points to an object of type TableRegistrationInfo.
 */
void Table_registrationOwnsTableInfo( HandlerRegistration* reg )
{
    if ( reg )
        Table_handlerOwnsTableInfo( reg->handler );
}

/** creates a table handler given the TableRegistrationInfo object,
 *  inserts it into the request chain and then calls
 *  AgentHandler_registerHandler() to register the table into the agent.
 */
int Table_registerTable( HandlerRegistration* reginfo,
    TableRegistrationInfo* tabreq )
{
    int rc = AgentHandler_injectHandler( reginfo, Table_getTableHandler( tabreq ) );
    if ( ErrorCode_SUCCESS != rc )
        return rc;

    return AgentHandler_registerHandler( reginfo );
}

int Table_unregisterTable( HandlerRegistration* reginfo )
{
    /* Locate "this" reginfo */
    /* MEMORY_FREE(reginfo->myvoid); */
    return AgentHandler_unregisterHandler( reginfo );
}

/** Extracts the processed table information from a given request.
 *  Call this from subhandlers on a request to extract the processed
 *  RequestInfo information.  The resulting information includes the
 *  index values and the column number.
 *
 * @param request populated netsnmp request structure
 *
 * @return populated TableRequestInfo structure
 */
TableRequestInfo*
Table_extractTableInfo( RequestInfo* request )
{
    return ( TableRequestInfo* )
        AgentHandler_requestGetListData( request, TABLE_HANDLER_NAME );
}

/** extracts the registered TableRegistrationInfo object from a
 *  HandlerRegistration object */
TableRegistrationInfo*
Table_findTableRegistrationInfo( HandlerRegistration* reginfo )
{
    return ( TableRegistrationInfo* )
        AgentHandler_findHandlerDataByName( reginfo, TABLE_HANDLER_NAME );
}

/** implements the table helper handler */
int Table_helperHandler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRegistrationInfo* tbl_info;
    int oid_index_pos;
    unsigned int oid_column_pos;
    unsigned int tmp_idx;
    ssize_t tmp_len;
    int incomplete, out_of_range;
    int status = PRIOT_ERR_NOERROR, need_processing = 0;
    oid* tmp_name;
    TableRequestInfo* tbl_req_info;
    VariableList* vb;

    if ( !reginfo || !handler )
        return ErrorCode_GENERR;

    oid_index_pos = reginfo->rootoid_len + 2;
    oid_column_pos = reginfo->rootoid_len + 1;
    tbl_info = ( TableRegistrationInfo* )handler->myvoid;

    if ( ( !handler->myvoid ) || ( !tbl_info->indexes ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "improperly registered table found\n" );
        Logger_log( LOGGER_PRIORITY_ERR, "name: %s, table info: %p, indexes: %p\n",
            handler->handler_name, handler->myvoid, tbl_info->indexes );

        /*
         * XXX-rks: unregister table?
         */
        return PRIOT_ERR_GENERR;
    }

    DEBUG_IF( "helper:table:req" )
    {
        DEBUG_MSGTL( ( "helper:table:req",
            "Got %s (%d) mode request for handler %s: base oid:",
            MapList_findLabel( "agentMode", reqinfo->mode ),
            reqinfo->mode, handler->handler_name ) );
        DEBUG_MSGOID( ( "helper:table:req", reginfo->rootoid,
            reginfo->rootoid_len ) );
        DEBUG_MSG( ( "helper:table:req", "\n" ) );
    }

    /*
     * if the agent request info has a state reference, then this is a
     * later pass of a set request and we can skip all the lookup stuff.
     *
     * xxx-rks: this might break for handlers which only handle one varbind
     * at a time... those handlers should not save data by their handler_name
     * in the AgentRequestInfo.
     */
    if ( Agent_getListData( reqinfo, handler->next->handler_name ) ) {
        if ( MODE_IS_SET( reqinfo->mode ) ) {
            return AgentHandler_callNextHandler( handler, reginfo, reqinfo,
                requests );
        } else {
            /** XXX-rks: memory leak. add cleanup handler? */
            Agent_freeAgentDataSets( reqinfo );
        }
    }

    if ( MODE_IS_SET( reqinfo->mode ) && ( reqinfo->mode != MODE_SET_RESERVE1 ) ) {
        /*
         * for later set modes, we can skip all the index parsing,
         * and we always need to let child handlers have a chance
         * to clean up, if they were called in the first place (i.e. have
         * a valid table info pointer).
         */
        if ( NULL == Table_extractTableInfo( requests ) ) {
            DEBUG_MSGTL( ( "helper:table", "no table info for set - skipping\n" ) );
        } else
            need_processing = 1;
    } else {
        /*
         * for GETS, only continue if we have at least one valid request.
         * for IMPL_RESERVE1, only continue if we have indexes for all requests.
         */

        /*
     * loop through requests
     */

        for ( request = requests; request; request = request->next ) {
            VariableList* var = request->requestvb;

            DEBUG_MSGOID( ( "verbose:table", var->name, var->nameLength ) );
            DEBUG_MSG( ( "verbose:table", "\n" ) );

            if ( request->processed ) {
                DEBUG_MSG( ( "verbose:table", "already processed\n" ) );
                continue;
            }
            Assert_assert( request->status == PRIOT_ERR_NOERROR );

            /*
         * this should probably be handled further up
         */
            if ( ( reqinfo->mode == MODE_GET ) && ( var->type != ASN01_NULL ) ) {
                /*
             * valid request if ASN01_NULL
             */
                DEBUG_MSGTL( ( "helper:table",
                    "  GET var type is not ASN01_NULL\n" ) );
                Agent_setRequestError( reqinfo, request,
                    PRIOT_ERR_WRONGTYPE );
                continue;
            }

            if ( reqinfo->mode == MODE_SET_RESERVE1 ) {
                DEBUG_IF( "helper:table:set" )
                {
                    u_char* buf = NULL;
                    size_t buf_len = 0, out_len = 0;
                    DEBUG_MSGTL( ( "helper:table:set", " SET_REQUEST for OID: " ) );
                    DEBUG_MSGOID( ( "helper:table:set", var->name, var->nameLength ) );
                    out_len = 0;
                    if ( Mib_sprintReallocByType( &buf, &buf_len, &out_len, 1,
                             var, NULL, NULL, NULL ) ) {
                        DEBUG_MSG( ( "helper:table:set", " type=%d(%02x), value=%s\n",
                            var->type, var->type, buf ) );
                    } else {
                        if ( buf != NULL ) {
                            DEBUG_MSG( ( "helper:table:set",
                                " type=%d(%02x), value=%s [TRUNCATED]\n",
                                var->type, var->type, buf ) );
                        } else {
                            DEBUG_MSG( ( "helper:table:set",
                                " type=%d(%02x), value=[NIL] [TRUNCATED]\n",
                                var->type, var->type ) );
                        }
                    }
                    if ( buf != NULL ) {
                        free( buf );
                    }
                }
            }

            /*
         * check to make sure its in table range
         */

            out_of_range = 0;
            /*
         * if our root oid is > var->name and this is not a GETNEXT,
         * then the oid is out of range. (only compare up to shorter
         * length)
         */
            if ( reginfo->rootoid_len > var->nameLength )
                tmp_len = var->nameLength;
            else
                tmp_len = reginfo->rootoid_len;
            if ( Api_oidCompare( reginfo->rootoid, reginfo->rootoid_len,
                     var->name, tmp_len )
                > 0 ) {
                if ( reqinfo->mode == MODE_GETNEXT ) {
                    if ( var->name != var->nameLoc )
                        MEMORY_FREE( var->name );
                    Client_setVarObjid( var, reginfo->rootoid,
                        reginfo->rootoid_len );
                } else {
                    DEBUG_MSGTL( ( "helper:table", "  oid is out of range.\n" ) );
                    out_of_range = 1;
                }
            }
            /*
         * if var->name is longer than the root, make sure it is
         * table.1 (table.ENTRY).
         */
            else if ( ( var->nameLength > reginfo->rootoid_len ) && ( var->name[ reginfo->rootoid_len ] != 1 ) ) {
                if ( ( var->name[ reginfo->rootoid_len ] < 1 ) && ( reqinfo->mode == MODE_GETNEXT ) ) {
                    var->name[ reginfo->rootoid_len ] = 1;
                    var->nameLength = reginfo->rootoid_len;
                } else {
                    out_of_range = 1;
                    DEBUG_MSGTL( ( "helper:table", "  oid is out of range.\n" ) );
                }
            }
            /*
         * if it is not in range, then mark it in the request list
         * because we can't process it, and set an error so
         * nobody else wastes time trying to process it either.
         */
            if ( out_of_range ) {
                DEBUG_MSGTL( ( "helper:table", "  Not processed: " ) );
                DEBUG_MSGOID( ( "helper:table", var->name, var->nameLength ) );
                DEBUG_MSG( ( "helper:table", "\n" ) );

                /*
             *  Reject requests of the form 'myTable.N'   (N != 1)
             */
                if ( reqinfo->mode == MODE_SET_RESERVE1 )
                    _Table_helperCleanup( reqinfo, request,
                        PRIOT_ERR_NOTWRITABLE );
                else if ( reqinfo->mode == MODE_GET )
                    _Table_helperCleanup( reqinfo, request,
                        PRIOT_NOSUCHOBJECT );
                continue;
            }

            /*
         * Check column ranges; set-up to pull out indexes from OID.
         */

            incomplete = 0;
            tbl_req_info = Table_extractTableInfo( request );
            if ( NULL == tbl_req_info ) {
                tbl_req_info = MEMORY_MALLOC_TYPEDEF( TableRequestInfo );
                if ( tbl_req_info == NULL ) {
                    _Table_helperCleanup( reqinfo, request,
                        PRIOT_ERR_GENERR );
                    continue;
                }
                tbl_req_info->reg_info = tbl_info;
                tbl_req_info->indexes = Client_cloneVarbind( tbl_info->indexes );
                tbl_req_info->number_indexes = 0; /* none yet */
                AgentHandler_requestAddListData( request,
                    Map_newElement( TABLE_HANDLER_NAME,
                                                     ( void* )tbl_req_info,
                                                     _Table_dataFreeFunc ) );
            } else {
                DEBUG_MSGTL( ( "helper:table", "  using existing tbl_req_info\n " ) );
            }

            /*
         * do we have a column?
         */
            if ( var->nameLength > oid_column_pos ) {
                /*
             * oid is long enough to contain COLUMN info
             */
                DEBUG_MSGTL( ( "helper:table:col",
                    "  have at least a column (%"
                    "l"
                    "d)\n",
                    var->name[ oid_column_pos ] ) );
                if ( var->name[ oid_column_pos ] < tbl_info->min_column ) {
                    DEBUG_MSGTL( ( "helper:table:col",
                        "    but it's less than min (%d)\n",
                        tbl_info->min_column ) );
                    if ( reqinfo->mode == MODE_GETNEXT ) {
                        /*
                     * fix column, truncate useless column info
                     */
                        var->nameLength = oid_column_pos;
                        tbl_req_info->colnum = tbl_info->min_column;
                    } else
                        out_of_range = 1;
                } else if ( var->name[ oid_column_pos ] > tbl_info->max_column )
                    out_of_range = 1;
                else
                    tbl_req_info->colnum = var->name[ oid_column_pos ];

                if ( out_of_range ) {
                    /*
                 * this is out of range...  remove from requests, free
                 * memory
                 */
                    DEBUG_MSGTL( ( "helper:table",
                        "    oid is out of range. Not processed: " ) );
                    DEBUG_MSGOID( ( "helper:table", var->name, var->nameLength ) );
                    DEBUG_MSG( ( "helper:table", "\n" ) );

                    /*
                 *  Reject requests of the form 'myEntry.N'   (invalid N)
                 */
                    if ( reqinfo->mode == MODE_SET_RESERVE1 )
                        _Table_helperCleanup( reqinfo, request,
                            PRIOT_ERR_NOTWRITABLE );
                    else if ( reqinfo->mode == MODE_GET )
                        _Table_helperCleanup( reqinfo, request,
                            PRIOT_NOSUCHOBJECT );
                    continue;
                }
                /*
             * use column verification
             */
                else if ( tbl_info->valid_columns ) {
                    tbl_req_info->colnum = Table_closestColumn( var->name[ oid_column_pos ],
                        tbl_info->valid_columns );
                    DEBUG_MSGTL( ( "helper:table:col", "    closest column is %d\n",
                        tbl_req_info->colnum ) );
                    /*
                 * xxx-rks: document why the continue...
                 */
                    if ( tbl_req_info->colnum == 0 )
                        continue;
                    if ( tbl_req_info->colnum != var->name[ oid_column_pos ] ) {
                        DEBUG_MSGTL( ( "helper:table:col",
                            "    which doesn't match req "
                            "%"
                            "l"
                            "d - truncating index info\n",
                            var->name[ oid_column_pos ] ) );
                        /*
                     * different column! truncate useless index info
                     */
                        var->nameLength = oid_column_pos + 1; /* pos is 0 based */
                    }
                }
                /*
             * var->nameLength may have changed - check again
             */
                if ( ( int )var->nameLength <= oid_index_pos ) { /* pos is 0 based */
                    DEBUG_MSGTL( ( "helper:table", "    not enough for indexes\n" ) );
                    tbl_req_info->index_oid_len = 0; /** none available */
                } else {
                    /*
                 * oid is long enough to contain INDEX info
                 */
                    tbl_req_info->index_oid_len = var->nameLength - oid_index_pos;
                    DEBUG_MSGTL( ( "helper:table", "    have %lu bytes of index\n",
                        ( unsigned long )tbl_req_info->index_oid_len ) );
                    Assert_assert( tbl_req_info->index_oid_len < ASN01_MAX_OID_LEN );
                    memcpy( tbl_req_info->index_oid, &var->name[ oid_index_pos ],
                        tbl_req_info->index_oid_len * sizeof( oid ) );
                    tmp_name = tbl_req_info->index_oid;
                }
            } else if ( reqinfo->mode == MODE_GETNEXT || reqinfo->mode == MODE_GETBULK ) {
                /*
             * oid is NOT long enough to contain column or index info, so start
             * at the minimum column. Set index oid len to 0 because we don't
             * have any index info in the OID.
             */
                DEBUG_MSGTL( ( "helper:table", "  no column/index in request\n" ) );
                tbl_req_info->index_oid_len = 0;
                tbl_req_info->colnum = tbl_info->min_column;
            } else {
                /*
             * oid is NOT long enough to contain index info,
             * so we can't do anything with it.
             *
             * Reject requests of the form 'myTable' or 'myEntry'
             */
                if ( reqinfo->mode == MODE_GET ) {
                    _Table_helperCleanup( reqinfo, request, PRIOT_NOSUCHOBJECT );
                } else if ( reqinfo->mode == MODE_SET_RESERVE1 ) {
                    _Table_helperCleanup( reqinfo, request, PRIOT_ERR_NOTWRITABLE );
                }
                continue;
            }

            /*
         * set up tmp_len to be the number of OIDs we have beyond the column;
         * these should be the index(s) for the table. If the index_oid_len
         * is 0, set tmp_len to -1 so that when we try to parse the index below,
         * we just zero fill everything.
         */
            if ( tbl_req_info->index_oid_len == 0 ) {
                incomplete = 1;
                tmp_len = -1;
            } else
                tmp_len = tbl_req_info->index_oid_len;

            /*
         * for each index type, try to extract the index from var->name
         */
            DEBUG_MSGTL( ( "helper:table", "  looking for %d indexes\n",
                tbl_info->number_indexes ) );
            for ( tmp_idx = 0, vb = tbl_req_info->indexes;
                  tmp_idx < tbl_info->number_indexes;
                  ++tmp_idx, vb = vb->next ) {
                size_t parsed_oid_len;

                if ( incomplete && tmp_len ) {
                    /*
                 * incomplete/illegal OID, set up dummy 0 to parse
                 */
                    DEBUG_MSGTL( ( "helper:table",
                        "  oid indexes not complete: " ) );
                    DEBUG_MSGOID( ( "helper:table", var->name, var->nameLength ) );
                    DEBUG_MSG( ( "helper:table", "\n" ) );

                    /*
                 * no sense in trying anymore if this is a GET/SET.
                 *
                 * Reject requests of the form 'myObject'   (no instance)
                 */
                    tmp_len = 0;
                    tmp_name = NULL;
                    break;
                }
                /*
             * try and parse current index
             */
                Assert_assert( tmp_len >= 0 );
                parsed_oid_len = tmp_len;
                if ( Mib_parseOneOidIndex( &tmp_name, &parsed_oid_len,
                         vb, 1 )
                    != ErrorCode_SUCCESS ) {
                    incomplete = 1;
                    tmp_len = -1; /* is this necessary? Better safe than
                                 * sorry */
                } else {
                    tmp_len = parsed_oid_len;
                    DEBUG_MSGTL( ( "helper:table", "  got 1 (incomplete=%d)\n",
                        incomplete ) );
                    /*
                 * do not count incomplete indexes
                 */
                    if ( incomplete )
                        continue;
                    ++tbl_req_info->number_indexes; /** got one ok */
                    if ( tmp_len <= 0 ) {
                        incomplete = 1;
                        tmp_len = -1; /* is this necessary? Better safe
                                         * than sorry */
                    }
                }
            } /** for loop */

            DEBUG_IF( "helper:table:results" )
            {
                unsigned int count;
                u_char* buf = NULL;
                size_t buf_len = 0, out_len = 0;
                DEBUG_MSGTL( ( "helper:table:results", "  found %d indexes\n",
                    tbl_req_info->number_indexes ) );
                DEBUG_MSGTL( ( "helper:table:results",
                    "  column: %d, indexes: %d",
                    tbl_req_info->colnum,
                    tbl_req_info->number_indexes ) );
                for ( vb = tbl_req_info->indexes, count = 0;
                      vb && count < tbl_req_info->number_indexes;
                      count++, vb = vb->next ) {
                    out_len = 0;
                    if ( Mib_sprintReallocByType( &buf, &buf_len, &out_len, 1,
                             vb, NULL, NULL, NULL ) ) {
                        DEBUG_MSG( ( "helper:table:results",
                            "   index: type=%d(%02x), value=%s",
                            vb->type, vb->type, buf ) );
                    } else {
                        if ( buf != NULL ) {
                            DEBUG_MSG( ( "helper:table:results",
                                "   index: type=%d(%02x), value=%s [TRUNCATED]",
                                vb->type, vb->type, buf ) );
                        } else {
                            DEBUG_MSG( ( "helper:table:results",
                                "   index: type=%d(%02x), value=[NIL] [TRUNCATED]",
                                vb->type, vb->type ) );
                        }
                    }
                }
                if ( buf != NULL ) {
                    free( buf );
                }
                DEBUG_MSG( ( "helper:table:results", "\n" ) );
            }

            /*
         * do we have sufficient index info to continue?
         */

            if ( ( reqinfo->mode != MODE_GETNEXT ) && ( ( tbl_req_info->number_indexes != tbl_info->number_indexes ) || ( tmp_len != -1 ) ) ) {

                DEBUG_MSGTL( ( "helper:table",
                    "invalid index(es) for table - skipping\n" ) );

                if ( MODE_IS_SET( reqinfo->mode ) ) {
                    /*
                 * no point in continuing without indexes for set.
                 */
                    Assert_assert( reqinfo->mode == MODE_SET_RESERVE1 );
                    /** clear first request so we wont try to run FREE mode */
                    AgentHandler_freeRequestDataSets( requests );
                    /** set actual error */
                    _Table_helperCleanup( reqinfo, request, PRIOT_ERR_NOCREATION );
                    need_processing = 0; /* don't call next handler */
                    break;
                }
                _Table_helperCleanup( reqinfo, request, PRIOT_NOSUCHINSTANCE );
                continue;
            }
            Assert_assert( request->status == PRIOT_ERR_NOERROR );

            ++need_processing;

        } /* for each request */
    }

    /*
     * bail if there is nothing for our child handlers
     */
    if ( 0 == need_processing )
        return status;

    /*
     * call our child access function
     */
    status = AgentHandler_callNextHandler( handler, reginfo, reqinfo, requests );

    /*
     * check for sparse tables
     */
    if ( reqinfo->mode == MODE_GETNEXT )
        _Table_sparseTableHelperHandler( handler, reginfo, reqinfo, requests );

    return status;
}

#define SPARSE_TABLE_HANDLER_NAME "sparseTable"

/** implements the sparse table helper handler
 * @internal
 *
 * @note
 * This function is static to prevent others from calling it
 * directly. It it automatically called by the table helper,
 *
 */
static int
_Table_sparseTableHelperHandler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    int status = PRIOT_ERR_NOERROR;
    RequestInfo* request;
    oid coloid[ ASN01_MAX_OID_LEN ];
    TableRequestInfo* table_info;

    /*
     * since we don't call child handlers, warn if one was registered
     * beneath us. A special exception for the table helper, which calls
     * the handler directly. Use handle custom flag to only log once.
     */
    if ( ( Table_helperHandler != handler->access_method ) && ( NULL != handler->next ) ) {
        /*
         * always warn if called without our own handler. If we
         * have our own handler, use custom bit 1 to only log once.
         */
        if ( ( _Table_sparseTableHelperHandler != handler->access_method ) || !( handler->flags & MIB_HANDLER_CUSTOM1 ) ) {
            Logger_log( LOGGER_PRIORITY_WARNING, "handler (%s) registered after sparse table "
                                                 "hander will not be called\n",
                handler->next->handler_name ? handler->next->handler_name : "" );
            if ( _Table_sparseTableHelperHandler == handler->access_method )
                handler->flags |= MIB_HANDLER_CUSTOM1;
        }
    }

    if ( reqinfo->mode == MODE_GETNEXT ) {
        for ( request = requests; request; request = request->next ) {
            if ( ( request->requestvb->type == ASN01_NULL && request->processed ) || request->delegated )
                continue;
            if ( request->requestvb->type == PRIOT_NOSUCHINSTANCE ) {
                /*
                 * get next skipped this value for this column, we
                 * need to keep searching forward
                 */
                DEBUG_MSGT( ( "sparse", "retry for NOSUCHINSTANCE\n" ) );
                request->requestvb->type = ASN01_PRIV_RETRY;
            }
            if ( request->requestvb->type == PRIOT_NOSUCHOBJECT || request->requestvb->type == PRIOT_ENDOFMIBVIEW ) {
                /*
                 * get next has completely finished with this column,
                 * so we need to try with the next column (if any)
                 */
                DEBUG_MSGT( ( "sparse", "retry for NOSUCHOBJECT\n" ) );
                table_info = Table_extractTableInfo( request );
                table_info->colnum = Table_nextColumn( table_info );
                if ( 0 != table_info->colnum ) {
                    memcpy( coloid, reginfo->rootoid,
                        reginfo->rootoid_len * sizeof( oid ) );
                    coloid[ reginfo->rootoid_len ] = 1; /* table.entry node */
                    coloid[ reginfo->rootoid_len + 1 ] = table_info->colnum;
                    Client_setVarObjid( request->requestvb,
                        coloid, reginfo->rootoid_len + 2 );

                    request->requestvb->type = ASN01_PRIV_RETRY;
                } else {
                    /*
                     * If we don't have column info, reset to null so
                     * the agent will move on to the next table.
                     */
                    request->requestvb->type = ASN01_NULL;
                }
            }
        }
    }
    return status;
}

/** create sparse table handler
 */
MibHandler*
Table_sparseTableHandlerGet( void )
{
    return AgentHandler_createHandler( SPARSE_TABLE_HANDLER_NAME,
        _Table_sparseTableHelperHandler );
}

/** creates a table handler given the TableRegistrationInfo object,
 *  inserts it into the request chain and then calls
 *  AgentHandler_registerHandler() to register the table into the agent.
 */
int Table_sparseTableRegister( HandlerRegistration* reginfo,
    TableRegistrationInfo* tabreq )
{
    MibHandler *handler1, *handler2;
    int rc;

    handler1 = AgentHandler_createHandler( SPARSE_TABLE_HANDLER_NAME,
        _Table_sparseTableHelperHandler );
    if ( NULL == handler1 )
        return PRIOT_ERR_GENERR;

    handler2 = Table_getTableHandler( tabreq );
    if ( NULL == handler2 ) {
        AgentHandler_handlerFree( handler1 );
        return PRIOT_ERR_GENERR;
    }

    rc = AgentHandler_injectHandler( reginfo, handler1 );
    if ( ErrorCode_SUCCESS != rc ) {
        AgentHandler_handlerFree( handler1 );
        AgentHandler_handlerFree( handler2 );
        return rc;
    }

    rc = AgentHandler_injectHandler( reginfo, handler2 );
    if ( ErrorCode_SUCCESS != rc ) {
        /** handler1 is in reginfo... remove and free?? */
        AgentHandler_handlerFree( handler2 );
        return rc;
    }

    /** both handlers now in reginfo, so nothing to do on error */
    return AgentHandler_registerHandler( reginfo );
}

/** Builds the result to be returned to the agent given the table information.
 *  Use this function to return results from lowel level handlers to
 *  the agent.  It takes care of building the proper resulting oid
 *  (containing proper indexing) and inserts the result value into the
 *  returning varbind.
 */
int Table_buildResult( HandlerRegistration* reginfo,
    RequestInfo* reqinfo,
    TableRequestInfo* table_info,
    u_char type, u_char* result, size_t result_len )
{

    VariableList* var;

    if ( !reqinfo || !table_info )
        return ErrorCode_GENERR;

    var = reqinfo->requestvb;

    if ( var->name != var->nameLoc )
        free( var->name );
    var->name = NULL;

    if ( Table_buildOid( reginfo, reqinfo, table_info ) != ErrorCode_SUCCESS )
        return ErrorCode_GENERR;

    Client_setVarTypedValue( var, type, result, result_len );

    return ErrorCode_SUCCESS;
}

/** given a registration info object, a request object and the table
 *  info object it builds the request->requestvb->name oid from the
 *  index values and column information found in the table_info
 *  object. Index values are extracted from the table_info varbinds.
 */
int Table_buildOid( HandlerRegistration* reginfo,
    RequestInfo* reqinfo,
    TableRequestInfo* table_info )
{
    oid tmpoid[ ASN01_MAX_OID_LEN ];
    VariableList* var;

    if ( !reginfo || !reqinfo || !table_info )
        return ErrorCode_GENERR;

    /*
     * xxx-rks: inefficent. we do a copy here, then Mib_buildOid does it
     *          again. either come up with a new utility routine, or
     *          do some hijinks here to eliminate extra copy.
     *          Probably could make sure all callers have the
     *          index & variable list updated, and use
     *          Table_buildOidFromIndex() instead of all this.
     */
    memcpy( tmpoid, reginfo->rootoid, reginfo->rootoid_len * sizeof( oid ) );
    tmpoid[ reginfo->rootoid_len ] = 1; /** .Entry */
    tmpoid[ reginfo->rootoid_len + 1 ] = table_info->colnum; /** .column */

    var = reqinfo->requestvb;
    if ( Mib_buildOid( &var->name, &var->nameLength,
             tmpoid, reginfo->rootoid_len + 2, table_info->indexes )
        != ErrorCode_SUCCESS )
        return ErrorCode_GENERR;

    return ErrorCode_SUCCESS;
}

/** given a registration info object, a request object and the table
 *  info object it builds the request->requestvb->name oid from the
 *  index values and column information found in the table_info
 *  object.  Index values are extracted from the table_info index oid.
 */
int Table_buildOidFromIndex( HandlerRegistration* reginfo,
    RequestInfo* reqinfo,
    TableRequestInfo* table_info )
{
    oid tmpoid[ ASN01_MAX_OID_LEN ];
    VariableList* var;
    int len;

    if ( !reginfo || !reqinfo || !table_info )
        return ErrorCode_GENERR;

    var = reqinfo->requestvb;
    len = reginfo->rootoid_len;
    memcpy( tmpoid, reginfo->rootoid, len * sizeof( oid ) );
    tmpoid[ len++ ] = 1; /* .Entry */
    tmpoid[ len++ ] = table_info->colnum; /* .column */
    memcpy( &tmpoid[ len ], table_info->index_oid,
        table_info->index_oid_len * sizeof( oid ) );
    len += table_info->index_oid_len;
    Client_setVarObjid( var, tmpoid, len );

    return ErrorCode_SUCCESS;
}

/** parses an OID into table indexses */
int Table_updateVariableListFromIndex( TableRequestInfo* tri )
{
    if ( !tri )
        return ErrorCode_GENERR;

    /*
     * free any existing allocated memory, then parse oid into varbinds
     */
    Client_resetVarBuffers( tri->indexes );

    return Mib_parseOidIndexes( tri->index_oid, tri->index_oid_len,
        tri->indexes );
}

/** builds an oid given a set of indexes. */
int Table_updateIndexesFromVariableList( TableRequestInfo* tri )
{
    if ( !tri )
        return ErrorCode_GENERR;

    return Mib_buildOidNoalloc( tri->index_oid, sizeof( tri->index_oid ),
        &tri->index_oid_len, NULL, 0, tri->indexes );
}

/**
 * checks the original request against the current data being passed in if
 * its greater than the request oid but less than the current valid
 * return, set the current valid return to the new value.
 *
 * returns 1 if outvar was replaced with the oid from newvar (success).
 * returns 0 if not.
 */
int Table_checkGetnextReply( RequestInfo* request,
    oid* prefix,
    size_t prefix_len,
    VariableList* newvar,
    VariableList** outvar )
{
    oid myname[ ASN01_MAX_OID_LEN ];
    size_t myname_len;

    Mib_buildOidNoalloc( myname, ASN01_MAX_OID_LEN, &myname_len,
        prefix, prefix_len, newvar );
    /*
     * is the build of the new indexes less than our current result
     */
    if ( ( !( *outvar ) || Api_oidCompare( myname + prefix_len, myname_len - prefix_len, ( *outvar )->name + prefix_len, ( *outvar )->nameLength - prefix_len ) < 0 ) ) {
        /*
         * and greater than the requested oid
         */
        if ( Api_oidCompare( myname, myname_len,
                 request->requestvb->name,
                 request->requestvb->nameLength )
            > 0 ) {
            /*
             * the new result must be better than the old
             */

            if ( *outvar )
                Api_freeVarbind( *outvar );
            *outvar = Client_cloneVarbind( newvar );
            Client_setVarObjid( *outvar, myname, myname_len );

            return 1;
        }
    }
    return 0;
}

TableRegistrationInfo*
Table_registrationInfoClone( TableRegistrationInfo* tri )
{
    TableRegistrationInfo* copy;
    copy = ( TableRegistrationInfo* )malloc( sizeof( *copy ) );
    if ( copy ) {
        *copy = *tri;
        copy->indexes = Client_cloneVarbind( tri->indexes );
        if ( !copy->indexes ) {
            free( copy );
            copy = NULL;
        }
    }
    return copy;
}

void Table_registrationInfoFree( TableRegistrationInfo* tri )
{
    if ( NULL == tri )
        return;

    if ( NULL != tri->indexes )
        Api_freeVarbind( tri->indexes );

    free( tri );
}

/** @} */

/*
 * internal routines
 */
static void
_Table_dataFreeFunc( void* data )
{
    TableRequestInfo* info = ( TableRequestInfo* )data;
    if ( !info )
        return;
    Api_freeVarbind( info->indexes );
    free( info );
}

static void
_Table_helperCleanup( AgentRequestInfo* reqinfo,
    RequestInfo* request, int status )
{
    Agent_setRequestError( reqinfo, request, status );
    AgentHandler_freeRequestDataSets( request );
    if ( !request )
        return;
    request->parent_data = NULL;
}

/*
 * find the closest column to current (which may be current).
 *
 * called when a table runs out of rows for column X. This
 * function is called with current = X + 1, to verify that
 * X + 1 is a valid column, or find the next closest column if not.
 *
 * All list types should be sorted, lowest to highest.
 */
unsigned int
Table_closestColumn( unsigned int current,
    ColumnInfo* valid_columns )
{
    unsigned int closest = 0;
    int idx;

    if ( valid_columns == NULL )
        return 0;

    for ( ; valid_columns; valid_columns = valid_columns->next ) {

        if ( valid_columns->isRange ) {
            /*
             * if current < low range, it might be closest.
             * otherwise, if it's < high range, current is in
             * the range, and thus is an exact match.
             */
            if ( current < valid_columns->details.range[ 0 ] ) {
                if ( ( valid_columns->details.range[ 0 ] < closest ) || ( 0 == closest ) ) {
                    closest = valid_columns->details.range[ 0 ];
                }
            } else if ( current <= valid_columns->details.range[ 1 ] ) {
                closest = current;
                break; /* can not get any closer! */
            }

        } /* range */
        else { /* list */
            /*
             * if current < first item, no need to iterate over list.
             * that item is either closest, or not.
             */
            if ( current < valid_columns->details.list[ 0 ] ) {
                if ( ( valid_columns->details.list[ 0 ] < closest ) || ( 0 == closest ) )
                    closest = valid_columns->details.list[ 0 ];
                continue;
            }

            /** if current > last item in list, no need to iterate */
            if ( current > valid_columns->details.list[ ( int )valid_columns->list_count - 1 ] )
                continue; /* not in list range. */

            /** skip anything less than current*/
            for ( idx = 0; valid_columns->details.list[ idx ] < current; ++idx )
                ;

            /** check for exact match */
            if ( current == valid_columns->details.list[ idx ] ) {
                closest = current;
                break; /* can not get any closer! */
            }

            /** list[idx] > current; is it < closest? */
            if ( ( valid_columns->details.list[ idx ] < closest ) || ( 0 == closest ) )
                closest = valid_columns->details.list[ idx ];

        } /* list */
    } /* for */

    return closest;
}

/**
 * This function can be used to setup the table's definition within
 * your module's initialize function, it takes a variable index parameter list
 * for example: the table_info structure is followed by two integer index types
 * Table_helperAddIndexes(
 *                  table_info,
 *	            ASN01_INTEGER,
 *		    ASN01_INTEGER,
 *		    0);
 *
 * @param tinfo is a pointer to a TableRegistrationInfo struct.
 *	The table handler needs to know up front how your table is structured.
 *	A netsnmp_table_registeration_info structure that is
 *	passed to the table handler should contain the asn index types for the
 *	table as well as the minimum and maximum column that should be used.
 *
 * @return void
 *
 */
void Table_helperAddIndexes( TableRegistrationInfo* tinfo,
    ... )
{
    va_list debugargs;
    int type;

    va_start( debugargs, tinfo );
    while ( ( type = va_arg( debugargs, int ) ) != 0 ) {
        Table_helperAddIndex( tinfo, type );
    }
    va_end( debugargs );
}

static void
_Table_rowStashDataListFree( void* ptr )
{
    OidStash_Node** tmp = ( OidStash_Node** )ptr;
    OidStash_free( tmp, NULL );
    free( ptr );
}

/** returns a row-wide place to store data in.
    @todo This function will likely change to add free pointer functions. */
OidStash_Node**
Table_getOrCreateRowStash( AgentRequestInfo* reqinfo,
    const u_char* storage_name )
{
    OidStash_Node** stashp = NULL;
    stashp = ( OidStash_Node** )
        Agent_getListData( reqinfo, ( const char* )storage_name );

    if ( !stashp ) {
        /*
         * hasn't be created yet.  we create it here.
         */
        stashp = MEMORY_MALLOC_TYPEDEF( OidStash_Node* );

        if ( !stashp )
            return NULL; /* ack. out of mem */

        Agent_addListData( reqinfo,
            Map_newElement( ( const char* )storage_name,
                               stashp,
                               _Table_rowStashDataListFree ) );
    }
    return stashp;
}

/*
 * advance the table info colnum to the next column, or 0 if there are no more
 *
 * @return new column, or 0 if there are no more
 */
unsigned int
Table_nextColumn( TableRequestInfo* table_info )
{
    if ( NULL == table_info )
        return 0;

    /*
     * try and validate next column
     */
    if ( table_info->reg_info->valid_columns )
        return Table_closestColumn( table_info->colnum + 1,
            table_info->reg_info->valid_columns );

    /*
     * can't validate. assume 1..max_column are valid
     */
    if ( table_info->colnum < table_info->reg_info->max_column )
        return table_info->colnum + 1;

    return 0; /* out of range */
}
