/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 1.48 $ of : mfd-top.m2c,v $ 
 *
 * $Id$
 */
/** \page MFD helper for ipAddressPrefixTable
 *
 * \section intro Introduction
 * Introductory text.
 *
 */

/*
 * include our parent header 
 */
#include "ipAddressPrefixTable.h"
#include "System/Util/Assert.h"
#include "System/Containers/Map.h"
#include "System/Util/Debug.h"
#include "Vars.h"
#include "ipAddressPrefixTable_constants.h"
#include "ipAddressPrefixTable_interface.h"
#include "siglog/agent/mfd.h"

const oid ipAddressPrefixTable_oid[] = { IPADDRESSPREFIXTABLE_OID };
const int ipAddressPrefixTable_oid_size = ASN01_OID_LENGTH( ipAddressPrefixTable_oid );

ipAddressPrefixTable_registration ipAddressPrefixTable_user_context;
static ipAddressPrefixTable_registration* ipAddressPrefixTable_user_context_p;

void initialize_table_ipAddressPrefixTable( void );
void shutdown_table_ipAddressPrefixTable( void );

/**
 * Initializes the ipAddressPrefixTable module
 */
void init_ipAddressPrefixTable( void )
{
    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:init_ipAddressPrefixTable",
        "called\n" ) );

    /*
     * TODO:300:o: Perform ipAddressPrefixTable one-time module initialization.
     */

    /*
     * here we initialize all the tables we're planning on supporting
     */
    if ( Vars_shouldInit( "ipAddressPrefixTable" ) )
        initialize_table_ipAddressPrefixTable();

} /* init_ipAddressPrefixTable */

/**
 * Shut-down the ipAddressPrefixTable module (agent is exiting)
 */
void shutdown_ipAddressPrefixTable( void )
{
    if ( Vars_shouldInit( "ipAddressPrefixTable" ) )
        shutdown_table_ipAddressPrefixTable();
}

/**
 * Initialize the table ipAddressPrefixTable 
 *    (Define its contents and how it's structured)
 */
void initialize_table_ipAddressPrefixTable( void )
{
    u_long flags;

    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:initialize_table_ipAddressPrefixTable", "called\n" ) );

    /*
     * TODO:301:o: Perform ipAddressPrefixTable one-time table initialization.
     */

    /*
     * TODO:302:o: |->Initialize ipAddressPrefixTable user context
     * if you'd like to pass in a pointer to some data for this
     * table, allocate or set it up here.
     */
    /*
     * a netsnmp_data_list is a simple way to store void pointers. A simple
     * string token is used to add, find or remove pointers.
     */
    ipAddressPrefixTable_user_context_p
        = Map_newElement( "ipAddressPrefixTable", NULL, NULL );

    /*
     * No support for any flags yet, but in the future you would
     * set any flags here.
     */
    flags = 0;

    /*
     * call interface initialization code
     */
    _ipAddressPrefixTable_initialize_interface( ipAddressPrefixTable_user_context_p, flags );
} /* initialize_table_ipAddressPrefixTable */

/**
 * Shutdown the table ipAddressPrefixTable 
 */
void shutdown_table_ipAddressPrefixTable( void )
{
    /*
     * call interface shutdown code
     */
    _ipAddressPrefixTable_shutdown_interface( ipAddressPrefixTable_user_context_p );
    Map_clear( ipAddressPrefixTable_user_context_p );
    ipAddressPrefixTable_user_context_p = NULL;
}

/**
 * extra context initialization (eg default values)
 *
 * @param rowreq_ctx    : row request context
 * @param user_init_ctx : void pointer for user (parameter to rowreq_ctx_allocate)
 *
 * @retval MFD_SUCCESS  : no errors
 * @retval MFD_ERROR    : error (context allocate will fail)
 */
int ipAddressPrefixTable_rowreq_ctx_init( ipAddressPrefixTable_rowreq_ctx*
                                              rowreq_ctx,
    void* user_init_ctx )
{
    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:ipAddressPrefixTable_rowreq_ctx_init", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:210:o: |-> Perform extra ipAddressPrefixTable rowreq initialization. (eg DEFVALS)
     */

    return MFD_SUCCESS;
} /* ipAddressPrefixTable_rowreq_ctx_init */

/**
 * extra context cleanup
 *
 */
void ipAddressPrefixTable_rowreq_ctx_cleanup( ipAddressPrefixTable_rowreq_ctx*
        rowreq_ctx )
{
    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:ipAddressPrefixTable_rowreq_ctx_cleanup", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:211:o: |-> Perform extra ipAddressPrefixTable rowreq cleanup.
     */
} /* ipAddressPrefixTable_rowreq_ctx_cleanup */

/**
 * pre-request callback
 *
 *
 * @retval MFD_SUCCESS              : success.
 * @retval MFD_ERROR                : other error
 */
int ipAddressPrefixTable_pre_request( ipAddressPrefixTable_registration*
        user_context )
{
    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:ipAddressPrefixTable_pre_request", "called\n" ) );

    /*
     * TODO:510:o: Perform ipAddressPrefixTable pre-request actions.
     */

    return MFD_SUCCESS;
} /* ipAddressPrefixTable_pre_request */

/**
 * post-request callback
 *
 * Note:
 *   New rows have been inserted into the container, and
 *   deleted rows have been removed from the container and
 *   released.
 *
 * @param user_context
 * @param rc : MFD_SUCCESS if all requests succeeded
 *
 * @retval MFD_SUCCESS : success.
 * @retval MFD_ERROR   : other error (ignored)
 */
int ipAddressPrefixTable_post_request( ipAddressPrefixTable_registration*
                                           user_context,
    int rc )
{
    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:ipAddressPrefixTable_post_request", "called\n" ) );

    /*
     * TODO:511:o: Perform ipAddressPrefixTable post-request actions.
     */

    return MFD_SUCCESS;
} /* ipAddressPrefixTable_post_request */

/**********************************************************************
 **********************************************************************
 ***
 *** Table ipAddressPrefixTable
 ***
 **********************************************************************
 **********************************************************************/
/*
 * IP-MIB::ipAddressPrefixTable is subid 32 of ip.
 * Its status is Current.
 * OID: .1.3.6.1.2.1.4.32, length: 8
 */

/*
 * ---------------------------------------------------------------------
 * * TODO:200:r: Implement ipAddressPrefixTable data context functions.
 */

/**
 * set mib index(es)
 *
 * @param tbl_idx mib index structure
 * @param ipAddressPrefixIfIndex_val
 * @param ipAddressPrefixType_val
 * @param ipAddressPrefixPrefix_val_ptr
 * @param ipAddressPrefixPrefix_val_ptr_len
 * @param ipAddressPrefixLength_val
 *
 * @retval MFD_SUCCESS     : success.
 * @retval MFD_ERROR       : other error.
 *
 * @remark
 *  This convenience function is useful for setting all the MIB index
 *  components with a single function call. It is assume that the C values
 *  have already been mapped from their native/rawformat to the MIB format.
 */
int ipAddressPrefixTable_indexes_set_tbl_idx( ipAddressPrefixTable_mib_index*
                                                  tbl_idx,
    long ipAddressPrefixIfIndex_val,
    u_long ipAddressPrefixType_val,
    u_char* ipAddressPrefixPrefix_val_ptr,
    size_t
        ipAddressPrefixPrefix_val_ptr_len,
    u_long ipAddressPrefixLength_val )
{
    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:ipAddressPrefixTable_indexes_set_tbl_idx", "called\n" ) );

    /*
     * ipAddressPrefixIfIndex(1)/InterfaceIndex/ASN_INTEGER/long(long)//l/a/w/e/R/d/H 
     */
    tbl_idx->ipAddressPrefixIfIndex = ipAddressPrefixIfIndex_val;

    /*
     * ipAddressPrefixType(2)/InetAddressType/ASN_INTEGER/long(u_long)//l/a/w/E/r/d/h 
     */
    tbl_idx->ipAddressPrefixType = ipAddressPrefixType_val;

    /*
     * ipAddressPrefixPrefix(3)/InetAddress/ASN_OCTET_STR/char(char)//L/a/w/e/R/d/h 
     */
    tbl_idx->ipAddressPrefixPrefix_len = sizeof( tbl_idx->ipAddressPrefixPrefix ) / sizeof( tbl_idx->ipAddressPrefixPrefix[ 0 ] ); /* max length */
    /*
     * make sure there is enough space for ipAddressPrefixPrefix data
     */
    if ( ( NULL == tbl_idx->ipAddressPrefixPrefix ) || ( tbl_idx->ipAddressPrefixPrefix_len < ( ipAddressPrefixPrefix_val_ptr_len ) ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "not enough space for value\n" );
        return MFD_ERROR;
    }
    tbl_idx->ipAddressPrefixPrefix_len = ipAddressPrefixPrefix_val_ptr_len;
    memcpy( tbl_idx->ipAddressPrefixPrefix, ipAddressPrefixPrefix_val_ptr,
        ipAddressPrefixPrefix_val_ptr_len * sizeof( ipAddressPrefixPrefix_val_ptr[ 0 ] ) );

    /*
     * ipAddressPrefixLength(4)/InetAddressPrefixLength/ASN_UNSIGNED/u_long(u_long)//l/a/w/e/R/d/H 
     */
    tbl_idx->ipAddressPrefixLength = ipAddressPrefixLength_val;

    return MFD_SUCCESS;
} /* ipAddressPrefixTable_indexes_set_tbl_idx */

/**
 * @internal
 * set row context indexes
 *
 * @param reqreq_ctx the row context that needs updated indexes
 *
 * @retval MFD_SUCCESS     : success.
 * @retval MFD_ERROR       : other error.
 *
 * @remark
 *  This function sets the mib indexs, then updates the oid indexs
 *  from the mib index.
 */
int ipAddressPrefixTable_indexes_set( ipAddressPrefixTable_rowreq_ctx*
                                          rowreq_ctx,
    long ipAddressPrefixIfIndex_val,
    u_long ipAddressPrefixType_val,
    u_char* ipAddressPrefixPrefix_val_ptr,
    size_t ipAddressPrefixPrefix_val_ptr_len,
    u_long ipAddressPrefixLength_val )
{
    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:ipAddressPrefixTable_indexes_set", "called\n" ) );

    if ( MFD_SUCCESS != ipAddressPrefixTable_indexes_set_tbl_idx( &rowreq_ctx->tbl_idx,
                            ipAddressPrefixIfIndex_val,
                            ipAddressPrefixType_val,
                            ipAddressPrefixPrefix_val_ptr,
                            ipAddressPrefixPrefix_val_ptr_len,
                            ipAddressPrefixLength_val ) )
        return MFD_ERROR;

    /*
     * convert mib index to oid index
     */
    rowreq_ctx->oid_idx.len = sizeof( rowreq_ctx->oid_tmp ) / sizeof( oid );
    if ( 0 != ipAddressPrefixTable_index_to_oid( &rowreq_ctx->oid_idx,
                  &rowreq_ctx->tbl_idx ) ) {
        return MFD_ERROR;
    }

    return MFD_SUCCESS;
} /* ipAddressPrefixTable_indexes_set */

/*---------------------------------------------------------------------
 * IP-MIB::ipAddressPrefixEntry.ipAddressPrefixOrigin
 * ipAddressPrefixOrigin is subid 5 of ipAddressPrefixEntry.
 * Its status is Current, and its access level is ReadOnly.
 * OID: .1.3.6.1.2.1.4.32.1.5
 * Description:
The origin of this prefix.
 *
 * Attributes:
 *   accessible 1     isscalar 0     enums  1      hasdefval 0
 *   readable   1     iscolumn 1     ranges 0      hashint   0
 *   settable   0
 *
 * Enum range: 4/8. Values:  other(1), manual(2), wellknown(3), dhcp(4), routeradv(5)
 *
 * Its syntax is IpAddressPrefixOriginTC (based on perltype INTEGER)
 * The net-snmp type is ASN01_INTEGER. The C type decl is long (u_long)
 */
/**
 * Extract the current value of the ipAddressPrefixOrigin data.
 *
 * Set a value using the data context for the row.
 *
 * @param rowreq_ctx
 *        Pointer to the row request context.
 * @param ipAddressPrefixOrigin_val_ptr
 *        Pointer to storage for a long variable
 *
 * @retval MFD_SUCCESS         : success
 * @retval MFD_SKIP            : skip this node (no value for now)
 * @retval MFD_ERROR           : Any other error
 */
int ipAddressPrefixOrigin_get( ipAddressPrefixTable_rowreq_ctx* rowreq_ctx,
    u_long* ipAddressPrefixOrigin_val_ptr )
{
    /** we should have a non-NULL pointer */
    Assert_assert( NULL != ipAddressPrefixOrigin_val_ptr );

    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:ipAddressPrefixOrigin_get",
        "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:231:o: |-> Extract the current value of the ipAddressPrefixOrigin data.
     * copy (* ipAddressPrefixOrigin_val_ptr ) from rowreq_ctx->data
     */
    ( *ipAddressPrefixOrigin_val_ptr ) = rowreq_ctx->data.ipAddressPrefixOrigin;

    return MFD_SUCCESS;
} /* ipAddressPrefixOrigin_get */

/*---------------------------------------------------------------------
 * IP-MIB::ipAddressPrefixEntry.ipAddressPrefixOnLinkFlag
 * ipAddressPrefixOnLinkFlag is subid 6 of ipAddressPrefixEntry.
 * Its status is Current, and its access level is ReadOnly.
 * OID: .1.3.6.1.2.1.4.32.1.6
 * Description:
This object has the value 'true(1)', if this prefix can be
            used  for on-link determination and the value 'false(2)'
            otherwise.


            The default for IPv4 prefixes is 'true(1)'.
 *
 * Attributes:
 *   accessible 1     isscalar 0     enums  1      hasdefval 0
 *   readable   1     iscolumn 1     ranges 0      hashint   0
 *   settable   0
 *
 * Enum range: 2/8. Values:  true(1), false(2)
 *
 * Its syntax is TruthValue (based on perltype INTEGER)
 * The net-snmp type is ASN01_INTEGER. The C type decl is long (u_long)
 */
/**
 * Extract the current value of the ipAddressPrefixOnLinkFlag data.
 *
 * Set a value using the data context for the row.
 *
 * @param rowreq_ctx
 *        Pointer to the row request context.
 * @param ipAddressPrefixOnLinkFlag_val_ptr
 *        Pointer to storage for a long variable
 *
 * @retval MFD_SUCCESS         : success
 * @retval MFD_SKIP            : skip this node (no value for now)
 * @retval MFD_ERROR           : Any other error
 */
int ipAddressPrefixOnLinkFlag_get( ipAddressPrefixTable_rowreq_ctx* rowreq_ctx,
    u_long* ipAddressPrefixOnLinkFlag_val_ptr )
{
    /** we should have a non-NULL pointer */
    Assert_assert( NULL != ipAddressPrefixOnLinkFlag_val_ptr );

    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:ipAddressPrefixOnLinkFlag_get", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:231:o: |-> Extract the current value of the ipAddressPrefixOnLinkFlag data.
     * copy (* ipAddressPrefixOnLinkFlag_val_ptr ) from rowreq_ctx->data
     */
    if ( INETADDRESSTYPE_IPV4 == rowreq_ctx->tbl_idx.ipAddressPrefixType ) {
        ( *ipAddressPrefixOnLinkFlag_val_ptr ) = 1; /* per MIB */
    } else
        ( *ipAddressPrefixOnLinkFlag_val_ptr ) = rowreq_ctx->data.ipAddressPrefixOnLinkFlag;

    return MFD_SUCCESS;
} /* ipAddressPrefixOnLinkFlag_get */

/*---------------------------------------------------------------------
 * IP-MIB::ipAddressPrefixEntry.ipAddressPrefixAutonomousFlag
 * ipAddressPrefixAutonomousFlag is subid 7 of ipAddressPrefixEntry.
 * Its status is Current, and its access level is ReadOnly.
 * OID: .1.3.6.1.2.1.4.32.1.7
 * Description:
Autonomous address configuration flag. When true(1),
            indicates that this prefix can be used for autonomous
            address configuration (i.e. can be used to form a local
            interface address).  If false(2), it is not used to auto-
            configure a local interface address.


            The default for IPv4 prefixes is 'false(2)'.
 *
 * Attributes:
 *   accessible 1     isscalar 0     enums  1      hasdefval 0
 *   readable   1     iscolumn 1     ranges 0      hashint   0
 *   settable   0
 *
 * Enum range: 2/8. Values:  true(1), false(2)
 *
 * Its syntax is TruthValue (based on perltype INTEGER)
 * The net-snmp type is ASN01_INTEGER. The C type decl is long (u_long)
 */
/**
 * Extract the current value of the ipAddressPrefixAutonomousFlag data.
 *
 * Set a value using the data context for the row.
 *
 * @param rowreq_ctx
 *        Pointer to the row request context.
 * @param ipAddressPrefixAutonomousFlag_val_ptr
 *        Pointer to storage for a long variable
 *
 * @retval MFD_SUCCESS         : success
 * @retval MFD_SKIP            : skip this node (no value for now)
 * @retval MFD_ERROR           : Any other error
 */
int ipAddressPrefixAutonomousFlag_get( ipAddressPrefixTable_rowreq_ctx*
                                           rowreq_ctx,
    u_long*
        ipAddressPrefixAutonomousFlag_val_ptr )
{
    /** we should have a non-NULL pointer */
    Assert_assert( NULL != ipAddressPrefixAutonomousFlag_val_ptr );

    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:ipAddressPrefixAutonomousFlag_get", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:231:o: |-> Extract the current value of the ipAddressPrefixAutonomousFlag data.
     * copy (* ipAddressPrefixAutonomousFlag_val_ptr ) from rowreq_ctx->data
     */
    if ( INETADDRESSTYPE_IPV4 == rowreq_ctx->tbl_idx.ipAddressPrefixType )
        ( *ipAddressPrefixAutonomousFlag_val_ptr ) = 2; /* per MIB */
    else
        ( *ipAddressPrefixAutonomousFlag_val_ptr ) = rowreq_ctx->data.ipAddressPrefixAutonomousFlag;

    return MFD_SUCCESS;
} /* ipAddressPrefixAutonomousFlag_get */

/*---------------------------------------------------------------------
 * IP-MIB::ipAddressPrefixEntry.ipAddressPrefixAdvPreferredLifetime
 * ipAddressPrefixAdvPreferredLifetime is subid 8 of ipAddressPrefixEntry.
 * Its status is Current, and its access level is ReadOnly.
 * OID: .1.3.6.1.2.1.4.32.1.8
 * Description:
The remaining length of time in seconds that this prefix
            will continue to be preferred, i.e. time until deprecation.




            A value of 4,294,967,295 represents infinity.


            The address generated from a deprecated prefix should no
            longer be used as a source address in new communications,
            but packets received on such an interface are processed as
            expected.


            The default for IPv4 prefixes is 4,294,967,295 (infinity).
 *
 * Attributes:
 *   accessible 1     isscalar 0     enums  0      hasdefval 0
 *   readable   1     iscolumn 1     ranges 0      hashint   0
 *   settable   0
 *
 *
 * Its syntax is UNSIGNED32 (based on perltype UNSIGNED32)
 * The net-snmp type is ASN01_UNSIGNED. The C type decl is u_long (u_long)
 */
/**
 * Extract the current value of the ipAddressPrefixAdvPreferredLifetime data.
 *
 * Set a value using the data context for the row.
 *
 * @param rowreq_ctx
 *        Pointer to the row request context.
 * @param ipAddressPrefixAdvPreferredLifetime_val_ptr
 *        Pointer to storage for a u_long variable
 *
 * @retval MFD_SUCCESS         : success
 * @retval MFD_SKIP            : skip this node (no value for now)
 * @retval MFD_ERROR           : Any other error
 */
int ipAddressPrefixAdvPreferredLifetime_get( ipAddressPrefixTable_rowreq_ctx*
                                                 rowreq_ctx,
    u_long*
        ipAddressPrefixAdvPreferredLifetime_val_ptr )
{
    /** we should have a non-NULL pointer */
    Assert_assert( NULL != ipAddressPrefixAdvPreferredLifetime_val_ptr );

    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:ipAddressPrefixAdvPreferredLifetime_get", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:231:o: |-> Extract the current value of the ipAddressPrefixAdvPreferredLifetime data.
     * copy (* ipAddressPrefixAdvPreferredLifetime_val_ptr ) from rowreq_ctx->data
     */
    if ( INETADDRESSTYPE_IPV4 == rowreq_ctx->tbl_idx.ipAddressPrefixType )
        ( *ipAddressPrefixAdvPreferredLifetime_val_ptr ) = 4294967295U; /* per MIB */
    else
        ( *ipAddressPrefixAdvPreferredLifetime_val_ptr ) = rowreq_ctx->data.ipAddressPrefixAdvPreferredLifetime;

    return MFD_SUCCESS;
} /* ipAddressPrefixAdvPreferredLifetime_get */

/*---------------------------------------------------------------------
 * IP-MIB::ipAddressPrefixEntry.ipAddressPrefixAdvValidLifetime
 * ipAddressPrefixAdvValidLifetime is subid 9 of ipAddressPrefixEntry.
 * Its status is Current, and its access level is ReadOnly.
 * OID: .1.3.6.1.2.1.4.32.1.9
 * Description:
The remaining length of time, in seconds, that this prefix
            will continue to be valid, i.e. time until invalidation.  A
            value of 4,294,967,295 represents infinity.


            The address generated from an invalidated prefix should not
            appear as the destination or source address of a packet.


            The default for IPv4 prefixes is 4,294,967,295 (infinity).
 *
 * Attributes:
 *   accessible 1     isscalar 0     enums  0      hasdefval 0
 *   readable   1     iscolumn 1     ranges 0      hashint   0
 *   settable   0
 *
 *
 * Its syntax is UNSIGNED32 (based on perltype UNSIGNED32)
 * The net-snmp type is ASN01_UNSIGNED. The C type decl is u_long (u_long)
 */
/**
 * Extract the current value of the ipAddressPrefixAdvValidLifetime data.
 *
 * Set a value using the data context for the row.
 *
 * @param rowreq_ctx
 *        Pointer to the row request context.
 * @param ipAddressPrefixAdvValidLifetime_val_ptr
 *        Pointer to storage for a u_long variable
 *
 * @retval MFD_SUCCESS         : success
 * @retval MFD_SKIP            : skip this node (no value for now)
 * @retval MFD_ERROR           : Any other error
 */
int ipAddressPrefixAdvValidLifetime_get( ipAddressPrefixTable_rowreq_ctx*
                                             rowreq_ctx,
    u_long*
        ipAddressPrefixAdvValidLifetime_val_ptr )
{
    /** we should have a non-NULL pointer */
    Assert_assert( NULL != ipAddressPrefixAdvValidLifetime_val_ptr );

    DEBUG_MSGTL( ( "verbose:ipAddressPrefixTable:ipAddressPrefixAdvValidLifetime_get", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:231:o: |-> Extract the current value of the ipAddressPrefixAdvValidLifetime data.
     * copy (* ipAddressPrefixAdvValidLifetime_val_ptr ) from rowreq_ctx->data
     */
    if ( INETADDRESSTYPE_IPV4 == rowreq_ctx->tbl_idx.ipAddressPrefixType )
        ( *ipAddressPrefixAdvValidLifetime_val_ptr ) = 4294967295U; /* per MIB */
    else
        ( *ipAddressPrefixAdvValidLifetime_val_ptr ) = rowreq_ctx->data.ipAddressPrefixAdvValidLifetime;

    return MFD_SUCCESS;
} /* ipAddressPrefixAdvValidLifetime_get */

/** @} */
/** @} */
/** @{ */
