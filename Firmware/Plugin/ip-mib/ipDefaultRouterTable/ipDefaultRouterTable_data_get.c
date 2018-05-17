/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 12088 $ of $ 
 *
 * $Id:$
 */
/*
 * standard Net-SNMP includes 
 */

/*
 * include our parent header 
 */
#include "ipDefaultRouterTable.h"
#include "System/Util/Assert.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "ipDefaultRouterTable_interface.h"
#include "siglog/agent/mfd.h"

/** @defgroup data_get data_get: Routines to get data
 *
 * TODO:230:M: Implement ipDefaultRouterTable get routines.
 * TODO:240:M: Implement ipDefaultRouterTable mapping routines (if any).
 *
 * These routine are used to get the value for individual objects. The
 * row context is passed, along with a pointer to the memory where the
 * value should be copied.
 *
 * @{
 */
/**********************************************************************
 **********************************************************************
 ***
 *** Table ipDefaultRouterTable
 ***
 **********************************************************************
 **********************************************************************/
/*
 * IP-MIB::ipDefaultRouterTable is subid 37 of ip.
 * Its status is Current.
 * OID: .1.3.6.1.2.1.4.37, length: 8
 */

/*
 * ---------------------------------------------------------------------
 * * TODO:200:r: Implement ipDefaultRouterTable data context functions.
 */
/*
 * ipDefaultRouterTable_allocate_data
 *
 * Purpose: create new ipDefaultRouterTable_data.
 */
ipDefaultRouterTable_data*
ipDefaultRouterTable_allocate_data( void )
{
    /*
     * TODO:201:r: |-> allocate memory for the ipDefaultRouterTable data context.
     */
    ipDefaultRouterTable_data* rtn = netsnmp_access_defaultrouter_entry_create();

    DEBUG_MSGTL( ( "verbose:ipDefaultRouterTable:ipDefaultRouterTable_allocate_data", "called\n" ) );

    if ( NULL == rtn ) {
        Logger_log( LOGGER_PRIORITY_ERR, "unable to malloc memory for new "
                                         "ipDefaultRouterTable_data.\n" );
    }

    return rtn;
} /* ipDefaultRouterTable_allocate_data */

/*
 * ipDefaultRouterTable_release_data
 *
 * Purpose: release ipDefaultRouterTable data.
 */
void ipDefaultRouterTable_release_data( ipDefaultRouterTable_data* data )
{
    DEBUG_MSGTL( ( "verbose:ipDefaultRouterTable:ipDefaultRouterTable_release_data", "called\n" ) );

    /*
     * TODO:202:r: |-> release memory for the ipDefaultRouterTable data context.
     */
    netsnmp_access_defaultrouter_entry_free( data );
} /* ipDefaultRouterTable_release_data */

/**
 * set mib index(es)
 *
 * @param tbl_idx mib index structure
 * @param ipDefaultRouterAddressType_val
 * @param ipDefaultRouterAddress_ptr
 * @param ipDefaultRouterAddress_ptr_len
 * @param ipDefaultRouterIfIndex_val
 *
 * @retval MFD_SUCCESS     : success.
 * @retval MFD_ERROR       : other error.
 *
 * @remark
 *  This convenience function is useful for setting all the MIB index
 *  components with a single function call. It is assume that the C values
 *  have already been mapped from their native/rawformat to the MIB format.
 */
int ipDefaultRouterTable_indexes_set_tbl_idx( ipDefaultRouterTable_mib_index*
                                                  tbl_idx,
    u_long
        ipDefaultRouterAddressType_val,
    char* ipDefaultRouterAddress_val_ptr,
    size_t
        ipDefaultRouterAddress_val_ptr_len,
    long ipDefaultRouterIfIndex_val )
{
    DEBUG_MSGTL( ( "verbose:ipDefaultRouterTable:ipDefaultRouterTable_indexes_set_tbl_idx", "called\n" ) );

    /*
     * ipDefaultRouterAddressType(1)/InetAddressType/ASN_INTEGER/long(u_long)//l/a/w/E/r/d/h 
     */
    /** WARNING: this code might not work for netsnmp_defaultrouter_entry */
    tbl_idx->ipDefaultRouterAddressType = ipDefaultRouterAddressType_val;

    /*
     * ipDefaultRouterAddress(2)/InetAddress/ASN_OCTET_STR/char(char)//L/a/w/e/R/d/h 
     */
    tbl_idx->ipDefaultRouterAddress_len = sizeof( tbl_idx->ipDefaultRouterAddress ) / sizeof( tbl_idx->ipDefaultRouterAddress[ 0 ] ); /* max length */
    /** WARNING: this code might not work for netsnmp_defaultrouter_entry */
    /*
     * make sure there is enough space for ipDefaultRouterAddress data
     */
    if ( ( NULL == tbl_idx->ipDefaultRouterAddress ) || ( tbl_idx->ipDefaultRouterAddress_len < ( ipDefaultRouterAddress_val_ptr_len ) ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "not enough space for value\n" );
        return MFD_ERROR;
    }
    tbl_idx->ipDefaultRouterAddress_len = ipDefaultRouterAddress_val_ptr_len;
    memcpy( tbl_idx->ipDefaultRouterAddress, ipDefaultRouterAddress_val_ptr,
        ipDefaultRouterAddress_val_ptr_len * sizeof( ipDefaultRouterAddress_val_ptr[ 0 ] ) );

    /*
     * ipDefaultRouterIfIndex(3)/InterfaceIndex/ASN_INTEGER/long(long)//l/a/w/e/R/d/H 
     */
    /** WARNING: this code might not work for netsnmp_defaultrouter_entry */
    tbl_idx->ipDefaultRouterIfIndex = ipDefaultRouterIfIndex_val;

    return MFD_SUCCESS;
} /* ipDefaultRouterTable_indexes_set_tbl_idx */

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
int ipDefaultRouterTable_indexes_set( ipDefaultRouterTable_rowreq_ctx*
                                          rowreq_ctx,
    u_long ipDefaultRouterAddressType_val,
    char* ipDefaultRouterAddress_val_ptr,
    size_t ipDefaultRouterAddress_val_ptr_len,
    long ipDefaultRouterIfIndex_val )
{
    DEBUG_MSGTL( ( "verbose:ipDefaultRouterTable:ipDefaultRouterTable_indexes_set", "called\n" ) );

    if ( MFD_SUCCESS != ipDefaultRouterTable_indexes_set_tbl_idx( &rowreq_ctx->tbl_idx,
                            ipDefaultRouterAddressType_val,
                            ipDefaultRouterAddress_val_ptr,
                            ipDefaultRouterAddress_val_ptr_len,
                            ipDefaultRouterIfIndex_val ) )
        return MFD_ERROR;

    /*
     * convert mib index to oid index
     */
    rowreq_ctx->oid_idx.len = sizeof( rowreq_ctx->oid_tmp ) / sizeof( oid );
    if ( 0 != ipDefaultRouterTable_index_to_oid( &rowreq_ctx->oid_idx,
                  &rowreq_ctx->tbl_idx ) ) {
        return MFD_ERROR;
    }

    return MFD_SUCCESS;
} /* ipDefaultRouterTable_indexes_set */

/*---------------------------------------------------------------------
 * IP-MIB::ipDefaultRouterEntry.ipDefaultRouterLifetime
 * ipDefaultRouterLifetime is subid 4 of ipDefaultRouterEntry.
 * Its status is Current, and its access level is ReadOnly.
 * OID: .1.3.6.1.2.1.4.37.1.4
 * Description:
The remaining length of time, in seconds, that this router
            will continue to be useful as a default router.  A value of
            zero indicates that it is no longer useful as a default
            router.  It is left to the implementer of the MIB as to
            whether a router with a lifetime of zero is removed from the
            list.

            For IPv6, this value should be extracted from the router
            advertisement messages.
 *
 * Attributes:
 *   accessible 1     isscalar 0     enums  0      hasdefval 0
 *   readable   1     iscolumn 1     ranges 1      hashint   0
 *   settable   0
 *
 * Ranges:  0 - 65535;
 *
 * Its syntax is UNSIGNED32 (based on perltype UNSIGNED32)
 * The net-snmp type is ASN01_UNSIGNED. The C type decl is u_long (u_long)
 */
/**
 * Extract the current value of the ipDefaultRouterLifetime data.
 *
 * Set a value using the data context for the row.
 *
 * @param rowreq_ctx
 *        Pointer to the row request context.
 * @param ipDefaultRouterLifetime_val_ptr
 *        Pointer to storage for a u_long variable
 *
 * @retval MFD_SUCCESS         : success
 * @retval MFD_SKIP            : skip this node (no value for now)
 * @retval MFD_ERROR           : Any other error
 */
int ipDefaultRouterLifetime_get( ipDefaultRouterTable_rowreq_ctx* rowreq_ctx,
    u_long* ipDefaultRouterLifetime_val_ptr )
{
    /** we should have a non-NULL pointer */
    Assert_assert( NULL != ipDefaultRouterLifetime_val_ptr );

    DEBUG_MSGTL( ( "verbose:ipDefaultRouterTable:ipDefaultRouterLifetime_get",
        "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:231:o: |-> Extract the current value of the ipDefaultRouterLifetime data.
     * copy (* ipDefaultRouterLifetime_val_ptr ) from rowreq_ctx->data
     */
    ( *ipDefaultRouterLifetime_val_ptr ) = rowreq_ctx->data->dr_lifetime;

    return MFD_SUCCESS;
} /* ipDefaultRouterLifetime_get */

/*---------------------------------------------------------------------
 * IP-MIB::ipDefaultRouterEntry.ipDefaultRouterPreference
 * ipDefaultRouterPreference is subid 5 of ipDefaultRouterEntry.
 * Its status is Current, and its access level is ReadOnly.
 * OID: .1.3.6.1.2.1.4.37.1.5
 * Description:
An indication of preference given to this router as a
            default router as described in he Default Router
            Preferences document.  Treating the value as a
            2 bit signed integer allows for simple arithmetic
            comparisons.

            For IPv4 routers or IPv6 routers that are not using the
            updated router advertisement format, this object is set to
            medium (0).
 *
 * Attributes:
 *   accessible 1     isscalar 0     enums  1      hasdefval 0
 *   readable   1     iscolumn 1     ranges 0      hashint   0
 *   settable   0
 *
 * Enum range: 3/8. Values:  reserved(-2), low(-1), medium(0), high(1)
 *
 * Its syntax is INTEGER (based on perltype INTEGER)
 * The net-snmp type is ASN01_INTEGER. The C type decl is long (u_long)
 */
/**
 * Extract the current value of the ipDefaultRouterPreference data.
 *
 * Set a value using the data context for the row.
 *
 * @param rowreq_ctx
 *        Pointer to the row request context.
 * @param ipDefaultRouterPreference_val_ptr
 *        Pointer to storage for a long variable
 *
 * @retval MFD_SUCCESS         : success
 * @retval MFD_SKIP            : skip this node (no value for now)
 * @retval MFD_ERROR           : Any other error
 */
int ipDefaultRouterPreference_get( ipDefaultRouterTable_rowreq_ctx* rowreq_ctx,
    u_long* ipDefaultRouterPreference_val_ptr )
{
    /** we should have a non-NULL pointer */
    Assert_assert( NULL != ipDefaultRouterPreference_val_ptr );

    DEBUG_MSGTL( ( "verbose:ipDefaultRouterTable:ipDefaultRouterPreference_get", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:231:o: |-> Extract the current value of the ipDefaultRouterPreference data.
     * copy (* ipDefaultRouterPreference_val_ptr ) from rowreq_ctx->data
     */
    ( *ipDefaultRouterPreference_val_ptr ) = rowreq_ctx->data->dr_preference;

    return MFD_SUCCESS;
} /* ipDefaultRouterPreference_get */

/** @} */
