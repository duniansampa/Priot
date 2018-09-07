/*
 *  Template MIB group implementation - at.c
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
#include "at.h"
#include "AgentRegistry.h"
#include "Impl.h"
#include "System/Util/Logger.h"
#include "PluginSettings.h"
#include "VarStruct.h"

#include "siglog/data_access/interface.h"
#include <net/if.h>
#include <net/route.h>
#include <netinet/if_ether.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

/*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

static void ARP_Scan_Init( void );
static int ARP_Scan_Next( in_addr_t*, char*, int*, u_long*, u_short* );

/*********************
	 *
	 *  Public interface functions
	 *
	 *********************/

/*
 * define the structure we're going to ask the agent to register our
 * information at 
 */
struct Variable1_s at_variables[] = {
    { ATIFINDEX, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_atEntry, 1, { 1 } },
    { ATPHYSADDRESS, asnOCTET_STR, IMPL_OLDAPI_RONLY,
        var_atEntry, 1, { 2 } },
    { ATNETADDRESS, asnIPADDRESS, IMPL_OLDAPI_RONLY,
        var_atEntry, 1, { 3 } }
};

/*
 * Define the OID pointer to the top of the mib tree that we're
 * registering underneath 
 */
oid at_variables_oid[] = { PRIOT_OID_MIB2, 3, 1, 1 };

void init_at( void )
{
    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB( "mibII/at", at_variables, Variable1_s, at_variables_oid );
}

/*
 * var_atEntry(...
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
var_atEntry( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
    /*
     * Address Translation table object identifier is of form:
     * 1.3.6.1.2.1.3.1.1.1.interface.1.A.B.C.D,  where A.B.C.D is IP address.
     * Interface is at offset 10,
     * IPADDR starts at offset 12.
     *
     * IP Net to Media table object identifier is of form:
     * 1.3.6.1.2.1.4.22.1.1.1.interface.A.B.C.D,  where A.B.C.D is IP address.
     * Interface is at offset 10,
     * IPADDR starts at offset 11.
     */
    u_char* cp;
    oid* op;
    oid lowest[ 16 ];
    oid current[ 16 ];
    static char PhysAddr[ MAX_MAC_ADDR_LEN ], LowPhysAddr[ MAX_MAC_ADDR_LEN ];
    static int PhysAddrLen, LowPhysAddrLen;
    in_addr_t Addr, LowAddr;
    int foundone;
    static in_addr_t addr_ret;
    u_short ifIndex, lowIfIndex = 0;
    u_long ifType, lowIfType = 0;

    int oid_length;

    /*
     * fill in object part of name for current (less sizeof instance part) 
     */
    memcpy( ( char* )current, ( char* )vp->name,
        ( int )vp->namelen * sizeof( oid ) );

    if ( current[ 6 ] == 3 ) { /* AT group oid */
        oid_length = 16;
    } else { /* IP NetToMedia group oid */
        oid_length = 15;
    }

    LowAddr = 0; /* Don't have one yet */
    foundone = 0;
    ARP_Scan_Init();
    for ( ;; ) {
        if ( ARP_Scan_Next( &Addr, PhysAddr, &PhysAddrLen, &ifType, &ifIndex ) == 0 )
            break;
        current[ 10 ] = ifIndex;

        if ( current[ 6 ] == 3 ) { /* AT group oid */
            current[ 11 ] = 1;
            op = current + 12;
        } else { /* IP NetToMedia group oid */
            op = current + 11;
        }

        cp = ( u_char* )&Addr;
        *op++ = *cp++;
        *op++ = *cp++;
        *op++ = *cp++;
        *op++ = *cp++;

        if ( exact ) {
            if ( Api_oidCompare( current, oid_length, name, *length ) == 0 ) {
                memcpy( ( char* )lowest, ( char* )current,
                    oid_length * sizeof( oid ) );
                LowAddr = Addr;
                foundone = 1;
                lowIfIndex = ifIndex;
                memcpy( LowPhysAddr, PhysAddr, sizeof( PhysAddr ) );
                LowPhysAddrLen = PhysAddrLen;
                lowIfType = ifType;
                break; /* no need to search further */
            }
        } else {
            if ( ( Api_oidCompare( current, oid_length, name, *length ) > 0 )
                && ( ( foundone == 0 )
                       || ( Api_oidCompare( current, oid_length, lowest, oid_length ) < 0 ) ) ) {
                /*
                 * if new one is greater than input and closer to input than
                 * previous lowest, save this one as the "next" one.
                 */
                memcpy( ( char* )lowest, ( char* )current,
                    oid_length * sizeof( oid ) );
                LowAddr = Addr;
                foundone = 1;
                lowIfIndex = ifIndex;
                memcpy( LowPhysAddr, PhysAddr, sizeof( PhysAddr ) );
                LowPhysAddrLen = PhysAddrLen;
                lowIfType = ifType;
            }
        }
    }
    if ( foundone == 0 )
        return ( NULL );

    memcpy( ( char* )name, ( char* )lowest, oid_length * sizeof( oid ) );
    *length = oid_length;
    *write_method = ( WriteMethodFT* )0;
    switch ( vp->magic ) {
    case IPMEDIAIFINDEX: /* also ATIFINDEX */
        *var_len = sizeof vars_longReturn;
        vars_longReturn = lowIfIndex;

        return ( u_char* )&vars_longReturn;
    case IPMEDIAPHYSADDRESS: /* also ATPHYSADDRESS */
        *var_len = LowPhysAddrLen;
        return ( u_char* )LowPhysAddr;
    case IPMEDIANETADDRESS: /* also ATNETADDRESS */
        *var_len = sizeof( addr_ret );
        addr_ret = LowAddr;
        return ( u_char* )&addr_ret;
    case IPMEDIATYPE:
        *var_len = sizeof vars_longReturn;
        vars_longReturn = lowIfType;
        return ( u_char* )&vars_longReturn;
    default:
        DEBUG_MSGTL( ( "snmpd", "unknown sub-id %d in var_atEntry\n",
            vp->magic ) );
    }
    return NULL;
}

/*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

static int arptab_size, arptab_current;

/*
 * at used to be allocated every time we needed to look at the arp cache.
 * This cause us to parse /proc/net/arp twice for each request and didn't
 * allow us to filter things like we'd like to.  So now we use it 
 * semi-statically.  We initialize it to size 0 and if we need more room
 * we realloc room for ARP_CACHE_INCR more entries in the table.
 * We never release what we've taken . . .
 */
#define ARP_CACHE_INCR 1024
static struct Arptab_s* at = NULL;

static void
ARP_Scan_Init( void )
{

    static time_t tm = 0; /* Time of last scan */
    FILE* in;
    int i, j;
    char line[ 128 ];
    int za, zb, zc, zd;
    char ifname[ 21 ];
    char mac[ 3 * MAX_MAC_ADDR_LEN + 1 ];
    char* tok;

    arptab_current = 0; /* Anytime this is called we need to reset 'current' */

    if ( time( NULL ) < tm + 1 ) { /*Our cool one second cache implementation :-) */
        return;
    }

    in = fopen( "/proc/net/arp", "r" );
    if ( !in ) {
        Logger_log( LOGGER_PRIORITY_ERR, "snmpd: Cannot open /proc/net/arp\n" );
        arptab_size = 0;
        return;
    }

    /*
     * Get rid of the header line 
     */
    fgets( line, sizeof( line ), in );

    i = 0;
    while ( fgets( line, sizeof( line ), in ) ) {
        static int arptab_curr_max_size;
        u_long tmp_a;
        unsigned int tmp_flags;

        if ( i >= arptab_curr_max_size ) {
            struct Arptab_s* newtab = ( struct Arptab_s* )
                realloc( at, ( sizeof( struct Arptab_s ) * ( arptab_curr_max_size + ARP_CACHE_INCR ) ) );
            if ( newtab == at ) {
                Logger_log( LOGGER_PRIORITY_ERR,
                    "Error allocating more space for arpcache.  "
                    "Cache will continue to be limited to %d entries",
                    arptab_curr_max_size );
                break;
            } else {
                arptab_curr_max_size += ARP_CACHE_INCR;
                at = newtab;
            }
        }
        if ( 7 != sscanf( line,
                      "%d.%d.%d.%d 0x%*x 0x%x %s %*[^ ] %20s\n",
                      &za, &zb, &zc, &zd, &tmp_flags, mac, ifname ) ) {
            Logger_log( LOGGER_PRIORITY_ERR, "Bad line in /proc/net/arp: %s", line );
            continue;
        }
        /*
         * Invalidated entries have their flag set to 0.
         * * We want to ignore them 
         */
        if ( tmp_flags == 0 ) {
            continue;
        }
        ifname[ sizeof( ifname ) - 1 ] = 0; /* make sure name is null terminated */
        at[ i ].at_flags = tmp_flags;
        tmp_a = ( ( u_long )za << 24 ) | ( ( u_long )zb << 16 ) | ( ( u_long )zc << 8 ) | ( ( u_long )zd );
        at[ i ].at_iaddr.s_addr = htonl( tmp_a );
        at[ i ].if_index = netsnmp_access_interface_index_find( ifname );

        for ( j = 0, tok = strtok( mac, ":" ); tok != NULL; tok = strtok( NULL, ":" ), j++ ) {
            at[ i ].at_enaddr[ j ] = strtol( tok, NULL, 16 );
        }
        at[ i ].at_enaddr_len = j;
        i++;
    }
    arptab_size = i;

    fclose( in );
    time( &tm );
}

static int
ARP_Scan_Next( in_addr_t* IPAddr, char* PhysAddr, int* PhysAddrLen,
    u_long* ifType, u_short* ifIndex )

{

    if ( arptab_current < arptab_size ) {
        /*
         * copy values 
         */
        *IPAddr = at[ arptab_current ].at_iaddr.s_addr;
        *ifType = ( at[ arptab_current ].at_flags & ATF_PERM ) ? 4 /*static */ : 3 /*dynamic */;
        *ifIndex = at[ arptab_current ].if_index;
        memcpy( PhysAddr, &at[ arptab_current ].at_enaddr,
            sizeof( at[ arptab_current ].at_enaddr ) );
        *PhysAddrLen = at[ arptab_current ].at_enaddr_len;

        /*
         * increment to point next entry 
         */
        arptab_current++;
        /*
         * return success 
         */
        return ( 1 );
    }

    return 0; /* we need someone with an irix box to fix this section */
}
