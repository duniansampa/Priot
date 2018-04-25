/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 1.17 $ of : mfd-data-access.m2c,v $
 *
 * $Id$
 */
#ifndef SNMPNOTIFYFILTERTABLE_DATA_ACCESS_H
#define SNMPNOTIFYFILTERTABLE_DATA_ACCESS_H

#include "snmpNotifyFilterTable.h"
#include "Container.h"
#include "Assert.h"
    /*
     *********************************************************************
     * function declarations
     */

    /*
     *********************************************************************
     * Table declarations
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


    int
     
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        snmpNotifyFilterTable_init_data(snmpNotifyFilterTable_registration
                                        * snmpNotifyFilterTable_reg);


    void            snmpNotifyFilterTable_container_init(Container_Container
                                                         **container_ptr_ptr);
    void
     
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        snmpNotifyFilterTable_container_shutdown(Container_Container
                                                 *container_ptr);

    int             snmpNotifyFilterTable_container_load(Container_Container
                                                         *container);
    void            snmpNotifyFilterTable_container_free(Container_Container
                                                         *container);

    int
     
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        snmpNotifyFilterTable_row_prep(snmpNotifyFilterTable_rowreq_ctx *
                                       rowreq_ctx);

    int
     
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        snmpNotifyFilterTable_validate_index
        (snmpNotifyFilterTable_registration * snmpNotifyFilterTable_reg,
         snmpNotifyFilterTable_rowreq_ctx * rowreq_ctx);
    int             snmpNotifyFilterTable_snmpNotifyFilterProfileName_check_index(snmpNotifyFilterTable_rowreq_ctx * rowreq_ctx);       /* external */
    int             snmpNotifyFilterSubtree_check_index(snmpNotifyFilterTable_rowreq_ctx * rowreq_ctx); /* internal */

    struct vacm_viewEntry *snmpNotifyFilterTable_vacm_view_subtree(const
                                                                   char
                                                                   *profile);


#endif                          /* SNMPNOTIFYFILTERTABLE_DATA_ACCESS_H */