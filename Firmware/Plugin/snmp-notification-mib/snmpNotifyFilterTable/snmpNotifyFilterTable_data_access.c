/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 1.17 $ of : mfd-data-access.m2c,v $ 
 *
 * $Id$
 */
/*
 * standard Net-SNMP includes 
 */

//#include <siglog/library/vacm.h>

#include "snmpNotifyFilterTable_data_access.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "Vacm.h"
#include "Vacm.h"
#include "siglog/agent/mfd.h"
#include "snmpNotifyFilterTable_interface.h"

/** @ingroup interface
 * @addtogroup data_access data_access: Routines to access data
 *
 * These routines are used to locate the data used to satisfy
 * requests.
 * 
 * @{
 */
/**********************************************************************
 **********************************************************************
 ***
 *** Table snmpNotifyFilterTable
 ***
 **********************************************************************
 **********************************************************************/
/*
 * SNMP-NOTIFICATION-MIB::snmpNotifyFilterTable is subid 3 of snmpNotifyObjects.
 * Its status is Current.
 * OID: .1.3.6.1.6.3.13.1.3, length: 9
 */

/**
 * initialization for snmpNotifyFilterTable data access
 *
 * This function is called during startup to allow you to
 * allocate any resources you need for the data table.
 *
 * @param snmpNotifyFilterTable_reg
 *        Pointer to snmpNotifyFilterTable_registration
 *
 * @retval MFD_SUCCESS : success.
 * @retval MFD_ERROR   : unrecoverable error.
 */
int snmpNotifyFilterTable_init_data( snmpNotifyFilterTable_registration*
        snmpNotifyFilterTable_reg )
{
    DEBUG_MSGTL( ( "verbose:snmpNotifyFilterTable:snmpNotifyFilterTable_init_data", "called\n" ) );

    /*
     * TODO:303:o: Initialize snmpNotifyFilterTable data.
     */

    return MFD_SUCCESS;
} /* snmpNotifyFilterTable_init_data */

/**
 * container overview
 *
 */

/**
 * container initialization
 *
 * @param container_ptr_ptr A pointer to a container pointer. If you
 *        create a custom container, use this parameter to return it
 *        to the MFD helper. If set to NULL, the MFD helper will
 *        allocate a container for you.
 *
 *  This function is called at startup to allow you to customize certain
 *  aspects of the access method. For the most part, it is for advanced
 *  users. The default code should suffice for most cases. If no custom
 *  container is allocated, the MFD code will create one for your.
 *
 * @remark
 *  This would also be a good place to do any initialization needed
 *  for you data source. For example, opening a connection to another
 *  process that will supply the data, opening a database, etc.
 */
void snmpNotifyFilterTable_container_init( Container_Container** container_ptr_ptr )
{
    DEBUG_MSGTL( ( "verbose:snmpNotifyFilterTable:snmpNotifyFilterTable_container_init", "called\n" ) );

    if ( NULL == container_ptr_ptr ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "bad container param to snmpNotifyFilterTable_container_init\n" );
        return;
    }

    /*
     * For advanced users, you can use a custom container. If you
     * do not create one, one will be created for you.
     */
    *container_ptr_ptr = NULL;

} /* snmpNotifyFilterTable_container_init */

/**
 * container shutdown
 *
 * @param container_ptr A pointer to the container.
 *
 *  This function is called at shutdown to allow you to customize certain
 *  aspects of the access method. For the most part, it is for advanced
 *  users. The default code should suffice for most cases.
 *
 *  This function is called before snmpNotifyFilterTable_container_free().
 *
 * @remark
 *  This would also be a good place to do any cleanup needed
 *  for you data source. For example, closing a connection to another
 *  process that supplied the data, closing a database, etc.
 */
void snmpNotifyFilterTable_container_shutdown( Container_Container* container_ptr )
{
    DEBUG_MSGTL( ( "verbose:snmpNotifyFilterTable:snmpNotifyFilterTable_container_shutdown", "called\n" ) );

    if ( NULL == container_ptr ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "bad params to snmpNotifyFilterTable_container_shutdown\n" );
        return;
    }

} /* snmpNotifyFilterTable_container_shutdown */

/**
 * load initial data
 *
 * TODO:350:M: Implement snmpNotifyFilterTable data load
 *
 * @param container container to which items should be inserted
 *
 * @retval MFD_SUCCESS              : success.
 * @retval MFD_RESOURCE_UNAVAILABLE : Can't access data source
 * @retval MFD_ERROR                : other error.
 *
 *  This function is called to load the index(es) (and data, optionally)
 *  for the every row in the data set.
 *
 * @remark
 *  While loading the data, the only important thing is the indexes.
 *  If access to your data is cheap/fast (e.g. you have a pointer to a
 *  structure in memory), it would make sense to update the data here.
 *  If, however, the accessing the data invovles more work (e.g. parsing
 *  some other existing data, or peforming calculations to derive the data),
 *  then you can limit yourself to setting the indexes and saving any
 *  information you will need later. Then use the saved information in
 *  snmpNotifyFilterTable_row_prep() for populating data.
 *
 * @note
 *  If you need consistency between rows (like you want statistics
 *  for each row to be from the same time frame), you should set all
 *  data here.
 *
 */
int snmpNotifyFilterTable_container_load( Container_Container* container )
{
    snmpNotifyFilterTable_rowreq_ctx* rowreq_ctx;
    size_t count = 0;

    /*
     * temporary storage for index values
     */
    /*
     * snmpNotifyFilterProfileName(1)/SnmpAdminString/ASN_OCTET_STR/char(char)//L/A/W/e/R/d/H
     */
    char snmpNotifyFilterProfileName[ 32 ];
    size_t snmpNotifyFilterProfileName_len;
    /*
     * snmpNotifyFilterSubtree(1)/OBJECTID/ASN_OBJECT_ID/oid(oid)//L/a/w/e/r/d/h
     */
    /** 128 - 1(entry) - 1(col) - 1(other indexes) = 114 */
    oid snmpNotifyFilterSubtree[ 114 ];
    size_t snmpNotifyFilterSubtree_len;

    DEBUG_MSGTL( ( "verbose:snmpNotifyFilterTable:snmpNotifyFilterTable_container_load", "called\n" ) );

    /*
     * TODO:351:M: |-> Load/update data in the snmpNotifyFilterTable container.
     * loop over your snmpNotifyFilterTable data, allocate a rowreq context,
     * set the index(es) [and data, optionally] and insert into
     * the container.
     */
    while ( 1 ) {
        /*
         * check for end of data; bail out if there is no more data
         */
        if ( 1 )
            break;

        /*
         * TODO:352:M: |   |-> set indexes in new snmpNotifyFilterTable rowreq context.
         * data context will be set from the param (unless NULL,
         *      in which case a new data context will be allocated)
         */
        rowreq_ctx = snmpNotifyFilterTable_allocate_rowreq_ctx( NULL );
        if ( NULL == rowreq_ctx ) {
            Logger_log( LOGGER_PRIORITY_ERR, "memory allocation failed\n" );
            return MFD_RESOURCE_UNAVAILABLE;
        }
        if ( MFD_SUCCESS != snmpNotifyFilterTable_indexes_set( rowreq_ctx,
                                snmpNotifyFilterProfileName,
                                snmpNotifyFilterProfileName_len,
                                snmpNotifyFilterSubtree,
                                snmpNotifyFilterSubtree_len ) ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "error setting index while loading "
                "snmpNotifyFilterTable data.\n" );
            snmpNotifyFilterTable_release_rowreq_ctx( rowreq_ctx );
            continue;
        }

        /*
         * TODO:352:r: |   |-> populate snmpNotifyFilterTable data context.
         * Populate data context here. (optionally, delay until row prep)
         */
        /*
         * non-TRANSIENT data: no need to copy. set pointer to data 
         */

        /*
         * insert into table container
         */
        CONTAINER_INSERT( container, rowreq_ctx );
        ++count;
    }

    DEBUG_MSGT( ( "verbose:snmpNotifyFilterTable:snmpNotifyFilterTable_container_load",
        "inserted %" NETSNMP_PRIz "u records\n", count ) );

    return MFD_SUCCESS;
} /* snmpNotifyFilterTable_container_load */

/**
 * container clean up
 *
 * @param container container with all current items
 *
 *  This optional callback is called prior to all
 *  item's being removed from the container. If you
 *  need to do any processing before that, do it here.
 *
 * @note
 *  The MFD helper will take care of releasing all the row contexts.
 *
 */
void snmpNotifyFilterTable_container_free( Container_Container* container )
{
    DEBUG_MSGTL( ( "verbose:snmpNotifyFilterTable:snmpNotifyFilterTable_container_free", "called\n" ) );

    /*
     * TODO:380:M: Free snmpNotifyFilterTable container data.
     */
} /* snmpNotifyFilterTable_container_free */

/**
 * prepare row for processing.
 *
 *  When the agent has located the row for a request, this function is
 *  called to prepare the row for processing. If you fully populated
 *  the data context during the index setup phase, you may not need to
 *  do anything.
 *
 * @param rowreq_ctx pointer to a context.
 *
 * @retval MFD_SUCCESS     : success.
 * @retval MFD_ERROR       : other error.
 */
int snmpNotifyFilterTable_row_prep( snmpNotifyFilterTable_rowreq_ctx*
        rowreq_ctx )
{
    DEBUG_MSGTL( ( "verbose:snmpNotifyFilterTable:snmpNotifyFilterTable_row_prep", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:390:o: Prepare row for request.
     * If populating row data was delayed, this is the place to
     * fill in the row for this request.
     */

    return MFD_SUCCESS;
} /* snmpNotifyFilterTable_row_prep */

/*
 * TODO:420:r: Implement snmpNotifyFilterTable index validation.
 */
/*---------------------------------------------------------------------
 * SNMP-NOTIFICATION-MIB::snmpNotifyFilterProfileEntry.snmpNotifyFilterProfileName
 * snmpNotifyFilterProfileName is subid 1 of snmpNotifyFilterProfileEntry.
 * Its status is Current, and its access level is Create.
 * OID: .1.3.6.1.6.3.13.1.2.1.1
 * Description:
The name of the filter profile to be used when generating
         notifications using the corresponding entry in the
         snmpTargetAddrTable.
 *
 * Attributes:
 *   accessible 1     isscalar 0     enums  0      hasdefval 0
 *   readable   1     iscolumn 1     ranges 1      hashint   1
 *   settable   1
 *   hint: 255t
 *
 * Ranges:  1 - 32;
 *
 * Its syntax is SnmpAdminString (based on perltype OCTETSTR)
 * The net-snmp type is ASN01_OCTET_STR. The C type decl is char (char)
 * This data type requires a length.  (Max 32)
 */
/**
 * check validity of snmpNotifyFilterProfileName external index portion
 *
 * NOTE: this is not the place to do any checks for the sanity
 *       of multiple indexes. Those types of checks should be done in the
 *       snmpNotifyFilterTable_validate_index() function.
 *
 * @retval MFD_SUCCESS   : the incoming value is legal
 * @retval MFD_ERROR     : the incoming value is NOT legal
 */
int

snmpNotifyFilterTable_snmpNotifyFilterProfileName_check_index( snmpNotifyFilterTable_rowreq_ctx* rowreq_ctx )
{
    DEBUG_MSGTL( ( "verbose:snmpNotifyFilterTable:snmpNotifyFilterTable_snmpNotifyFilterProfileName_check_index", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:424:M: |-> Check snmpNotifyFilterTable external index snmpNotifyFilterProfileName.
     * check that index value in the table context (rowreq_ctx)
     * for the external index snmpNotifyFilterProfileName is legal.
     */

    return MFD_SUCCESS; /*  external index snmpNotifyFilterProfileName ok */
} /* snmpNotifyFilterTable_snmpNotifyFilterProfileName_check_index */

/*---------------------------------------------------------------------
 * SNMP-NOTIFICATION-MIB::snmpNotifyFilterEntry.snmpNotifyFilterSubtree
 * snmpNotifyFilterSubtree is subid 1 of snmpNotifyFilterEntry.
 * Its status is Current, and its access level is NoAccess.
 * OID: .1.3.6.1.6.3.13.1.3.1.1
 * Description:
The MIB subtree which, when combined with the corresponding
         instance of snmpNotifyFilterMask, defines a family of
         subtrees which are included in or excluded from the
         filter profile.
 *
 * Attributes:
 *   accessible 0     isscalar 0     enums  0      hasdefval 0
 *   readable   0     iscolumn 1     ranges 0      hashint   0
 *   settable   0
 *
 *
 * Its syntax is OBJECTID (based on perltype OBJECTID)
 * The net-snmp type is ASN01_OBJECT_ID. The C type decl is oid (oid)
 * This data type requires a length.
 *
 *
 *
 * NOTE: NODE snmpNotifyFilterSubtree IS NOT ACCESSIBLE
 *
 *
 */
/**
 * check validity of snmpNotifyFilterSubtree index portion
 *
 * @retval MFD_SUCCESS   : the incoming value is legal
 * @retval MFD_ERROR     : the incoming value is NOT legal
 *
 * @note this is not the place to do any checks for the sanity
 *       of multiple indexes. Those types of checks should be done in the
 *       snmpNotifyFilterTable_validate_index() function.
 *
 * @note Also keep in mind that if the index refers to a row in this or
 *       some other table, you can't check for that row here to make
 *       decisions, since that row might not be created yet, but may
 *       be created during the processing this request. If you have
 *       such checks, they should be done in the check_dependencies
 *       function, because any new/deleted/changed rows should be
 *       available then.
 *
 * The following checks have already been done for you:
 *
 * If there a no other checks you need to do, simply return MFD_SUCCESS.
 */
int snmpNotifyFilterSubtree_check_index( snmpNotifyFilterTable_rowreq_ctx*
        rowreq_ctx )
{
    DEBUG_MSGTL( ( "verbose:snmpNotifyFilterTable:snmpNotifyFilterSubtree_check_index", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:426:M: |-> Check snmpNotifyFilterTable index snmpNotifyFilterSubtree.
     * check that index value in the table context is legal.
     * (rowreq_ctx->tbl_index.snmpNotifyFilterSubtree)
     */

    return MFD_SUCCESS; /* snmpNotifyFilterSubtree index ok */
} /* snmpNotifyFilterSubtree_check_index */

/**
 * verify specified index is valid.
 *
 * This check is independent of whether or not the values specified for
 * the columns of the new row are valid. Column values and row consistency
 * will be checked later. At this point, only the index values should be
 * checked.
 *
 * All of the individual index validation functions have been called, so this
 * is the place to make sure they are valid as a whole when combined. If
 * you only have one index, then you probably don't need to do anything else
 * here.
 * 
 * @note Keep in mind that if the indexes refer to a row in this or
 *       some other table, you can't check for that row here to make
 *       decisions, since that row might not be created yet, but may
 *       be created during the processing this request. If you have
 *       such checks, they should be done in the check_dependencies
 *       function, because any new/deleted/changed rows should be
 *       available then.
 *
 *
 * @param snmpNotifyFilterTable_reg
 *        Pointer to the user registration data
 * @param rowreq_ctx
 *        Pointer to the users context.
 * @retval MFD_SUCCESS            : success
 * @retval MFD_CANNOT_CREATE_NOW  : index not valid right now
 * @retval MFD_CANNOT_CREATE_EVER : index never valid
 */
int snmpNotifyFilterTable_validate_index( snmpNotifyFilterTable_registration*
                                              snmpNotifyFilterTable_reg,
    snmpNotifyFilterTable_rowreq_ctx*
        rowreq_ctx )
{
    int rc = MFD_SUCCESS;

    DEBUG_MSGTL( ( "verbose:snmpNotifyFilterTable:snmpNotifyFilterTable_validate_index", "called\n" ) );

    /** we should have a non-NULL pointer */
    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:430:M: |-> Validate potential snmpNotifyFilterTable index.
     */

    return rc;
} /* snmpNotifyFilterTable_validate_index */

/** @} */

/*
 * ugly, inefficient hack: create a dummy viewEntry list from the filter table
 * entries matching a profile name. This lets us use the existing vacm
 * routines for matching oids to views.
 */
struct vacm_viewEntry*
snmpNotifyFilterTable_vacm_view_subtree( const char* profile )
{
    oid tmp_oid[ ASN01_MAX_OID_LEN ];
    Types_Index tmp_idx;
    size_t i, j;
    Types_VoidArray* s;
    struct Vacm_ViewEntry_s* tmp;
    snmpNotifyFilterTable_rowreq_ctx* rowreq;
    Container_Container* c;

    tmp_idx.len = 0;
    tmp_idx.oids = tmp_oid;

    /*
     * get the container
     */
    c = snmpNotifyFilterTable_container_get();
    if ( ( NULL == profile ) || ( NULL == c ) )
        return NULL;

    /*
     * get the profile subset
     */
    tmp_idx.oids[ 0 ] = strlen( profile );
    tmp_idx.len = tmp_idx.oids[ 0 ] + 1;
    for ( i = 0; i < tmp_idx.len; ++i )
        tmp_idx.oids[ i + 1 ] = profile[ i ];
    s = c->getSubset( c, &tmp_idx );
    if ( NULL == s )
        return NULL;

    /*
     * allocate temporary storage
     */
    tmp = ( struct Vacm_ViewEntry_s* )calloc( sizeof( struct Vacm_ViewEntry_s ), s->size + 1 );
    if ( NULL == tmp ) {
        free( s->array );
        free( s );
        return NULL;
    }

    /*
     * copy data
     */
    for ( i = 0, j = 0; i < s->size; ++i ) {
        rowreq = ( snmpNotifyFilterTable_rowreq_ctx* )s->array[ i ];

        /*
         * must match profile name exactly, and subset will return
         * longer matches, if they exist.
         */
        if ( tmp_idx.oids[ 0 ] != rowreq->tbl_idx.snmpNotifyFilterProfileName_len )
            continue;

        /*
         * exact match, copy data
         * vacm_viewEntry viewName and viewSubtree are prefixed with length
         */

        tmp[ j ].viewName[ 0 ] = rowreq->tbl_idx.snmpNotifyFilterProfileName_len;
        memcpy( &tmp[ j ].viewName[ 1 ],
            rowreq->tbl_idx.snmpNotifyFilterProfileName,
            tmp[ j ].viewName[ 0 ] );

        tmp[ j ].viewSubtree[ 0 ] = rowreq->tbl_idx.snmpNotifyFilterSubtree_len;
        memcpy( &tmp[ j ].viewSubtree[ 1 ],
            rowreq->tbl_idx.snmpNotifyFilterSubtree,
            tmp[ j ].viewSubtree[ 0 ] * sizeof( oid ) );
        tmp[ j ].viewSubtreeLen = tmp[ j ].viewSubtree[ 0 ] + 1;

        tmp[ j ].viewMaskLen = rowreq->data.snmpNotifyFilterMask_len;
        memcpy( tmp[ j ].viewMask, rowreq->data.snmpNotifyFilterMask,
            tmp[ j ].viewMaskLen * sizeof( oid ) );

        tmp[ j ].viewType = rowreq->data.snmpNotifyFilterType;

        tmp[ j ].next = &tmp[ j + 1 ];
        ++j;
    }
    if ( j )
        tmp[ j - 1 ].next = NULL;
    else {
        MEMORY_FREE( tmp );
    }

    free( s->array );
    free( s );

    return tmp;
}
