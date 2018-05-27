/*
 * snmp_var_route.c - return a pointer to the named variable.
 *
 *
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/***********************************************************
	Copyright 1988, 1989 by Carnegie Mellon University
	Copyright 1989	TGV, Incorporated

		      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of CMU and TGV not be used
in advertising or publicity pertaining to distribution of the software
without specific, written prior permission.

CMU AND TGV DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
EVENT SHALL CMU OR TGV BE LIABLE FOR ANY SPECIAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
******************************************************************/
/*
 * Portions of this file are copyrighted by:
 * Copyright � 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

/*
 * additions, fixes and enhancements for Linux by Erik Schoenfelder
 * (schoenfr@ibr.cs.tu-bs.de) 1994/1995.
 * Linux additions taken from CMU to UCD stack by Jennifer Bray of Origin
 * (jbray@origin-at.co.uk) 1997
 * Support for sysctl({CTL_NET,PF_ROUTE,...) by Simon Leinen
 * (simon@switch.ch) 1997
 */

#define CACHE_TIME ( 120 ) /* Seconds */

#include "var_route.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "PluginSettings.h"
#include "System/String.h"
#include "interfaces.h"
#include "ip.h"
#include "route_headers.h"
#include "siglog/agent/auto_nlist.h"
#include "siglog/data_access/interface.h"
#include "struct.h"
#include <net/route.h>

extern WriteMethodFT write_rte;

static struct rtentry** rthead = NULL;
static int rtsize = 0, rtallocate = 0;

static void Route_Scan_Reload( void );

struct rtentry** netsnmp_get_routes( size_t* size )
{
    Route_Scan_Reload();
    if ( size )
        *size = rtsize;
    return rthead;
}

void init_var_route( void )
{
    auto_nlist( RTTABLES_SYMBOL, 0, 0 );
    auto_nlist( RTHASHSIZE_SYMBOL, 0, 0 );
    auto_nlist( RTHOST_SYMBOL, 0, 0 );
    auto_nlist( RTNET_SYMBOL, 0, 0 );
}

u_char*
var_ipRouteEntry( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
    /*
     * object identifier is of form:
     * 1.3.6.1.2.1.4.21.1.1.A.B.C.D,  where A.B.C.D is IP address.
     * IPADDR starts at offset 10.
     */
    register int Save_Valid, result, RtIndex;
    static size_t saveNameLen = 0;
    static int saveExact = 0, saveRtIndex = 0;
    static oid saveName[ ASN01_MAX_OID_LEN ], Current[ ASN01_MAX_OID_LEN ];
    u_char* cp;
    oid* op;
    static in_addr_t addr_ret;

    *write_method = NULL; /* write_rte;  XXX:  SET support not really implemented */

    /**
     ** this optimisation fails, if there is only a single route avail.
     ** it is a very special case, but better leave it out ...
     **/
    saveNameLen = 0;
    if ( rtsize <= 1 )
        Save_Valid = 0;
    else
        /*
         *  OPTIMIZATION:
         *
         *  If the name was the same as the last name, with the possible
         *  exception of the [9]th token, then don't read the routing table
         *
         */

        if ( ( saveNameLen == *length ) && ( saveExact == exact ) ) {
        register int temp = name[ 9 ];
        name[ 9 ] = 0;
        Save_Valid = ( Api_oidCompare( name, *length, saveName, saveNameLen ) == 0 );
        name[ 9 ] = temp;
    } else
        Save_Valid = 0;

    if ( Save_Valid ) {
        register int temp = name[ 9 ]; /* Fix up 'lowest' found entry */
        memcpy( ( char* )name, ( char* )Current, 14 * sizeof( oid ) );
        name[ 9 ] = temp;
        *length = 14;
        RtIndex = saveRtIndex;
    } else {
        /*
         * fill in object part of name for current (less sizeof instance part) 
         */

        memcpy( ( char* )Current, ( char* )vp->name,
            ( int )( vp->namelen ) * sizeof( oid ) );

        Route_Scan_Reload();

        for ( RtIndex = 0; RtIndex < rtsize; RtIndex++ ) {

            cp = ( u_char* )&( ( ( struct sockaddr_in* )&( rthead[ RtIndex ]->rt_dst ) )->sin_addr.s_addr );
            op = Current + 10;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;
            *op++ = *cp++;

            result = Api_oidCompare( name, *length, Current, 14 );
            if ( ( exact && ( result == 0 ) ) || ( !exact && ( result < 0 ) ) )
                break;
        }
        if ( RtIndex >= rtsize )
            return ( NULL );
        /*
         *  Save in the 'cache'
         */
        memcpy( ( char* )saveName, ( char* )name,
            UTILITIES_MIN_VALUE( *length, ASN01_MAX_OID_LEN ) * sizeof( oid ) );
        saveName[ 9 ] = 0;
        saveNameLen = *length;
        saveExact = exact;
        saveRtIndex = RtIndex;
        /*
         *  Return the name
         */
        memcpy( ( char* )name, ( char* )Current, 14 * sizeof( oid ) );
        *length = 14;
    }

    *var_len = sizeof( vars_longReturn );

    switch ( vp->magic ) {
    case IPROUTEDEST:
        *var_len = sizeof( addr_ret );
        return ( u_char* )&( ( struct sockaddr_in* )&rthead[ RtIndex ]->rt_dst )->sin_addr.s_addr;
    case IPROUTEIFINDEX:
        vars_longReturn = ( u_long )rthead[ RtIndex ]->rt_unit;
        return ( u_char* )&vars_longReturn;
    case IPROUTEMETRIC1:
        vars_longReturn = ( rthead[ RtIndex ]->rt_flags & RTF_GATEWAY ) ? 1 : 0;
        return ( u_char* )&vars_longReturn;
    case IPROUTEMETRIC2:
        return NULL;
        vars_longReturn = -1;
        return ( u_char* )&vars_longReturn;
    case IPROUTEMETRIC3:

        return NULL;
        vars_longReturn = -1;
        return ( u_char* )&vars_longReturn;
    case IPROUTEMETRIC4:

        return NULL;
        vars_longReturn = -1;
        return ( u_char* )&vars_longReturn;
    case IPROUTEMETRIC5:
        return NULL;
        vars_longReturn = -1;
        return ( u_char* )&vars_longReturn;
    case IPROUTENEXTHOP:
        *var_len = sizeof( addr_ret );

        return ( u_char* )&( ( struct sockaddr_in* )&rthead[ RtIndex ]->rt_gateway )->sin_addr.s_addr;
    case IPROUTETYPE:
        if ( rthead[ RtIndex ]->rt_flags & RTF_UP ) {
            if ( rthead[ RtIndex ]->rt_flags & RTF_GATEWAY ) {
                vars_longReturn = 4; /*  indirect(4)  */
            } else {
                vars_longReturn = 3; /*  direct(3)  */
            }
        } else {
            vars_longReturn = 2; /*  invalid(2)  */
        }
        return ( u_char* )&vars_longReturn;
    case IPROUTEPROTO:

        vars_longReturn = ( rthead[ RtIndex ]->rt_flags & RTF_DYNAMIC ) ? 4 : 2;
        return ( u_char* )&vars_longReturn;
    case IPROUTEAGE:

        return NULL;
        vars_longReturn = 0;
        return ( u_char* )&vars_longReturn;
    case IPROUTEMASK:
        *var_len = sizeof( addr_ret );

        if ( ( ( struct sockaddr_in* )&rthead[ RtIndex ]->rt_dst )->sin_addr.s_addr == 0 )
            addr_ret = 0; /* Default route */
        else {

            cp = ( u_char* )&( ( ( struct sockaddr_in* )&( rthead[ RtIndex ]->rt_dst ) )->sin_addr.s_addr );
            return ( u_char* )&( ( ( struct sockaddr_in* )&( rthead[ RtIndex ]->rt_genmask ) )->sin_addr.s_addr );
        }
        return ( u_char* )&addr_ret;
    case IPROUTEINFO:
        *var_len = vars_nullOidLen;
        return ( u_char* )vars_nullOid;
    default:
        DEBUG_MSGTL( ( "snmpd", "unknown sub-id %d in var_ipRouteEntry\n",
            vp->magic ) );
    }
    return NULL;
}

static int qsort_compare( const void*, const void* );

static void
Route_Scan_Reload( void )
{
    FILE* in;
    char line[ 256 ];
    struct rtentry* rt;
    char name[ 16 ];
    static time_t Time_Of_Last_Reload;
    struct timeval now;

    Time_getMonotonicClock( &now );
    if ( Time_Of_Last_Reload + CACHE_TIME > now.tv_sec )
        return;
    Time_Of_Last_Reload = now.tv_sec;

    /*
     *  Makes sure we have SOME space allocated for new routing entries
     */
    if ( !rthead ) {
        rthead = ( struct rtentry** )calloc( 100, sizeof( struct rtentry* ) );
        if ( !rthead ) {
            Logger_log( LOGGER_PRIORITY_ERR, "route table malloc fail\n" );
            return;
        }
        rtallocate = 100;
    }

    /*
     * fetch routes from the proc file-system:
     */

    rtsize = 0;

    if ( !( in = fopen( "/proc/net/route", "r" ) ) ) {
        LOGGER_LOGONCE( ( LOGGER_PRIORITY_ERR, "cannot open /proc/net/route - burps\n" ) );
        return;
    }

    while ( fgets( line, sizeof( line ), in ) ) {
        struct rtentry rtent;
        char rtent_name[ 32 ];
        int refcnt, metric;
        unsigned flags, use;

        rt = &rtent;
        memset( ( char* )rt, ( 0 ), sizeof( *rt ) );
        rt->rt_dev = rtent_name;

        /*
         * as with 1.99.14:
         * Iface Dest GW Flags RefCnt Use Metric Mask MTU Win IRTT
         * eth0 0A0A0A0A 00000000 05 0 0 0 FFFFFFFF 1500 0 0 
         */
        if ( 8 != sscanf( line, "%s %x %x %x %d %u %d %x %*d %*d %*d\n",
                      rt->rt_dev,
                      &( ( ( struct sockaddr_in* )&( rtent.rt_dst ) )->sin_addr.s_addr ),
                      &( ( ( struct sockaddr_in* )&( rtent.rt_gateway ) )->sin_addr.s_addr ),
                      /*
                         * XXX: fix type of the args 
                         */
                      &flags, &refcnt, &use, &metric,
                      &( ( ( struct sockaddr_in* )&( rtent.rt_genmask ) )->sin_addr.s_addr ) ) )
            continue;

        String_copyTruncate( name, rt->rt_dev, sizeof( name ) );

        rt->rt_flags = flags, rt->rt_refcnt = refcnt;
        rt->rt_use = use, rt->rt_metric = metric;

        rt->rt_unit = netsnmp_access_interface_index_find( name );

        /*
         *  Allocate a block to hold it and add it to the database
         */
        if ( rtsize >= rtallocate ) {
            rthead = ( struct rtentry** )realloc( ( char* )rthead,
                2 * rtallocate * sizeof( struct rtentry* ) );
            memset( &rthead[ rtallocate ], 0,
                rtallocate * sizeof( struct rtentry* ) );
            rtallocate *= 2;
        }
        if ( !rthead[ rtsize ] )
            rthead[ rtsize ] = ( struct rtentry* )malloc( sizeof( struct rtentry ) );
        /*
         *  Add this to the database
         */
        memcpy( ( char* )rthead[ rtsize ], ( char* )rt,
            sizeof( struct rtentry ) );
        rtsize++;
    }

    fclose( in );

    /*
     *  Sort it!
     */
    qsort( ( char* )rthead, rtsize, sizeof( rthead[ 0 ] ), qsort_compare );
}

/*
 *      Create a host table
 */

static int
qsort_compare( const void* v1, const void* v2 )
{
    struct rtentry* const* r1 = ( struct rtentry * const* )v1;
    struct rtentry* const* r2 = ( struct rtentry * const* )v2;

    register u_long dst1 = ntohl( ( ( const struct sockaddr_in* )&( ( *r1 )->rt_dst ) )->sin_addr.s_addr );
    register u_long dst2 = ntohl( ( ( const struct sockaddr_in* )&( ( *r2 )->rt_dst ) )->sin_addr.s_addr );

    /*
     *      Do the comparison
     */
    if ( dst1 == dst2 )
        return ( 0 );
    if ( dst1 > dst2 )
        return ( 1 );
    return ( -1 );
}
