/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 1.17 $ of : mfd-data-access.m2c,v $
 *
 * $Id$
 */
#ifndef INETNETTOMEDIATABLE_DATA_ACCESS_H
#define INETNETTOMEDIATABLE_DATA_ACCESS_H

#include "Container.h"
#include "CacheHandler.h"
#include "inetNetToMediaTable.h"

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
 *** Table inetNetToMediaTable
 ***
 **********************************************************************
 **********************************************************************/
    /*
     * IP-MIB::inetNetToMediaTable is subid 35 of ip.
     * Its status is Current.
     * OID: .1.3.6.1.2.1.4.35, length: 8
     */


    int
        inetNetToMediaTable_init_data(inetNetToMediaTable_registration *
                                      inetNetToMediaTable_reg);


    /*
     * TODO:180:o: Review inetNetToMediaTable cache timeout.
     * The number of seconds before the cache times out
     */
#define INETNETTOMEDIATABLE_CACHE_TIMEOUT   60

    void            inetNetToMediaTable_container_init(Container_Container
                                                       **container_ptr_ptr,
                                                       Cache *
                                                       cache);
    void
                    inetNetToMediaTable_container_shutdown(Container_Container
                                                           *container_ptr);

    int             inetNetToMediaTable_container_load(Container_Container
                                                       *container);
    void            inetNetToMediaTable_container_free(Container_Container
                                                       *container);

    int             inetNetToMediaTable_cache_load(Container_Container
                                                   *container);
    void            inetNetToMediaTable_cache_free(Container_Container
                                                   *container);

    int
        inetNetToMediaTable_row_prep(inetNetToMediaTable_rowreq_ctx *
                                     rowreq_ctx);

    int
        inetNetToMediaTable_validate_index(inetNetToMediaTable_registration
                                           * inetNetToMediaTable_reg,
                                           inetNetToMediaTable_rowreq_ctx *
                                           rowreq_ctx);
    int             inetNetToMediaIfIndex_check_index(inetNetToMediaTable_rowreq_ctx * rowreq_ctx);     /* internal */
    int             inetNetToMediaNetAddressType_check_index(inetNetToMediaTable_rowreq_ctx * rowreq_ctx);      /* internal */
    int             inetNetToMediaNetAddress_check_index(inetNetToMediaTable_rowreq_ctx * rowreq_ctx);  /* internal */


#endif                          /* INETNETTOMEDIATABLE_DATA_ACCESS_H */