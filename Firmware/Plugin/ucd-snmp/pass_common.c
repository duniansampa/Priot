#include "pass_common.h"
#include "Logger.h"
#include "util_funcs.h"

static int
netsnmp_internal_asc2bin( char* p )
{
    char *r, *q = p;
    char c;
    int n = 0;

    for ( ;; ) {
        c = ( char )strtol( q, &r, 16 );
        if ( r == q )
            break;
        *p++ = c;
        q = r;
        n++;
    }
    return n;
}

static int
netsnmp_internal_bin2asc( char* p, size_t n )
{
    int i, flag = 0;
    char buffer[ TOOLS_MAXBUF ];

    /* prevent buffer overflow */
    if ( ( int )n > ( sizeof( buffer ) - 1 ) )
        n = sizeof( buffer ) - 1;

    for ( i = 0; i < ( int )n; i++ ) {
        buffer[ i ] = p[ i ];
        if ( !isprint( ( unsigned char )( p[ i ] ) ) )
            flag = 1;
    }
    if ( flag == 0 ) {
        p[ n ] = 0;
        return n;
    }
    for ( i = 0; i < ( int )n; i++ ) {
        sprintf( p, "%02x ", ( unsigned char )( buffer[ i ] & 0xff ) );
        p += 3;
    }
    *--p = 0;
    return 3 * n - 1;
}

int netsnmp_internal_pass_str_to_errno( const char* buf )
{
    if ( !strncasecmp( buf, "too-big", 7 ) ) {
        /* Shouldn't happen */
        return PRIOT_ERR_TOOBIG;
    } else if ( !strncasecmp( buf, "no-such-name", 12 ) ) {
        return PRIOT_ERR_NOSUCHNAME;
    } else if ( !strncasecmp( buf, "bad-value", 9 ) ) {
        return PRIOT_ERR_BADVALUE;
    } else if ( !strncasecmp( buf, "read-only", 9 ) ) {
        return PRIOT_ERR_READONLY;
    } else if ( !strncasecmp( buf, "gen-error", 9 ) ) {
        return PRIOT_ERR_GENERR;
    } else if ( !strncasecmp( buf, "no-access", 9 ) ) {
        return PRIOT_ERR_NOACCESS;
    } else if ( !strncasecmp( buf, "wrong-type", 10 ) ) {
        return PRIOT_ERR_WRONGTYPE;
    } else if ( !strncasecmp( buf, "wrong-length", 12 ) ) {
        return PRIOT_ERR_WRONGLENGTH;
    } else if ( !strncasecmp( buf, "wrong-encoding", 14 ) ) {
        return PRIOT_ERR_WRONGENCODING;
    } else if ( !strncasecmp( buf, "wrong-value", 11 ) ) {
        return PRIOT_ERR_WRONGVALUE;
    } else if ( !strncasecmp( buf, "no-creation", 11 ) ) {
        return PRIOT_ERR_NOCREATION;
    } else if ( !strncasecmp( buf, "inconsistent-value", 18 ) ) {
        return PRIOT_ERR_INCONSISTENTVALUE;
    } else if ( !strncasecmp( buf, "resource-unavailable", 20 ) ) {
        return PRIOT_ERR_RESOURCEUNAVAILABLE;
    } else if ( !strncasecmp( buf, "commit-failed", 13 ) ) {
        return PRIOT_ERR_COMMITFAILED;
    } else if ( !strncasecmp( buf, "undo-failed", 11 ) ) {
        return PRIOT_ERR_UNDOFAILED;
    } else if ( !strncasecmp( buf, "authorization-error", 19 ) ) {
        return PRIOT_ERR_AUTHORIZATIONERROR;
    } else if ( !strncasecmp( buf, "not-writable", 12 ) ) {
        return PRIOT_ERR_NOTWRITABLE;
    } else if ( !strncasecmp( buf, "inconsistent-name", 17 ) ) {
        return PRIOT_ERR_INCONSISTENTNAME;
    }

    return PRIOT_ERR_NOERROR;
}

unsigned char*
netsnmp_internal_pass_parse( char* buf,
    char* buf2,
    size_t* var_len,
    struct Variable_s* vp )
{
    static long long_ret;
    static in_addr_t addr_ret;
    int newlen;
    static oid objid[ ASN01_MAX_OID_LEN ];

    /*
     * buf contains the return type, and buf2 contains the data
     */
    if ( !strncasecmp( buf, "string", 6 ) ) {
        buf2[ strlen( buf2 ) - 1 ] = 0; /* zap the linefeed */
        if ( buf2[ strlen( buf2 ) - 1 ] == '\r' )
            buf2[ strlen( buf2 ) - 1 ] = 0; /* zap the carriage-return */
        *var_len = strlen( buf2 );
        vp->type = ASN01_OCTET_STR;
        return ( ( unsigned char* )buf2 );
    } else if ( !strncasecmp( buf, "integer64", 9 ) ) {
        static struct Asn01_Counter64_s c64;
        uint64_t v64 = strtoull( buf2, NULL, 10 );
        c64.high = ( unsigned long )( v64 >> 32 );
        c64.low = ( unsigned long )( v64 & 0xffffffff );
        *var_len = sizeof( c64 );
        vp->type = ASN01_INTEGER64;
        return ( ( unsigned char* )&c64 );
    } else if ( !strncasecmp( buf, "integer", 7 ) ) {
        *var_len = sizeof( long_ret );
        long_ret = strtol( buf2, NULL, 10 );
        vp->type = ASN01_INTEGER;
        return ( ( unsigned char* )&long_ret );
    } else if ( !strncasecmp( buf, "unsigned", 8 ) ) {
        *var_len = sizeof( long_ret );
        long_ret = strtoul( buf2, NULL, 10 );
        vp->type = ASN01_UNSIGNED;
        return ( ( unsigned char* )&long_ret );
    } else if ( !strncasecmp( buf, "counter64", 9 ) ) {
        static struct Asn01_Counter64_s c64;
        uint64_t v64 = strtoull( buf2, NULL, 10 );
        c64.high = ( unsigned long )( v64 >> 32 );
        c64.low = ( unsigned long )( v64 & 0xffffffff );
        *var_len = sizeof( c64 );
        vp->type = ASN01_COUNTER64;
        return ( ( unsigned char* )&c64 );
    } else if ( !strncasecmp( buf, "counter", 7 ) ) {
        *var_len = sizeof( long_ret );
        long_ret = strtoul( buf2, NULL, 10 );
        vp->type = ASN01_COUNTER;
        return ( ( unsigned char* )&long_ret );
    } else if ( !strncasecmp( buf, "octet", 5 ) ) {
        *var_len = netsnmp_internal_asc2bin( buf2 );
        vp->type = ASN01_OCTET_STR;
        return ( ( unsigned char* )buf2 );
    } else if ( !strncasecmp( buf, "opaque", 6 ) ) {
        *var_len = netsnmp_internal_asc2bin( buf2 );
        vp->type = ASN01_OPAQUE;
        return ( ( unsigned char* )buf2 );
    } else if ( !strncasecmp( buf, "gauge", 5 ) ) {
        *var_len = sizeof( long_ret );
        long_ret = strtoul( buf2, NULL, 10 );
        vp->type = ASN01_GAUGE;
        return ( ( unsigned char* )&long_ret );
    } else if ( !strncasecmp( buf, "objectid", 8 ) ) {
        newlen = parse_miboid( buf2, objid );
        *var_len = newlen * sizeof( oid );
        vp->type = ASN01_OBJECT_ID;
        return ( ( unsigned char* )objid );
    } else if ( !strncasecmp( buf, "timetick", 8 ) ) {
        *var_len = sizeof( long_ret );
        long_ret = strtoul( buf2, NULL, 10 );
        vp->type = ASN01_TIMETICKS;
        return ( ( unsigned char* )&long_ret );
    } else if ( !strncasecmp( buf, "ipaddress", 9 ) ) {
        newlen = parse_miboid( buf2, objid );
        if ( newlen != 4 ) {
            Logger_log( LOGGER_PRIORITY_ERR, "invalid ipaddress returned:  %s\n", buf2 );
            *var_len = 0;
            return ( NULL );
        }
        addr_ret = ( objid[ 0 ] << ( 8 * 3 ) ) + ( objid[ 1 ] << ( 8 * 2 ) ) + ( objid[ 2 ] << 8 ) + objid[ 3 ];
        addr_ret = htonl( addr_ret );
        *var_len = sizeof( addr_ret );
        vp->type = ASN01_IPADDRESS;
        return ( ( unsigned char* )&addr_ret );
    }
    *var_len = 0;
    return ( NULL );
}

void netsnmp_internal_pass_set_format( char* buf,
    const u_char* var_val,
    u_char var_val_type,
    size_t var_val_len )
{
    char buf2[ TOOLS_MAXBUF ];
    long tmp;
    unsigned long utmp;

    switch ( var_val_type ) {
    case ASN01_INTEGER:
    case ASN01_COUNTER:
    case ASN01_GAUGE:
    case ASN01_TIMETICKS:
        tmp = *( ( const long* )var_val );
        switch ( var_val_type ) {
        case ASN01_INTEGER:
            sprintf( buf, "integer %d\n", ( int )tmp );
            break;
        case ASN01_COUNTER:
            sprintf( buf, "counter %d\n", ( int )tmp );
            break;
        case ASN01_GAUGE:
            sprintf( buf, "gauge %d\n", ( int )tmp );
            break;
        case ASN01_TIMETICKS:
            sprintf( buf, "timeticks %d\n", ( int )tmp );
            break;
        }
        break;
    case ASN01_IPADDRESS:
        utmp = *( ( const u_long* )var_val );
        utmp = ntohl( utmp );
        sprintf( buf, "ipaddress %d.%d.%d.%d\n",
            ( int )( ( utmp & 0xff000000 ) >> ( 8 * 3 ) ),
            ( int )( ( utmp & 0xff0000 ) >> ( 8 * 2 ) ),
            ( int )( ( utmp & 0xff00 ) >> ( 8 ) ),
            ( int )( ( utmp & 0xff ) ) );
        break;
    case ASN01_OCTET_STR:
        memcpy( buf2, var_val, var_val_len );
        if ( var_val_len == 0 )
            sprintf( buf, "string \"\"\n" );
        else if ( netsnmp_internal_bin2asc( buf2, var_val_len ) == ( int )var_val_len )
            snprintf( buf, TOOLS_MAXBUF, "string \"%s\"\n", buf2 );
        else
            snprintf( buf, TOOLS_MAXBUF, "octet \"%s\"\n", buf2 );
        buf[ TOOLS_MAXBUF - 1 ] = 0;
        break;
    case ASN01_OBJECT_ID:
        sprint_mib_oid( buf2, ( const oid* )var_val, var_val_len / sizeof( oid ) );
        snprintf( buf, TOOLS_MAXBUF, "objectid \"%s\"\n", buf2 );
        buf[ TOOLS_MAXBUF - 1 ] = 0;
        break;
    }
}
