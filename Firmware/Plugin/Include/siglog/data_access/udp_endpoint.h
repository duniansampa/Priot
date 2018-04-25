/*
 * udp_endpoint data access header
 *
 * $Id$
 */
#ifndef NETSNMP_ACCESS_UDP_ENDPOINT_H
#define NETSNMP_ACCESS_UDP_ENDPOINT_H


#include "siglog/data_access/ipaddress.h"

/**---------------------------------------------------------------------*/
/*
 * structure definitions
 */

/*
 * netsnmp_udp_endpoint_entry
 *   - primary udp_endpoint structure for both ipv4 & ipv6
 */
    typedef struct netsnmp_udp_endpoint_s {

        Types_Index oid_index;   /* MUST BE FIRST!! for container use */
        oid           index; /* sl */

        int       flags; /* for net-snmp use */

        u_char    loc_addr[NETSNMP_ACCESS_IPADDRESS_BUF_SIZE];
        u_char    rmt_addr[NETSNMP_ACCESS_IPADDRESS_BUF_SIZE];

        u_char    loc_addr_len;/* address len, 4 | 16 */
        u_char    rmt_addr_len;/* address len, 4 | 16 */
        u_char    state; /* not in the mib, but what the heck */

        u_short   loc_port;
        u_short   rmt_port;

        u_int     instance;
        u_int     pid;
   
    } netsnmp_udp_endpoint_entry;


/**---------------------------------------------------------------------*/
/*
 * ACCESS function prototypes
 */
/*
 * ifcontainer init
 */
    Container_Container *
    netsnmp_access_udp_endpoint_container_init(u_int init_flags);
#define NETSNMP_ACCESS_UDP_ENDPOINT_INIT_NOFLAGS               0x0000

/*
 * ifcontainer load and free
 */
    Container_Container*
    netsnmp_access_udp_endpoint_container_load(Container_Container* c,
                                          u_int load_flags);
#define NETSNMP_ACCESS_UDP_ENDPOINT_LOAD_NOFLAGS               0x0000

    void netsnmp_access_udp_endpoint_container_free(Container_Container *c,
                                               u_int free_flags);
#define NETSNMP_ACCESS_UDP_ENDPOINT_FREE_NOFLAGS               0x0000
#define NETSNMP_ACCESS_UDP_ENDPOINT_FREE_DONT_CLEAR            0x0001
#define NETSNMP_ACCESS_UDP_ENDPOINT_FREE_KEEP_CONTAINER        0x0002


/*
 * create/free a udp_endpoint entry
 */
    netsnmp_udp_endpoint_entry *
    netsnmp_access_udp_endpoint_entry_create(void);

    void netsnmp_access_udp_endpoint_entry_free(netsnmp_udp_endpoint_entry *e);

/*
 * update/compare
 */

/*
 * find entry in container
 */
/** not yet */



/**---------------------------------------------------------------------*/


#endif /* NETSNMP_ACCESS_UDP_ENDPOINT_H */
