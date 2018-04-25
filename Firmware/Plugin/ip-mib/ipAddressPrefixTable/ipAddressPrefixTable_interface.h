/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 1.67 $ of : mfd-interface.m2c,v $
 *
 * $Id$
 */
/** @ingroup interface Routines to interface to Net-SNMP
 *
 * \warning This code should not be modified, called directly,
 *          or used to interpret functionality. It is subject to
 *          change at any time.
 * 
 * @{
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
#ifndef IPADDRESSPREFIXTABLE_INTERFACE_H
#define IPADDRESSPREFIXTABLE_INTERFACE_H


#include "Container.h"
#include "Table.h"
#include "ipAddressPrefixTable.h"



    /*
     ********************************************************************
     * Table declarations
     */

    /*
     * PUBLIC interface initialization routine 
     */
    void
        _ipAddressPrefixTable_initialize_interface
        (ipAddressPrefixTable_registration * user_ctx, u_long flags);
    void
        _ipAddressPrefixTable_shutdown_interface
        (ipAddressPrefixTable_registration * user_ctx);

        ipAddressPrefixTable_registration
        * ipAddressPrefixTable_registration_get(void);

        ipAddressPrefixTable_registration
        * ipAddressPrefixTable_registration_set
        (ipAddressPrefixTable_registration * newreg);

    Container_Container *ipAddressPrefixTable_container_get(void);
    int             ipAddressPrefixTable_container_size(void);
        ipAddressPrefixTable_rowreq_ctx
        * ipAddressPrefixTable_allocate_rowreq_ctx(void *);
    void
        ipAddressPrefixTable_release_rowreq_ctx
        (ipAddressPrefixTable_rowreq_ctx * rowreq_ctx);

    int             ipAddressPrefixTable_index_to_oid(Types_Index *
                                                      oid_idx,
                                                      ipAddressPrefixTable_mib_index
                                                      * mib_idx);
    int             ipAddressPrefixTable_index_from_oid(Types_Index *
                                                        oid_idx,
                                                        ipAddressPrefixTable_mib_index
                                                        * mib_idx);

    /*
     * access to certain internals. use with caution!
     */
    void
           ipAddressPrefixTable_valid_columns_set(ColumnInfo *vc);


#endif                          /* IPADDRESSPREFIXTABLE_INTERFACE_H */
/**  @} */
