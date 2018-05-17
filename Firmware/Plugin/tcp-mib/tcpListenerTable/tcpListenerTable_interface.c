/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 1.67 $ of : mfd-interface.m2c,v $ 
 *
 * $Id$
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

#include "tcpListenerTable_interface.h"
#include "System/Util/Assert.h"
#include "BabySteps.h"
#include "CacheHandler.h"
#include "Client.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "Mib.h"
#include "RowMerge.h"
#include "TableContainer.h"
#include "siglog/agent/mfd.h"
#include "tcpListenerTable_constants.h"
#include "tcpListenerTable_data_access.h"

//#include "tcpListenerTable.h"

/**********************************************************************
 **********************************************************************
 ***
 *** Table tcpListenerTable
 ***
 **********************************************************************
 **********************************************************************/
/*
 * TCP-MIB::tcpListenerTable is subid 20 of tcp.
 * Its status is Current.
 * OID: .1.3.6.1.2.1.6.20, length: 8
 */
typedef struct tcpListenerTable_interface_ctx_s {

    Container_Container* container;
    Cache* cache;

    tcpListenerTable_registration* user_ctx;

    TableRegistrationInfo tbl_info;

    BabyStepsAccessMethods access_multiplexer;

} tcpListenerTable_interface_ctx;

static tcpListenerTable_interface_ctx tcpListenerTable_if_ctx;

static void
_tcpListenerTable_container_init( tcpListenerTable_interface_ctx* if_ctx );
static void
_tcpListenerTable_container_shutdown( tcpListenerTable_interface_ctx*
        if_ctx );

Container_Container*
tcpListenerTable_container_get( void )
{
    return tcpListenerTable_if_ctx.container;
}

tcpListenerTable_registration*
tcpListenerTable_registration_get( void )
{
    return tcpListenerTable_if_ctx.user_ctx;
}

tcpListenerTable_registration*
tcpListenerTable_registration_set( tcpListenerTable_registration* newreg )
{
    tcpListenerTable_registration* old = tcpListenerTable_if_ctx.user_ctx;
    tcpListenerTable_if_ctx.user_ctx = newreg;
    return old;
}

int tcpListenerTable_container_size( void )
{
    return CONTAINER_SIZE( tcpListenerTable_if_ctx.container );
}

/*
 * mfd multiplexer modes
 */
static NodeHandlerFT _mfd_tcpListenerTable_pre_request;
static NodeHandlerFT _mfd_tcpListenerTable_post_request;
static NodeHandlerFT _mfd_tcpListenerTable_object_lookup;
static NodeHandlerFT _mfd_tcpListenerTable_get_values;
/**
 * @internal
 * Initialize the table tcpListenerTable 
 *    (Define its contents and how it's structured)
 */
void _tcpListenerTable_initialize_interface( tcpListenerTable_registration*
                                                 reg_ptr,
    u_long flags )
{
    BabyStepsAccessMethods* access_multiplexer = &tcpListenerTable_if_ctx.access_multiplexer;
    TableRegistrationInfo* tbl_info = &tcpListenerTable_if_ctx.tbl_info;
    HandlerRegistration* reginfo;
    MibHandler* handler;
    int mfd_modes = 0;

    DEBUG_MSGTL( ( "internal:tcpListenerTable:_tcpListenerTable_initialize_interface", "called\n" ) );

    /*************************************************
     *
     * save interface context for tcpListenerTable
     */
    /*
     * Setting up the table's definition
     */
    Table_helperAddIndexes( tbl_info, ASN01_INTEGER,
        /** index: tcpListenerLocalAddressType */
        ASN01_OCTET_STR,
        /** index: tcpListenerLocalAddress */
        ASN01_UNSIGNED,
        /** index: tcpListenerLocalPort */
        0 );

    /*
     * Define the minimum and maximum accessible columns.  This
     * optimizes retrieval. 
     */
    tbl_info->min_column = TCPLISTENERTABLE_MIN_COL;
    tbl_info->max_column = TCPLISTENERTABLE_MAX_COL;

    /*
     * save users context
     */
    tcpListenerTable_if_ctx.user_ctx = reg_ptr;

    /*
     * call data access initialization code
     */
    tcpListenerTable_init_data( reg_ptr );

    /*
     * set up the container
     */
    _tcpListenerTable_container_init( &tcpListenerTable_if_ctx );
    if ( NULL == tcpListenerTable_if_ctx.container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "could not initialize container for tcpListenerTable\n" );
        return;
    }

    /*
     * access_multiplexer: REQUIRED wrapper for get request handling
     */
    access_multiplexer->object_lookup = _mfd_tcpListenerTable_object_lookup;
    access_multiplexer->get_values = _mfd_tcpListenerTable_get_values;

    /*
     * no wrappers yet
     */
    access_multiplexer->pre_request = _mfd_tcpListenerTable_pre_request;
    access_multiplexer->post_request = _mfd_tcpListenerTable_post_request;

    /*************************************************
     *
     * Create a registration, save our reg data, register table.
     */
    DEBUG_MSGTL( ( "tcpListenerTable:init_tcpListenerTable",
        "Registering tcpListenerTable as a mibs-for-dummies table.\n" ) );
    handler = BabySteps_accessMultiplexerGet( access_multiplexer );
    reginfo = AgentHandler_registrationCreate( "tcpListenerTable", handler,
        tcpListenerTable_oid,
        tcpListenerTable_oid_size,
        HANDLER_CAN_BABY_STEP | HANDLER_CAN_RONLY );
    if ( NULL == reginfo ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error registering table tcpListenerTable\n" );
        return;
    }
    reginfo->my_reg_void = &tcpListenerTable_if_ctx;

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

    /* XXX - are these actually necessary? */
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
        tcpListenerTable_if_ctx.container,
        TABLE_CONTAINER_KEY_NETSNMP_INDEX );
    AgentHandler_injectHandler( reginfo, handler );

    /*************************************************
     *
     * inject cache helper
     */
    if ( NULL != tcpListenerTable_if_ctx.cache ) {
        handler = CacheHandler_handlerGet( tcpListenerTable_if_ctx.cache );
        AgentHandler_injectHandler( reginfo, handler );
    }

    /*
     * register table
     */
    Table_registerTable( reginfo, tbl_info );

} /* _tcpListenerTable_initialize_interface */

/**
 * @internal
 * Shutdown the table tcpListenerTable
 */
void _tcpListenerTable_shutdown_interface( tcpListenerTable_registration*
        reg_ptr )
{
    /*
     * shutdown the container
     */
    _tcpListenerTable_container_shutdown( &tcpListenerTable_if_ctx );
}

void tcpListenerTable_valid_columns_set( ColumnInfo* vc )
{
    tcpListenerTable_if_ctx.tbl_info.valid_columns = vc;
} /* tcpListenerTable_valid_columns_set */

/**
 * @internal
 * convert the index component stored in the context to an oid
 */
int tcpListenerTable_index_to_oid( Types_Index* oid_idx,
    tcpListenerTable_mib_index* mib_idx )
{
    int err = PRIOT_ERR_NOERROR;

    /*
     * temp storage for parsing indexes
     */
    /*
     * tcpListenerLocalAddressType(1)/InetAddressType/ASN_INTEGER/long(u_long)//l/a/w/E/r/d/h
     */
    Types_VariableList var_tcpListenerLocalAddressType;
    /*
     * tcpListenerLocalAddress(2)/InetAddress/ASN_OCTET_STR/char(char)//L/a/w/e/R/d/h
     */
    Types_VariableList var_tcpListenerLocalAddress;
    /*
     * tcpListenerLocalPort(3)/InetPortNumber/ASN_UNSIGNED/u_long(u_long)//l/a/w/e/R/d/H
     */
    Types_VariableList var_tcpListenerLocalPort;

    /*
     * set up varbinds
     */
    memset( &var_tcpListenerLocalAddressType, 0x00,
        sizeof( var_tcpListenerLocalAddressType ) );
    var_tcpListenerLocalAddressType.type = ASN01_INTEGER;
    memset( &var_tcpListenerLocalAddress, 0x00,
        sizeof( var_tcpListenerLocalAddress ) );
    var_tcpListenerLocalAddress.type = ASN01_OCTET_STR;
    memset( &var_tcpListenerLocalPort, 0x00,
        sizeof( var_tcpListenerLocalPort ) );
    var_tcpListenerLocalPort.type = ASN01_UNSIGNED;

    /*
     * chain temp index varbinds together
     */
    var_tcpListenerLocalAddressType.nextVariable = &var_tcpListenerLocalAddress;
    var_tcpListenerLocalAddress.nextVariable = &var_tcpListenerLocalPort;
    var_tcpListenerLocalPort.nextVariable = NULL;

    DEBUG_MSGTL( ( "verbose:tcpListenerTable:tcpListenerTable_index_to_oid",
        "called\n" ) );

    /*
     * tcpListenerLocalAddressType(1)/InetAddressType/ASN_INTEGER/long(u_long)//l/a/w/E/r/d/h 
     */
    Client_setVarValue( &var_tcpListenerLocalAddressType,
        ( u_char* )&mib_idx->tcpListenerLocalAddressType,
        sizeof( mib_idx->tcpListenerLocalAddressType ) );

    /*
     * tcpListenerLocalAddress(2)/InetAddress/ASN_OCTET_STR/char(char)//L/a/w/e/R/d/h 
     */
    Client_setVarValue( &var_tcpListenerLocalAddress,
        ( u_char* )&mib_idx->tcpListenerLocalAddress,
        mib_idx->tcpListenerLocalAddress_len * sizeof( mib_idx->tcpListenerLocalAddress[ 0 ] ) );

    /*
     * tcpListenerLocalPort(3)/InetPortNumber/ASN_UNSIGNED/u_long(u_long)//l/a/w/e/R/d/H 
     */
    Client_setVarValue( &var_tcpListenerLocalPort,
        ( u_char* )&mib_idx->tcpListenerLocalPort,
        sizeof( mib_idx->tcpListenerLocalPort ) );

    err = Mib_buildOidNoalloc( oid_idx->oids, oid_idx->len, &oid_idx->len,
        NULL, 0, &var_tcpListenerLocalAddressType );
    if ( err )
        Logger_log( LOGGER_PRIORITY_ERR, "error %d converting index to oid\n", err );

    /*
     * parsing may have allocated memory. free it.
     */
    Client_resetVarBuffers( &var_tcpListenerLocalAddressType );

    return err;
} /* tcpListenerTable_index_to_oid */

/**
 * extract tcpListenerTable indexes from a Types_Index
 *
 * @retval PRIOT_ERR_NOERROR  : no error
 * @retval ErrorCode_GENERR   : error
 */
int tcpListenerTable_index_from_oid( Types_Index* oid_idx,
    tcpListenerTable_mib_index* mib_idx )
{
    int err = PRIOT_ERR_NOERROR;

    /*
     * temp storage for parsing indexes
     */
    /*
     * tcpListenerLocalAddressType(1)/InetAddressType/ASN_INTEGER/long(u_long)//l/a/w/E/r/d/h
     */
    Types_VariableList var_tcpListenerLocalAddressType;
    /*
     * tcpListenerLocalAddress(2)/InetAddress/ASN_OCTET_STR/char(char)//L/a/w/e/R/d/h
     */
    Types_VariableList var_tcpListenerLocalAddress;
    /*
     * tcpListenerLocalPort(3)/InetPortNumber/ASN_UNSIGNED/u_long(u_long)//l/a/w/e/R/d/H
     */
    Types_VariableList var_tcpListenerLocalPort;

    /*
     * set up varbinds
     */
    memset( &var_tcpListenerLocalAddressType, 0x00,
        sizeof( var_tcpListenerLocalAddressType ) );
    var_tcpListenerLocalAddressType.type = ASN01_INTEGER;
    memset( &var_tcpListenerLocalAddress, 0x00,
        sizeof( var_tcpListenerLocalAddress ) );
    var_tcpListenerLocalAddress.type = ASN01_OCTET_STR;
    memset( &var_tcpListenerLocalPort, 0x00,
        sizeof( var_tcpListenerLocalPort ) );
    var_tcpListenerLocalPort.type = ASN01_UNSIGNED;

    /*
     * chain temp index varbinds together
     */
    var_tcpListenerLocalAddressType.nextVariable = &var_tcpListenerLocalAddress;
    var_tcpListenerLocalAddress.nextVariable = &var_tcpListenerLocalPort;
    var_tcpListenerLocalPort.nextVariable = NULL;

    DEBUG_MSGTL( ( "verbose:tcpListenerTable:tcpListenerTable_index_from_oid",
        "called\n" ) );

    /*
     * parse the oid into the individual index components
     */
    err = Mib_parseOidIndexes( oid_idx->oids, oid_idx->len,
        &var_tcpListenerLocalAddressType );
    if ( err == PRIOT_ERR_NOERROR ) {
        /*
         * copy out values
         */
        mib_idx->tcpListenerLocalAddressType = *( ( u_long* )var_tcpListenerLocalAddressType.val.string );
        /*
         * NOTE: val_len is in bytes, tcpListenerLocalAddress_len might not be
         */
        if ( var_tcpListenerLocalAddress.valLen > sizeof( mib_idx->tcpListenerLocalAddress ) )
            err = ErrorCode_GENERR;
        else {
            memcpy( mib_idx->tcpListenerLocalAddress,
                var_tcpListenerLocalAddress.val.string,
                var_tcpListenerLocalAddress.valLen );
            mib_idx->tcpListenerLocalAddress_len = var_tcpListenerLocalAddress.valLen / sizeof( mib_idx->tcpListenerLocalAddress[ 0 ] );
        }
        mib_idx->tcpListenerLocalPort = *( ( u_long* )var_tcpListenerLocalPort.val.string );
    }

    /*
     * parsing may have allocated memory. free it.
     */
    Client_resetVarBuffers( &var_tcpListenerLocalAddressType );

    return err;
} /* tcpListenerTable_index_from_oid */

/*
 *********************************************************************
 * @internal
 * allocate resources for a tcpListenerTable_rowreq_ctx
 */
tcpListenerTable_rowreq_ctx*
tcpListenerTable_allocate_rowreq_ctx( tcpListenerTable_data* data,
    void* user_init_ctx )
{
    tcpListenerTable_rowreq_ctx* rowreq_ctx = MEMORY_MALLOC_TYPEDEF( tcpListenerTable_rowreq_ctx );

    DEBUG_MSGTL( ( "internal:tcpListenerTable:tcpListenerTable_allocate_rowreq_ctx", "called\n" ) );

    if ( NULL == rowreq_ctx ) {
        Logger_log( LOGGER_PRIORITY_ERR, "Couldn't allocate memory for a "
                                         "tcpListenerTable_rowreq_ctx.\n" );
        return NULL;
    } else {
        if ( NULL != data ) {
            /*
             * track if we got data from user
             */
            rowreq_ctx->rowreq_flags |= MFD_ROW_DATA_FROM_USER;
            rowreq_ctx->data = data;
        } else if ( NULL == ( rowreq_ctx->data = tcpListenerTable_allocate_data() ) ) {
            MEMORY_FREE( rowreq_ctx );
            return NULL;
        }
    }

    /*
     * undo context will be allocated when needed (in *_undo_setup)
     */

    rowreq_ctx->oid_idx.oids = rowreq_ctx->oid_tmp;

    rowreq_ctx->tcpListenerTable_data_list = NULL;

    /*
     * if we allocated data, call init routine
     */
    if ( !( rowreq_ctx->rowreq_flags & MFD_ROW_DATA_FROM_USER ) ) {
        if ( ErrorCode_SUCCESS != tcpListenerTable_rowreq_ctx_init( rowreq_ctx, user_init_ctx ) ) {
            tcpListenerTable_release_rowreq_ctx( rowreq_ctx );
            rowreq_ctx = NULL;
        }
    }

    return rowreq_ctx;
} /* tcpListenerTable_allocate_rowreq_ctx */

/*
 * @internal
 * release resources for a tcpListenerTable_rowreq_ctx
 */
void tcpListenerTable_release_rowreq_ctx( tcpListenerTable_rowreq_ctx*
        rowreq_ctx )
{
    DEBUG_MSGTL( ( "internal:tcpListenerTable:tcpListenerTable_release_rowreq_ctx", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    tcpListenerTable_rowreq_ctx_cleanup( rowreq_ctx );

    /*
     * for non-transient data, don't free data we got from the user
     */
    if ( ( rowreq_ctx->data ) && !( rowreq_ctx->rowreq_flags & MFD_ROW_DATA_FROM_USER ) )
        tcpListenerTable_release_data( rowreq_ctx->data );

    /*
     * free index oid pointer
     */
    if ( rowreq_ctx->oid_idx.oids != rowreq_ctx->oid_tmp )
        free( rowreq_ctx->oid_idx.oids );

    MEMORY_FREE( rowreq_ctx );
} /* tcpListenerTable_release_rowreq_ctx */

/**
 * @internal
 * wrapper
 */
static int
_mfd_tcpListenerTable_pre_request( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* agtreq_info,
    RequestInfo* requests )
{
    int rc;

    DEBUG_MSGTL( ( "internal:tcpListenerTable:_mfd_tcpListenerTable_pre_request", "called\n" ) );

    if ( 1 != RowMerge_statusFirst( reginfo, agtreq_info ) ) {
        DEBUG_MSGTL( ( "internal:tcpListenerTable",
            "skipping additional pre_request\n" ) );
        return PRIOT_ERR_NOERROR;
    }

    rc = tcpListenerTable_pre_request( tcpListenerTable_if_ctx.user_ctx );
    if ( MFD_SUCCESS != rc ) {
        /*
         * nothing we can do about it but log it
         */
        DEBUG_MSGTL( ( "tcpListenerTable", "error %d from "
                                           "tcpListenerTable_pre_request\n",
            rc ) );
        Agent_requestSetErrorAll( requests, PRIOT_VALIDATE_ERR( rc ) );
    }

    return PRIOT_ERR_NOERROR;
} /* _mfd_tcpListenerTable_pre_request */

/**
 * @internal
 * wrapper
 */
static int
_mfd_tcpListenerTable_post_request( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* agtreq_info,
    RequestInfo* requests )
{
    tcpListenerTable_rowreq_ctx* rowreq_ctx = ( tcpListenerTable_rowreq_ctx* )
        TableContainer_rowExtract( requests );
    int rc, packet_rc;

    DEBUG_MSGTL( ( "internal:tcpListenerTable:_mfd_tcpListenerTable_post_request", "called\n" ) );

    /*
     * release row context, if deleted
     */
    if ( rowreq_ctx && ( rowreq_ctx->rowreq_flags & MFD_ROW_DELETED ) )
        tcpListenerTable_release_rowreq_ctx( rowreq_ctx );

    /*
     * wait for last call before calling user
     */
    if ( 1 != RowMerge_statusLast( reginfo, agtreq_info ) ) {
        DEBUG_MSGTL( ( "internal:tcpListenerTable",
            "waiting for last post_request\n" ) );
        return PRIOT_ERR_NOERROR;
    }

    packet_rc = Agent_checkAllRequestsError( agtreq_info->asp, 0 );
    rc = tcpListenerTable_post_request( tcpListenerTable_if_ctx.user_ctx,
        packet_rc );
    if ( MFD_SUCCESS != rc ) {
        /*
         * nothing we can do about it but log it
         */
        DEBUG_MSGTL( ( "tcpListenerTable", "error %d from "
                                           "tcpListenerTable_post_request\n",
            rc ) );
    }

    return PRIOT_ERR_NOERROR;
} /* _mfd_tcpListenerTable_post_request */

/**
 * @internal
 * wrapper
 */
static int
_mfd_tcpListenerTable_object_lookup( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* agtreq_info,
    RequestInfo* requests )
{
    int rc = PRIOT_ERR_NOERROR;
    tcpListenerTable_rowreq_ctx* rowreq_ctx = ( tcpListenerTable_rowreq_ctx* )
        TableContainer_rowExtract( requests );

    DEBUG_MSGTL( ( "internal:tcpListenerTable:_mfd_tcpListenerTable_object_lookup", "called\n" ) );

    /*
     * get our context from mfd
     * tcpListenerTable_interface_ctx *if_ctx =
     *             (tcpListenerTable_interface_ctx *)reginfo->my_reg_void;
     */

    if ( NULL == rowreq_ctx ) {
        rc = PRIOT_ERR_NOCREATION;
    }

    if ( MFD_SUCCESS != rc )
        Agent_requestSetErrorAll( requests, rc );
    else
        tcpListenerTable_row_prep( rowreq_ctx );

    return PRIOT_VALIDATE_ERR( rc );
} /* _mfd_tcpListenerTable_object_lookup */

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
_tcpListenerTable_get_column( tcpListenerTable_rowreq_ctx* rowreq_ctx,
    Types_VariableList* var, int column )
{
    int rc = ErrorCode_SUCCESS;

    DEBUG_MSGTL( ( "internal:tcpListenerTable:_mfd_tcpListenerTable_get_column", "called for %d\n", column ) );

    Assert_assert( NULL != rowreq_ctx );

    switch ( column ) {

    /*
         * tcpListenerProcess(4)/UNSIGNED32/ASN_UNSIGNED/u_long(u_long)//l/A/w/e/r/d/h 
         */
    case COLUMN_TCPLISTENERPROCESS:
        var->valLen = sizeof( u_long );
        var->type = ASN01_UNSIGNED;
        rc = tcpListenerProcess_get( rowreq_ctx,
            ( u_long* )var->val.string );
        break;

    default:
        Logger_log( LOGGER_PRIORITY_ERR,
            "unknown column %d in _tcpListenerTable_get_column\n",
            column );
        break;
    }

    return rc;
} /* _tcpListenerTable_get_column */

int _mfd_tcpListenerTable_get_values( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* agtreq_info,
    RequestInfo* requests )
{
    tcpListenerTable_rowreq_ctx* rowreq_ctx = ( tcpListenerTable_rowreq_ctx* )
        TableContainer_rowExtract( requests );
    TableRequestInfo* tri;
    u_char* old_string;
    void ( *dataFreeHook )( void* );
    int rc;

    DEBUG_MSGTL( ( "internal:tcpListenerTable:_mfd_tcpListenerTable_get_values", "called\n" ) );

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

        rc = _tcpListenerTable_get_column( rowreq_ctx, requests->requestvb,
            tri->colnum );
        if ( rc ) {
            if ( MFD_SKIP == rc ) {
                requests->requestvb->type = PRIOT_NOSUCHINSTANCE;
                rc = PRIOT_ERR_NOERROR;
            }
        } else if ( NULL == requests->requestvb->val.string ) {
            Logger_log( LOGGER_PRIORITY_ERR, "NULL varbind data pointer!\n" );
            rc = ErrorCode_GENERR;
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
} /* _mfd_tcpListenerTable_get_values */

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
static void _container_free( Container_Container* container );

/**
 * @internal
 */
static int
_cache_load( Cache* cache, void* vmagic )
{
    DEBUG_MSGTL( ( "internal:tcpListenerTable:_cache_load", "called\n" ) );

    if ( ( NULL == cache ) || ( NULL == cache->magic ) ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid cache for tcpListenerTable_cache_load\n" );
        return -1;
    }

    /** should only be called for an invalid or expired cache */
    Assert_assert( ( 0 == cache->valid ) || ( 1 == cache->expired ) );

    /*
     * call user code
     */
    return tcpListenerTable_container_load( ( Container_Container* )cache->magic );
} /* _cache_load */

/**
 * @internal
 */
static void
_cache_free( Cache* cache, void* magic )
{
    Container_Container* container;

    DEBUG_MSGTL( ( "internal:tcpListenerTable:_cache_free", "called\n" ) );

    if ( ( NULL == cache ) || ( NULL == cache->magic ) ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid cache in tcpListenerTable_cache_free\n" );
        return;
    }

    container = ( Container_Container* )cache->magic;

    _container_free( container );
} /* _cache_free */

/**
 * @internal
 */
static void
_container_item_free( tcpListenerTable_rowreq_ctx* rowreq_ctx,
    void* context )
{
    DEBUG_MSGTL( ( "internal:tcpListenerTable:_container_item_free",
        "called\n" ) );

    if ( NULL == rowreq_ctx )
        return;

    tcpListenerTable_release_rowreq_ctx( rowreq_ctx );
} /* _container_item_free */

/**
 * @internal
 */
static void
_container_free( Container_Container* container )
{
    DEBUG_MSGTL( ( "internal:tcpListenerTable:_container_free", "called\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid container in tcpListenerTable_container_free\n" );
        return;
    }

    /*
     * call user code
     */
    tcpListenerTable_container_free( container );

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
void _tcpListenerTable_container_init( tcpListenerTable_interface_ctx* if_ctx )
{
    DEBUG_MSGTL( ( "internal:tcpListenerTable:_tcpListenerTable_container_init", "called\n" ) );

    /*
     * cache init
     */
    if_ctx->cache = CacheHandler_create( 30, /* timeout in seconds */
        _cache_load, _cache_free,
        tcpListenerTable_oid,
        tcpListenerTable_oid_size );

    if ( NULL == if_ctx->cache ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error creating cache for tcpListenerTable\n" );
        return;
    }

    if_ctx->cache->flags = CacheOperation_DONT_INVALIDATE_ON_SET;

    tcpListenerTable_container_init( &if_ctx->container, if_ctx->cache );
    if ( NULL == if_ctx->container ) {
        if_ctx->container = Container_find( "tcpListenerTable:table_container" );
        if ( if_ctx->container )
            if_ctx->container->containerName = strdup( "tcpListenerTable" );
    }
    if ( NULL == if_ctx->container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error creating container in "
                                         "tcpListenerTable_container_init\n" );
        return;
    }

    if ( NULL != if_ctx->cache )
        if_ctx->cache->magic = ( void* )if_ctx->container;
} /* _tcpListenerTable_container_init */

/**
 * @internal
 * shutdown the container with functions or wrappers
 */
void _tcpListenerTable_container_shutdown( tcpListenerTable_interface_ctx*
        if_ctx )
{
    DEBUG_MSGTL( ( "internal:tcpListenerTable:_tcpListenerTable_container_shutdown", "called\n" ) );

    tcpListenerTable_container_shutdown( if_ctx->container );

    _container_free( if_ctx->container );

} /* _tcpListenerTable_container_shutdown */

tcpListenerTable_rowreq_ctx*
tcpListenerTable_row_find_by_mib_index( tcpListenerTable_mib_index*
        mib_idx )
{
    tcpListenerTable_rowreq_ctx* rowreq_ctx;
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
    rc = tcpListenerTable_index_to_oid( &oid_idx, mib_idx );
    if ( MFD_SUCCESS != rc )
        return NULL;

    rowreq_ctx = ( tcpListenerTable_rowreq_ctx* )
        CONTAINER_FIND( tcpListenerTable_if_ctx.container, &oid_idx );

    return rowreq_ctx;
}
