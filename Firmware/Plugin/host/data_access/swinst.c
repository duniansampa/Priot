/*
 * swinst.c : hrSWInstalledTable data access
 */
/*
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include "swinst.h"
#include "System/Containers/Container.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "siglog/data_access/swinst.h"

/* ---------------------------------------------------------------------
 */

static void netsnmp_swinst_entry_free_cb( netsnmp_swinst_entry*, void* );

extern void netsnmp_swinst_arch_init( void );
extern void netsnmp_swinst_arch_shutdown( void );
extern int netsnmp_swinst_arch_load( Container_Container*, u_int );

void init_swinst( void )
{
    static int initialized = 0;

    DEBUG_MSGTL( ( "swinst", "init called\n" ) );

    if ( initialized )
        return; /* already initialized */

    /*
     * call arch init code
     */
    netsnmp_swinst_arch_init();
}

void shutdown_swinst( void )
{
    DEBUG_MSGTL( ( "swinst", "shutdown called\n" ) );

    netsnmp_swinst_arch_shutdown();
}

/* ---------------------------------------------------------------------
 */

/*
 * load a container with installed software. If user_container is NULL,
 * a new container will be allocated and returned, and the caller
 * is responsible for releasing the allocated memory when done.
 *
 * if flags contains NETSNMP_SWINST_ALL_OR_NONE and any error occurs,
 * the container will be completely cleared.
 */
Container_Container*
netsnmp_swinst_container_load( Container_Container* user_container, int flags )
{
    Container_Container* container = user_container;
    int arch_rc;

    DEBUG_MSGTL( ( "swinst:container", "load\n" ) );

    /*
     * create the container, if needed
     */
    if ( NULL == container ) {
        container = Container_find( "swinst:tableContainer" );
        if ( NULL == container )
            return NULL;
    }
    if ( NULL == container->containerName )
        container->containerName = strdup( "swinst container" );

    /*
     * call the arch specific code to load the container
     */
    arch_rc = netsnmp_swinst_arch_load( container, flags );
    if ( arch_rc && ( flags & NETSNMP_SWINST_ALL_OR_NONE ) ) {
        /*
         * caller does not want a partial load, so empty the container.
         * if we created the container, destroy it.
         */
        netsnmp_swinst_container_free_items( container );
        if ( container != user_container ) {
            netsnmp_swinst_container_free( container, flags );
        }
    }

    return container;
}

void netsnmp_swinst_container_free( Container_Container* container, u_int free_flags )
{
    DEBUG_MSGTL( ( "swinst:container", "free\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid container for netsnmp_swinst_container_free\n" );
        return;
    }

    if ( !( free_flags & NETSNMP_SWINST_DONT_FREE_ITEMS ) )
        netsnmp_swinst_container_free_items( container );

    CONTAINER_FREE( container );
}

/*
 * free a swinst container
 */
void netsnmp_swinst_container_free_items( Container_Container* container )
{
    DEBUG_MSGTL( ( "swinst:container", "free_items\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid container for netsnmp_swinst_container_free_items\n" );
        return;
    }

    /*
     * free all items.
     */
    CONTAINER_CLEAR( container,
        ( Container_FuncObjFunc* )netsnmp_swinst_entry_free_cb,
        NULL );
}

/* ---------------------------------------------------------------------
 */

/*
 * create a new row in the table 
 */
netsnmp_swinst_entry*
netsnmp_swinst_entry_create( int32_t swIndex )
{
    netsnmp_swinst_entry* entry;

    entry = MEMORY_MALLOC_TYPEDEF( netsnmp_swinst_entry );
    if ( !entry )
        return NULL;

    entry->swIndex = swIndex;
    entry->oid_index.len = 1;
    entry->oid_index.oids = &entry->swIndex;

    entry->swType = HRSWINSTALLEDTYPE_APPLICATION;

    return entry;
}

/*
 * free a row
 */
void netsnmp_swinst_entry_free( netsnmp_swinst_entry* entry )
{
    DEBUG_MSGTL( ( "swinst:entry:free", "index %"
                                        "l"
                                        "u\n",
        entry->swIndex ) );

    free( entry );
}

/*
 * free a row
 */
static void
netsnmp_swinst_entry_free_cb( netsnmp_swinst_entry* entry, void* context )
{
    free( entry );
}

/*
 * remove a row from the table 
 */
void netsnmp_swinst_entry_remove( Container_Container* container,
    netsnmp_swinst_entry* entry )
{
    DEBUG_MSGTL( ( "swinst:container", "remove\n" ) );
    if ( !entry )
        return; /* Nothing to remove */
    CONTAINER_REMOVE( container, entry );
}

/* ---------------------------------------------------------------------
 */
