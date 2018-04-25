/*
 *  Interface MIB architecture support
 *
 * $Id$
 */

#include "ipaddress_linux.h"
#include "Container.h"
#include "Debug.h"
#include "ipaddress_ioctl.h"
#include "siglog/data_access/ipaddress.h"

int _load_v6( Container_Container* container, int idx_offset );
int netsnmp_access_ipaddress_extra_prefix_info( int index,
    u_long* preferedlt,
    ulong* validlt,
    char* addr );

/*
 * initialize arch specific storage
 *
 * @retval  0: success
 * @retval <0: error
 */
int netsnmp_arch_ipaddress_entry_init( netsnmp_ipaddress_entry* entry )
{
    /*
     * init ipv4 stuff
     */
    if ( NULL == netsnmp_ioctl_ipaddress_entry_init( entry ) )
        return -1;

    /*
     * init ipv6 stuff
     *   so far, we can just share the ipv4 stuff, so nothing to do
     */

    return 0;
}

/*
 * cleanup arch specific storage
 */
void netsnmp_arch_ipaddress_entry_cleanup( netsnmp_ipaddress_entry* entry )
{
    /*
     * cleanup ipv4 stuff
     */
    netsnmp_ioctl_ipaddress_entry_cleanup( entry );

    /*
     * cleanup ipv6 stuff
     *   so far, we can just share the ipv4 stuff, so nothing to do
     */
}

/*
 * copy arch specific storage
 */
int netsnmp_arch_ipaddress_entry_copy( netsnmp_ipaddress_entry* lhs,
    netsnmp_ipaddress_entry* rhs )
{
    int rc;

    /*
     * copy ipv4 stuff
     */
    rc = netsnmp_ioctl_ipaddress_entry_copy( lhs, rhs );
    if ( rc )
        return rc;

    /*
     * copy ipv6 stuff
     *   so far, we can just share the ipv4 stuff, so nothing to do
     */

    return rc;
}

/*
 * create a new entry
 */
int netsnmp_arch_ipaddress_create( netsnmp_ipaddress_entry* entry )
{
    if ( NULL == entry )
        return -1;

    if ( 4 == entry->ia_address_len ) {
        return _netsnmp_ioctl_ipaddress_set_v4( entry );
    } else if ( 16 == entry->ia_address_len ) {
        return _netsnmp_ioctl_ipaddress_set_v6( entry );
    } else {
        DEBUG_MSGT( ( "access:ipaddress:create", "wrong length of IP address\n" ) );
        return -2;
    }
}

/*
 * create a new entry
 */
int netsnmp_arch_ipaddress_delete( netsnmp_ipaddress_entry* entry )
{
    if ( NULL == entry )
        return -1;

    if ( 4 == entry->ia_address_len ) {
        return _netsnmp_ioctl_ipaddress_delete_v4( entry );
    } else if ( 16 == entry->ia_address_len ) {
        return _netsnmp_ioctl_ipaddress_delete_v6( entry );
    } else {
        DEBUG_MSGT( ( "access:ipaddress:create", "only ipv4 supported\n" ) );
        return -2;
    }
}

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
int netsnmp_arch_ipaddress_container_load( Container_Container* container,
    u_int load_flags )
{
    int rc = 0, idx_offset = 0;

    if ( 0 == ( load_flags & NETSNMP_ACCESS_IPADDRESS_LOAD_IPV6_ONLY ) ) {
        rc = _netsnmp_ioctl_ipaddress_container_load_v4( container, idx_offset );
        if ( rc < 0 ) {
            u_int flags = NETSNMP_ACCESS_IPADDRESS_FREE_KEEP_CONTAINER;
            netsnmp_access_ipaddress_container_free( container, flags );
        }
    }

    /*
     * return no errors (0) if we found any interfaces
     */
    if ( rc > 0 )
        rc = 0;

    return rc;
}
