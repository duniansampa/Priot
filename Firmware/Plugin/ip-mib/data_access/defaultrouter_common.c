/*
 *  defaultrouter MIB architecture support
 *
 * $Id:$
 */

#include "siglog/data_access/defaultrouter.h"
#include "System/Util/Assert.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "Priot.h"
#include "ip-mib/ipDefaultRouterTable/ipDefaultRouterTable_enums.h"

/**---------------------------------------------------------------------*/
/*
 * local static prototypes
 */
static int _access_defaultrouter_entry_compare_addr( const void* lhs,
    const void* rhs );
static void _access_defaultrouter_entry_release( netsnmp_defaultrouter_entry* entry,
    void* unused );

/**---------------------------------------------------------------------*/
/*
 * external per-architecture functions prototypes
 *
 * These shouldn't be called by the general public, so they aren't in
 * the header file.
 */
extern int
netsnmp_arch_defaultrouter_entry_init( netsnmp_defaultrouter_entry* entry );

extern int
netsnmp_arch_defaultrouter_container_load( Container_Container* container,
    u_int load_flags );

/**---------------------------------------------------------------------*/
/*
 * container functions
 */
/**
 */
Container_Container*
netsnmp_access_defaultrouter_container_init( u_int flags )
{
    Container_Container* container1;

    DEBUG_MSGTL( ( "access:defaultrouter:container", "init\n" ) );

    /*
     * create the containers. one indexed by ifIndex, the other
     * indexed by ifName.
     */
    container1 = Container_find( "accessDefaultrouter:tableContainer" );
    if ( NULL == container1 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "defaultrouter primary container is not found\n" );
        return NULL;
    }
    container1->containerName = strdup( "dr_index" );

    if ( flags & NETSNMP_ACCESS_DEFAULTROUTER_INIT_ADDL_IDX_BY_ADDR ) {
        Container_Container* container2 = Container_find( "defaultrouterAddr:accessDefaultrouter:tableContainer" );
        if ( NULL == container2 ) {
            Logger_log( LOGGER_PRIORITY_ERR, "defaultrouter secondary container not found\n" );
            CONTAINER_FREE( container1 );
            return NULL;
        }

        container2->compare = _access_defaultrouter_entry_compare_addr;
        container2->containerName = strdup( "dr_addr" );

        Container_addIndex( container1, container2 );
    }

    return container1;
}

/**
 * @retval NULL  error
 * @retval !NULL pointer to container
 */
Container_Container*
netsnmp_access_defaultrouter_container_load( Container_Container* container,
    u_int load_flags )
{
    int rc;
    u_int container_flags = 0;

    DEBUG_MSGTL( ( "access:defaultrouter:container", "load\n" ) );

    if ( NULL == container ) {
        if ( load_flags & NETSNMP_ACCESS_DEFAULTROUTER_LOAD_ADDL_IDX_BY_ADDR ) {
            container_flags |= NETSNMP_ACCESS_DEFAULTROUTER_INIT_ADDL_IDX_BY_ADDR;
        }
        container = netsnmp_access_defaultrouter_container_init( container_flags );
    }

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "no container specified/found for access_defaultrouter\n" );
        return NULL;
    }

    rc = netsnmp_arch_defaultrouter_container_load( container, load_flags );
    if ( 0 != rc ) {
        netsnmp_access_defaultrouter_container_free( container,
            NETSNMP_ACCESS_DEFAULTROUTER_FREE_NOFLAGS );
        container = NULL;
    }

    return container;
}

void netsnmp_access_defaultrouter_container_free( Container_Container* container,
    u_int free_flags )
{
    DEBUG_MSGTL( ( "access:defaultrouter:container", "free\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid container for netsnmp_access_defaultrouter_free\n" );
        return;
    }

    if ( !( free_flags & NETSNMP_ACCESS_DEFAULTROUTER_FREE_DONT_CLEAR ) ) {
        /*
         * free all items.
         */
        CONTAINER_CLEAR( container,
            ( Container_FuncObjFunc* )_access_defaultrouter_entry_release,
            NULL );
    }

    if ( !( free_flags & NETSNMP_ACCESS_DEFAULTROUTER_FREE_KEEP_CONTAINER ) )
        CONTAINER_FREE( container );
}

/**---------------------------------------------------------------------*/
/*
 * defaultrouter_entry functions
 */
/**
 */
/**
 */
netsnmp_defaultrouter_entry*
netsnmp_access_defaultrouter_entry_create( void )
{
    int rc = 0;
    netsnmp_defaultrouter_entry* entry = MEMORY_MALLOC_TYPEDEF( netsnmp_defaultrouter_entry );

    DEBUG_MSGTL( ( "access:defaultrouter:entry", "create\n" ) );

    if ( NULL == entry )
        return NULL;

    entry->oid_index.len = 1;
    entry->oid_index.oids = &entry->ns_dr_index;

    /*
     * set up defaults
     */
    entry->dr_lifetime = IPDEFAULTROUTERLIFETIME_MAX;
    entry->dr_preference = IPDEFAULTROUTERPREFERENCE_MEDIUM;

    rc = netsnmp_arch_defaultrouter_entry_init( entry );
    if ( PRIOT_ERR_NOERROR != rc ) {
        DEBUG_MSGT( ( "access:defaultrouter:create", "error %d in arch init\n", rc ) );
        netsnmp_access_defaultrouter_entry_free( entry );
        entry = NULL;
    }

    return entry;
}

void netsnmp_access_defaultrouter_entry_free( netsnmp_defaultrouter_entry* entry )
{
    if ( NULL == entry )
        return;

    free( entry );
}

/**
 * update an old defaultrouter_entry from a new one
 *
 * @note: only mib related items are compared. Internal objects
 * such as oid_index, ns_dr_index and flags are not compared.
 *
 * @retval -1  : error
 * @retval >=0 : number of fields updated
 */
int netsnmp_access_defaultrouter_entry_update( netsnmp_defaultrouter_entry* lhs,
    netsnmp_defaultrouter_entry* rhs )
{
    int changed = 0;

    if ( lhs->dr_addresstype != rhs->dr_addresstype ) {
        ++changed;
        lhs->dr_addresstype = rhs->dr_addresstype;
    }

    if ( lhs->dr_address_len != rhs->dr_address_len ) {
        changed += 2;
        lhs->dr_address_len = rhs->dr_address_len;
        memcpy( lhs->dr_address, rhs->dr_address, rhs->dr_address_len );
    } else if ( memcmp( lhs->dr_address, rhs->dr_address, rhs->dr_address_len ) != 0 ) {
        ++changed;
        memcpy( lhs->dr_address, rhs->dr_address, rhs->dr_address_len );
    }

    if ( lhs->dr_if_index != rhs->dr_if_index ) {
        ++changed;
        lhs->dr_if_index = rhs->dr_if_index;
    }

    if ( lhs->dr_lifetime != rhs->dr_lifetime ) {
        ++changed;
        lhs->dr_lifetime = rhs->dr_lifetime;
    }

    if ( lhs->dr_preference != rhs->dr_preference ) {
        ++changed;
        lhs->dr_preference = rhs->dr_preference;
    }

    return changed;
}

/**
 * copy an  defaultrouter_entry
 *
 * @retval -1  : error
 * @retval 0   : no error
 */
int netsnmp_access_defaultrouter_entry_copy( netsnmp_defaultrouter_entry* lhs,
    netsnmp_defaultrouter_entry* rhs )
{
    lhs->dr_addresstype = rhs->dr_addresstype;
    lhs->dr_address_len = rhs->dr_address_len;
    memcpy( lhs->dr_address, rhs->dr_address, rhs->dr_address_len );
    lhs->dr_if_index = rhs->dr_if_index;
    lhs->dr_lifetime = rhs->dr_lifetime;
    lhs->dr_preference = rhs->dr_preference;

    return 0;
}

/**---------------------------------------------------------------------*/
/*
 * Utility routines
 */

/**
 */
void _access_defaultrouter_entry_release( netsnmp_defaultrouter_entry* entry, void* context )
{
    netsnmp_access_defaultrouter_entry_free( entry );
}

static int _access_defaultrouter_entry_compare_addr( const void* lhs,
    const void* rhs )
{
    const netsnmp_defaultrouter_entry* lh = ( const netsnmp_defaultrouter_entry* )lhs;
    const netsnmp_defaultrouter_entry* rh = ( const netsnmp_defaultrouter_entry* )rhs;

    Assert_assert( NULL != lhs );
    Assert_assert( NULL != rhs );

    /*
     * compare address length
     */
    if ( lh->dr_address_len < rh->dr_address_len )
        return -1;
    else if ( lh->dr_address_len > rh->dr_address_len )
        return 1;

    /*
     * length equal, compare address
     */
    return memcmp( lh->dr_address, rh->dr_address, lh->dr_address_len );
}
