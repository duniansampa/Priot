/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 14170 $ of $
 *
 * $Id:ipv6ScopeZoneIndexTable_data_access.h 14170 2007-04-29 00:12:32Z varun_c$
 */
#ifndef IPV6SCOPEZONEINDEXTABLE_DATA_ACCESS_H
#define IPV6SCOPEZONEINDEXTABLE_DATA_ACCESS_H

#include "System/Containers/Container.h"
#include "ipv6ScopeZoneIndexTable.h"
#include "CacheHandler.h"

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
 *** Table ipv6ScopeZoneIndexTable
 ***
 **********************************************************************
 **********************************************************************/
    /*
     * IP-MIB::ipv6ScopeZoneIndexTable is subid 36 of ip.
     * Its status is Current.
     * OID: .1.3.6.1.2.1.4.36, length: 8
     */


    int            
        ipv6ScopeZoneIndexTable_init_data
        (ipv6ScopeZoneIndexTable_registration *
         ipv6ScopeZoneIndexTable_reg);


    void           
        ipv6ScopeZoneIndexTable_container_init(Container_Container **
                                               container_ptr_ptr,
                                               Cache *
                                               cache);
    void           
        ipv6ScopeZoneIndexTable_container_shutdown(Container_Container *
                                                   container_ptr);

    int            
        ipv6ScopeZoneIndexTable_container_load(Container_Container *
                                               container);
    void           
        ipv6ScopeZoneIndexTable_container_free(Container_Container *
                                               container);

#define MAX_LINE_SIZE 256
    int            
        ipv6ScopeZoneIndexTable_row_prep(ipv6ScopeZoneIndexTable_rowreq_ctx
                                         * rowreq_ctx);


#endif                          /* IPV6SCOPEZONEINDEXTABLE_DATA_ACCESS_H */
