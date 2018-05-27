/*
 *  IP MIB group implementation - ip.c
 *
 */

/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright � 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include "ipAddr.h"
#include "System/Util/Trace.h"
#include "PluginSettings.h"
#include "Vars.h"
#include "interfaces.h"
#include "ip.h"
#include "siglog/data_access/interface.h"
#include <net/if.h>
#include <sys/ioctl.h>

/*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

/*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

/*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

static void Address_Scan_Init( void );

static struct ifconf ifc;
static int Address_Scan_Next( short*, struct ifnet* );

/*
 * var_ipAddrEntry(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */

u_char*
var_ipAddrEntry( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.20.1.?.A.B.C.D,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */
    oid lowest[ 14 ];
    oid current[ 14 ], *op;
    u_char* cp;
    int lowinterface = 0;
    short interface;

    static struct ifnet lowin_ifnet;
    static struct ifnet ifnet;
    static in_addr_t addr_ret;

    /*
     * fill in object part of name for current (less sizeof instance part) 
     */

    memcpy( ( char* )current, ( char* )vp->name,
        ( int )vp->namelen * sizeof( oid ) );

    Address_Scan_Init();
    for ( ;; ) {

        if ( Address_Scan_Next( &interface, &ifnet ) == 0 )
            break;

        cp = ( u_char* )&( ( ( struct sockaddr_in* )&( ifnet.if_addr ) )->sin_addr.s_addr );

        op = current + 10;
        *op++ = *cp++;
        *op++ = *cp++;
        *op++ = *cp++;
        *op++ = *cp++;
        if ( exact ) {
            if ( Api_oidCompare( current, 14, name, *length ) == 0 ) {
                memcpy( ( char* )lowest, ( char* )current,
                    14 * sizeof( oid ) );
                lowinterface = interface;
                lowin_ifnet = ifnet;
                break; /* no need to search further */
            }
        } else {
            if ( ( Api_oidCompare( current, 14, name, *length ) > 0 ) && ( !lowinterface
                                                                             || ( Api_oidCompare( current, 14, lowest, 14 ) < 0 ) ) ) {
                /*
                 * if new one is greater than input and closer to input than
                 * previous lowest, save this one as the "next" one.
                 */
                lowinterface = interface;
                lowin_ifnet = ifnet;
                memcpy( ( char* )lowest, ( char* )current,
                    14 * sizeof( oid ) );
            }
        }
    }

    MEMORY_FREE( ifc.ifc_buf );

    if ( !lowinterface )
        return ( NULL );
    memcpy( ( char* )name, ( char* )lowest, 14 * sizeof( oid ) );
    *length = 14;
    *write_method = ( WriteMethodFT* )0;
    *var_len = sizeof( vars_longReturn );
    switch ( vp->magic ) {
    case IPADADDR:
        *var_len = sizeof( addr_ret );

        return ( u_char* )&( ( struct sockaddr_in* )&lowin_ifnet.if_addr )->sin_addr.s_addr;

    case IPADIFINDEX:
        vars_longReturn = lowinterface;
        return ( u_char* )&vars_longReturn;
    case IPADNETMASK:
        *var_len = sizeof( addr_ret );

        return ( u_char* )&( ( struct sockaddr_in* )&lowin_ifnet.ia_subnetmask )->sin_addr.s_addr;

    case IPADBCASTADDR:

        *var_len = sizeof( vars_longReturn );
        vars_longReturn = ntohl( ( ( struct sockaddr_in* )&lowin_ifnet.ifu_broadaddr )->sin_addr.s_addr ) & 1;
        return ( u_char* )&vars_longReturn;
    case IPADREASMMAX:

        return NULL;

    default:
        DEBUG_MSGTL( ( "snmpd", "unknown sub-id %d in var_ipAddrEntry\n",
            vp->magic ) );
    }
    return NULL;
}

static struct ifreq* ifr;
static int ifr_counter;

static void
Address_Scan_Init( void )
{
    int num_interfaces = 0;
    int fd;

    /* get info about all interfaces */

    ifc.ifc_len = 0;
    MEMORY_FREE( ifc.ifc_buf );
    ifr_counter = 0;

    do {
        if ( ( fd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
            DEBUG_MSGTL( ( "snmpd", "socket open failure in Address_Scan_Init\n" ) );
            return;
        }
        num_interfaces += 16;

        ifc.ifc_len = sizeof( struct ifreq ) * num_interfaces;
        ifc.ifc_buf = ( char* )realloc( ifc.ifc_buf, ifc.ifc_len );

        if ( ioctl( fd, SIOCGIFCONF, &ifc ) < 0 ) {
            ifr = NULL;
            close( fd );
            return;
        }
        close( fd );
    } while ( ifc.ifc_len >= ( sizeof( struct ifreq ) * num_interfaces ) );

    ifr = ifc.ifc_req;
    close( fd );
}

/*
 * NB: Index is the number of the corresponding interface, not of the address 
 */
static int
Address_Scan_Next( short* Index, struct ifnet* Retifnet )
{
    struct ifnet ifnet_store;
    int fd;
    if ( ( fd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
        DEBUG_MSGTL( ( "snmpd", "socket open failure in Address_Scan_Next\n" ) );
        return ( 0 );
    }

    while ( ifr ) {

        ifnet_store.if_addr = ifr->ifr_addr;

        if ( Retifnet ) {
            Retifnet->if_addr = ifr->ifr_addr;

            if ( ioctl( fd, SIOCGIFBRDADDR, ifr ) < 0 ) {
                memset( ( char* )&Retifnet->ifu_broadaddr, 0, sizeof( Retifnet->ifu_broadaddr ) );
            } else
                Retifnet->ifu_broadaddr = ifr->ifr_broadaddr;

            ifr->ifr_addr = Retifnet->if_addr;
            if ( ioctl( fd, SIOCGIFNETMASK, ifr ) < 0 ) {
                memset( ( char* )&Retifnet->ia_subnetmask, 0, sizeof( Retifnet->ia_subnetmask ) );
            } else
                Retifnet->ia_subnetmask = ifr->ifr_netmask;
        }

        if ( Index ) {
            ifr->ifr_addr = ifnet_store.if_addr;
            *Index = netsnmp_access_interface_index_find( ifr->ifr_name );
        }

        ifr++;
        ifr_counter += sizeof( struct ifreq );
        if ( ifr_counter >= ifc.ifc_len ) {
            ifr = NULL; /* beyond the end */
        }

        close( fd );
        return ( 1 ); /* DONE */
    }
    close( fd );
    return ( 0 ); /* EOF */
}
