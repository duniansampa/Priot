/*
 *  Interface MIB architecture support
 *
 * $Id$
 */

#include "siglog/data_access/route.h"
#include "System/Util/Assert.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "Priot.h"

/**---------------------------------------------------------------------*/
/*
 * local static prototypes
 */
static void _access_route_entry_release( netsnmp_route_entry* entry, void* unused );

/**---------------------------------------------------------------------*/
/*
 * external per-architecture functions prototypes
 *
 * These shouldn't be called by the general public, so they aren't in
 * the header file.
 */
extern int netsnmp_access_route_container_arch_load( Container_Container* container,
    u_int load_flags );
extern int
netsnmp_arch_route_create( netsnmp_route_entry* entry );
extern int
netsnmp_arch_route_delete( netsnmp_route_entry* entry );

/**---------------------------------------------------------------------*/
/*
 * container functions
 */

/**
 * @retval NULL  error
 * @retval !NULL pointer to container
 */
Container_Container*
netsnmp_access_route_container_load( Container_Container* container, u_int load_flags )
{
    int rc;

    DEBUG_MSGTL( ( "access:route:container", "load\n" ) );

    if ( NULL == container ) {
        container = Container_find( "access:_route:fifo" );
        if ( NULL == container ) {
            Logger_log( LOGGER_PRIORITY_ERR, "no container specified/found for access_route\n" );
            return NULL;
        }
    }

    container->containerName = strdup( "_route" );

    rc = netsnmp_access_route_container_arch_load( container, load_flags );
    if ( 0 != rc ) {
        netsnmp_access_route_container_free( container, NETSNMP_ACCESS_ROUTE_FREE_NOFLAGS );
        container = NULL;
    }

    return container;
}

void netsnmp_access_route_container_free( Container_Container* container, u_int free_flags )
{
    DEBUG_MSGTL( ( "access:route:container", "free\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "invalid container for netsnmp_access_route_free\n" );
        return;
    }

    if ( !( free_flags & NETSNMP_ACCESS_ROUTE_FREE_DONT_CLEAR ) ) {
        /*
         * free all items.
         */
        CONTAINER_CLEAR( container,
            ( Container_FuncObjFunc* )_access_route_entry_release,
            NULL );
    }

    if ( !( free_flags & NETSNMP_ACCESS_ROUTE_FREE_KEEP_CONTAINER ) )
        CONTAINER_FREE( container );
}

/**---------------------------------------------------------------------*/
/*
 * ifentry functions
 */
/** create route entry
 *
 * @note:
 *  if you create a route for entry into a container of your own, you
 *  must set ns_rt_index to a unique index for your container.
 */
netsnmp_route_entry*
netsnmp_access_route_entry_create( void )
{
    netsnmp_route_entry* entry = MEMORY_MALLOC_TYPEDEF( netsnmp_route_entry );
    if ( NULL == entry ) {
        Logger_log( LOGGER_PRIORITY_ERR, "could not allocate route entry\n" );
        return NULL;
    }

    entry->oid_index.oids = &entry->ns_rt_index;
    entry->oid_index.len = 1;

    entry->rt_metric1 = -1;
    entry->rt_metric2 = -1;
    entry->rt_metric3 = -1;
    entry->rt_metric4 = -1;
    entry->rt_metric5 = -1;

    /** entry->row_status? */

    return entry;
}

/**
 */
void netsnmp_access_route_entry_free( netsnmp_route_entry* entry )
{
    if ( NULL == entry )
        return;

    if ( ( NULL != entry->rt_policy ) && !( entry->flags & NETSNMP_ACCESS_ROUTE_POLICY_STATIC ) )
        free( entry->rt_policy );

    if ( NULL != entry->rt_info )
        free( entry->rt_info );

    free( entry );
}

/**
 * update underlying data store (kernel) for entry
 *
 * @retval  0 : success
 * @retval -1 : error
 */
int netsnmp_access_route_entry_set( netsnmp_route_entry* entry )
{
    int rc = PRIOT_ERR_NOERROR;

    if ( NULL == entry ) {
        Assert_assert( NULL != entry );
        return -1;
    }

    /*
     *
     */
    if ( entry->flags & NETSNMP_ACCESS_ROUTE_CREATE ) {
        rc = netsnmp_arch_route_create( entry );
    } else if ( entry->flags & NETSNMP_ACCESS_ROUTE_CHANGE ) {
        /** xxx-rks:9 route change not implemented */
        Logger_log( LOGGER_PRIORITY_ERR, "netsnmp_access_route_entry_set change not supported yet\n" );
        rc = -1;
    } else if ( entry->flags & NETSNMP_ACCESS_ROUTE_DELETE ) {
        rc = netsnmp_arch_route_delete( entry );
    } else {
        Logger_log( LOGGER_PRIORITY_ERR, "netsnmp_access_route_entry_set with no mode\n" );
        Assert_assert( !"route_entry_set == unknown mode" ); /* always false */
        rc = -1;
    }

    return rc;
}

/**
 * copy an  route_entry
 *
 * @retval -1  : error
 * @retval 0   : no error
 */
int netsnmp_access_route_entry_copy( netsnmp_route_entry* lhs,
    netsnmp_route_entry* rhs )
{

    lhs->if_index = rhs->if_index;

    lhs->rt_dest_len = rhs->rt_dest_len;
    memcpy( lhs->rt_dest, rhs->rt_dest, rhs->rt_dest_len );
    lhs->rt_dest_type = rhs->rt_dest_type;

    lhs->rt_nexthop_len = rhs->rt_nexthop_len;
    memcpy( lhs->rt_nexthop, rhs->rt_nexthop, rhs->rt_nexthop_len );
    lhs->rt_nexthop_type = rhs->rt_nexthop_type;

    if ( NULL != lhs->rt_policy ) {
        if ( NETSNMP_ACCESS_ROUTE_POLICY_STATIC & lhs->flags )
            lhs->rt_policy = NULL;
        else {
            MEMORY_FREE( lhs->rt_policy );
        }
    }
    if ( NULL != rhs->rt_policy ) {
        if ( ( NETSNMP_ACCESS_ROUTE_POLICY_STATIC & rhs->flags ) && !( NETSNMP_ACCESS_ROUTE_POLICY_DEEP_COPY & rhs->flags ) ) {
            lhs->rt_policy = rhs->rt_policy;
        } else {
            Client_cloneMem( ( void** )&lhs->rt_policy, rhs->rt_policy,
                rhs->rt_policy_len * sizeof( oid ) );
        }
    }
    lhs->rt_policy_len = rhs->rt_policy_len;

    lhs->rt_pfx_len = rhs->rt_pfx_len;
    lhs->rt_type = rhs->rt_type;
    lhs->rt_proto = rhs->rt_proto;

    MEMORY_FREE( lhs->rt_info );
    if ( NULL != rhs->rt_info )
        Client_cloneMem( ( void** )&lhs->rt_info, rhs->rt_info,
            rhs->rt_info_len * sizeof( oid ) );
    lhs->rt_info_len = rhs->rt_info_len;

    lhs->rt_mask = rhs->rt_mask;
    lhs->rt_tos = rhs->rt_tos;

    lhs->rt_age = rhs->rt_age;
    lhs->rt_nexthop_as = rhs->rt_nexthop_as;

    lhs->rt_metric1 = rhs->rt_metric1;
    lhs->rt_metric2 = rhs->rt_metric2;
    lhs->rt_metric3 = rhs->rt_metric3;
    lhs->rt_metric4 = rhs->rt_metric4;
    lhs->rt_metric5 = rhs->rt_metric5;

    lhs->flags = rhs->flags;

    return 0;
}

/**---------------------------------------------------------------------*/
/*
 * Utility routines
 */

/**
 */
void _access_route_entry_release( netsnmp_route_entry* entry, void* context )
{
    netsnmp_access_route_entry_free( entry );
}
