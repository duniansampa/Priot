#include "TableIterator.h"
#include "Api.h"
#include "System/Util/Assert.h"
#include "Client.h"
#include "System/Containers/Map.h"
#include "System/Util/Debug.h"
#include "System/Containers/MapList.h"
#include "Mib.h"
#include "StashCache.h"
#include "System/Util/Utilities.h"

/** @defgroup table_iterator table_iterator
 *  The table iterator helper is designed to simplify the task of writing a table handler for the net-snmp agent when the data being accessed is not in an oid sorted form and must be accessed externally.
 *  @ingroup table
    Functionally, it is a specialized version of the more
    generic table helper but easies the burden of GETNEXT processing by
    manually looping through all the data indexes retrieved through
    function calls which should be supplied by the module that wishes
    help.  The module the table_iterator helps should, afterwards,
    never be called for the case of "MODE_GETNEXT" and only for the GET
    and SET related modes instead.

    The fundamental notion between the table iterator is that it
    allows your code to iterate over each "row" within your data
    storage mechanism, without requiring that it be sorted in a
    SNMP-index-compliant manner.  Through the get_first_data_point and
    get_next_data_point hooks, the table_iterator helper will
    repeatedly call your hooks to find the "proper" row of data that
    needs processing.  The following concepts are important:

      - A loop context is a pointer which indicates where in the
        current processing of a set of rows you currently are.  Allows
    the get_*_data_point routines to move from one row to the next,
    once the iterator handler has identified the appropriate row for
    this request, the job of the loop context is done.  The
        most simple example would be a pointer to an integer which
        simply counts rows from 1 to X.  More commonly, it might be a
        pointer to a linked list node, or someother internal or
        external reference to a data set (file seek value, array
        pointer, ...).  If allocated during iteration, either the
        free_loop_context_at_end (preferably) or the free_loop_context
        pointers should be set.

      - A data context is something that your handler code can use
        in order to retrieve the rest of the data for the needed
        row.  This data can be accessed in your handler via
    TableIterator_extractIteratorContext api with the RequestInfo
    structure that's passed in.
    The important difference between a loop context and a
        data context is that multiple data contexts can be kept by the
        table_iterator helper, where as only one loop context will
        ever be held by the table_iterator helper.  If allocated
        during iteration the free_data_context pointer should be set
        to an appropriate function.

    The table iterator operates in a series of steps that call your
    code hooks from your IteratorInfo registration pointer.

      - the get_first_data_point hook is called at the beginning of
        processing.  It should set the variable list to a list of
        indexes for the given table.  It should also set the
        loop_context and maybe a data_context which you will get a
        pointer back to when it needs to call your code to retrieve
        actual data later.  The list of indexes should be returned
        after being update.

      - the get_next_data_point hook is then called repeatedly and is
        passed the loop context and the data context for it to update.
        The indexes, loop context and data context should all be
        updated if more data is available, otherwise they should be
        left alone and a NULL should be returned.  Ideally, it should
        update the loop context without the need to reallocate it.  If
        reallocation is necessary for every iterative step, then the
        free_loop_context function pointer should be set.  If not,
        then the free_loop_context_at_end pointer should be set, which
        is more efficient since a malloc/free will only be performed
        once for every iteration.
 *
 *  @{
 */

/* ==================================
 *
 * Iterator API: Table maintenance
 *
 * ================================== */

/*
     * Iterator-based tables are typically maintained by external
     *  code, and this helper is really only concerned with
     *  mapping between a walk through this local representation,
     *  and the requirements of SNMP table ordering.
     * However, there's a case to be made for considering the
     *  iterator info structure as encapsulating the table, so
     *  it's probably worth defining the table creation/deletion
     *  routines from the generic API.
     *
     * Time will show whether this is a sensible approach or not.
     */
IteratorInfo*
TableIterator_createTable( FirstDataPointFT* firstDP,
    NextDataPointFT* nextDP,
    FirstDataPointFT* getidx,
    Types_VariableList* indexes )
{
    IteratorInfo* iinfo = MEMORY_MALLOC_TYPEDEF( IteratorInfo );

    if ( !iinfo )
        return NULL;

    if ( indexes )
        iinfo->indexes = Client_cloneVarbind( indexes );
    iinfo->get_first_data_point = firstDP;
    iinfo->get_next_data_point = nextDP;
    iinfo->get_row_indexes = getidx;

    return iinfo;
}

/** Free the memory that was allocated for a table iterator. */
void TableIterator_deleteTable( IteratorInfo* iinfo )
{
    if ( !iinfo )
        return;

    if ( iinfo->indexes ) {
        Api_freeVarbind( iinfo->indexes );
        iinfo->indexes = NULL;
    }
    Table_registrationInfoFree( iinfo->table_reginfo );
    MEMORY_FREE( iinfo );
}

/*
     * The rest of the table maintenance section of the
     *   generic table API is Not Applicable to this helper.
     *
     * The contents of a iterator-based table will be
     *  maintained by the table-specific module itself.
     */

/* ==================================
 *
 * Iterator API: MIB maintenance
 *
 * ================================== */

static IteratorInfo*
_TableIterator_ref( IteratorInfo* iinfo )
{
    iinfo->refcnt++;
    return iinfo;
}

static void
_TableIterator_deref( IteratorInfo* iinfo )
{
    if ( --iinfo->refcnt == 0 )
        TableIterator_deleteTable( iinfo );
}

void TableIterator_handlerOwnsIteratorInfo( MibHandler* h )
{
    Assert_assert( h );
    Assert_assert( h->myvoid );
    ( ( IteratorInfo* )( h->myvoid ) )->refcnt++;
    h->data_clone = ( void* ( * )( void* ))_TableIterator_ref;
    h->data_free = ( void ( * )( void* ) )_TableIterator_deref;
}

/**
 * Returns a MibHandler object for the table_iterator helper.
 *
 * The caller remains the owner of the iterator information object if
 * the flag NETSNMP_HANDLER_OWNS_IINFO has not been set, and the created
 * handler becomes the owner of the iterator information if the flag
 * NETSNMP_HANDLER_OWNS_IINFO has been set.
 */
MibHandler*
TableIterator_getTableIteratorHandler( IteratorInfo* iinfo )
{
    MibHandler* me;

    if ( !iinfo )
        return NULL;

    me = AgentHandler_createHandler( TABLE_ITERATOR_NAME,
        TableIterator_helperHandler );

    if ( !me )
        return NULL;

    me->myvoid = iinfo;
    if ( iinfo->flags & NETSNMP_HANDLER_OWNS_IINFO )
        TableIterator_handlerOwnsIteratorInfo( me );
    return me;
}

/**
 * Creates and registers a table iterator helper handler calling
 * AgentHandler_createHandler with a handler name set to TABLE_ITERATOR_NAME
 * and access method, TableIterator_helperHandler.
 *
 * If NOT_SERIALIZED is not defined the function injects the serialize
 * handler into the calling chain prior to calling Table_registerTable.
 *
 * @param reginfo is a pointer to a HandlerRegistration struct
 *
 * @param iinfo A pointer to a IteratorInfo struct. If the flag
 * NETSNMP_HANDLER_OWNS_IINFO is not set in iinfo->flags, the caller remains
 * the owner of this structure. And if the flag NETSNMP_HANDLER_OWNS_IINFO is
 * set in iinfo->flags, ownership of this data structure is passed to the
 * handler.
 *
 * @return MIB_REGISTERED_OK is returned if the registration was a success.
 *	Failures are MIB_REGISTRATION_FAILED, MIB_DUPLICATE_REGISTRATION.
 *	If iinfo is NULL, ErrorCode_GENERR is returned.
 *
 */
int TableIterator_registerTableIterator( HandlerRegistration* reginfo,
    IteratorInfo* iinfo )
{
    reginfo->modes |= HANDLER_CAN_STASH;
    AgentHandler_injectHandler( reginfo,
        TableIterator_getTableIteratorHandler( iinfo ) );
    if ( !iinfo )
        return ErrorCode_GENERR;
    if ( !iinfo->indexes && iinfo->table_reginfo && iinfo->table_reginfo->indexes )
        iinfo->indexes = Client_cloneVarbind( iinfo->table_reginfo->indexes );

    return Table_registerTable( reginfo, iinfo->table_reginfo );
}

/** extracts the table_iterator specific data from a request.
 * This function extracts the table iterator specific data from a
 * RequestInfo object.  Calls AgentHandler_requestGetListData
 * with request->parent_data set with data from a request that was added
 * previously by a module and TABLE_ITERATOR_NAME handler name.
 *
 * @param request the netsnmp request info structure
 *
 * @return a void pointer(request->parent_data->data), otherwise NULL is
 *         returned if request is NULL or request->parent_data is NULL or
 *         request->parent_data object is not found.the net
 *
 */
void* TableIterator_extractIteratorContext( RequestInfo* request )
{
    return AgentHandler_requestGetListData( request, TABLE_ITERATOR_NAME );
}

/** inserts table_iterator specific data for a newly
 *  created row into a request */
void TableIterator_insertIteratorContext( RequestInfo* request, void* data )
{
    RequestInfo* req;
    TableRequestInfo* table_info = NULL;
    Types_VariableList* this_index = NULL;
    Types_VariableList* that_index = NULL;
    oid base_oid[] = { 0, 0 }; /* Make sure index OIDs are legal! */
    oid this_oid[ ASN01_MAX_OID_LEN ];
    oid that_oid[ ASN01_MAX_OID_LEN ];
    size_t this_oid_len, that_oid_len;

    if ( !request )
        return;

    /*
     * We'll add the new row information to any request
     * structure with the same index values as the request
     * passed in (which includes that one!).
     *
     * So construct an OID based on these index values.
     */

    table_info = Table_extractTableInfo( request );
    this_index = table_info->indexes;
    Mib_buildOidNoalloc( this_oid, ASN01_MAX_OID_LEN, &this_oid_len,
        base_oid, 2, this_index );

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
    for ( req = request; req->prev; req = req->prev )
        ;

    /*
     * ... and then start looking for matching indexes
     * (by constructing OIDs from these index values)
     */
    for ( ; req; req = req->next ) {
        table_info = Table_extractTableInfo( req );
        that_index = table_info->indexes;
        Mib_buildOidNoalloc( that_oid, ASN01_MAX_OID_LEN, &that_oid_len,
            base_oid, 2, that_index );

        /*
         * This request has the same index values,
         * so add the newly-created row information.
         */
        if ( Api_oidCompare( this_oid, this_oid_len,
                 that_oid, that_oid_len )
            == 0 ) {
            AgentHandler_requestAddListData( req,
                Map_newElement( TABLE_ITERATOR_NAME, data, NULL ) );
        }
    }
}

#define TI_REQUEST_CACHE "tiCache"

typedef struct TiCacheInfo_s {
    oid best_match[ ASN01_MAX_OID_LEN ];
    size_t best_match_len;
    void* data_context;
    FreeDataContextFT* free_context;
    IteratorInfo* iinfo;
    Types_VariableList* results;
} TiCacheInfo;

static void
_TableIterator_freeTiCache( void* it )
{
    TiCacheInfo* beer = ( TiCacheInfo* )it;
    if ( !it )
        return;
    if ( beer->data_context && beer->free_context ) {
        ( beer->free_context )( beer->data_context, beer->iinfo );
    }
    if ( beer->results ) {
        Api_freeVarbind( beer->results );
    }
    free( beer );
}

/* caches information (in the request) we'll need at a later point in time */
static TiCacheInfo*
_TableIterator_remember( RequestInfo* request,
    oid* oid_to_save,
    size_t oid_to_save_len,
    void* callback_data_context,
    void* callback_loop_context,
    IteratorInfo* iinfo )
{
    TiCacheInfo* ti_info;

    if ( !request || !oid_to_save || oid_to_save_len > ASN01_MAX_OID_LEN )
        return NULL;

    /* extract existing cached state */
    ti_info = ( TiCacheInfo* )AgentHandler_requestGetListData( request, TI_REQUEST_CACHE );

    /* no existing cached state.  make a new one. */
    if ( !ti_info ) {
        ti_info = MEMORY_MALLOC_TYPEDEF( TiCacheInfo );
        if ( ti_info == NULL )
            return NULL;
        AgentHandler_requestAddListData( request,
            Map_newElement( TI_REQUEST_CACHE,
                                             ti_info,
                                             _TableIterator_freeTiCache ) );
    }

    /* free existing cache before replacing */
    if ( ti_info->data_context && ti_info->free_context )
        ( ti_info->free_context )( ti_info->data_context, iinfo );

    /* maybe generate it from the loop context? */
    if ( iinfo->make_data_context && !callback_data_context ) {
        callback_data_context = ( iinfo->make_data_context )( callback_loop_context, iinfo );
    }

    /* save data as requested */
    ti_info->data_context = callback_data_context;
    ti_info->free_context = iinfo->free_data_context;
    ti_info->best_match_len = oid_to_save_len;
    ti_info->iinfo = iinfo;
    if ( oid_to_save_len )
        memcpy( ti_info->best_match, oid_to_save, oid_to_save_len * sizeof( oid ) );

    return ti_info;
}

#define TABLE_ITERATOR_NOTAGAIN 255
/* implements the table_iterator helper */
int TableIterator_helperHandler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    TableRegistrationInfo* tbl_info;
    TableRequestInfo* table_info = NULL;
    oid coloid[ ASN01_MAX_OID_LEN ];
    size_t coloid_len;
    int ret = PRIOT_ERR_NOERROR;
    static oid myname[ ASN01_MAX_OID_LEN ];
    size_t myname_len;
    int oldmode = 0;
    IteratorInfo* iinfo;
    int notdone;
    int hintok = 0;
    RequestInfo *request, *reqtmp = NULL;
    Types_VariableList* index_search = NULL;
    Types_VariableList* free_this_index_search = NULL;
    void *callback_loop_context = NULL, *last_loop_context;
    void* callback_data_context = NULL;
    TiCacheInfo* ti_info = NULL;
    int request_count = 0;
    OidStash_Node** cinfo = NULL;
    Types_VariableList *old_indexes = NULL, *vb;
    TableRegistrationInfo* table_reg_info = NULL;
    int i;
    Map* ldata = NULL;

    iinfo = ( IteratorInfo* )handler->myvoid;
    if ( !iinfo || !reginfo || !reqinfo )
        return PRIOT_ERR_GENERR;

    tbl_info = iinfo->table_reginfo;

    /*
     * copy in the table registration oid for later use
     */
    coloid_len = reginfo->rootoid_len + 2;
    memcpy( coloid, reginfo->rootoid, reginfo->rootoid_len * sizeof( oid ) );
    coloid[ reginfo->rootoid_len ] = 1; /* table.entry node */

    /*
     * illegally got here if these functions aren't defined
     */
    if ( iinfo->get_first_data_point == NULL || iinfo->get_next_data_point == NULL ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "table_iterator helper called without data accessor functions\n" );
        return PRIOT_ERR_GENERR;
    }

    /* preliminary analysis */
    switch ( reqinfo->mode ) {
    case MODE_GET_STASH:
        cinfo = StashCache_extractStashCache( reqinfo );
        table_reg_info = Table_findTableRegistrationInfo( reginfo );

        /* XXX: move this malloc to stash_cache handler? */
        reqtmp = MEMORY_MALLOC_TYPEDEF( RequestInfo );
        if ( reqtmp == NULL )
            return PRIOT_ERR_GENERR;
        reqtmp->subtree = requests->subtree;
        table_info = Table_extractTableInfo( requests );
        AgentHandler_requestAddListData( reqtmp,
            Map_newElement( TABLE_HANDLER_NAME,
                                             ( void* )table_info, NULL ) );

        /* remember the indexes that were originally parsed. */
        old_indexes = table_info->indexes;
        break;
    case MODE_GETNEXT:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;
            table_info = Table_extractTableInfo( request );
            if ( table_info == NULL ) {
                /*
                 * Cleanup
                 */
                if ( free_this_index_search )
                    Api_freeVarbind( free_this_index_search );
                return PRIOT_ERR_GENERR;
            }
            if ( table_info->colnum < tbl_info->min_column - 1 ) {
                /* XXX: optimize better than this */
                /* for now, just increase to colnum-1 */
                /* we need to jump to the lowest result of the min_column
                   and take it, comparing to nothing from the request */
                table_info->colnum = tbl_info->min_column - 1;
            } else if ( table_info->colnum > tbl_info->max_column ) {
                request->processed = TABLE_ITERATOR_NOTAGAIN;
            }

            ti_info = ( TiCacheInfo* )
                AgentHandler_requestGetListData( request, TI_REQUEST_CACHE );
            if ( !ti_info ) {
                ti_info = MEMORY_MALLOC_TYPEDEF( TiCacheInfo );
                if ( ti_info == NULL ) {
                    /*
                     * Cleanup
                     */
                    if ( free_this_index_search )
                        Api_freeVarbind( free_this_index_search );
                    return PRIOT_ERR_GENERR;
                }
                AgentHandler_requestAddListData( request,
                    Map_newElement( TI_REQUEST_CACHE,
                                                     ti_info,
                                                     _TableIterator_freeTiCache ) );
            }

            /* XXX: if no valid requests, don't even loop below */
        }
        break;
    }

    /*
     * collect all information for each needed row
     */
    if ( reqinfo->mode == MODE_GET || reqinfo->mode == MODE_GETNEXT || reqinfo->mode == MODE_GET_STASH
        || reqinfo->mode == MODE_SET_RESERVE1 ) {
        /*
         * Count the number of request in the list,
         *   so that we'll know when we're finished
         */
        for ( request = requests; request; request = request->next )
            if ( !request->processed )
                request_count++;
        notdone = 1;
        hintok = 1;
        while ( notdone ) {
            notdone = 0;

            /* find first data point */
            if ( !index_search ) {
                if ( free_this_index_search ) {
                    /* previously done */
                    index_search = free_this_index_search;
                } else {
                    for ( request = requests; request; request = request->next ) {
                        table_info = Table_extractTableInfo( request );
                        if ( table_info )
                            break;
                    }
                    if ( !table_info ) {
                        Logger_log( LOGGER_PRIORITY_WARNING,
                            "no valid requests for iterator table %s\n",
                            reginfo->handlerName );
                        AgentHandler_freeRequestDataSets( reqtmp );
                        MEMORY_FREE( reqtmp );
                        return PRIOT_ERR_NOERROR;
                    }
                    index_search = Client_cloneVarbind( table_info->indexes );
                    free_this_index_search = index_search;

                    /* setup, malloc search data: */
                    if ( !index_search ) {
                        /*
                         * hmmm....  invalid table?
                         */
                        Logger_log( LOGGER_PRIORITY_WARNING,
                            "invalid index list or failed malloc for table %s\n",
                            reginfo->handlerName );
                        AgentHandler_freeRequestDataSets( reqtmp );
                        MEMORY_FREE( reqtmp );
                        return PRIOT_ERR_NOERROR;
                    }
                }
            }

            /* if sorted, pass in a hint */
            if ( hintok && ( iinfo->flags & NETSNMP_ITERATOR_FLAG_SORTED ) ) {
                callback_loop_context = table_info;
            }
            index_search = ( iinfo->get_first_data_point )( &callback_loop_context,
                &callback_data_context,
                index_search, iinfo );

            /* loop over each data point */
            while ( index_search ) {

                /* remember to free this later */
                free_this_index_search = index_search;

                /* compare against each request*/
                for ( request = requests; request; request = request->next ) {
                    if ( request->processed )
                        continue;

                    /* XXX: store in an array for faster retrieval */
                    table_info = Table_extractTableInfo( request );
                    if ( table_info == NULL ) {
                        /*
                         * Cleanup
                         */
                        if ( free_this_index_search )
                            Api_freeVarbind( free_this_index_search );
                        AgentHandler_freeRequestDataSets( reqtmp );
                        MEMORY_FREE( reqtmp );
                        return PRIOT_ERR_GENERR;
                    }
                    coloid[ reginfo->rootoid_len + 1 ] = table_info->colnum;

                    ti_info = ( TiCacheInfo* )
                        AgentHandler_requestGetListData( request, TI_REQUEST_CACHE );

                    switch ( reqinfo->mode ) {
                    case MODE_GET:
                    case MODE_SET_RESERVE1:
                        /* looking for exact matches */
                        Mib_buildOidNoalloc( myname, ASN01_MAX_OID_LEN, &myname_len,
                            coloid, coloid_len, index_search );
                        if ( Api_oidCompare( myname, myname_len,
                                 request->requestvb->name,
                                 request->requestvb->nameLength )
                            == 0 ) {
                            /*
                             * keep this
                             */
                            if ( _TableIterator_remember( request,
                                     myname,
                                     myname_len,
                                     callback_data_context,
                                     callback_loop_context,
                                     iinfo )
                                == NULL ) {
                                /*
                                 * Cleanup
                                 */
                                if ( free_this_index_search )
                                    Api_freeVarbind( free_this_index_search );
                                AgentHandler_freeRequestDataSets( reqtmp );
                                MEMORY_FREE( reqtmp );
                                return PRIOT_ERR_GENERR;
                            }
                            request_count--; /* One less to look for */
                        } else {
                            if ( iinfo->free_data_context && callback_data_context ) {
                                ( iinfo->free_data_context )( callback_data_context,
                                    iinfo );
                            }
                        }
                        break;

                    case MODE_GET_STASH:
                        /* collect data for each column for every row */
                        Mib_buildOidNoalloc( myname, ASN01_MAX_OID_LEN, &myname_len,
                            coloid, coloid_len, index_search );
                        reqinfo->mode = MODE_GET;
                        if ( reqtmp )
                            ldata = Map_find( reqtmp->parent_data,
                                TABLE_ITERATOR_NAME );
                        if ( !ldata ) {
                            AgentHandler_requestAddListData( reqtmp,
                                Map_newElement( TABLE_ITERATOR_NAME,
                                                                 callback_data_context,
                                                                 NULL ) );
                        } else {
                            /* may have changed */
                            ldata->value = callback_data_context;
                        }

                        table_info->indexes = index_search;
                        for ( i = table_reg_info->min_column;
                              i <= ( int )table_reg_info->max_column; i++ ) {
                            myname[ reginfo->rootoid_len + 1 ] = i;
                            table_info->colnum = i;
                            vb = reqtmp->requestvb = MEMORY_MALLOC_TYPEDEF( Types_VariableList );
                            if ( vb == NULL ) {
                                /*
                                 * Cleanup
                                 */
                                if ( free_this_index_search )
                                    Api_freeVarbind( free_this_index_search );
                                return PRIOT_ERR_GENERR;
                            }
                            vb->type = ASN01_NULL;
                            Client_setVarObjid( vb, myname, myname_len );
                            AgentHandler_callNextHandler( handler, reginfo,
                                reqinfo, reqtmp );
                            reqtmp->requestvb = NULL;
                            reqtmp->processed = 0;
                            if ( vb->type != ASN01_NULL ) { /* XXX, not all */
                                OidStash_addData( cinfo, myname,
                                    myname_len, vb );
                            } else {
                                Api_freeVar( vb );
                            }
                        }
                        reqinfo->mode = MODE_GET_STASH;
                        break;

                    case MODE_GETNEXT:
                        /* looking for "next" matches */
                        if ( Table_checkGetnextReply( request, coloid, coloid_len, index_search,
                                 &ti_info->results ) ) {
                            if ( _TableIterator_remember( request,
                                     ti_info->results->name,
                                     ti_info->results->nameLength,
                                     callback_data_context,
                                     callback_loop_context,
                                     iinfo )
                                == NULL ) {
                                /*
                                 * Cleanup
                                 */
                                if ( free_this_index_search )
                                    Api_freeVarbind( free_this_index_search );
                                return PRIOT_ERR_GENERR;
                            }
                            /*
                             *  If we've been told that the rows are sorted,
                             *   then the first valid one we find
                             *   must be the right one.
                             */
                            if ( iinfo->flags & NETSNMP_ITERATOR_FLAG_SORTED )
                                request_count--;

                        } else {
                            if ( iinfo->free_data_context && callback_data_context ) {
                                ( iinfo->free_data_context )( callback_data_context,
                                    iinfo );
                            }
                        }
                        break;

                    case MODE_SET_RESERVE2:
                    case MODE_SET_FREE:
                    case MODE_SET_UNDO:
                    case MODE_SET_COMMIT:
                        /* needed processing already done in IMPL_RESERVE1 */
                        break;

                    default:
                        Logger_log( LOGGER_PRIORITY_ERR,
                            "table_iterator called with unsupported mode\n" );
                        break; /* XXX return */
                    }
                }

                /* Is there any point in carrying on? */
                if ( !request_count )
                    break;
                /* get the next search possibility */
                last_loop_context = callback_loop_context;
                index_search = ( iinfo->get_next_data_point )( &callback_loop_context,
                    &callback_data_context,
                    index_search, iinfo );
                if ( iinfo->free_loop_context && last_loop_context && callback_data_context != last_loop_context ) {
                    ( iinfo->free_loop_context )( last_loop_context, iinfo );
                    last_loop_context = NULL;
                }
            }

            /* free loop context before going on */
            if ( callback_loop_context && iinfo->free_loop_context_at_end ) {
                ( iinfo->free_loop_context_at_end )( callback_loop_context,
                    iinfo );
                callback_loop_context = NULL;
            }

            /* decide which (GETNEXT) requests are not yet filled */
            if ( reqinfo->mode == MODE_GETNEXT ) {
                for ( request = requests; request; request = request->next ) {
                    if ( request->processed )
                        continue;
                    ti_info = ( TiCacheInfo* )
                        AgentHandler_requestGetListData( request,
                            TI_REQUEST_CACHE );
                    if ( !ti_info->results ) {
                        int nc;
                        table_info = Table_extractTableInfo( request );
                        nc = Table_nextColumn( table_info );
                        if ( 0 == nc ) {
                            coloid[ reginfo->rootoid_len + 1 ] = table_info->colnum + 1;
                            Client_setVarObjid( request->requestvb,
                                coloid, reginfo->rootoid_len + 2 );
                            request->processed = TABLE_ITERATOR_NOTAGAIN;
                            break;
                        } else {
                            table_info->colnum = nc;
                            hintok = 0;
                            notdone = 1;
                        }
                    }
                }
            }
        }
    }

    if ( reqinfo->mode == MODE_GET || reqinfo->mode == MODE_GETNEXT
        || reqinfo->mode == MODE_SET_RESERVE1 ) {
        /* per request last minute processing */
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;
            ti_info = ( TiCacheInfo* )
                AgentHandler_requestGetListData( request, TI_REQUEST_CACHE );
            table_info = Table_extractTableInfo( request );

            if ( !ti_info )
                continue;

            switch ( reqinfo->mode ) {

            case MODE_GETNEXT:
                if ( ti_info->best_match_len )
                    Client_setVarObjid( request->requestvb, ti_info->best_match,
                        ti_info->best_match_len );
                else {
                    coloid[ reginfo->rootoid_len + 1 ] = Table_nextColumn( table_info );
                    if ( 0 == coloid[ reginfo->rootoid_len + 1 ] ) {
                        /* out of range. */
                        coloid[ reginfo->rootoid_len + 1 ] = tbl_info->max_column + 1;
                        request->processed = TABLE_ITERATOR_NOTAGAIN;
                    }
                    Client_setVarObjid( request->requestvb,
                        coloid, reginfo->rootoid_len + 2 );
                    request->processed = 1;
                }
                Api_freeVarbind( table_info->indexes );
                table_info->indexes = Client_cloneVarbind( ti_info->results );
            /* FALL THROUGH */

            case MODE_GET:
            case MODE_SET_RESERVE1:
                if ( ti_info->data_context )
                    /* we don't add a free pointer, since it's in the
                       TI_REQUEST_CACHE instead */
                    AgentHandler_requestAddListData( request,
                        Map_newElement( TABLE_ITERATOR_NAME,
                                                         ti_info->data_context,
                                                         NULL ) );
                break;

            default:
                break;
            }
        }

        /* we change all GETNEXT operations into GET operations.
           why? because we're just so nice to the lower levels.
           maybe someday they'll pay us for it.  doubtful though. */
        oldmode = reqinfo->mode;
        if ( reqinfo->mode == MODE_GETNEXT ) {
            reqinfo->mode = MODE_GET;
        }
    } else if ( reqinfo->mode == MODE_GET_STASH ) {
        AgentHandler_freeRequestDataSets( reqtmp );
        MEMORY_FREE( reqtmp );
        table_info->indexes = old_indexes;
    }

    /* Finally, we get to call the next handler below us.  Boy, wasn't
       all that simple?  They better be glad they don't have to do it! */
    if ( reqinfo->mode != MODE_GET_STASH ) {
        DEBUG_MSGTL( ( "tableIterator", "call subhandler for mode: %s\n",
            MapList_findLabel( "agentMode", oldmode ) ) );
        ret = AgentHandler_callNextHandler( handler, reginfo, reqinfo, requests );
    }

    /* reverse the previously saved mode if we were a getnext */
    if ( oldmode == MODE_GETNEXT ) {
        reqinfo->mode = oldmode;
    }

    /* cleanup */
    if ( free_this_index_search )
        Api_freeVarbind( free_this_index_search );

    return ret;
}

/* ==================================
 *
 * Iterator API: Row operations
 *
 * ================================== */

void* TableIterator_rowFirst( IteratorInfo* iinfo )
{
    Types_VariableList *vp1, *vp2;
    void *ctx1, *ctx2;

    if ( !iinfo )
        return NULL;

    vp1 = Client_cloneVarbind( iinfo->indexes );
    vp2 = iinfo->get_first_data_point( &ctx1, &ctx2, vp1, iinfo );

    if ( !vp2 )
        ctx2 = NULL;

    /* free loop context ?? */
    Api_freeVarbind( vp1 );
    return ctx2; /* or *ctx2 ?? */
}

void* TableIterator_rowGet( IteratorInfo* iinfo, void* row )
{
    Types_VariableList *vp1, *vp2;
    void *ctx1, *ctx2;

    if ( !iinfo || !row )
        return NULL;

    /*
         * This routine relies on being able to
         *   determine the indexes for a given row.
         */
    if ( !iinfo->get_row_indexes )
        return NULL;

    vp1 = Client_cloneVarbind( iinfo->indexes );
    ctx1 = row; /* Probably only need one of these ... */
    ctx2 = row;
    vp2 = iinfo->get_row_indexes( &ctx1, &ctx2, vp1, iinfo );

    ctx2 = NULL;
    if ( vp2 ) {
        ctx2 = TableIterator_rowGetByidx( iinfo, vp2 );
    }
    Api_freeVarbind( vp1 );
    return ctx2;
}

void* TableIterator_rowNext( IteratorInfo* iinfo, void* row )
{
    Types_VariableList *vp1, *vp2;
    void *ctx1, *ctx2;

    if ( !iinfo || !row )
        return NULL;

    /*
         * This routine relies on being able to
         *   determine the indexes for a given row.
         */
    if ( !iinfo->get_row_indexes )
        return NULL;

    vp1 = Client_cloneVarbind( iinfo->indexes );
    ctx1 = row; /* Probably only need one of these ... */
    ctx2 = row;
    vp2 = iinfo->get_row_indexes( &ctx1, &ctx2, vp1, iinfo );

    ctx2 = NULL;
    if ( vp2 ) {
        ctx2 = TableIterator_rowNextByidx( iinfo, vp2 );
    }
    Api_freeVarbind( vp1 );
    return ctx2;
}

void* TableIterator_rowGetByidx( IteratorInfo* iinfo,
    Types_VariableList* indexes )
{
    oid dummy[] = { 0, 0 }; /* Keep 'build_oid' happy */
    oid instance[ ASN01_MAX_OID_LEN ];
    size_t len = ASN01_MAX_OID_LEN;

    if ( !iinfo || !indexes )
        return NULL;

    Mib_buildOidNoalloc( instance, ASN01_MAX_OID_LEN, &len,
        dummy, 2, indexes );
    return TableIterator_rowGetByoid( iinfo, instance + 2, len - 2 );
}

void* TableIterator_rowNextByidx( IteratorInfo* iinfo,
    Types_VariableList* indexes )
{
    oid dummy[] = { 0, 0 };
    oid instance[ ASN01_MAX_OID_LEN ];
    size_t len = ASN01_MAX_OID_LEN;

    if ( !iinfo || !indexes )
        return NULL;

    Mib_buildOidNoalloc( instance, ASN01_MAX_OID_LEN, &len,
        dummy, 2, indexes );
    return TableIterator_rowNextByoid( iinfo, instance + 2, len - 2 );
}

void* TableIterator_rowGetByoid( IteratorInfo* iinfo,
    oid* instance, size_t len )
{
    oid dummy[] = { 0, 0 };
    oid this_inst[ ASN01_MAX_OID_LEN ];
    size_t this_len;
    Types_VariableList *vp1, *vp2;
    void *ctx1, *ctx2;
    int n;

    if ( !iinfo || !iinfo->get_first_data_point
        || !iinfo->get_next_data_point )
        return NULL;

    if ( !instance || !len )
        return NULL;

    vp1 = Client_cloneVarbind( iinfo->indexes );
    vp2 = iinfo->get_first_data_point( &ctx1, &ctx2, vp1, iinfo );
    DEBUG_MSGTL( ( "table:iterator:get", "first DP: %p %p %p\n",
        ctx1, ctx2, vp2 ) );

    /* XXX - free context ? */

    while ( vp2 ) {
        this_len = ASN01_MAX_OID_LEN;
        Mib_buildOidNoalloc( this_inst, ASN01_MAX_OID_LEN, &this_len, dummy, 2, vp2 );
        n = Api_oidCompare( instance, len, this_inst + 2, this_len - 2 );
        if ( n == 0 )
            break; /* Found matching row */

        if ( ( n > 0 ) && ( iinfo->flags & NETSNMP_ITERATOR_FLAG_SORTED ) ) {
            vp2 = NULL; /* Row not present */
            break;
        }

        vp2 = iinfo->get_next_data_point( &ctx1, &ctx2, vp2, iinfo );
        DEBUG_MSGTL( ( "table:iterator:get", "next DP: %p %p %p\n",
            ctx1, ctx2, vp2 ) );
        /* XXX - free context ? */
    }

    /* XXX - final free context ? */
    Api_freeVarbind( vp1 );

    return ( vp2 ? ctx2 : NULL );
}

void* TableIterator_rowNextByoid( IteratorInfo* iinfo,
    oid* instance, size_t len )
{
    oid dummy[] = { 0, 0 };
    oid this_inst[ ASN01_MAX_OID_LEN ];
    size_t this_len;
    oid best_inst[ ASN01_MAX_OID_LEN ];
    size_t best_len = 0;
    Types_VariableList *vp1, *vp2;
    void *ctx1, *ctx2;
    int n;

    if ( !iinfo || !iinfo->get_first_data_point
        || !iinfo->get_next_data_point )
        return NULL;

    vp1 = Client_cloneVarbind( iinfo->indexes );
    vp2 = iinfo->get_first_data_point( &ctx1, &ctx2, vp1, iinfo );
    DEBUG_MSGTL( ( "table:iterator:get", "first DP: %p %p %p\n",
        ctx1, ctx2, vp2 ) );

    if ( !instance || !len ) {
        Api_freeVarbind( vp1 );
        return ( vp2 ? ctx2 : NULL ); /* First entry */
    }

    /* XXX - free context ? */

    while ( vp2 ) {
        this_len = ASN01_MAX_OID_LEN;
        Mib_buildOidNoalloc( this_inst, ASN01_MAX_OID_LEN, &this_len, dummy, 2, vp2 );
        n = Api_oidCompare( instance, len, this_inst + 2, this_len - 2 );

        /*
         * Look for the best-fit candidate for the next row
         *   (bearing in mind the rows may not be ordered "correctly")
         */
        if ( n > 0 ) {
            if ( best_len == 0 ) {
                memcpy( best_inst, this_inst, sizeof( this_inst ) );
                best_len = this_len;
                if ( iinfo->flags & NETSNMP_ITERATOR_FLAG_SORTED )
                    break;
            } else {
                n = Api_oidCompare( best_inst, best_len, this_inst, this_len );
                if ( n < 0 ) {
                    memcpy( best_inst, this_inst, sizeof( this_inst ) );
                    best_len = this_len;
                    if ( iinfo->flags & NETSNMP_ITERATOR_FLAG_SORTED )
                        break;
                }
            }
        }

        vp2 = iinfo->get_next_data_point( &ctx1, &ctx2, vp2, iinfo );
        DEBUG_MSGTL( ( "table:iterator:get", "next DP: %p %p %p\n",
            ctx1, ctx2, vp2 ) );
        /*netsnmp_ XXX - free context ? */
    }

    /* XXX - final free context ? */
    Api_freeVarbind( vp1 );

    return ( vp2 ? ctx2 : NULL );
}

int TableIterator_rowCount( IteratorInfo* iinfo )
{
    Types_VariableList *vp1, *vp2;
    void *ctx1, *ctx2;
    int i = 0;

    if ( !iinfo || !iinfo->get_first_data_point
        || !iinfo->get_next_data_point )
        return 0;

    vp1 = Client_cloneVarbind( iinfo->indexes );
    vp2 = iinfo->get_first_data_point( &ctx1, &ctx2, vp1, iinfo );
    if ( !vp2 ) {
        Api_freeVarbind( vp1 );
        return 0;
    }

    DEBUG_MSGTL( ( "table:iterator:count", "first DP: %p %p %p\n",
        ctx1, ctx2, vp2 ) );

    /* XXX - free context ? */

    while ( vp2 ) {
        i++;
        vp2 = iinfo->get_next_data_point( &ctx1, &ctx2, vp2, iinfo );
        DEBUG_MSGTL( ( "table:iterator:count", "next DP: %p %p %p (%d)\n",
            ctx1, ctx2, vp2, i ) );
        /* XXX - free context ? */
    }

    /* XXX - final free context ? */
    Api_freeVarbind( vp1 );
    return i;
}
