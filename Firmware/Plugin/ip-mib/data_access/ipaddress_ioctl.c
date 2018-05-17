/*
 *  Interface MIB architecture support
 *
 * $Id$
 */

#include "ipaddress_ioctl.h"
#include "System/Util/Assert.h"
#include "System/Containers/Map.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "Priot.h"
#include "System/String.h"
#include "if-mib/data_access/interface_ioctl.h"
#include "ip-mib/ipAddressTable/ipAddressTable_constants.h"
#include <sys/ioctl.h>

#include "PluginSettings.h"

static void _print_flags( short flags );

#define LIST_TOKEN "ioctl_extras"

/*
 * get extra structure
 *
 * @returns the extras structure from the entry
 */
_ioctl_extras*
netsnmp_ioctl_ipaddress_extras_get( netsnmp_ipaddress_entry* entry )
{
    if ( ( NULL == entry ) || ( NULL == entry->arch_data ) )
        return NULL;

    return ( _ioctl_extras* )Map_at( entry->arch_data, LIST_TOKEN );
}

/**
 * initialize ioctl extras
 *
 * @returns _ioctl_extras pointer, or NULL on error
 */
_ioctl_extras*
netsnmp_ioctl_ipaddress_entry_init( netsnmp_ipaddress_entry* entry )
{
    Map* node;
    _ioctl_extras* extras;

    if ( NULL == entry )
        return NULL;

    extras = MEMORY_MALLOC_TYPEDEF( _ioctl_extras );
    if ( NULL == extras )
        return NULL;

    node = Map_newElement( LIST_TOKEN, extras, free );
    if ( NULL == node ) {
        free( extras );
        return NULL;
    }

    Map_insert( &entry->arch_data, node );

    return extras;
}

/**
 * cleanup ioctl extras
 */
void netsnmp_ioctl_ipaddress_entry_cleanup( netsnmp_ipaddress_entry* entry )
{
    if ( NULL == entry ) {
        Assert_assert( NULL != entry );
        return;
    }

    if ( NULL == entry->arch_data ) {
        Assert_assert( NULL != entry->arch_data );
        return;
    }

    Map_erase( &entry->arch_data, LIST_TOKEN );
}

/**
 * copy ioctl extras
 *
 * @retval  0: success
 * @retval <0: error
 */
int netsnmp_ioctl_ipaddress_entry_copy( netsnmp_ipaddress_entry* lhs,
    netsnmp_ipaddress_entry* rhs )
{
    _ioctl_extras *lhs_extras, *rhs_extras;
    int rc = PRIOT_ERR_NOERROR;

    if ( ( NULL == lhs ) || ( NULL == rhs ) ) {
        Assert_assert( ( NULL != lhs ) && ( NULL != rhs ) );
        return -1;
    }

    rhs_extras = netsnmp_ioctl_ipaddress_extras_get( rhs );
    lhs_extras = netsnmp_ioctl_ipaddress_extras_get( lhs );
    if ( NULL == rhs_extras ) {
        if ( NULL != lhs_extras )
            netsnmp_ioctl_ipaddress_entry_cleanup( lhs );
    } else {
        if ( NULL == lhs_extras )
            lhs_extras = netsnmp_ioctl_ipaddress_entry_init( lhs );

        if ( NULL != lhs_extras )
            memcpy( lhs_extras, rhs_extras, sizeof( _ioctl_extras ) );
        else
            rc = -1;
    }

    return rc;
}

/**
 * load ipv4 address via ioctl
 */
int _netsnmp_ioctl_ipaddress_container_load_v4( Container_Container* container,
    int idx_offset )
{
    int i, sd, rc = 0, interfaces = 0;
    struct ifconf ifc;
    struct ifreq* ifrp;
    struct sockaddr save_addr;
    struct sockaddr_in* si;
    struct address_flag_info addr_info;
    in_addr_t ipval;
    _ioctl_extras* extras;

    if ( ( sd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "could not create socket\n" );
        return -1;
    }

    interfaces = netsnmp_access_ipaddress_ioctl_get_interface_count( sd, &ifc );
    if ( interfaces < 0 ) {
        close( sd );
        return -2;
    }
    Assert_assert( NULL != ifc.ifc_buf );
    DEBUG_MSGTL( ( "access:ipaddress:container", "processing %d interfaces\n", interfaces ) );

    ifrp = ifc.ifc_req;
    for ( i = 0; i < interfaces; ++i, ++ifrp ) {
        netsnmp_ipaddress_entry *entry, *bcastentry = NULL;

        DEBUG_MSGTL( ( "access:ipaddress:container",
            " interface %d, %s\n", i, ifrp->ifr_name ) );

        if ( AF_INET != ifrp->ifr_addr.sa_family ) {
            DEBUG_MSGTL( ( "access:ipaddress:container",
                " skipping %s; non AF_INET family %d\n",
                ifrp->ifr_name, ifrp->ifr_addr.sa_family ) );
            continue;
        }

        /*
         */
        entry = netsnmp_access_ipaddress_entry_create();
        if ( NULL == entry ) {
            rc = -3;
            break;
        }
        entry->ns_ia_index = ++idx_offset;

        /*
         * save if name
         */
        extras = netsnmp_ioctl_ipaddress_extras_get( entry );
        memcpy( extras->name, ifrp->ifr_name, sizeof( extras->name ) );

        /*
         * each time we make an ioctl, we need to specify the address, but
         * it will be overwritten in the call. so we save address here.
         */
        save_addr = ifrp->ifr_addr;

        /*
         * set indexes
         */
        si = ( struct sockaddr_in* )&ifrp->ifr_addr;
        entry->ia_address_len = sizeof( si->sin_addr.s_addr );
        ipval = si->sin_addr.s_addr;
        memcpy( entry->ia_address, &si->sin_addr.s_addr,
            entry->ia_address_len );

        /*
         * get ifindex
         */
        {
            /*
             * I think that Linux and Solaris both use ':' in the
             * interface name for aliases. When a new arch is added
             * that uses some other indicator, a new function, maybe
             * netsnmp_access_ipaddress_entry_name_alias_check(), will
             * need to be written.
             */
            char* ptr = strchr( ifrp->ifr_name, ':' );
            if ( NULL != ptr ) {
                entry->flags |= NETSNMP_ACCESS_IPADDRESS_ISALIAS;
                *ptr = 0;
            }
        }
        entry->if_index = netsnmp_access_interface_ioctl_ifindex_get( sd, ifrp->ifr_name );
        if ( 0 == entry->if_index ) {
            Logger_log( LOGGER_PRIORITY_ERR, "no ifindex found for interface\n" );
            netsnmp_access_ipaddress_entry_free( entry );
            continue;
        }

        /* restore the interface name if we modifed it due to unaliasing
         * above
         */
        if ( entry->flags & NETSNMP_ACCESS_IPADDRESS_ISALIAS ) {
            memcpy( ifrp->ifr_name, extras->name, sizeof( extras->name ) );
        }

        /*
         * get broadcast
         */
        memset( &addr_info, 0, sizeof( struct address_flag_info ) );

        /*
         * get netmask
         */
        ifrp->ifr_addr = save_addr;
        if ( ioctl( sd, SIOCGIFNETMASK, ifrp ) < 0 ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "error getting netmask for interface %d\n", i );
            netsnmp_access_ipaddress_entry_free( entry );
            continue;
        }
        Assert_assert( AF_INET == ifrp->ifr_addr.sa_family );
        si = ( struct sockaddr_in* )&ifrp->ifr_addr;
        entry->ia_prefix_len = netsnmp_ipaddress_ipv4_prefix_len( si->sin_addr.s_addr );
        if ( bcastentry )
            bcastentry->ia_prefix_len = entry->ia_prefix_len;

        /*
         * get flags
         */
        ifrp->ifr_addr = save_addr;
        if ( ioctl( sd, SIOCGIFFLAGS, ifrp ) < 0 ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "error getting if_flags for interface %d\n", i );
            netsnmp_access_ipaddress_entry_free( entry );
            continue;
        }
        extras->flags = ifrp->ifr_flags;

        if ( bcastentry )
            bcastentry->ia_type = IPADDRESSTYPE_BROADCAST;
        if ( addr_info.anycastflg )
            entry->ia_type = IPADDRESSTYPE_ANYCAST;
        else
            entry->ia_type = IPADDRESSTYPE_UNICAST;

        /** entry->ia_prefix_oid ? */

        /*
         * per the MIB:
         *   In the absence of other information, an IPv4 address is
         *   always preferred(1).
         */
        entry->ia_status = IPADDRESSSTATUSTC_PREFERRED;
        if ( bcastentry )
            bcastentry->ia_status = IPADDRESSSTATUSTC_PREFERRED;

        /*
         * can we figure out if an address is from DHCP?
         * use manual until then...
         */
        if ( IS_APIPA( ipval ) ) {
            entry->ia_origin = IPADDRESSORIGINTC_RANDOM;
            if ( bcastentry )
                bcastentry->ia_origin = IPADDRESSORIGINTC_RANDOM;
        } else {
            entry->ia_origin = IPADDRESSORIGINTC_MANUAL;
            if ( bcastentry )
                bcastentry->ia_origin = IPADDRESSORIGINTC_MANUAL;
        }

        DEBUG_IF( "access:ipaddress:container" )
        {
            DEBUG_MSGT_NC( ( "access:ipaddress:container",
                " if %d: addr len %d, index 0x%"
                "l"
                "x\n",
                i, entry->ia_address_len, entry->if_index ) );
            if ( 4 == entry->ia_address_len )
                DEBUG_MSGT_NC( ( "access:ipaddress:container", " address %p\n",
                    *( ( void** )entry->ia_address ) ) );
            DEBUG_MSGT_NC( ( "access:ipaddress:container", "flags 0x%x\n",
                extras->flags ) );
            _print_flags( extras->flags );
        }

        /*
         * add entry to container
         */
        if ( bcastentry ) {
            if ( CONTAINER_INSERT( container, bcastentry ) < 0 ) {
                DEBUG_MSGTL( ( "access:ipaddress:container", "error with ipaddress_entry: insert broadcast entry into container failed.\n" ) );
                netsnmp_access_ipaddress_entry_free( bcastentry );
                netsnmp_access_ipaddress_entry_free( entry );
                continue;
            }
            bcastentry = NULL;
        }

        if ( CONTAINER_INSERT( container, entry ) < 0 ) {
            DEBUG_MSGTL( ( "access:ipaddress:container", "error with ipaddress_entry: insert into container failed.\n" ) );
            LOGGER_LOGONCE( ( LOGGER_PRIORITY_ERR, "Duplicate IPv4 address detected, some interfaces may not be visible in IP-MIB\n" ) );
            netsnmp_access_ipaddress_entry_free( entry );
            continue;
        }
    }

    /*
     * clean up
     */
    free( ifc.ifc_buf );
    close( sd );

    /*
     * return number of interfaces seen
     */
    if ( rc < 0 )
        return rc;

    return idx_offset;
}

/**
 * find unused alias number
 */
static int
_next_alias( const char* if_name )
{
    int i, j, k, sd, interfaces = 0, len;
    struct ifconf ifc;
    struct ifreq* ifrp;
    char* alias;
    int* alias_list;

    if ( NULL == if_name )
        return -1;
    len = strlen( if_name );

    if ( ( sd = socket( AF_INET, SOCK_DGRAM, 0 ) ) < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "could not create socket\n" );
        return -1;
    }

    interfaces = netsnmp_access_ipaddress_ioctl_get_interface_count( sd, &ifc );
    if ( interfaces < 0 ) {
        close( sd );
        return -2;
    }
    Assert_assert( NULL != ifc.ifc_buf );
    DEBUG_MSGTL( ( "access:ipaddress:container", "processing %d interfaces\n", interfaces ) );

    alias_list = ( int* )malloc( interfaces * sizeof( int ) );
    if ( NULL == alias_list ) {
        close( sd );
        return -2;
    }

    ifrp = ifc.ifc_req;
    for ( i = 0, j = 0; i < interfaces; ++i, ++ifrp ) {

        if ( strncmp( ifrp->ifr_name, if_name, len ) != 0 )
            continue;

        DEBUG_MSGTL( ( "access:ipaddress:container",
            " interface %d, %s\n", i, ifrp->ifr_name ) );

        alias = strchr( ifrp->ifr_name, ':' );
        if ( NULL == alias )
            continue;

        ++alias; /* skip ':' */
        alias_list[ j++ ] = atoi( alias );
    }

    /*
     * clean up
     */
    free( ifc.ifc_buf );
    close( sd );

    /*
     * return first unused alias
     */
    for ( i = 1; i <= interfaces; ++i ) {
        for ( k = 0; k < j; ++k )
            if ( alias_list[ k ] == i )
                break;
        if ( k == j ) {
            free( alias_list );
            return i;
        }
    }

    free( alias_list );
    return interfaces + 1;
}

/**
 *
 * @retval  0 : no error
 * @retval -1 : bad parameter
 * @retval -2 : couldn't create socket
 * @retval -3 : ioctl failed
 */
int _netsnmp_ioctl_ipaddress_set_v4( netsnmp_ipaddress_entry* entry )
{
    struct ifreq ifrq;
    struct sockaddr_in* sin;
    int rc, fd = -1;
    _ioctl_extras* extras;

    if ( NULL == entry )
        return -1;

    Assert_assert( 4 == entry->ia_address_len );

    extras = netsnmp_ioctl_ipaddress_extras_get( entry );
    if ( NULL == extras )
        return -1;

    fd = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( fd < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "couldn't create socket\n" );
        return -2;
    }
    memset( &ifrq, 0, sizeof( ifrq ) );

    if ( '\0' == extras->name[ 0 ] ) {
        const char* name = netsnmp_access_interface_name_find( entry->if_index );
        int alias_idx;

        if ( NULL == name ) {
            DEBUG_MSGT( ( "access:ipaddress:set",
                "cant find name for index %"
                "l"
                "d\n",
                entry->if_index ) );
            close( fd );
            return -1;
        }

        /*
         * search for unused alias
         */
        alias_idx = _next_alias( name );
        snprintf( ifrq.ifr_name, sizeof( ifrq.ifr_name ), "%s:%d",
            name, alias_idx );
        ifrq.ifr_name[ sizeof( ifrq.ifr_name ) - 1 ] = 0;
    } else
        String_copyTruncate( ifrq.ifr_name, ( char* )extras->name, sizeof( ifrq.ifr_name ) );

    sin = ( struct sockaddr_in* )&ifrq.ifr_addr;
    sin->sin_family = AF_INET;
    memcpy( &sin->sin_addr.s_addr, entry->ia_address,
        entry->ia_address_len );

    rc = ioctl( fd, SIOCSIFADDR, &ifrq );
    close( fd );
    if ( rc < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error setting address\n" );
        return -3;
    }

    return 0;
}

/**
 *
 * @retval  0 : no error
 * @retval -1 : bad parameter
 * @retval -2 : couldn't create socket
 * @retval -3 : ioctl failed
 */
int _netsnmp_ioctl_ipaddress_delete_v4( netsnmp_ipaddress_entry* entry )
{
    struct ifreq ifrq;
    int rc, fd = -1;
    _ioctl_extras* extras;

    if ( NULL == entry )
        return -1;

    Assert_assert( 4 == entry->ia_address_len );

    extras = netsnmp_ioctl_ipaddress_extras_get( entry );
    if ( NULL == extras )
        return -1;

    fd = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( fd < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "couldn't create socket\n" );
        return -2;
    }

    memset( &ifrq, 0, sizeof( ifrq ) );

    String_copyTruncate( ifrq.ifr_name, ( char* )extras->name, sizeof( ifrq.ifr_name ) );

    ifrq.ifr_flags = 0;

    rc = ioctl( fd, SIOCSIFFLAGS, &ifrq );
    close( fd );
    if ( rc < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error deleting address\n" );
        return -3;
    }

    return 0;
}

/**
 * Add/remove IPv6 address using ioctl.
 * @retval  0 : no error
 * @retval -1 : bad parameter
 * @retval -2 : couldn't create socket
 * @retval -3 : ioctl failed
 */
int _netsnmp_ioctl_ipaddress_v6( netsnmp_ipaddress_entry* entry, int operation )
{
    /*
     * From linux/ipv6.h. It cannot be included because it collides
     * with netinet/in.h
     */
    struct in6_ifreq {
        struct in6_addr ifr6_addr;
        uint32_t ifr6_prefixlen;
        int ifr6_ifindex;
    };

    struct in6_ifreq ifrq;
    int rc, fd = -1;

    DEBUG_MSGT( ( "access:ipaddress:set", "_netsnmp_ioctl_ipaddress_set_v6 started\n" ) );

    if ( NULL == entry )
        return -1;

    Assert_assert( 16 == entry->ia_address_len );

    fd = socket( AF_INET6, SOCK_DGRAM, 0 );
    if ( fd < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "couldn't create socket\n" );
        return -2;
    }
    memset( &ifrq, 0, sizeof( ifrq ) );
    ifrq.ifr6_ifindex = entry->if_index;
    ifrq.ifr6_prefixlen = 64;

    memcpy( &ifrq.ifr6_addr, entry->ia_address,
        entry->ia_address_len );

    rc = ioctl( fd, operation, &ifrq );
    close( fd );
    if ( rc < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error setting address: %s(%d)\n", strerror( errno ), errno );
        return -3;
    }
    DEBUG_MSGT( ( "access:ipaddress:set", "_netsnmp_ioctl_ipaddress_set_v6 finished\n" ) );
    return 0;
}

/**
 *
 * @retval  0 : no error
 * @retval -1 : bad parameter
 * @retval -2 : couldn't create socket
 * @retval -3 : ioctl failed
 */
int _netsnmp_ioctl_ipaddress_set_v6( netsnmp_ipaddress_entry* entry )
{
    return _netsnmp_ioctl_ipaddress_v6( entry, SIOCSIFADDR );
}

/**
 *
 * @retval  0 : no error
 * @retval -1 : bad parameter
 * @retval -2 : couldn't create socket
 * @retval -3 : ioctl failed
 */
int _netsnmp_ioctl_ipaddress_delete_v6( netsnmp_ipaddress_entry* entry )
{
    return _netsnmp_ioctl_ipaddress_v6( entry, SIOCDIFADDR );
}

/**
 * get the interface count and populate the ifc_buf
 *
 * Note: the caller assumes responsbility for the ifc->ifc_buf
 *       memory, and should free() it when done.
 *
 * @retval -1 : malloc error
 */
int netsnmp_access_ipaddress_ioctl_get_interface_count( int sd, struct ifconf* ifc )
{
    int lastlen = 0, i;
    struct ifconf ifc_tmp;

    if ( NULL == ifc ) {
        memset( &ifc_tmp, 0x0, sizeof( ifc_tmp ) );
        ifc = &ifc_tmp;
    }

    /*
     * Cope with lots of interfaces and brokenness of ioctl SIOCGIFCONF
     * on some platforms; see W. R. Stevens, ``Unix Network Programming
     * Volume I'', p.435.  
     */

    for ( i = 8;; i *= 2 ) {
        ifc->ifc_buf = ( caddr_t )calloc( i, sizeof( struct ifreq ) );
        if ( NULL == ifc->ifc_buf ) {
            Logger_log( LOGGER_PRIORITY_ERR, "could not allocate memory for %d interfaces\n",
                i );
            return -1;
        }
        ifc->ifc_len = i * sizeof( struct ifreq );

        if ( ioctl( sd, SIOCGIFCONF, ( char* )ifc ) < 0 ) {
            if ( errno != EINVAL || lastlen != 0 ) {
                /*
                 * Something has gone genuinely wrong.  
                 */
                Logger_log( LOGGER_PRIORITY_ERR, "bad rc from ioctl, errno %d", errno );
                MEMORY_FREE( ifc->ifc_buf );
                return -1;
            }
            /*
             * Otherwise, it could just be that the buffer is too small.  
             */
        } else {
            if ( ifc->ifc_len == lastlen ) {
                /*
                 * The length is the same as the last time; we're done.  
                 */
                break;
            }
            lastlen = ifc->ifc_len;
        }
        free( ifc->ifc_buf ); /* no MEMORY_FREE, getting ready to reassign */
    }

    if ( ifc == &ifc_tmp )
        free( ifc_tmp.ifc_buf );

    return ifc->ifc_len / sizeof( struct ifreq );
}

/**
 */
static void
_print_flags( short flags )
{
    /** Standard interface flags. */
    struct {
        short flag;
        const char* name;
    } map[] = {
        { IFF_UP, "interface is up" },
        { IFF_BROADCAST, "broadcast address valid" },
        { IFF_DEBUG, "turn on debugging" },
        { IFF_LOOPBACK, "is a loopback net" },
        { IFF_POINTOPOINT, "interface is has p-p link" },
        { IFF_NOTRAILERS, "avoid use of trailers" },
        { IFF_RUNNING, "resources allocated" },
        { IFF_NOARP, "no ARP protocol" },
        { IFF_PROMISC, "receive all packets" },
        { IFF_ALLMULTI, "receive all multicast packets" },
        { IFF_MASTER, "master of a load balancer" },
        { IFF_SLAVE, "slave of a load balancer" },
        { IFF_MULTICAST, "Supports multicast" },
        { IFF_PORTSEL, "can set media type" },
        { IFF_AUTOMEDIA, "auto media select active" },
    };
    short unknown = flags;
    size_t i;

    for ( i = 0; i < sizeof( map ) / sizeof( map[ 0 ] ); ++i )
        if ( flags & map[ i ].flag ) {
            DEBUG_MSGT_NC( ( "access:ipaddress:container", "  %s\n", map[ i ].name ) );
            unknown &= ~map[ i ].flag;
        }

    if ( unknown )
        DEBUG_MSGT_NC( ( "access:ipaddress:container", "  unknown 0x%x\n", unknown ) );
}
