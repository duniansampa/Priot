/*
 *  ipv6ScopeIndexTable MIB architecture support
 *
 * $Id: ipv6scopezone_common.c 14170 2007-04-29 02:22:12Z varun_c $
 */

#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "siglog/data_access/scopezone.h"
/*
 * local static prototypes
 */
static void _entry_release( netsnmp_v6scopezone_entry* entry, void* unused );

/**---------------------------------------------------------------------*/
/*
 * external per-architecture functions prototypes
 *
 * These shouldn't be called by the general public, so they aren't in
 * the header file.
 */
extern int
netsnmp_access_scopezone_container_arch_load( Container_Container* container,
    u_int load_flags );
extern void
netsnmp_access_scopezone_arch_init( void );

/**
 * initialize systemstats container
 */
Container_Container*
netsnmp_access_scopezone_container_init( u_int flags )
{
    Container_Container* container;

    DEBUG_MSGTL( ( "access:scopezone:container", "init\n" ) );
    /*
     * create the containers. one indexed by ifIndex, the other
     * indexed by ifName.
     */
    container = Container_find( "accessScopezone:tableContainer" );
    if ( NULL == container )
        return NULL;

    return container;
}

/**
 * load scopezone information in specified container
 *
 * @param container empty container, or NULL to have one created for you
 * @param load_flags flags to modify behaviour.
 *
 * @retval NULL  error
 * @retval !NULL pointer to container
 */
Container_Container*
netsnmp_access_scopezone_container_load( Container_Container* container, u_int load_flags )
{
    int rc;

    DEBUG_MSGTL( ( "access:scopezone:container", "load\n" ) );

    if ( NULL == container ) {
        container = netsnmp_access_scopezone_container_init( load_flags );
        if ( container )
            container->containerName = strdup( "scopezone" );
    }
    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "no container specified/found for access_scopezone\n" );
        return NULL;
    }

    rc = netsnmp_access_scopezone_container_arch_load( container, load_flags );
    if ( 0 != rc ) {
        netsnmp_access_scopezone_container_free( container,
            NETSNMP_ACCESS_SCOPEZONE_FREE_NOFLAGS );
        container = NULL;
    }

    return container;
}

void netsnmp_access_scopezone_container_free( Container_Container* container, u_int free_flags )
{
    DEBUG_MSGTL( ( "access:scopezone:container", "free\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "invalid container for netsnmp_access_scopezone_free\n" );
        return;
    }

    if ( !( free_flags & NETSNMP_ACCESS_SCOPEZONE_FREE_DONT_CLEAR ) ) {
        /*
         * free all items.
         */
        CONTAINER_CLEAR( container,
            ( Container_FuncObjFunc* )_entry_release,
            NULL );
    }

    CONTAINER_FREE( container );
}

/**
 */
netsnmp_v6scopezone_entry*
netsnmp_access_scopezone_entry_create( void )
{
    netsnmp_v6scopezone_entry* entry = MEMORY_MALLOC_TYPEDEF( netsnmp_v6scopezone_entry );

    DEBUG_MSGTL( ( "access:scopezone:entry", "create\n" ) );

    if ( NULL == entry )
        return NULL;

    entry->oid_index.len = 1;
    entry->oid_index.oids = &entry->ns_scopezone_index;

    return entry;
}

/**
 */
void netsnmp_access_scopezone_entry_free( netsnmp_v6scopezone_entry* entry )
{
    DEBUG_MSGTL( ( "access:scopezone:entry", "free\n" ) );

    if ( NULL == entry )
        return;

    free( entry );
}

/**---------------------------------------------------------------------*/
/*
 * Utility routines
 */

/**
 * \internal
 */
static void
_entry_release( netsnmp_v6scopezone_entry* entry, void* context )
{
    netsnmp_access_scopezone_entry_free( entry );
}
