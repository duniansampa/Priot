/*
 *  TcpConn MIB architecture support
 *
 * $Id$
 */

#include "siglog/data_access/tcpConn.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "Priot.h"

/**---------------------------------------------------------------------*/
/*
 * local static prototypes
 */
static void _access_tcpconn_entry_release( netsnmp_tcpconn_entry* entry,
    void* unused );

/**---------------------------------------------------------------------*/
/*
 * external per-architecture functions prototypes
 *
 * These shouldn't be called by the general public, so they aren't in
 * the header file.
 */
extern int
netsnmp_arch_tcpconn_container_load( Container_Container* container,
    u_int load_flags );
extern int
netsnmp_arch_tcpconn_entry_init( netsnmp_tcpconn_entry* entry );
extern int
netsnmp_arch_tcpconn_entry_copy( netsnmp_tcpconn_entry* lhs,
    netsnmp_tcpconn_entry* rhs );
extern void
netsnmp_arch_tcpconn_entry_cleanup( netsnmp_tcpconn_entry* entry );

/**---------------------------------------------------------------------*/
/*
 * container functions
 */
/**
 */
Container_Container*
netsnmp_access_tcpconn_container_init( u_int flags )
{
    Container_Container* container1;

    DEBUG_MSGTL( ( "access:tcpconn:container", "init\n" ) );

    /*
     * create the container
     */
    container1 = Container_find( "access:tcpconn:table_container" );
    if ( NULL == container1 ) {
        Logger_log( LOGGER_PRIORITY_ERR, "tcpconn primary container not found\n" );
        return NULL;
    }
    container1->containerName = strdup( "tcpConnTable" );

    return container1;
}

/**
 * @retval NULL  error
 * @retval !NULL pointer to container
 */
Container_Container*
netsnmp_access_tcpconn_container_load( Container_Container* container, u_int load_flags )
{
    int rc;

    DEBUG_MSGTL( ( "access:tcpconn:container", "load\n" ) );

    if ( NULL == container )
        container = netsnmp_access_tcpconn_container_init( load_flags );
    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "no container specified/found for access_tcpconn\n" );
        return NULL;
    }

    rc = netsnmp_arch_tcpconn_container_load( container, load_flags );
    if ( 0 != rc ) {
        netsnmp_access_tcpconn_container_free( container,
            NETSNMP_ACCESS_TCPCONN_FREE_NOFLAGS );
        container = NULL;
    }

    return container;
}

void netsnmp_access_tcpconn_container_free( Container_Container* container, u_int free_flags )
{
    DEBUG_MSGTL( ( "access:tcpconn:container", "free\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "invalid container for netsnmp_access_tcpconn_free\n" );
        return;
    }

    if ( !( free_flags & NETSNMP_ACCESS_TCPCONN_FREE_DONT_CLEAR ) ) {
        /*
         * free all items.
         */
        CONTAINER_CLEAR( container,
            ( Container_FuncObjFunc* )_access_tcpconn_entry_release,
            NULL );
    }

    if ( !( free_flags & NETSNMP_ACCESS_TCPCONN_FREE_KEEP_CONTAINER ) )
        CONTAINER_FREE( container );
}

/**---------------------------------------------------------------------*/
/*
 * tcpconn_entry functions
 */
/**
 */
/**
 */
netsnmp_tcpconn_entry*
netsnmp_access_tcpconn_entry_create( void )
{
    netsnmp_tcpconn_entry* entry = MEMORY_MALLOC_TYPEDEF( netsnmp_tcpconn_entry );
    int rc = 0;

    DEBUG_MSGTL( ( "verbose:access:tcpconn:entry", "create\n" ) );

    entry->oid_index.len = 1;
    entry->oid_index.oids = &entry->arbitrary_index;

    /*
     * init arch data
     */
    rc = netsnmp_arch_tcpconn_entry_init( entry );
    if ( PRIOT_ERR_NOERROR != rc ) {
        DEBUG_MSGT( ( "access:tcpconn:create", "error %d in arch init\n", rc ) );
        netsnmp_access_tcpconn_entry_free( entry );
    }

    return entry;
}

/**
 */
void netsnmp_access_tcpconn_entry_free( netsnmp_tcpconn_entry* entry )
{
    if ( NULL == entry )
        return;

    DEBUG_MSGTL( ( "verbose:access:tcpconn:entry", "free\n" ) );

    if ( NULL != entry->arch_data )
        netsnmp_arch_tcpconn_entry_cleanup( entry );

    free( entry );
}

/**
 * update an old tcpconn_entry from a new one
 *
 * @note: only mib related items are compared. Internal objects
 * such as oid_index, ns_ia_index and flags are not compared.
 *
 * @retval -1  : error
 * @retval >=0 : number of fileds updated
 */
int netsnmp_access_tcpconn_entry_update( netsnmp_tcpconn_entry* lhs,
    netsnmp_tcpconn_entry* rhs )
{
    int rc, changed = 0;

    DEBUG_MSGTL( ( "access:tcpconn:entry", "update\n" ) );

    if ( lhs->tcpConnState != rhs->tcpConnState ) {
        ++changed;
        lhs->tcpConnState = rhs->tcpConnState;
    }

    if ( lhs->pid != rhs->pid ) {
        ++changed;
        lhs->pid = rhs->pid;
    }

    /*
     * copy arch stuff. we don't care if it changed
     */
    rc = netsnmp_arch_tcpconn_entry_copy( lhs, rhs );
    if ( 0 != rc ) {
        Logger_log( LOGGER_PRIORITY_ERR, "arch tcpconn copy failed\n" );
        return -1;
    }

    return changed;
}

/**---------------------------------------------------------------------*/
/*
 * Utility routines
 */

/**
 */
void _access_tcpconn_entry_release( netsnmp_tcpconn_entry* entry, void* context )
{
    netsnmp_access_tcpconn_entry_free( entry );
}
