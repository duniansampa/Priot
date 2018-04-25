/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 15899 $ of $ 
 *
 * $Id:ipv6ScopeZoneIndexTable_interface.c 14170 2007-04-29 00:12:32Z varun_c$
 */
/*
 * *********************************************************************
 * *********************************************************************
 * *********************************************************************
 * ***                                                               ***
 * ***  NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE NOTE  ***
 * ***                                                               ***
 * ***                                                               ***
 * ***       THIS FILE DOES NOT CONTAIN ANY USER EDITABLE CODE.      ***
 * ***                                                               ***
 * ***                                                               ***
 * ***       THE GENERATED CODE IS INTERNAL IMPLEMENTATION, AND      ***
 * ***                                                               ***
 * ***                                                               ***
 * ***    IS SUBJECT TO CHANGE WITHOUT WARNING IN FUTURE RELEASES.   ***
 * ***                                                               ***
 * ***                                                               ***
 * *********************************************************************
 * *********************************************************************
 * *********************************************************************
 */

#include "ipv6ScopeZoneIndexTable_interface.h"
#include "Assert.h"
#include "BabySteps.h"
#include "CacheHandler.h"
#include "Client.h"
#include "Debug.h"
#include "Logger.h"
#include "Mib.h"
#include "RowMerge.h"
#include "TableContainer.h"
#include "ipv6ScopeZoneIndexTable_data_access.h"
#include "ipv6ScopeZoneIndexTable_data_access.h"
#include "ipv6ScopeZoneIndexTable_oids.h"
#include "siglog/agent/mfd.h"

/**********************************************************************
 **********************************************************************
 ***
 *** Table ipv6ScopeZoneIndexTable
 ***
 **********************************************************************
 **********************************************************************/
/*
 * IP-MIB::ipv6ScopeZoneIndexTable is subid 36 of ip.
 * Its status is Current.
 * OID: .1.3.6.1.2.1.4.36, length: 8
 */
typedef struct ipv6ScopeZoneIndexTable_interface_ctx_s {

    Container_Container* container;
    Cache* cache;
    ipv6ScopeZoneIndexTable_registration* user_ctx;

    TableRegistrationInfo tbl_info;

    BabyStepsAccessMethods access_multiplexer;

} ipv6ScopeZoneIndexTable_interface_ctx;

static ipv6ScopeZoneIndexTable_interface_ctx
    ipv6ScopeZoneIndexTable_if_ctx;

static void
_ipv6ScopeZoneIndexTable_container_init( ipv6ScopeZoneIndexTable_interface_ctx* if_ctx );
static void
_ipv6ScopeZoneIndexTable_container_shutdown( ipv6ScopeZoneIndexTable_interface_ctx* if_ctx );
static int
_cache_load( Cache* cache, void* vmagic );
static void
_cache_free( Cache* cache, void* magic );

Container_Container*
ipv6ScopeZoneIndexTable_container_get( void )
{
    return ipv6ScopeZoneIndexTable_if_ctx.container;
}

ipv6ScopeZoneIndexTable_registration*
ipv6ScopeZoneIndexTable_registration_get( void )
{
    return ipv6ScopeZoneIndexTable_if_ctx.user_ctx;
}

ipv6ScopeZoneIndexTable_registration*
ipv6ScopeZoneIndexTable_registration_set( ipv6ScopeZoneIndexTable_registration* newreg )
{
    ipv6ScopeZoneIndexTable_registration* old = ipv6ScopeZoneIndexTable_if_ctx.user_ctx;
    ipv6ScopeZoneIndexTable_if_ctx.user_ctx = newreg;
    return old;
}

int ipv6ScopeZoneIndexTable_container_size( void )
{
    return CONTAINER_SIZE( ipv6ScopeZoneIndexTable_if_ctx.container );
}

/*
 * mfd multiplexer modes
 */
static NodeHandlerFT _mfd_ipv6ScopeZoneIndexTable_pre_request;
static NodeHandlerFT _mfd_ipv6ScopeZoneIndexTable_post_request;
static NodeHandlerFT _mfd_ipv6ScopeZoneIndexTable_object_lookup;
static NodeHandlerFT _mfd_ipv6ScopeZoneIndexTable_get_values;
/**
 * @internal
 * Initialize the table ipv6ScopeZoneIndexTable 
 *    (Define its contents and how it's structured)
 */
void _ipv6ScopeZoneIndexTable_initialize_interface( ipv6ScopeZoneIndexTable_registration* reg_ptr, u_long flags )
{
    BabyStepsAccessMethods* access_multiplexer = &ipv6ScopeZoneIndexTable_if_ctx.access_multiplexer;
    TableRegistrationInfo* tbl_info = &ipv6ScopeZoneIndexTable_if_ctx.tbl_info;
    HandlerRegistration* reginfo;
    MibHandler* handler;
    int mfd_modes = 0;

    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_ipv6ScopeZoneIndexTable_initialize_interface", "called\n" ) );

    /*************************************************
     *
     * save interface context for ipv6ScopeZoneIndexTable
     */
    /*
     * Setting up the table's definition
     */
    Table_helperAddIndexes( tbl_info, ASN01_INTEGER,
        /** index: ipv6ScopeZoneIndexIfIndex */
        0 );

    /*
     * Define the minimum and maximum accessible columns.  This
     * optimizes retrieval. 
     */
    tbl_info->min_column = IPV6SCOPEZONEINDEXTABLE_MIN_COL;
    tbl_info->max_column = IPV6SCOPEZONEINDEXTABLE_MAX_COL;

    /*
     * save users context
     */
    ipv6ScopeZoneIndexTable_if_ctx.user_ctx = reg_ptr;

    /*
     * call data access initialization code
     */
    ipv6ScopeZoneIndexTable_init_data( reg_ptr );

    /*
     * set up the container
     */
    _ipv6ScopeZoneIndexTable_container_init( &ipv6ScopeZoneIndexTable_if_ctx );
    if ( NULL == ipv6ScopeZoneIndexTable_if_ctx.container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "could not initialize container for ipv6ScopeZoneIndexTable\n" );
        return;
    }

    /*
     * access_multiplexer: REQUIRED wrapper for get request handling
     */
    access_multiplexer->object_lookup = _mfd_ipv6ScopeZoneIndexTable_object_lookup;
    access_multiplexer->get_values = _mfd_ipv6ScopeZoneIndexTable_get_values;

    /*
     * no wrappers yet
     */
    access_multiplexer->pre_request = _mfd_ipv6ScopeZoneIndexTable_pre_request;
    access_multiplexer->post_request = _mfd_ipv6ScopeZoneIndexTable_post_request;

    /*************************************************
     *
     * Create a registration, save our reg data, register table.
     */
    DEBUG_MSGTL( ( "ipv6ScopeZoneIndexTable:init_ipv6ScopeZoneIndexTable",
        "Registering ipv6ScopeZoneIndexTable as a mibs-for-dummies table.\n" ) );
    handler = BabySteps_accessMultiplexerGet( access_multiplexer );
    reginfo = AgentHandler_registrationCreate( "ipv6ScopeZoneIndexTable",
        handler,
        ipv6ScopeZoneIndexTable_oid,
        ipv6ScopeZoneIndexTable_oid_size,
        HANDLER_CAN_BABY_STEP | HANDLER_CAN_RONLY );
    if ( NULL == reginfo ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "error registering table ipv6ScopeZoneIndexTable\n" );
        return;
    }
    reginfo->my_reg_void = &ipv6ScopeZoneIndexTable_if_ctx;

    /*************************************************
     *
     * set up baby steps handler, create it and inject it
     */
    if ( access_multiplexer->object_lookup )
        mfd_modes |= BabyStepsMode_OBJECT_LOOKUP;

    if ( access_multiplexer->pre_request )
        mfd_modes |= BabyStepsMode_PRE_REQUEST;
    if ( access_multiplexer->post_request )
        mfd_modes |= BabyStepsMode_POST_REQUEST;

    if ( access_multiplexer->set_values )
        mfd_modes |= BabyStepsMode_SET_VALUES;
    if ( access_multiplexer->irreversible_commit )
        mfd_modes |= BabyStepsMode_IRREVERSIBLE_COMMIT;
    if ( access_multiplexer->object_syntax_checks )
        mfd_modes |= BabyStepsMode_CHECK_OBJECT;

    if ( access_multiplexer->undo_setup )
        mfd_modes |= BabyStepsMode_UNDO_SETUP;
    if ( access_multiplexer->undo_cleanup )
        mfd_modes |= BabyStepsMode_UNDO_CLEANUP;
    if ( access_multiplexer->undo_sets )
        mfd_modes |= BabyStepsMode_UNDO_SETS;

    if ( access_multiplexer->row_creation )
        mfd_modes |= BabyStepsMode_ROW_CREATE;
    if ( access_multiplexer->consistency_checks )
        mfd_modes |= BabyStepsMode_CHECK_CONSISTENCY;
    if ( access_multiplexer->commit )
        mfd_modes |= BabyStepsMode_COMMIT;
    if ( access_multiplexer->undo_commit )
        mfd_modes |= BabyStepsMode_UNDO_COMMIT;

    handler = BabySteps_handlerGet( mfd_modes );
    AgentHandler_injectHandler( reginfo, handler );

    /*************************************************
     *
     * inject row_merge helper with prefix rootoid_len + 2 (entry.col)
     */
    handler = RowMerge_getRowMergeHandler( reginfo->rootoid_len + 2 );
    AgentHandler_injectHandler( reginfo, handler );

    /*************************************************
     *
     * inject container_table helper
     */
    handler = TableContainer_handlerGet( tbl_info,
        ipv6ScopeZoneIndexTable_if_ctx.container,
        TABLE_CONTAINER_KEY_NETSNMP_INDEX );
    AgentHandler_injectHandler( reginfo, handler );

    if ( NULL != ipv6ScopeZoneIndexTable_if_ctx.cache ) {
        handler = CacheHandler_handlerGet( ipv6ScopeZoneIndexTable_if_ctx.cache );
        AgentHandler_injectHandler( reginfo, handler );
    }

    /*
     * register table
     */
    Table_registerTable( reginfo, tbl_info );

} /* _ipv6ScopeZoneIndexTable_initialize_interface */

/**
 * @internal
 * Shutdown the table ipv6ScopeZoneIndexTable
 */
void _ipv6ScopeZoneIndexTable_shutdown_interface( ipv6ScopeZoneIndexTable_registration* reg_ptr )
{
    /*
     * shutdown the container
     */
    _ipv6ScopeZoneIndexTable_container_shutdown( &ipv6ScopeZoneIndexTable_if_ctx );
}

void ipv6ScopeZoneIndexTable_valid_columns_set( ColumnInfo* vc )
{
    ipv6ScopeZoneIndexTable_if_ctx.tbl_info.valid_columns = vc;
} /* ipv6ScopeZoneIndexTable_valid_columns_set */

/**
 * @internal
 * convert the index component stored in the context to an oid
 */
int ipv6ScopeZoneIndexTable_index_to_oid( Types_Index* oid_idx,
    ipv6ScopeZoneIndexTable_mib_index*
        mib_idx )
{
    int err = PRIOT_ERR_NOERROR;

    /*
     * temp storage for parsing indexes
     */
    /*
     * ipv6ScopeZoneIndexIfIndex(1)/InterfaceIndex/ASN_INTEGER/long(long)//l/a/w/e/R/d/H
     */
    Types_VariableList var_ipv6ScopeZoneIndexIfIndex;

    /*
     * set up varbinds
     */
    memset( &var_ipv6ScopeZoneIndexIfIndex, 0x00,
        sizeof( var_ipv6ScopeZoneIndexIfIndex ) );
    var_ipv6ScopeZoneIndexIfIndex.type = ASN01_INTEGER;

    /*
     * chain temp index varbinds together
     */
    var_ipv6ScopeZoneIndexIfIndex.nextVariable = NULL;

    DEBUG_MSGTL( ( "verbose:ipv6ScopeZoneIndexTable:ipv6ScopeZoneIndexTable_index_to_oid", "called\n" ) );

    /*
     * ipv6ScopeZoneIndexIfIndex(1)/InterfaceIndex/ASN_INTEGER/long(long)//l/a/w/e/R/d/H 
     */
    Client_setVarValue( &var_ipv6ScopeZoneIndexIfIndex,
        ( u_char* )&mib_idx->ipv6ScopeZoneIndexIfIndex,
        sizeof( mib_idx->ipv6ScopeZoneIndexIfIndex ) );

    err = Mib_buildOidNoalloc( oid_idx->oids, oid_idx->len, &oid_idx->len,
        NULL, 0, &var_ipv6ScopeZoneIndexIfIndex );
    if ( err )
        Logger_log( LOGGER_PRIORITY_ERR, "error %d converting index to oid\n", err );

    /*
     * parsing may have allocated memory. free it.
     */
    Client_resetVarBuffers( &var_ipv6ScopeZoneIndexIfIndex );

    return err;
} /* ipv6ScopeZoneIndexTable_index_to_oid */

/**
 * extract ipv6ScopeZoneIndexTable indexes from a Types_Index
 *
 * @retval PRIOT_ERR_NOERROR  : no error
 * @retval PRIOT_ERR_GENERR   : error
 */
int ipv6ScopeZoneIndexTable_index_from_oid( Types_Index* oid_idx,
    ipv6ScopeZoneIndexTable_mib_index*
        mib_idx )
{
    int err = PRIOT_ERR_NOERROR;

    /*
     * temp storage for parsing indexes
     */
    /*
     * ipv6ScopeZoneIndexIfIndex(1)/InterfaceIndex/ASN_INTEGER/long(long)//l/a/w/e/R/d/H
     */
    Types_VariableList var_ipv6ScopeZoneIndexIfIndex;

    /*
     * set up varbinds
     */
    memset( &var_ipv6ScopeZoneIndexIfIndex, 0x00,
        sizeof( var_ipv6ScopeZoneIndexIfIndex ) );
    var_ipv6ScopeZoneIndexIfIndex.type = ASN01_INTEGER;

    /*
     * chain temp index varbinds together
     */
    var_ipv6ScopeZoneIndexIfIndex.nextVariable = NULL;

    DEBUG_MSGTL( ( "verbose:ipv6ScopeZoneIndexTable:ipv6ScopeZoneIndexTable_index_from_oid", "called\n" ) );

    /*
     * parse the oid into the individual index components
     */
    err = Mib_parseOidIndexes( oid_idx->oids, oid_idx->len,
        &var_ipv6ScopeZoneIndexIfIndex );
    if ( err == PRIOT_ERR_NOERROR ) {
        /*
         * copy out values
         */
        mib_idx->ipv6ScopeZoneIndexIfIndex = *( ( long* )var_ipv6ScopeZoneIndexIfIndex.val.string );
    }

    /*
     * parsing may have allocated memory. free it.
     */
    Client_resetVarBuffers( &var_ipv6ScopeZoneIndexIfIndex );

    return err;
} /* ipv6ScopeZoneIndexTable_index_from_oid */

/*
 *********************************************************************
 * @internal
 * allocate resources for a ipv6ScopeZoneIndexTable_rowreq_ctx
 */
ipv6ScopeZoneIndexTable_rowreq_ctx*
ipv6ScopeZoneIndexTable_allocate_rowreq_ctx( ipv6ScopeZoneIndexTable_data* data,
    void* user_init_ctx )
{
    ipv6ScopeZoneIndexTable_rowreq_ctx* rowreq_ctx = TOOLS_MALLOC_TYPEDEF( ipv6ScopeZoneIndexTable_rowreq_ctx );
    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:ipv6ScopeZoneIndexTable_allocate_rowreq_ctx", "called\n" ) );

    if ( NULL == rowreq_ctx ) {
        Logger_log( LOGGER_PRIORITY_ERR, "Couldn't allocate memory for a "
                                         "ipv6ScopeZoneIndexTable_rowreq_ctx.\n" );
        return NULL;
    } else {
        if ( NULL != data ) {
            /*
             * track if we got data from user
             */
            rowreq_ctx->rowreq_flags |= MFD_ROW_DATA_FROM_USER;
            rowreq_ctx->data = data;
        } else if ( NULL == ( rowreq_ctx->data = ipv6ScopeZoneIndexTable_allocate_data() ) ) {
            TOOLS_FREE( rowreq_ctx );
            return NULL;
        }
    }

    rowreq_ctx->oid_idx.oids = rowreq_ctx->oid_tmp;

    rowreq_ctx->ipv6ScopeZoneIndexTable_data_list = NULL;

    /*
     * if we allocated data, call init routine
     */
    if ( !( rowreq_ctx->rowreq_flags & MFD_ROW_DATA_FROM_USER ) ) {
        if ( ErrorCode_SUCCESS != ipv6ScopeZoneIndexTable_rowreq_ctx_init( rowreq_ctx,
                                      user_init_ctx ) ) {
            ipv6ScopeZoneIndexTable_release_rowreq_ctx( rowreq_ctx );
            rowreq_ctx = NULL;
        }
    }

    return rowreq_ctx;
} /* ipv6ScopeZoneIndexTable_allocate_rowreq_ctx */

/*
 * @internal
 * release resources for a ipv6ScopeZoneIndexTable_rowreq_ctx
 */
void ipv6ScopeZoneIndexTable_release_rowreq_ctx( ipv6ScopeZoneIndexTable_rowreq_ctx* rowreq_ctx )
{
    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:ipv6ScopeZoneIndexTable_release_rowreq_ctx", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    ipv6ScopeZoneIndexTable_rowreq_ctx_cleanup( rowreq_ctx );
    /*
     * for non-transient data, don't free data we got from the user
     */
    if ( ( rowreq_ctx->data ) && !( rowreq_ctx->rowreq_flags & MFD_ROW_DATA_FROM_USER ) )
        ipv6ScopeZoneIndexTable_release_data( rowreq_ctx->data );

    /*
     * free index oid pointer
     */
    if ( rowreq_ctx->oid_idx.oids != rowreq_ctx->oid_tmp )
        free( rowreq_ctx->oid_idx.oids );

    TOOLS_FREE( rowreq_ctx );
} /* ipv6ScopeZoneIndexTable_release_rowreq_ctx */

/**
 * @internal
 * wrapper
 */
static int
_mfd_ipv6ScopeZoneIndexTable_pre_request( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* agtreq_info,
    RequestInfo* requests )
{
    int rc;

    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_mfd_ipv6ScopeZoneIndexTable_pre_request", "called\n" ) );

    if ( 1 != RowMerge_statusFirst( reginfo, agtreq_info ) ) {
        DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable",
            "skipping additional pre_request\n" ) );
        return PRIOT_ERR_NOERROR;
    }

    rc = ipv6ScopeZoneIndexTable_pre_request( ipv6ScopeZoneIndexTable_if_ctx.user_ctx );
    if ( MFD_SUCCESS != rc ) {
        /*
         * nothing we can do about it but log it
         */
        DEBUG_MSGTL( ( "ipv6ScopeZoneIndexTable", "error %d from "
                                                  "ipv6ScopeZoneIndexTable_pre_request\n",
            rc ) );
        Agent_requestSetErrorAll( requests, PRIOT_VALIDATE_ERR( rc ) );
    }

    return PRIOT_ERR_NOERROR;
} /* _mfd_ipv6ScopeZoneIndexTable_pre_request */

/**
 * @internal
 * wrapper
 */
static int
_mfd_ipv6ScopeZoneIndexTable_post_request( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* agtreq_info,
    RequestInfo* requests )
{
    ipv6ScopeZoneIndexTable_rowreq_ctx* rowreq_ctx = ( ipv6ScopeZoneIndexTable_rowreq_ctx* )
        TableContainer_rowExtract( requests );
    int rc, packet_rc;

    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_mfd_ipv6ScopeZoneIndexTable_post_request", "called\n" ) );

    /*
     * release row context, if deleted
     */
    if ( rowreq_ctx && ( rowreq_ctx->rowreq_flags & MFD_ROW_DELETED ) )
        ipv6ScopeZoneIndexTable_release_rowreq_ctx( rowreq_ctx );

    /*
     * wait for last call before calling user
     */
    if ( 1 != RowMerge_statusLast( reginfo, agtreq_info ) ) {
        DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable",
            "waiting for last post_request\n" ) );
        return PRIOT_ERR_NOERROR;
    }

    packet_rc = Agent_checkAllRequestsError( agtreq_info->asp, 0 );
    rc = ipv6ScopeZoneIndexTable_post_request( ipv6ScopeZoneIndexTable_if_ctx.user_ctx, packet_rc );
    if ( MFD_SUCCESS != rc ) {
        /*
         * nothing we can do about it but log it
         */
        DEBUG_MSGTL( ( "ipv6ScopeZoneIndexTable", "error %d from "
                                                  "ipv6ScopeZoneIndexTable_post_request\n",
            rc ) );
    }

    return PRIOT_ERR_NOERROR;
} /* _mfd_ipv6ScopeZoneIndexTable_post_request */

/**
 * @internal
 * wrapper
 */
static ipv6ScopeZoneIndexTable_rowreq_ctx*
_mfd_ipv6ScopeZoneIndexTable_rowreq_from_index( Types_Index* oid_idx,
    int* rc_ptr )
{
    ipv6ScopeZoneIndexTable_rowreq_ctx* rowreq_ctx;
    ipv6ScopeZoneIndexTable_mib_index mib_idx;
    int rc;

    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_mfd_ipv6ScopeZoneIndexTable_rowreq_from_index", "called\n" ) );

    if ( NULL == rc_ptr )
        rc_ptr = &rc;
    *rc_ptr = MFD_SUCCESS;

    memset( &mib_idx, 0x0, sizeof( mib_idx ) );

    /*
     * try to parse oid
     */
    *rc_ptr = ipv6ScopeZoneIndexTable_index_from_oid( oid_idx, &mib_idx );
    if ( MFD_SUCCESS != *rc_ptr ) {
        DEBUG_MSGT( ( "ipv6ScopeZoneIndexTable", "error parsing index\n" ) );
        return NULL;
    }

    /*
     * allocate new context
     */
    rowreq_ctx = ipv6ScopeZoneIndexTable_allocate_rowreq_ctx( NULL, NULL );
    if ( NULL == rowreq_ctx ) {
        *rc_ptr = MFD_ERROR;
        return NULL; /* msg already logged */
    }

    memcpy( &rowreq_ctx->tbl_idx, &mib_idx, sizeof( mib_idx ) );

    /*
     * copy indexes
     */
    rowreq_ctx->oid_idx.len = oid_idx->len;
    memcpy( rowreq_ctx->oid_idx.oids, oid_idx->oids,
        oid_idx->len * sizeof( oid ) );

    return rowreq_ctx;
} /* _mfd_ipv6ScopeZoneIndexTable_rowreq_from_index */

/**
 * @internal
 * wrapper
 */
static int
_mfd_ipv6ScopeZoneIndexTable_object_lookup( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* agtreq_info,
    RequestInfo* requests )
{
    int rc = PRIOT_ERR_NOERROR;
    ipv6ScopeZoneIndexTable_rowreq_ctx* rowreq_ctx = ( ipv6ScopeZoneIndexTable_rowreq_ctx* )
        TableContainer_rowExtract( requests );

    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_mfd_ipv6ScopeZoneIndexTable_object_lookup", "called\n" ) );

    /*
     * get our context from mfd
     * ipv6ScopeZoneIndexTable_interface_ctx *if_ctx =
     *             (ipv6ScopeZoneIndexTable_interface_ctx *)reginfo->my_reg_void;
     */

    if ( NULL == rowreq_ctx ) {
        TableRequestInfo* tblreq_info;
        Types_Index oid_idx;

        tblreq_info = Table_extractTableInfo( requests );
        if ( NULL == tblreq_info ) {
            Logger_log( LOGGER_PRIORITY_ERR, "request had no table info\n" );
            return MFD_ERROR;
        }

        /*
         * try create rowreq
         */
        oid_idx.oids = tblreq_info->index_oid;
        oid_idx.len = tblreq_info->index_oid_len;

        rowreq_ctx = _mfd_ipv6ScopeZoneIndexTable_rowreq_from_index( &oid_idx, &rc );
        if ( MFD_SUCCESS == rc ) {
            Assert_assert( NULL != rowreq_ctx );
            rowreq_ctx->rowreq_flags |= MFD_ROW_CREATED;
            /*
             * add rowreq_ctx to request data lists
             */
            TableContainer_rowInsert( requests, ( Types_Index* )
                                                    rowreq_ctx );
        }
    }

    if ( MFD_SUCCESS != rc )
        Agent_requestSetErrorAll( requests, rc );
    else
        ipv6ScopeZoneIndexTable_row_prep( rowreq_ctx );

    return PRIOT_VALIDATE_ERR( rc );
} /* _mfd_ipv6ScopeZoneIndexTable_object_lookup */

/***********************************************************************
 *
 * GET processing
 *
 ***********************************************************************/
/*
 * @internal
 * Retrieve the value for a particular column
 */
static inline int
_ipv6ScopeZoneIndexTable_get_column( ipv6ScopeZoneIndexTable_rowreq_ctx*
                                         rowreq_ctx,
    Types_VariableList* var,
    int column )
{
    int rc = ErrorCode_SUCCESS;

    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_mfd_ipv6ScopeZoneIndexTable_get_column", "called for %d\n", column ) );

    Assert_assert( NULL != rowreq_ctx );

    switch ( column ) {

    /*
         * ipv6ScopeZoneIndexLinkLocal(2)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEXLINKLOCAL:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndexLinkLocal_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    /*
         * ipv6ScopeZoneIndex3(3)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEX3:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndex3_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    /*
         * ipv6ScopeZoneIndexAdminLocal(4)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEXADMINLOCAL:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndexAdminLocal_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    /*
         * ipv6ScopeZoneIndexSiteLocal(5)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEXSITELOCAL:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndexSiteLocal_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    /*
         * ipv6ScopeZoneIndex6(6)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEX6:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndex6_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    /*
         * ipv6ScopeZoneIndex7(7)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEX7:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndex7_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    /*
         * ipv6ScopeZoneIndexOrganizationLocal(8)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEXORGANIZATIONLOCAL:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndexOrganizationLocal_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    /*
         * ipv6ScopeZoneIndex9(9)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEX9:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndex9_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    /*
         * ipv6ScopeZoneIndexA(10)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEXA:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndexA_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    /*
         * ipv6ScopeZoneIndexB(11)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEXB:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndexB_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    /*
         * ipv6ScopeZoneIndexC(12)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEXC:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndexC_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    /*
         * ipv6ScopeZoneIndexD(13)/InetZoneIndex/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/H 
         */
    case COLUMN_IPV6SCOPEZONEINDEXD:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = ipv6ScopeZoneIndexD_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    default:
        if ( IPV6SCOPEZONEINDEXTABLE_MIN_COL <= column
            && column <= IPV6SCOPEZONEINDEXTABLE_MAX_COL ) {
            DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_mfd_ipv6ScopeZoneIndexTable_get_column", "assume column %d is reserved\n", column ) );
            rc = MFD_SKIP;
        } else {
            Logger_log( LOGGER_PRIORITY_ERR,
                "unknown column %d in _ipv6ScopeZoneIndexTable_get_column\n",
                column );
        }
        break;
    }

    return rc;
} /* _ipv6ScopeZoneIndexTable_get_column */

int _mfd_ipv6ScopeZoneIndexTable_get_values( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* agtreq_info,
    RequestInfo* requests )
{
    ipv6ScopeZoneIndexTable_rowreq_ctx* rowreq_ctx = ( ipv6ScopeZoneIndexTable_rowreq_ctx* )
        TableContainer_rowExtract( requests );
    TableRequestInfo* tri;
    u_char* old_string;
    void ( *dataFreeHook )( void* );
    int rc;

    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_mfd_ipv6ScopeZoneIndexTable_get_values", "called\n" ) );
    Assert_assert( NULL != rowreq_ctx );

    for ( ; requests; requests = requests->next ) {
        /*
         * save old pointer, so we can free it if replaced
         */
        old_string = requests->requestvb->val.string;
        dataFreeHook = requests->requestvb->dataFreeHook;
        if ( NULL == requests->requestvb->val.string ) {
            requests->requestvb->val.string = requests->requestvb->buf;
            requests->requestvb->valLen = sizeof( requests->requestvb->buf );
        } else if ( requests->requestvb->buf == requests->requestvb->val.string ) {
            if ( requests->requestvb->valLen != sizeof( requests->requestvb->buf ) )
                requests->requestvb->valLen = sizeof( requests->requestvb->buf );
        }

        /*
         * get column data
         */
        tri = Table_extractTableInfo( requests );
        if ( NULL == tri )
            continue;

        rc = _ipv6ScopeZoneIndexTable_get_column( rowreq_ctx,
            requests->requestvb,
            tri->colnum );
        if ( rc ) {
            if ( MFD_SKIP == rc ) {
                requests->requestvb->type = PRIOT_NOSUCHINSTANCE;
                rc = PRIOT_ERR_NOERROR;
            }
        } else if ( NULL == requests->requestvb->val.string ) {
            Logger_log( LOGGER_PRIORITY_ERR, "NULL varbind data pointer!\n" );
            rc = PRIOT_ERR_GENERR;
        }
        if ( rc )
            Agent_requestSetError( requests, PRIOT_VALIDATE_ERR( rc ) );

        /*
         * if the buffer wasn't used previously for the old data (i.e. it
         * was allcoated memory)  and the get routine replaced the pointer,
         * we need to free the previous pointer.
         */
        if ( old_string && ( old_string != requests->requestvb->buf ) && ( requests->requestvb->val.string != old_string ) ) {
            if ( dataFreeHook )
                ( *dataFreeHook )( old_string );
            else
                free( old_string );
        }
    } /* for results */

    return PRIOT_ERR_NOERROR;
} /* _mfd_ipv6ScopeZoneIndexTable_get_values */

/***********************************************************************
 *
 * SET processing
 *
 ***********************************************************************/

/*
 * SET PROCESSING NOT APPLICABLE (per MIB or user setting)
 */
/***********************************************************************
 *
 * DATA ACCESS
 *
 ***********************************************************************/
/**
 * @internal
 */
static void
_container_item_free( ipv6ScopeZoneIndexTable_rowreq_ctx* rowreq_ctx,
    void* context )
{
    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_container_item_free",
        "called\n" ) );

    if ( NULL == rowreq_ctx )
        return;

    ipv6ScopeZoneIndexTable_release_rowreq_ctx( rowreq_ctx );
} /* _container_item_free */

/**
 * @internal
 */
static void
_container_free( Container_Container* container )
{
    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_container_free",
        "called\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid container in ipv6ScopeZoneIndexTable_container_free\n" );
        return;
    }

    /*
     * call user code
     */
    ipv6ScopeZoneIndexTable_container_free( container );

    /*
     * free all items. inefficient, but easy.
     */
    CONTAINER_CLEAR( container,
        ( Container_FuncObjFunc* )_container_item_free,
        NULL );
} /* _container_free */

/**
 * @internal
 * initialize the container with functions or wrappers
 */
void _ipv6ScopeZoneIndexTable_container_init( ipv6ScopeZoneIndexTable_interface_ctx* if_ctx )
{
    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_ipv6ScopeZoneIndexTable_container_init", "called\n" ) );

    /*
     * container init
     */
    if_ctx->cache = CacheHandler_create( 30, /* timeout in seconds */
        _cache_load, _cache_free,
        ipv6ScopeZoneIndexTable_oid,
        ipv6ScopeZoneIndexTable_oid_size );

    if ( NULL == if_ctx->cache ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error creating cache for ipScopeZoneIndexTable\n" );
        return;
    }

    if_ctx->cache->flags = CacheOperation_DONT_INVALIDATE_ON_SET;

    ipv6ScopeZoneIndexTable_container_init( &if_ctx->container, if_ctx->cache );
    if ( NULL == if_ctx->container ) {
        if_ctx->container = Container_find( "ipv6ScopeZoneIndexTable:tableContainer" );
        if ( if_ctx->container )
            if_ctx->container->containerName = strdup( "ipv6ScopeZoneIndexTable" );
    }
    if ( NULL == if_ctx->container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error creating container in "
                                         "ipv6ScopeZoneIndexTable_container_init\n" );
        return;
    }
    if ( NULL != if_ctx->cache )
        if_ctx->cache->magic = ( void* )if_ctx->container;

} /* _ipv6ScopeZoneIndexTable_container_init */

/**
 * @internal
 * shutdown the container with functions or wrappers
 */
void _ipv6ScopeZoneIndexTable_container_shutdown( ipv6ScopeZoneIndexTable_interface_ctx* if_ctx )
{
    DEBUG_MSGTL( ( "internal:ipv6ScopeZoneIndexTable:_ipv6ScopeZoneIndexTable_container_shutdown", "called\n" ) );

    ipv6ScopeZoneIndexTable_container_shutdown( if_ctx->container );

    _container_free( if_ctx->container );

} /* _ipv6ScopeZoneIndexTable_container_shutdown */

ipv6ScopeZoneIndexTable_rowreq_ctx*
ipv6ScopeZoneIndexTable_row_find_by_mib_index( ipv6ScopeZoneIndexTable_mib_index* mib_idx )
{
    ipv6ScopeZoneIndexTable_rowreq_ctx* rowreq_ctx;
    oid oid_tmp[ ASN01_MAX_OID_LEN ];
    Types_Index oid_idx;
    int rc;

    /*
     * set up storage for OID
     */
    oid_idx.oids = oid_tmp;
    oid_idx.len = sizeof( oid_tmp ) / sizeof( oid );

    /*
     * convert
     */
    rc = ipv6ScopeZoneIndexTable_index_to_oid( &oid_idx, mib_idx );
    if ( MFD_SUCCESS != rc )
        return NULL;

    rowreq_ctx = ( ipv6ScopeZoneIndexTable_rowreq_ctx* )
        CONTAINER_FIND( ipv6ScopeZoneIndexTable_if_ctx.container, &oid_idx );

    return rowreq_ctx;
}

static int
_cache_load( Cache* cache, void* vmagic )
{
    DEBUG_MSGTL( ( "internal:ipScopeZoneIndexTable:_cache_load", "called\n" ) );

    if ( ( NULL == cache ) || ( NULL == cache->magic ) ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid cache for ipScopeZoneIndexTable_cache_load\n" );
        return -1;
    }
    /** should only be called for an invalid or expired cache */
    Assert_assert( ( 0 == cache->valid ) || ( 1 == cache->expired ) );

    /*
     * call user code
     */
    return ipv6ScopeZoneIndexTable_container_load( ( Container_Container* )cache->magic );
} /* _cache_load */

/**
 * @internal
 */
static void
_cache_free( Cache* cache, void* magic )
{
    Container_Container* container;

    DEBUG_MSGTL( ( "internal:ipScopeZoneIndexTable:_cache_free", "called\n" ) );

    if ( ( NULL == cache ) || ( NULL == cache->magic ) ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid cache in ipScopeZoneIndexTable_cache_free\n" );
        return;
    }

    container = ( Container_Container* )cache->magic;

    _container_free( container );
} /* _cache_free */