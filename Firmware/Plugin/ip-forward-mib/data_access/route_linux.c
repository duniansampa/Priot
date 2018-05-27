/*
 *  Interface MIB architecture support
 *
 * $Id$
 */

#include "siglog/data_access/route.h"
#include "System/Util/Assert.h"
#include "System/Containers/Container.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "System/String.h"
#include "if-mib/data_access/interface_ioctl.h"
#include "ip-forward-mib/data_access/route_ioctl.h"
#include "ip-forward-mib/inetCidrRouteTable/inetCidrRouteTable_constants.h"
#include "siglog/data_access/ipaddress.h"
#include <net/route.h>

static int
_type_from_flags( unsigned int flags )
{
    /*
     *  RTF_GATEWAY RTF_UP RTF_DYNAMIC RTF_CACHE
     *  RTF_MODIFIED RTF_EXPIRES RTF_NONEXTHOP
     *  RTF_DYNAMIC RTF_LOCAL RTF_PREFIX_RT
     *
     * xxx: can we distinguish between reject & blackhole?
     */
    if ( flags & RTF_UP ) {
        if ( flags & RTF_GATEWAY )
            return INETCIDRROUTETYPE_REMOTE;
        else /*if (flags & RTF_LOCAL) */
            return INETCIDRROUTETYPE_LOCAL;
    } else
        return 0; /* route not up */
}
static int
_load_ipv4( Container_Container* container, u_long* index )
{
    FILE* in;
    char line[ 256 ];
    netsnmp_route_entry* entry = NULL;
    char name[ 16 ];
    int fd;

    DEBUG_MSGTL( ( "access:route:container",
        "route_container_arch_load ipv4\n" ) );

    Assert_assert( NULL != container );

    /*
     * fetch routes from the proc file-system:
     */
    if ( !( in = fopen( "/proc/net/route", "r" ) ) ) {
        LOGGER_LOGONCE( ( LOGGER_PRIORITY_ERR, "cannot open /proc/net/route\n" ) );
        return -2;
    }

    /*
     * create socket for ioctls (see NOTE[1], below)
     */
    fd = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( fd < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "could not create socket\n" );
        fclose( in );
        return -2;
    }

    fgets( line, sizeof( line ), in ); /* skip header */

    while ( fgets( line, sizeof( line ), in ) ) {
        char rtent_name[ 32 ];
        int refcnt, rc;
        uint32_t dest, nexthop, mask;
        unsigned flags, use;

        entry = netsnmp_access_route_entry_create();

        /*
         * as with 1.99.14:
         *    Iface Dest     GW       Flags RefCnt Use Met Mask     MTU  Win IRTT
         * BE eth0  00000000 C0A80101 0003  0      0   0   FFFFFFFF 1500 0   0 
         * LE eth0  00000000 0101A8C0 0003  0      0   0   00FFFFFF    0 0   0  
         */
        rc = sscanf( line, "%s %x %x %x %d %u %d %x %*d %*d %*d\n",
            rtent_name, &dest, &nexthop,
            /*
                     * XXX: fix type of the args 
                     */
            &flags, &refcnt, &use, &entry->rt_metric1,
            &mask );
        DEBUG_MSGTL( ( "9:access:route:container", "line |%s|\n", line ) );
        if ( 8 != rc ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "/proc/net/route data format error (%d!=8), line ==|%s|",
                rc, line );

            netsnmp_access_route_entry_free( entry );
            continue;
        }

        /*
         * temporary null terminated name
         */
        String_copyTruncate( name, rtent_name, sizeof( name ) );

        /*
         * don't bother to try and get the ifindex for routes with
         * no interface name.
         * NOTE[1]: normally we'd use netsnmp_access_interface_index_find,
         * but since that will open/close a socket, and we might
         * have a lot of routes, call the ioctl routine directly.
         */
        if ( '*' != name[ 0 ] )
            entry->if_index = netsnmp_access_interface_ioctl_ifindex_get( fd, name );

        /*
         * arbitrary index
         */
        entry->ns_rt_index = ++( *index );

        memcpy( &entry->rt_mask, &mask, 4 );
        /** entry->rt_tos = XXX; */
        /** rt info ?? */
        /*
         * copy dest & next hop
         */
        entry->rt_dest_type = INETADDRESSTYPE_IPV4;
        entry->rt_dest_len = 4;
        memcpy( entry->rt_dest, &dest, 4 );

        entry->rt_nexthop_type = INETADDRESSTYPE_IPV4;
        entry->rt_nexthop_len = 4;
        memcpy( entry->rt_nexthop, &nexthop, 4 );

        /*
         * count bits in mask
         */
        entry->rt_pfx_len = netsnmp_ipaddress_ipv4_prefix_len( mask );

        /*
    inetCidrRoutePolicy OBJECT-TYPE 
        SYNTAX     OBJECT IDENTIFIER 
        MAX-ACCESS not-accessible 
        STATUS     current 
        DESCRIPTION 
               "This object is an opaque object without any defined 
                semantics.  Its purpose is to serve as an additional 
                index which may delineate between multiple entries to 
                the same destination.  The value { 0 0 } shall be used 
                as the default value for this object."
        */
        /*
         * on linux, default routes all look alike, and would have the same
         * indexed based on dest and next hop. So we use the if index
         * as the policy, to distinguise between them. Hopefully this is
         * unique.
         * xxx-rks: It should really only be for the duplicate case, but that
         *     would be more complicated than I want to get into now. Fix later.
         */
        if ( 0 == nexthop ) {
            entry->rt_policy = ( oid* )calloc( 3, sizeof( oid ) );
            entry->rt_policy[ 2 ] = entry->if_index;
            entry->rt_policy_len = sizeof( oid ) * 3;
        }

        /*
         * get protocol and type from flags
         */
        entry->rt_type = _type_from_flags( flags );

        entry->rt_proto = ( flags & RTF_DYNAMIC )
            ? IANAIPROUTEPROTOCOL_ICMP
            : IANAIPROUTEPROTOCOL_LOCAL;

        /*
         * insert into container
         */
        if ( CONTAINER_INSERT( container, entry ) < 0 ) {
            DEBUG_MSGTL( ( "access:route:container", "error with route_entry: insert into container failed.\n" ) );
            netsnmp_access_route_entry_free( entry );
            continue;
        }
    }

    fclose( in );
    close( fd );
    return 0;
}

/** arch specific load
 * @internal
 *
 * @retval  0 success
 * @retval -1 no container specified
 * @retval -2 could not open data file
 */
int netsnmp_access_route_container_arch_load( Container_Container* container,
    u_int load_flags )
{
    u_long count = 0;
    int rc;

    DEBUG_MSGTL( ( "access:route:container",
        "route_container_arch_load (flags %x)\n", load_flags ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "no container specified/found for access_route\n" );
        return -1;
    }

    rc = _load_ipv4( container, &count );

    return rc;
}

/*
 * create a new entry
 */
int netsnmp_arch_route_create( netsnmp_route_entry* entry )
{
    if ( NULL == entry )
        return -1;

    if ( 4 != entry->rt_dest_len ) {
        DEBUG_MSGT( ( "access:route:create", "only ipv4 supported\n" ) );
        return -2;
    }

    return _netsnmp_ioctl_route_set_v4( entry );
}

/*
 * create a new entry
 */
int netsnmp_arch_route_delete( netsnmp_route_entry* entry )
{
    if ( NULL == entry )
        return -1;

    if ( 4 != entry->rt_dest_len ) {
        DEBUG_MSGT( ( "access:route:create", "only ipv4 supported\n" ) );
        return -2;
    }

    return _netsnmp_ioctl_route_delete_v4( entry );
}
