/*
 *  UDP-MIB endpoint architecture support
 *
 * $Id$
 */

//#include "udp-mib/udpEndpointTable/udpEndpointTable_constants.h"

#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "udp_endpoint_private.h"

//#include <siglog/data_access/ipaddress.h>
//#include <siglog/data_access/udp_endpoint.h>

/**---------------------------------------------------------------------*/
/*
 * local static vars
 */

/**---------------------------------------------------------------------*/
/*
 * initialization
 */

/**---------------------------------------------------------------------*/
/*
 * container functions
 */
/**
 * initialize udp_endpoint container
 */
Container_Container* netsnmp_access_udp_endpoint_container_init( u_int flags )
{
    Container_Container* container;

    DEBUG_MSGTL( ( "access:udp_endpoint:container", "init\n" ) );

    /*
     * create the containers.
     */
    container = Container_find( "access_udp_endpoint:tableContainer" );
    if ( NULL == container )
        return NULL;

    return container;
}

/**
 * load udp_endpoint information in specified container
 *
 * @param container empty container, or NULL to have one created for you
 * @param load_flags flags to modify behaviour.
 *
 * @retval NULL  error
 * @retval !NULL pointer to container
 */
Container_Container*
netsnmp_access_udp_endpoint_container_load( Container_Container* container,
    u_int load_flags )
{
    int rc;

    DEBUG_MSGTL( ( "access:udp_endpoint:container", "load\n" ) );

    if ( NULL == container )
        container = netsnmp_access_udp_endpoint_container_init( 0 );
    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "no container specified/found for access_udp_endpoint\n" );
        return NULL;
    }

    rc = netsnmp_arch_udp_endpoint_container_load( container, load_flags );
    if ( 0 != rc ) {
        netsnmp_access_udp_endpoint_container_free( container, 0 );
        container = NULL;
    }

    return container;
}

void netsnmp_access_udp_endpoint_container_free( Container_Container* container,
    u_int free_flags )
{
    DEBUG_MSGTL( ( "access:udp_endpoint:container", "free\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid container for netsnmp_access_udp_endpoint_free\n" );
        return;
    }

    if ( !( free_flags & NETSNMP_ACCESS_UDP_ENDPOINT_FREE_DONT_CLEAR ) ) {
        /*
         * free all items.
         */
        CONTAINER_CLEAR( container,
            ( Container_FuncObjFunc* )
                netsnmp_access_udp_endpoint_entry_free,
            NULL );
    }

    if ( !( free_flags & NETSNMP_ACCESS_UDP_ENDPOINT_FREE_KEEP_CONTAINER ) )
        CONTAINER_FREE( container );
}

/**---------------------------------------------------------------------*/
/*
 * entry functions
 */
/**
 */
netsnmp_udp_endpoint_entry*
netsnmp_access_udp_endpoint_entry_create( void )
{
    netsnmp_udp_endpoint_entry* entry = MEMORY_MALLOC_TYPEDEF( netsnmp_udp_endpoint_entry );

    DEBUG_MSGTL( ( "access:udp_endpoint:entry", "create\n" ) );

    if ( NULL == entry )
        return NULL;

    entry->oid_index.len = 1;
    entry->oid_index.oids = ( oid* )&entry->index;

    return entry;
}

/**
 */
void netsnmp_access_udp_endpoint_entry_free( netsnmp_udp_endpoint_entry* entry )
{
    DEBUG_MSGTL( ( "access:udp_endpoint:entry", "free\n" ) );

    if ( NULL == entry )
        return;

    /*
     * MEMORY_FREE not needed, for any of these, 
     * since the whole entry is about to be freed
     */

    free( entry );
}
