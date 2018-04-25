/*
 *  Interface MIB architecture support
 *
 * $Id$
 */

#include "interface_ioctl.h"
#include "Assert.h"
#include "Debug.h"
#include "Logger.h"
#include "Strlcpy.h"
#include "if-mib/ifTable/ifTable_constants.h"
#include "ip-mib/data_access/ipaddress_ioctl.h"
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>

/**
 * ioctl wrapper
 *
 * @param      fd : socket fd to use w/ioctl, or -1 to open/close one
 * @param  which
 * @param ifrq
 * param ifentry : ifentry to update
 * @param name
 *
 * @retval  0 : success
 * @retval -1 : invalid parameters
 * @retval -2 : couldn't create socket
 * @retval -3 : ioctl call failed
 */
static int
_ioctl_get( int fd, int which, struct ifreq* ifrq, const char* name )
{
    int ourfd = -1, rc = 0;

    DEBUG_MSGTL( ( "verbose:access:interface:ioctl",
        "ioctl %d for %s\n", which, name ) );

    /*
     * sanity checks
     */
    if ( NULL == name ) {
        Logger_log( LOGGER_PRIORITY_ERR, "invalid ifentry\n" );
        return -1;
    }

    /*
     * create socket for ioctls
     */
    if ( fd < 0 ) {
        fd = ourfd = socket( AF_INET, SOCK_DGRAM, 0 );
        if ( ourfd < 0 ) {
            Logger_log( LOGGER_PRIORITY_ERR, "couldn't create socket\n" );
            return -2;
        }
    }

    Strlcpy_strlcpy( ifrq->ifr_name, name, sizeof( ifrq->ifr_name ) );
    rc = ioctl( fd, which, ifrq );
    if ( rc < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "ioctl %d returned %d\n", which, rc );
        rc = -3;
    }

    if ( ourfd >= 0 )
        close( ourfd );

    return rc;
}

/**
 * interface entry physaddr ioctl wrapper
 *
 * @param      fd : socket fd to use w/ioctl, or -1 to open/close one
 * @param ifentry : ifentry to update
 *
 * @retval  0 : success
 * @retval -1 : invalid parameters
 * @retval -2 : couldn't create socket
 * @retval -3 : ioctl call failed
 * @retval -4 : malloc error
 */
int netsnmp_access_interface_ioctl_physaddr_get( int fd,
    netsnmp_interface_entry* ifentry )
{
    struct ifreq ifrq;
    int rc = 0;

    DEBUG_MSGTL( ( "access:interface:ioctl", "physaddr_get\n" ) );

    if ( ( NULL != ifentry->paddr ) && ( ifentry->paddr_len != IFHWADDRLEN ) ) {
        TOOLS_FREE( ifentry->paddr );
    }
    if ( NULL == ifentry->paddr )
        ifentry->paddr = ( char* )malloc( IFHWADDRLEN );

    if ( NULL == ifentry->paddr ) {
        rc = -4;
    } else {

        /*
         * NOTE: this ioctl does not guarantee 6 bytes of a physaddr.
         * In particular, a 'sit0' interface only appears to get back
         * 4 bytes of sa_data. Uncomment this memset, and suddenly
         * the sit interface will be 0:0:0:0:?:? where ? is whatever was
         * in the memory before. Not sure if this memset should be done
         * for every ioctl, as the rest seem to work ok...
         */
        memset( ifrq.ifr_hwaddr.sa_data, ( 0 ), IFHWADDRLEN );
        ifentry->paddr_len = IFHWADDRLEN;
        rc = _ioctl_get( fd, SIOCGIFHWADDR, &ifrq, ifentry->name );
        if ( rc < 0 ) {
            memset( ifentry->paddr, ( 0 ), IFHWADDRLEN );
            rc = -3; /* msg already logged */
        } else {
            memcpy( ifentry->paddr, ifrq.ifr_hwaddr.sa_data, IFHWADDRLEN );

            /*
             * arphrd defines vary greatly. ETHER seems to be the only common one
             */
            switch ( ifrq.ifr_hwaddr.sa_family ) {
            case ARPHRD_ETHER:
                ifentry->type = IANAIFTYPE_ETHERNETCSMACD;
                break;

            case ARPHRD_TUNNEL:
            case ARPHRD_TUNNEL6:

            case ARPHRD_IPGRE:

            case ARPHRD_SIT:
                ifentry->type = IANAIFTYPE_TUNNEL;
                break; /* tunnel */

            case ARPHRD_INFINIBAND:
                ifentry->type = IANAIFTYPE_INFINIBAND;
                break;

            case ARPHRD_SLIP:
            case ARPHRD_CSLIP:
            case ARPHRD_SLIP6:
            case ARPHRD_CSLIP6:
                ifentry->type = IANAIFTYPE_SLIP;
                break; /* slip */

            case ARPHRD_PPP:
                ifentry->type = IANAIFTYPE_PPP;
                break; /* ppp */

            case ARPHRD_LOOPBACK:
                ifentry->type = IANAIFTYPE_SOFTWARELOOPBACK;
                break; /* softwareLoopback */

            case ARPHRD_FDDI:
                ifentry->type = IANAIFTYPE_FDDI;
                break;

            case ARPHRD_ARCNET:
                ifentry->type = IANAIFTYPE_ARCNET;
                break;

            case ARPHRD_LOCALTLK:
                ifentry->type = IANAIFTYPE_LOCALTALK;
                break;

            case ARPHRD_HIPPI:
                ifentry->type = IANAIFTYPE_HIPPI;
                break;

            case ARPHRD_ATM:
                ifentry->type = IANAIFTYPE_ATM;
                break;
            /*
                 * XXX: more if_arp.h:ARPHRD_xxx to IANAifType mappings... 
                 */
            default:
                DEBUG_MSGTL( ( "access:interface:ioctl", "unknown entry type %d\n",
                    ifrq.ifr_hwaddr.sa_family ) );
                ifentry->type = IANAIFTYPE_OTHER;
            } /* switch */
        }
    }

    return rc;
}

/**
 * interface entry flags ioctl wrapper
 *
 * @param      fd : socket fd to use w/ioctl, or -1 to open/close one
 * @param ifentry : ifentry to update
 *
 * @retval  0 : success
 * @retval -1 : invalid parameters
 * @retval -2 : couldn't create socket
 * @retval -3 : ioctl call failed
 */
int netsnmp_access_interface_ioctl_flags_get( int fd,
    netsnmp_interface_entry* ifentry )
{
    struct ifreq ifrq;
    int rc = 0;

    DEBUG_MSGTL( ( "access:interface:ioctl", "flags_get\n" ) );

    rc = _ioctl_get( fd, SIOCGIFFLAGS, &ifrq, ifentry->name );
    if ( rc < 0 ) {
        ifentry->ns_flags &= ~NETSNMP_INTERFACE_FLAGS_HAS_IF_FLAGS;
        return rc; /* msg already logged */
    } else {
        ifentry->ns_flags |= NETSNMP_INTERFACE_FLAGS_HAS_IF_FLAGS;
        ifentry->os_flags = ifrq.ifr_flags;

        /*
         * ifOperStatus description:
         *   If ifAdminStatus is down(2) then ifOperStatus should be down(2).
         */
        if ( ifentry->os_flags & IFF_UP ) {
            ifentry->admin_status = IFADMINSTATUS_UP;
            if ( ifentry->os_flags & IFF_RUNNING )
                ifentry->oper_status = IFOPERSTATUS_UP;
            else
                ifentry->oper_status = IFOPERSTATUS_DOWN;
        } else {
            ifentry->admin_status = IFADMINSTATUS_DOWN;
            ifentry->oper_status = IFOPERSTATUS_DOWN;
        }

        /*
         * ifConnectorPresent description:
         *   This object has the value 'true(1)' if the interface sublayer has a
         *   physical connector and the value 'false(2)' otherwise."
         * So, at very least, false(2) should be returned for loopback devices.
         */
        if ( ifentry->os_flags & IFF_LOOPBACK ) {
            ifentry->connector_present = 0;
        } else {
            ifentry->connector_present = 1;
        }
    }

    return rc;
}

/**
 * interface entry flags ioctl wrapper
 *
 * @param      fd : socket fd to use w/ioctl, or -1 to open/close one
 * @param ifentry : ifentry to update
 *
 * @retval  0 : success
 * @retval -1 : invalid parameters
 * @retval -2 : couldn't create socket
 * @retval -3 : ioctl get call failed
 * @retval -4 : ioctl set call failed
 */
int netsnmp_access_interface_ioctl_flags_set( int fd,
    netsnmp_interface_entry* ifentry,
    unsigned int flags, int and_complement )
{
    struct ifreq ifrq;
    int ourfd = -1, rc = 0;

    DEBUG_MSGTL( ( "access:interface:ioctl", "flags_set\n" ) );

    /*
     * sanity checks
     */
    if ( ( NULL == ifentry ) || ( NULL == ifentry->name ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "invalid ifentry\n" );
        return -1;
    }

    /*
     * create socket for ioctls
     */
    if ( fd < 0 ) {
        fd = ourfd = socket( AF_INET, SOCK_DGRAM, 0 );
        if ( ourfd < 0 ) {
            Logger_log( LOGGER_PRIORITY_ERR, "couldn't create socket\n" );
            return -2;
        }
    }

    Strlcpy_strlcpy( ifrq.ifr_name, ifentry->name, sizeof( ifrq.ifr_name ) );
    rc = ioctl( fd, SIOCGIFFLAGS, &ifrq );
    if ( rc < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error getting flags\n" );
        close( fd );
        return -3;
    }
    if ( 0 == and_complement )
        ifrq.ifr_flags |= flags;
    else
        ifrq.ifr_flags &= ~flags;
    rc = ioctl( fd, SIOCSIFFLAGS, &ifrq );
    if ( rc < 0 ) {
        close( fd );
        Logger_log( LOGGER_PRIORITY_ERR, "error setting flags\n" );
        ifentry->os_flags = 0;
        return -4;
    }

    if ( ourfd >= 0 )
        close( ourfd );

    ifentry->os_flags = ifrq.ifr_flags;

    return 0;
}

/**
 * interface entry mtu ioctl wrapper
 *
 * @param      fd : socket fd to use w/ioctl, or -1 to open/close one
 * @param ifentry : ifentry to update
 *
 * @retval  0 : success
 * @retval -1 : invalid parameters
 * @retval -2 : couldn't create socket
 * @retval -3 : ioctl call failed
 */
int netsnmp_access_interface_ioctl_mtu_get( int fd,
    netsnmp_interface_entry* ifentry )
{
    struct ifreq ifrq;
    int rc = 0;

    DEBUG_MSGTL( ( "access:interface:ioctl", "mtu_get\n" ) );

    rc = _ioctl_get( fd, SIOCGIFMTU, &ifrq, ifentry->name );
    if ( rc < 0 ) {
        ifentry->mtu = 0;
        return rc; /* msg already logged */
    } else {
        ifentry->mtu = ifrq.ifr_mtu;
    }

    return rc;
}

/**
 * interface entry ifIndex ioctl wrapper
 *
 * @param      fd : socket fd to use w/ioctl, or -1 to open/close one
 * @param name   : ifentry to update
 *
 * @retval  0 : not found
 * @retval !0 : ifIndex
 */
oid netsnmp_access_interface_ioctl_ifindex_get( int fd, const char* name )
{

    struct ifreq ifrq;
    int rc = 0;

    DEBUG_MSGTL( ( "access:interface:ioctl", "ifindex_get\n" ) );

    rc = _ioctl_get( fd, SIOCGIFINDEX, &ifrq, name );
    if ( rc < 0 ) {
        DEBUG_MSGTL( ( "access:interface:ioctl",
            "ifindex_get error on inerface '%s'\n", name ) );
        return 0;
    }

    return ifrq.ifr_ifindex;
}

/**
 * check an interface for ipv4 addresses
 *
 * @param sd      : open socket descriptor
 * @param if_name : optional name. takes precedent over if_index.
 * @param if_index: optional if index. only used if no if_name specified
 * @param flags   :
 *
 * @retval < 0 : error
 * @retval   0 : no ip v4 addresses
 * @retval   1 : 1 or more ip v4 addresses
 */
int netsnmp_access_interface_ioctl_has_ipv4( int sd, const char* if_name,
    int if_index, u_int* flags )
{
    int i, interfaces = 0;
    struct ifconf ifc;
    struct ifreq* ifrp;

    /*
     * one or the other
     */
    if ( ( NULL == flags ) || ( ( 0 == if_index ) && ( NULL == if_name ) ) ) {
        return -1;
    }

    interfaces = netsnmp_access_ipaddress_ioctl_get_interface_count( sd, &ifc );
    if ( interfaces < 0 ) {
        close( sd );
        return -2;
    }
    Assert_assert( NULL != ifc.ifc_buf );

    *flags &= ~NETSNMP_INTERFACE_FLAGS_HAS_IPV4;

    ifrp = ifc.ifc_req;
    for ( i = 0; i < interfaces; ++i, ++ifrp ) {

        DEBUG_MSGTL( ( "access:ipaddress:container",
            " interface %d, %s\n", i, ifrp->ifr_name ) );

        /*
         * search for matching if_name or if_index
         */
        if ( NULL != if_name ) {
            if ( strncmp( if_name, ifrp->ifr_name, sizeof( ifrp->ifr_name ) ) != 0 )
                continue;
        } else {
            /*
             * I think that Linux and Solaris both use ':' in the
             * interface name for aliases.
             */
            char* ptr = strchr( ifrp->ifr_name, ':' );
            if ( NULL != ptr )
                *ptr = 0;

            if ( if_index != ( int )netsnmp_access_interface_ioctl_ifindex_get( sd, ifrp->ifr_name ) )
                continue;
        }

        /*
         * check and set v4 or v6 flag, and break if we've found both
         */
        if ( AF_INET == ifrp->ifr_addr.sa_family ) {
            *flags |= NETSNMP_INTERFACE_FLAGS_HAS_IPV4;
            break;
        }
    }

    /*
     * clean up
     */
    free( ifc.ifc_buf );

    return 0;
}
