/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 1.17 $ of : mfd-data-access.m2c,v $
 *
 * $Id$
 */
#ifndef TCPLISTENERTABLE_DATA_ACCESS_H
#define TCPLISTENERTABLE_DATA_ACCESS_H

#include "Container.h"
#include "CacheHandler.h"
#include "tcpListenerTable.h"

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
 *** Table tcpListenerTable
 ***
 **********************************************************************
 **********************************************************************/
    /*
     * TCP-MIB::tcpListenerTable is subid 20 of tcp.
     * Its status is Current.
     * OID: .1.3.6.1.2.1.6.20, length: 8
     */


    int
        tcpListenerTable_init_data(tcpListenerTable_registration *
                                   tcpListenerTable_reg);


    /*
     * TODO:180:o: Review tcpListenerTable cache timeout.
     * The number of seconds before the cache times out
     */
#define TCPLISTENERTABLE_CACHE_TIMEOUT   60

    void            tcpListenerTable_container_init(Container_Container
                                                    **container_ptr_ptr,
                                                    Cache * cache);
    void            tcpListenerTable_container_shutdown(Container_Container
                                                        *container_ptr);

    int             tcpListenerTable_container_load(Container_Container
                                                    *container);
    void            tcpListenerTable_container_free(Container_Container
                                                    *container);

    int             tcpListenerTable_cache_load(Container_Container
                                                *container);
    void            tcpListenerTable_cache_free(Container_Container
                                                *container);

    int             tcpListenerTable_row_prep(tcpListenerTable_rowreq_ctx *
                                              rowreq_ctx);



#endif                          /* TCPLISTENERTABLE_DATA_ACCESS_H */
