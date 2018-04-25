/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 1.17 $ of : mfd-data-access.m2c,v $
 *
 * $Id$
 */
#ifndef IFTABLE_DATA_ACCESS_H
#define IFTABLE_DATA_ACCESS_H

#include "Container.h"
#include "ifTable.h"
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
 *** Table ifTable
 ***
 **********************************************************************
 **********************************************************************/
    /*
     * IF-MIB::ifTable is subid 2 of interfaces.
     * Its status is Current.
     * OID: .1.3.6.1.2.1.2.2, length: 8
     */


    int             ifTable_init_data(ifTable_registration * ifTable_reg);


    /*
     * TODO:180:o: Review ifTable cache timeout.
     * The number of seconds before the cache times out
     */
    /*
     * A 10 Mbps stream can wrap if*Octets in ~57 minutes.
     * At 100 Mbps it is ~5 minutes, and at 1 Gbps, ~34 seconds.
     */
#define IFTABLE_CACHE_TIMEOUT   3

#define IFTABLE_REMOVE_MISSING_AFTER     (5 * 60) /* seconds */

    void            ifTable_container_init(Container_Container
                                           **container_ptr_ptr,
                                           Cache * cache);
    void            ifTable_container_shutdown(Container_Container
                                               *container_ptr);

    int             ifTable_container_load(Container_Container *container);
    void            ifTable_container_free(Container_Container *container);

    void            ifTable_container_shutdown(Container_Container
                                               *container_ptr);

    int             ifTable_container_load(Container_Container *container);
    void            ifTable_container_free(Container_Container *container);

    void            ifTable_container_shutdown(Container_Container
                                               *container_ptr);

    int             ifTable_container_load(Container_Container *container);
    void            ifTable_container_free(Container_Container *container);

    int             ifTable_cache_load(Container_Container *container);
    void            ifTable_cache_free(Container_Container *container);

    int             ifTable_row_prep(ifTable_rowreq_ctx * rowreq_ctx);



#endif                          /* IFTABLE_DATA_ACCESS_H */
