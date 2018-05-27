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

#include "route_write.h"
#include "System/Util/Assert.h"
#include "System/Util/Trace.h"
#include "Impl.h"
#include "System/Util/Logger.h"
#include "ip.h"
#include <net/route.h>
#include <sys/ioctl.h>

#define rt_hash rt_pad1

#define NETSNMP_ROUTE_WRITE_PROTOCOL PF_ROUTE

int addRoute( u_long dstip, u_long gwip, u_long iff, u_short flags )
{
    struct sockaddr_in dst;
    struct sockaddr_in gateway;
    int s, rc;
    struct rtentry route;

    s = socket( AF_INET, SOCK_RAW, NETSNMP_ROUTE_WRITE_PROTOCOL );
    if ( s < 0 ) {
        Logger_logPerror( "socket" );
        return -1;
    }

    flags |= RTF_UP;

    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl( dstip );

    gateway.sin_family = AF_INET;
    gateway.sin_addr.s_addr = htonl( gwip );

    memcpy( &route.rt_dst, &dst, sizeof( struct sockaddr_in ) );
    memcpy( &route.rt_gateway, &gateway, sizeof( struct sockaddr_in ) );

    route.rt_flags = flags;
    route.rt_hash = iff;

    rc = ioctl( s, SIOCADDRT, ( caddr_t )&route );
    close( s );
    if ( rc < 0 )
        Logger_logPerror( "ioctl" );
    return rc;
}

int delRoute( u_long dstip, u_long gwip, u_long iff, u_short flags )
{
    struct sockaddr_in dst;
    struct sockaddr_in gateway;
    int s, rc;
    struct rtentry route;

    s = socket( AF_INET, SOCK_RAW, NETSNMP_ROUTE_WRITE_PROTOCOL );
    if ( s < 0 ) {
        Logger_logPerror( "socket" );
        return 0;
    }

    flags |= RTF_UP;

    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = htonl( dstip );

    gateway.sin_family = AF_INET;
    gateway.sin_addr.s_addr = htonl( gwip );

    memcpy( &route.rt_dst, &dst, sizeof( struct sockaddr_in ) );
    memcpy( &route.rt_gateway, &gateway, sizeof( struct sockaddr_in ) );

    route.rt_flags = flags;
    route.rt_hash = iff;

    rc = ioctl( s, SIOCDELRT, ( caddr_t )&route );
    close( s );
    return rc;
}

#define MAX_CACHE 8

struct rtent {

    u_long in_use;
    u_long old_dst;
    u_long old_nextIR;
    u_long old_ifix;
    u_long old_flags;

    u_long rt_dst; /*  main entries    */
    u_long rt_ifix;
    u_long rt_metric1;
    u_long rt_nextIR;
    u_long rt_type;
    u_long rt_proto;

    u_long xx_dst; /*  shadow entries  */
    u_long xx_ifix;
    u_long xx_metric1;
    u_long xx_nextIR;
    u_long xx_type;
    u_long xx_proto;
};

struct rtent rtcache[ MAX_CACHE ];

struct rtent*
findCacheRTE( u_long dst )
{
    int i;

    for ( i = 0; i < MAX_CACHE; i++ ) {

        if ( rtcache[ i ].in_use && ( rtcache[ i ].rt_dst == dst ) ) { /* valid & match? */
            return ( &rtcache[ i ] );
        }
    }
    return NULL;
}

struct rtent*
newCacheRTE( void )
{

    int i;

    for ( i = 0; i < MAX_CACHE; i++ ) {

        if ( !rtcache[ i ].in_use ) {
            rtcache[ i ].in_use = 1;
            return ( &rtcache[ i ] );
        }
    }
    return NULL;
}

int delCacheRTE( u_long dst )
{
    struct rtent* rt;

    rt = findCacheRTE( dst );
    if ( !rt ) {
        return 0;
    }

    rt->in_use = 0;
    return 1;
}

struct rtent*
cacheKernelRTE( u_long dst )
{
    return NULL; /* for now */
    /*
     * ...... 
     */
}

/*
 * If statP is non-NULL, the referenced object is at that location.
 * If statP is NULL and ap is non-NULL, the instance exists, but not this variable.
 * If statP is NULL and ap is NULL, then neither this instance nor the variable exists.
 */

int write_rte( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len, u_char* statP, oid* name, size_t length )
{
    struct rtent* rp;
    int var;
    long val;
    u_long dst;
    char buf[ 8 ];
    u_short flags;
    int oldty;

    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.21.1.X.A.B.C.D ,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */

    if ( length != 14 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "length error\n" );
        return PRIOT_ERR_NOCREATION;
    }

    var = name[ 9 ];

    dst = *( ( u_long* )&name[ 10 ] );

    rp = findCacheRTE( dst );

    if ( !rp ) {
        rp = cacheKernelRTE( dst );
    }

    if ( action == IMPL_RESERVE1 && !rp ) {

        rp = newCacheRTE();
        if ( !rp ) {
            Logger_log( LOGGER_PRIORITY_ERR, "newCacheRTE" );
            return PRIOT_ERR_RESOURCEUNAVAILABLE;
        }
        rp->rt_dst = dst;
        rp->rt_type = rp->xx_type = 2;

    } else if ( action == IMPL_COMMIT ) {

    } else if ( action == IMPL_FREE ) {
        if ( rp && rp->rt_type == 2 ) { /* was invalid before */
            delCacheRTE( dst );
        }
    }

    Assert_assert( NULL != rp ); /* should have found or created rp */

    switch ( var ) {

    case IPROUTEDEST:

        if ( action == IMPL_RESERVE1 ) {

            if ( var_val_type != ASN01_IPADDRESS ) {
                Logger_log( LOGGER_PRIORITY_ERR, "not IP address" );
                return PRIOT_ERR_WRONGTYPE;
            }

            memcpy( buf, var_val, ( var_val_len > 8 ) ? 8 : var_val_len );

            rp->xx_dst = *( ( u_long* )buf );

        } else if ( action == IMPL_COMMIT ) {
            rp->rt_dst = rp->xx_dst;
        }
        break;

    case IPROUTEMETRIC1:

        if ( action == IMPL_RESERVE1 ) {
            if ( var_val_type != ASN01_INTEGER ) {
                Logger_log( LOGGER_PRIORITY_ERR, "not int1" );
                return PRIOT_ERR_WRONGTYPE;
            }

            val = *( ( long* )var_val );

            if ( val < -1 ) {
                Logger_log( LOGGER_PRIORITY_ERR, "not right1" );
                return PRIOT_ERR_WRONGVALUE;
            }

            rp->xx_metric1 = val;

        } else if ( action == IMPL_RESERVE2 ) {

            if ( ( rp->xx_metric1 == 1 ) && ( rp->xx_type != 4 ) ) {
                Logger_log( LOGGER_PRIORITY_ERR, "reserve2 failed\n" );
                return PRIOT_ERR_WRONGVALUE;
            }

        } else if ( action == IMPL_COMMIT ) {
            rp->rt_metric1 = rp->xx_metric1;
        }
        break;

    case IPROUTEIFINDEX:

        if ( action == IMPL_RESERVE1 ) {
            if ( var_val_type != ASN01_INTEGER ) {
                Logger_log( LOGGER_PRIORITY_ERR, "not right2" );
                return PRIOT_ERR_WRONGTYPE;
            }

            val = *( ( long* )var_val );

            if ( val <= 0 ) {
                Logger_log( LOGGER_PRIORITY_ERR, "not right3" );
                return PRIOT_ERR_WRONGVALUE;
            }

            rp->xx_ifix = val;

        } else if ( action == IMPL_COMMIT ) {
            rp->rt_ifix = rp->xx_ifix;
        }
        break;

    case IPROUTENEXTHOP:

        if ( action == IMPL_RESERVE1 ) {

            if ( var_val_type != ASN01_IPADDRESS ) {
                Logger_log( LOGGER_PRIORITY_ERR, "not right4" );
                return PRIOT_ERR_WRONGTYPE;
            }

            memcpy( buf, var_val, ( var_val_len > 8 ) ? 8 : var_val_len );

            rp->xx_nextIR = *( ( u_long* )buf );

        } else if ( action == IMPL_COMMIT ) {
            rp->rt_nextIR = rp->xx_nextIR;
        }
        break;

    case IPROUTETYPE:

        /*
         *  flag meaning:
         *
         *  IPROUTEPROTO (rt_proto): none: (cant set == 3 (netmgmt)) 
         *
         *  IPROUTEMETRIC1:  1 iff gateway, 0 otherwise
         *  IPROUTETYPE:     4 iff gateway, 3 otherwise
         */

        if ( action == IMPL_RESERVE1 ) {
            if ( var_val_type != ASN01_INTEGER ) {
                return PRIOT_ERR_WRONGTYPE;
            }

            val = *( ( long* )var_val );

            if ( ( val < 2 ) || ( val > 4 ) ) { /* only accept invalid, direct, indirect */
                Logger_log( LOGGER_PRIORITY_ERR, "not right6" );
                return PRIOT_ERR_WRONGVALUE;
            }

            rp->xx_type = val;

        } else if ( action == IMPL_COMMIT ) {

            oldty = rp->rt_type;
            rp->rt_type = rp->xx_type;

            if ( rp->rt_type == 2 ) { /* invalid, so delete from kernel */

                if ( delRoute( rp->rt_dst, rp->rt_nextIR, rp->rt_ifix,
                         rp->old_flags )
                    < 0 ) {
                    Logger_logPerror( "delRoute" );
                }

            } else {

                /*
                 * it must be valid now, so flush to kernel 
                 */

                if ( oldty != 2 ) { /* was the old entry valid ?  */
                    if ( delRoute( rp->old_dst, rp->old_nextIR, rp->old_ifix,
                             rp->old_flags )
                        < 0 ) {
                        Logger_logPerror( "delRoute" );
                    }
                }

                /*
                 * not invalid, so remove from cache 
                 */

                flags = ( rp->rt_type == 4 ? RTF_GATEWAY : 0 );

                if ( addRoute( rp->rt_dst, rp->rt_nextIR, rp->rt_ifix, flags )
                    < 0 ) {
                    Logger_logPerror( "addRoute" );
                }

                delCacheRTE( rp->rt_type );
            }
        }
        break;

    case IPROUTEPROTO:

    default:
        DEBUG_MSGTL( ( "snmpd", "unknown sub-id %d in write_rte\n", var ) );
        return PRIOT_ERR_NOCREATION;
    }

    return PRIOT_ERR_NOERROR;
}
