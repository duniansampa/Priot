/*
 *  Swrun MIB architecture support
 *
 * $Id: swrun.c 15768 2007-01-22 16:18:29Z rstory $
 */
/*
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include "swrun.h"
#include "System/Util/Assert.h"
#include "CacheHandler.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "System/Util/Logger.h"
#include "Types.h"
#include "siglog/data_access/swrun.h"

/**---------------------------------------------------------------------*/
/*
 * local static vars
 */
static int _swrun_init = 0;
int _swrun_max = 0;
static Cache* swrun_cache = NULL;
static Container_Container* swrun_container = NULL;

Container_Container* netsnmp_swrun_container( void );
Cache* netsnmp_swrun_cache( void );

/*
 * local static prototypes
 */
static void _swrun_entry_release( netsnmp_swrun_entry* entry,
    void* unused );

/**---------------------------------------------------------------------*/
/*
 * external per-architecture functions prototypes
 *
 * These shouldn't be called by the general public, so they aren't in
 * the header file.
 */
extern void netsnmp_arch_swrun_init( void );
extern int netsnmp_arch_swrun_container_load( Container_Container* container,
    u_int load_flags );

/**
 * initialization
 */
void init_swrun( void )
{
    DEBUG_MSGTL( ( "swrun:access", "init\n" ) );

    if ( 1 == _swrun_init )
        return;

    _swrun_init = 1;

    ( void )netsnmp_swrun_container();
    netsnmp_arch_swrun_init();
    ( void )netsnmp_swrun_cache();
}

void shutdown_swrun( void )
{
    DEBUG_MSGTL( ( "swrun:access", "shutdown\n" ) );
}

int swrun_count_processes( void )
{
    CacheHandler_checkAndReload( swrun_cache );
    return ( swrun_container ? CONTAINER_SIZE( swrun_container ) : 0 );
}

int swrun_max_processes( void )
{
    return _swrun_max;
}

int swrun_count_processes_by_name( char* name )
{
    netsnmp_swrun_entry* entry;
    Container_Iterator* it;
    int i = 0;

    CacheHandler_checkAndReload( swrun_cache );
    if ( !swrun_container || !name )
        return 0; /* or -1 */

    it = CONTAINER_ITERATOR( swrun_container );
    while ( ( entry = ( netsnmp_swrun_entry* )CONTAINER_ITERATOR_NEXT( it ) ) != NULL ) {
        if ( 0 == strcmp( entry->hrSWRunName, name ) )
            i++;
    }
    CONTAINER_ITERATOR_RELEASE( it );

    return i;
}

/**---------------------------------------------------------------------*/
/*
 * cache functions
 */

static int
_cache_load( Cache* cache, void* magic )
{
    netsnmp_swrun_container_load( swrun_container, 0 );
    return 0;
}

static void
_cache_free( Cache* cache, void* magic )
{
    netsnmp_swrun_container_free_items( swrun_container );
    return;
}

/**
 * create swrun cache
 */
Cache*
netsnmp_swrun_cache( void )
{
    oid hrSWRunTable_oid[] = { 1, 3, 6, 1, 2, 1, 25, 4, 2 };
    size_t hrSWRunTable_oid_len = ASN01_OID_LENGTH( hrSWRunTable_oid );

    if ( !swrun_cache ) {
        swrun_cache = CacheHandler_create( 30, /* timeout in seconds */
            _cache_load, _cache_free,
            hrSWRunTable_oid, hrSWRunTable_oid_len );
        if ( swrun_cache )
            swrun_cache->flags = CacheOperation_DONT_INVALIDATE_ON_SET;
    }
    return swrun_cache;
}

/**---------------------------------------------------------------------*/
/*
 * container functions
 */
/**
 * create swrun container
 */
Container_Container*
netsnmp_swrun_container( void )
{
    DEBUG_MSGTL( ( "swrun:container", "init\n" ) );

    /*
     * create the container.
     */
    if ( !swrun_container ) {
        swrun_container = Container_find( "swrun:tableContainer" );
        if ( NULL == swrun_container )
            return NULL;

        swrun_container->containerName = strdup( "swrun container" );
    }

    return swrun_container;
}

/**
 * load swrun information in specified container
 *
 * @param container empty container to be filled.
 *                  pass NULL to have the function create one.
 * @param load_flags flags to modify behaviour. Examples:
 *                   NETSNMP_SWRUN_ALL_OR_NONE
 *
 * @retval NULL  error
 * @retval !NULL pointer to container
 */
Container_Container*
netsnmp_swrun_container_load( Container_Container* user_container, u_int load_flags )
{
    Container_Container* container = user_container;
    int rc;

    DEBUG_MSGTL( ( "swrun:container:load", "load\n" ) );
    Assert_assert( 1 == _swrun_init );

    if ( NULL == container )
        container = netsnmp_swrun_container();
    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "no container specified/found for swrun\n" );
        return NULL;
    }

    rc = netsnmp_arch_swrun_container_load( container, load_flags );
    if ( 0 != rc ) {
        if ( NULL == user_container ) {
            netsnmp_swrun_container_free( container, NETSNMP_SWRUN_NOFLAGS );
            container = NULL;
        } else if ( load_flags & NETSNMP_SWRUN_ALL_OR_NONE ) {
            DEBUG_MSGTL( ( "swrun:container:load",
                " discarding partial results\n" ) );
            netsnmp_swrun_container_free_items( container );
        }
    }

    return container;
}

void netsnmp_swrun_container_free( Container_Container* container, u_int free_flags )
{
    DEBUG_MSGTL( ( "swrun:container", "free\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "invalid container for netsnmp_swrun_container_free\n" );
        return;
    }

    if ( !( free_flags & NETSNMP_SWRUN_DONT_FREE_ITEMS ) )
        netsnmp_swrun_container_free_items( container );

    CONTAINER_FREE( container );
}

void netsnmp_swrun_container_free_items( Container_Container* container )
{
    DEBUG_MSGTL( ( "swrun:container", "free_items\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "invalid container for netsnmp_swrun_container_free_items\n" );
        return;
    }

    /*
     * free all items.
     */
    CONTAINER_CLEAR( container,
        ( Container_FuncObjFunc* )_swrun_entry_release,
        NULL );
}

/**---------------------------------------------------------------------*/
/*
 * swrun_entry functions
 */
/**
 */
netsnmp_swrun_entry*
netsnmp_swrun_entry_get_by_index( Container_Container* container, oid index )
{
    Types_Index tmp;

    DEBUG_MSGTL( ( "swrun:entry", "by_index\n" ) );
    Assert_assert( 1 == _swrun_init );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid container for netsnmp_swrun_entry_get_by_index\n" );
        return NULL;
    }

    tmp.len = 1;
    tmp.oids = &index;

    return ( netsnmp_swrun_entry* )CONTAINER_FIND( container, &tmp );
}

/**
 */
netsnmp_swrun_entry*
netsnmp_swrun_entry_create( int32_t index )
{
    netsnmp_swrun_entry* entry = MEMORY_MALLOC_TYPEDEF( netsnmp_swrun_entry );

    if ( NULL == entry )
        return NULL;

    entry->hrSWRunIndex = index;
    entry->hrSWRunType = 1; /* unknown */
    entry->hrSWRunStatus = 2; /* runnable */

    entry->oid_index.len = 1;
    entry->oid_index.oids = ( oid* )&entry->hrSWRunIndex;

    return entry;
}

/**
 */
void netsnmp_swrun_entry_free( netsnmp_swrun_entry* entry )
{
    if ( NULL == entry )
        return;

    /*
     * MEMORY_FREE not needed, for any of these, 
     * since the whole entry is about to be freed
     */
    free( entry );
}

/**---------------------------------------------------------------------*/
/*
 * Utility routines
 */

/**
 */
static void
_swrun_entry_release( netsnmp_swrun_entry* entry, void* context )
{
    netsnmp_swrun_entry_free( entry );
}
