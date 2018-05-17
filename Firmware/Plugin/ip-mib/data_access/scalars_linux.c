/*
 *  Arp MIB architecture support
 *
 * $Id$
 */

#include "System/Util/Debug.h"
#include "Instance.h"
#include "Priot.h"

static const char ipfw_name[]
    = "/proc/sys/net/ipv4/conf/all/forwarding";
static const char ipttl_name[] = "/proc/sys/net/ipv4/ip_default_ttl";
static const char ipfw6_name[] = "/proc/sys/net/ipv6/conf/all/forwarding";
static const char iphop6_name[] = "/proc/sys/net/ipv6/conf/all/hop_limit";

int netsnmp_arch_ip_scalars_ipForwarding_get( u_long* value )
{
    FILE* filep;
    int rc;

    if ( NULL == value )
        return -1;

    filep = fopen( ipfw_name, "r" );
    if ( NULL == filep ) {
        DEBUG_MSGTL( ( "access:ipForwarding", "could not open %s\n",
            ipfw_name ) );
        return -2;
    }

    rc = fscanf( filep, "%lu", value );
    fclose( filep );
    if ( 1 != rc ) {
        DEBUG_MSGTL( ( "access:ipForwarding", "could not read %s\n",
            ipfw_name ) );
        return -3;
    }

    if ( ( 0 != *value ) && ( 1 != *value ) ) {
        DEBUG_MSGTL( ( "access:ipForwarding", "unexpected value %ld in %s\n",
            *value, ipfw_name ) );
        return -4;
    }

    return 0;
}

int netsnmp_arch_ip_scalars_ipForwarding_set( u_long value )
{
    FILE* filep;
    int rc;

    if ( 1 == value )
        ;
    else if ( 2 == value )
        value = 0;
    else {
        DEBUG_MSGTL( ( "access:ipForwarding", "bad value %ld for %s\n",
            value, ipfw_name ) );
        return PRIOT_ERR_WRONGVALUE;
    }

    filep = fopen( ipfw_name, "w" );
    if ( NULL == filep ) {
        DEBUG_MSGTL( ( "access:ipForwarding", "could not open %s\n",
            ipfw_name ) );
        return PRIOT_ERR_RESOURCEUNAVAILABLE;
    }

    rc = fprintf( filep, "%ld", value );
    fclose( filep );
    if ( 1 != rc ) {
        DEBUG_MSGTL( ( "access:ipForwarding", "could not write %s\n",
            ipfw_name ) );
        return PRIOT_ERR_GENERR;
    }

    return 0;
}

int netsnmp_arch_ip_scalars_ipDefaultTTL_get( u_long* value )
{
    FILE* filep;
    int rc;

    if ( NULL == value )
        return -1;

    filep = fopen( ipttl_name, "r" );
    if ( NULL == filep ) {
        DEBUG_MSGTL( ( "access:ipDefaultTTL", "could not open %s\n",
            ipttl_name ) );
        return -2;
    }

    rc = fscanf( filep, "%lu", value );
    fclose( filep );
    if ( 1 != rc ) {
        DEBUG_MSGTL( ( "access:ipDefaultTTL", "could not read %s\n",
            ipttl_name ) );
        return -3;
    }

    if ( ( 0 == *value ) || ( 255 < *value ) ) {
        DEBUG_MSGTL( ( "access:ipDefaultTTL", "unexpected value %ld in %s\n",
            *value, ipttl_name ) );
        return -4;
    }

    return 0;
}

int netsnmp_arch_ip_scalars_ipDefaultTTL_set( u_long value )
{
    FILE* filep;
    int rc;

    if ( value == 0 || value > 255 ) {
        DEBUG_MSGTL( ( "access:ipDefaultTTL", "bad value %ld for %s\n",
            value, ipttl_name ) );
        return PRIOT_ERR_WRONGVALUE;
    }

    filep = fopen( ipttl_name, "w" );
    if ( NULL == filep ) {
        DEBUG_MSGTL( ( "access:ipDefaultTTL", "could not open %s\n",
            ipttl_name ) );
        return PRIOT_ERR_RESOURCEUNAVAILABLE;
    }

    rc = fprintf( filep, "%ld", value );
    fclose( filep );
    if ( 1 != rc ) {
        DEBUG_MSGTL( ( "access:ipDefaultTTL", "could not write %s\n",
            ipttl_name ) );
        return PRIOT_ERR_GENERR;
    }

    return 0;
}

int netsnmp_arch_ip_scalars_ipv6IpForwarding_get( u_long* value )
{
    FILE* filep;
    int rc;

    if ( NULL == value )
        return -1;

    filep = fopen( ipfw6_name, "r" );
    if ( NULL == filep ) {
        DEBUG_MSGTL( ( "access:ipv6IpForwarding", "could not open %s\n",
            ipfw6_name ) );
        return -2;
    }

    rc = fscanf( filep, "%lu", value );
    fclose( filep );
    if ( 1 != rc ) {
        DEBUG_MSGTL( ( "access:ipv6IpForwarding", "could not read %s\n",
            ipfw6_name ) );
        return -3;
    }

    if ( ( 0 != *value ) && ( 1 != *value ) ) {
        DEBUG_MSGTL( ( "access:ipv6IpForwarding", "unexpected value %ld in %s\n",
            *value, ipfw6_name ) );
        return -4;
    }

    return 0;
}

int netsnmp_arch_ip_scalars_ipv6IpForwarding_set( u_long value )
{
    FILE* filep;
    int rc;

    if ( 1 == value )
        ;
    else if ( 2 == value )
        value = 0;
    else {
        DEBUG_MSGTL( ( "access:ipv6IpForwarding",
            "bad value %ld for ipv6IpForwarding\n", value ) );
        return PRIOT_ERR_WRONGVALUE;
    }

    filep = fopen( ipfw6_name, "w" );
    if ( NULL == filep ) {
        DEBUG_MSGTL( ( "access:ipv6IpForwarding", "could not open %s\n",
            ipfw6_name ) );
        return PRIOT_ERR_RESOURCEUNAVAILABLE;
    }

    rc = fprintf( filep, "%ld", value );
    fclose( filep );
    if ( 1 != rc ) {
        DEBUG_MSGTL( ( "access:ipv6IpForwarding", "could not write %s\n",
            ipfw6_name ) );
        return PRIOT_ERR_GENERR;
    }

    return 0;
}

int netsnmp_arch_ip_scalars_ipv6IpDefaultHopLimit_get( u_long* value )
{
    FILE* filep;
    int rc;

    if ( NULL == value )
        return -1;

    filep = fopen( iphop6_name, "r" );
    if ( NULL == filep ) {
        DEBUG_MSGTL( ( "access:ipDefaultHopLimit", "could not open %s\n",
            iphop6_name ) );
        return -2;
    }

    rc = fscanf( filep, "%lu", value );
    fclose( filep );
    if ( 1 != rc ) {
        DEBUG_MSGTL( ( "access:ipDefaultHopLimit", "could not read %s\n",
            iphop6_name ) );
        return -3;
    }

    if ( ( 0 == *value ) || ( 255 < *value ) ) {
        DEBUG_MSGTL( ( "access:ipDefaultHopLimit", "unexpected value %ld in %s\n",
            *value, iphop6_name ) );
        return -4;
    }

    return 0;
}

int netsnmp_arch_ip_scalars_ipv6IpDefaultHopLimit_set( u_long value )
{
    FILE* filep;
    int rc;

    if ( value == 0 || value > 255 ) {
        DEBUG_MSGTL( ( "access:ipDefaultHopLimit", "bad value %ld for %s\n",
            value, iphop6_name ) );
        return PRIOT_ERR_WRONGVALUE;
    }

    filep = fopen( iphop6_name, "w" );
    if ( NULL == filep ) {
        DEBUG_MSGTL( ( "access:ipDefaultHopLimit", "could not open %s\n",
            iphop6_name ) );
        return PRIOT_ERR_RESOURCEUNAVAILABLE;
    }

    rc = fprintf( filep, "%ld", value );
    fclose( filep );
    if ( 1 != rc ) {
        DEBUG_MSGTL( ( "access:ipDefaultHopLimit", "could not write %s\n",
            iphop6_name ) );
        return PRIOT_ERR_GENERR;
    }

    return 0;
}

void netsnmp_arch_ip_scalars_register_handlers( void )
{
    static oid ipReasmTimeout_oid[] = { 1, 3, 6, 1, 2, 1, 4, 13, 0 };

    Instance_registerNumFileInstance( "ipReasmTimeout",
        ipReasmTimeout_oid, ASN01_OID_LENGTH( ipReasmTimeout_oid ),
        "/proc/sys/net/ipv4/ipfrag_time", ASN01_INTEGER,
        HANDLER_CAN_RONLY, NULL, NULL );
}
