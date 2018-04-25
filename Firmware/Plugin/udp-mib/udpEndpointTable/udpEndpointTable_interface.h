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
#ifndef UDPENDPOINTTABLE_INTERFACE_H
#define UDPENDPOINTTABLE_INTERFACE_H


#include "udpEndpointTable.h"
#include "Container.h"
#include "Table.h"


    /*
     ********************************************************************
     * Table declarations
     */

    /*
     * PUBLIC interface initialization routine 
     */
    void
        _udpEndpointTable_initialize_interface
        (udpEndpointTable_registration * user_ctx, u_long flags);
    void
        _udpEndpointTable_shutdown_interface(udpEndpointTable_registration
                                             * user_ctx);

    udpEndpointTable_registration *udpEndpointTable_registration_get(void);

        udpEndpointTable_registration
        * udpEndpointTable_registration_set(udpEndpointTable_registration *
                                            newreg);

    Container_Container *udpEndpointTable_container_get(void);
    int             udpEndpointTable_container_size(void);

        udpEndpointTable_rowreq_ctx
        * udpEndpointTable_allocate_rowreq_ctx(void);
    void
        udpEndpointTable_release_rowreq_ctx(udpEndpointTable_rowreq_ctx *
                                            rowreq_ctx);

    int             udpEndpointTable_index_to_oid(Types_Index * oid_idx,
                                                  udpEndpointTable_mib_index
                                                  * mib_idx);
    int             udpEndpointTable_index_from_oid(Types_Index *
                                                    oid_idx,
                                                    udpEndpointTable_mib_index
                                                    * mib_idx);

    /*
     * access to certain internals. use with caution!
     */
    void            udpEndpointTable_valid_columns_set(ColumnInfo
                                                       *vc);


#endif                          /* UDPENDPOINTTABLE_INTERFACE_H */
/**  @} */

