/*
 * Portions of this file are subject to copyright(s).  See the Net-SNMP's
 * COPYING file for more details and other copyrights that may apply.
 */

#include "route_ioctl.h"
#include "System/Util/Assert.h"
#include "System/Util/Trace.h"
#include "ip-forward-mib/inetCidrRouteTable/inetCidrRouteTable_constants.h"
#include "siglog/data_access/ipaddress.h"
#include <arpa/inet.h>
#include <net/route.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#define rt_hash rt_pad1

#define NETSNMP_ROUTE_WRITE_PROTOCOL PF_ROUTE

int _netsnmp_ioctl_route_set_v4( netsnmp_route_entry* entry )
{
    struct sockaddr_in dst, gateway, mask;
    int s, rc;
    struct rtentry route;
    char* DEBUGSTR;

    Assert_assert( NULL != entry ); /* checked in netsnmp_arch_route_create */
    Assert_assert( ( 4 == entry->rt_dest_len ) && ( 4 == entry->rt_nexthop_len ) );

    s = socket( AF_INET, SOCK_RAW, NETSNMP_ROUTE_WRITE_PROTOCOL );
    if ( s < 0 ) {
        Logger_logPerror( "netsnmp_ioctl_route_set_v4: socket" );
        return -3;
    }

    memset( &route, 0, sizeof( route ) );

    dst.sin_family = AF_INET;
    memcpy( &dst.sin_addr.s_addr, entry->rt_dest, 4 );
    DEBUGSTR = inet_ntoa( dst.sin_addr );
    DEBUG_MSGTL( ( "access:route", "add route to %s\n", DEBUGSTR ) );

    gateway.sin_family = AF_INET;
    memcpy( &gateway.sin_addr.s_addr, entry->rt_nexthop, 4 );
    DEBUGSTR = inet_ntoa( gateway.sin_addr );
    DEBUG_MSGTL( ( "access:route", "    via %s\n", DEBUGSTR ) );

    mask.sin_family = AF_INET;
    if ( entry->rt_pfx_len != 0 )
        mask.sin_addr.s_addr = netsnmp_ipaddress_ipv4_mask( entry->rt_pfx_len );
    else
        mask.sin_addr.s_addr = entry->rt_mask;
    DEBUGSTR = inet_ntoa( mask.sin_addr );
    DEBUG_MSGTL( ( "access:route", "    mask %s\n", DEBUGSTR ) );

    memcpy( &route.rt_dst, &dst, sizeof( struct sockaddr_in ) );
    memcpy( &route.rt_gateway, &gateway, sizeof( struct sockaddr_in ) );
    memcpy( &route.rt_genmask, &mask, sizeof( struct sockaddr_in ) );

    if ( 32 == entry->rt_pfx_len )
        route.rt_flags |= RTF_HOST;
    if ( INETCIDRROUTETYPE_REMOTE == entry->rt_type )
        route.rt_flags |= RTF_GATEWAY;
    route.rt_flags |= RTF_UP;

    route.rt_hash = entry->if_index;

    rc = ioctl( s, SIOCADDRT, ( caddr_t )&route );
    if ( rc < 0 ) {
        Logger_logPerror( "netsnmp_ioctl_route_set_v4: ioctl" );
        rc = -4;
    }
    close( s );

    return rc;
}

int _netsnmp_ioctl_route_delete_v4( netsnmp_route_entry* entry )
{
    struct sockaddr_in dst, mask;
    struct sockaddr_in gateway;
    int s, rc;
    struct rtentry route;
    char* DEBUGSTR;

    Assert_assert( NULL != entry ); /* checked in netsnmp_arch_route_delete */
    Assert_assert( ( 4 == entry->rt_dest_len ) && ( 4 == entry->rt_nexthop_len ) );

    s = socket( AF_INET, SOCK_RAW, NETSNMP_ROUTE_WRITE_PROTOCOL );
    if ( s < 0 ) {
        Logger_logPerror( "netsnmp_ioctl_route_delete_v4: socket" );
        return -3;
    }

    memset( &route, 0, sizeof( route ) );

    dst.sin_family = AF_INET;
    memcpy( &dst.sin_addr.s_addr, entry->rt_dest, 4 );
    DEBUGSTR = inet_ntoa( dst.sin_addr );
    DEBUG_MSGTL( ( "access:route", "del route to %s\n", DEBUGSTR ) );

    mask.sin_family = AF_INET;
    if ( entry->rt_pfx_len != 0 )
        mask.sin_addr.s_addr = netsnmp_ipaddress_ipv4_mask( entry->rt_pfx_len );
    else
        mask.sin_addr.s_addr = entry->rt_mask;

    gateway.sin_family = AF_INET;
    memcpy( &gateway.sin_addr.s_addr, entry->rt_nexthop, 4 );

    memcpy( &route.rt_dst, &dst, sizeof( struct sockaddr_in ) );
    memcpy( &route.rt_genmask, &mask, sizeof( struct sockaddr_in ) );
    memcpy( &route.rt_gateway, &gateway, sizeof( struct sockaddr_in ) );

    if ( 32 == entry->rt_pfx_len )
        route.rt_flags |= RTF_HOST;
    if ( INETCIDRROUTETYPE_REMOTE == entry->rt_type )
        route.rt_flags |= RTF_GATEWAY;
    route.rt_flags |= RTF_UP;

    route.rt_hash = entry->if_index;

    rc = ioctl( s, SIOCDELRT, ( caddr_t )&route );
    if ( rc < 0 ) {
        Logger_logPerror( "netsnmp_ioctl_route_delete_v4: ioctl" );
        rc = -4;
    }
    close( s );

    return rc;
}
