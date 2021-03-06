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
#ifndef INETCIDRROUTETABLE_INTERFACE_H
#define INETCIDRROUTETABLE_INTERFACE_H

#include "inetCidrRouteTable.h"
#include "CacheHandler.h"
#include "Table.h"


    /*
     ********************************************************************
     * Table declarations
     */

    /*
     * PUBLIC interface initialization routine 
     */
    void
        _inetCidrRouteTable_initialize_interface
        (inetCidrRouteTable_registration * user_ctx, u_long flags);
    void
        _inetCidrRouteTable_shutdown_interface
        (inetCidrRouteTable_registration * user_ctx);
         
    inetCidrRouteTable_registration
        * inetCidrRouteTable_registration_set
        (inetCidrRouteTable_registration * newreg);

    Container_Container *inetCidrRouteTable_container_get(void);
    int             inetCidrRouteTable_container_size(void);

    u_int           inetCidrRouteTable_dirty_get(void);
    void            inetCidrRouteTable_dirty_set(u_int status);
         
    Cache  *inetCidrRouteTable_get_cache(void);

    inetCidrRouteTable_rowreq_ctx
        * inetCidrRouteTable_allocate_rowreq_ctx(inetCidrRouteTable_data *,
                                                 void *);
    void
        inetCidrRouteTable_release_rowreq_ctx(inetCidrRouteTable_rowreq_ctx
                                              * rowreq_ctx);

    int             inetCidrRouteTable_index_to_oid(Types_Index *
                                                    oid_idx,
                                                    inetCidrRouteTable_mib_index
                                                    * mib_idx);
    int             inetCidrRouteTable_index_from_oid(Types_Index *
                                                      oid_idx,
                                                      inetCidrRouteTable_mib_index
                                                      * mib_idx);

    /*
     * access to certain internals. use with caution!
     */
    void
             inetCidrRouteTable_valid_columns_set(ColumnInfo *vc);


#endif                          /* INETCIDRROUTETABLE_INTERFACE_H */
/**  @} */

