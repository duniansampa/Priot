

#include "System/Util/Alarm.h"
#include "CacheHandler.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "System/String.h"
#include "siglog/agent/hardware/fsys.h"
#include <inttypes.h>

extern void netsnmp_fsys_arch_load( void );
extern void netsnmp_fsys_arch_init( void );
static int _fsys_load( void );
static void _fsys_free( void );

static int _fsysAutoUpdate = 0; /* 0 means on-demand caching */
static void _fsys_update_stats( unsigned int, void* );

Cache* _fsys_cache = NULL;
Container_Container* _fsys_container = NULL;
static int _fsys_idx = 0;
static netsnmp_fsys_info* _fsys_create_entry( void );

void init_hw_fsys( void )
{

    if ( _fsys_container )
        return; /* Already initialised */

    DEBUG_MSGTL( ( "fsys", "Initialise Hardware FileSystem module\n" ) );

    /*
     * Define a container to hold the list of filesystems
     */
    _fsys_container = Container_find( "fsysTable:tableContainer" );
    if ( NULL == _fsys_container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "failed to create container for fsysTable" );
        return;
    }
    netsnmp_fsys_arch_init();

    /*
     * If we're sampling the file system information automatically,
     *   then arrange for this to be triggered regularly.
     *
     * If we're not sampling these values regularly,
     *   create a suitable cache handler instead.
     */
    if ( _fsysAutoUpdate ) {
        DEBUG_MSGTL( ( "fsys", "Reloading Hardware FileSystems automatically (%d)\n",
            _fsysAutoUpdate ) );
        Alarm_register( _fsysAutoUpdate, AlarmFlag_REPEAT,
            _fsys_update_stats, NULL );
    } else {
        _fsys_cache = CacheHandler_create( 5, netsnmp_fsys_load,
            netsnmp_fsys_free, NULL, 0 );
        DEBUG_MSGTL( ( "fsys", "Reloading Hardware FileSystems on-demand (%p)\n",
            _fsys_cache ) );
    }
}

void shutdown_hw_fsys( void )
{
    _fsys_free();
}

/*
 *  Return the main fsys container
 */
Container_Container* netsnmp_fsys_get_container( void ) { return _fsys_container; }

/*
 *  Return the main fsys cache control structure (if defined)
 */
Cache* netsnmp_fsys_get_cache( void ) { return _fsys_cache; }

/*
 * Wrapper routine for automatically updating fsys information
 */
void _fsys_update_stats( unsigned int clientreg, void* data )
{
    _fsys_free();
    _fsys_load();
}

/*
 * Wrapper routine for re-loading filesystem statistics on demand
 */
int netsnmp_fsys_load( Cache* cache, void* data )
{
    /* XXX - check cache timeliness */
    return _fsys_load();
}

/*
 * Wrapper routine for releasing expired filesystem statistics
 */
void netsnmp_fsys_free( Cache* cache, void* data )
{
    _fsys_free();
}

/*
 * Architecture-independent processing of loading filesystem statistics
 */
static int
_fsys_load( void )
{
    netsnmp_fsys_arch_load();
    /* XXX - update cache timestamp */
    return 0;
}

/*
 * Architecture-independent release of filesystem statistics
 */
static void
_fsys_free( void )
{
    netsnmp_fsys_info* sp;

    for ( sp = ( netsnmp_fsys_info* )CONTAINER_FIRST( _fsys_container );
          sp;
          sp = ( netsnmp_fsys_info* )CONTAINER_NEXT( _fsys_container, sp ) ) {

        sp->flags &= ~NETSNMP_FS_FLAG_ACTIVE;
    }
}

netsnmp_fsys_info* netsnmp_fsys_get_first( void )
{
    return ( netsnmp_fsys_info* )CONTAINER_FIRST( _fsys_container );
}
netsnmp_fsys_info* netsnmp_fsys_get_next( netsnmp_fsys_info* this_ptr )
{
    return ( netsnmp_fsys_info* )CONTAINER_NEXT( _fsys_container, this_ptr );
}

/*
 * Retrieve a filesystem entry based on the path where it is mounted,
 *  or (optionally) insert a new one into the container
 */
netsnmp_fsys_info*
netsnmp_fsys_by_path( char* path, int create_type )
{
    netsnmp_fsys_info* sp;

    DEBUG_MSGTL( ( "fsys:path", "Get filesystem entry (%s)\n", path ) );

    /*
     *  Look through the list for a matching entry
     */
    /* .. or use a secondary index container ?? */
    for ( sp = ( netsnmp_fsys_info* )CONTAINER_FIRST( _fsys_container );
          sp;
          sp = ( netsnmp_fsys_info* )CONTAINER_NEXT( _fsys_container, sp ) ) {

        if ( !strcmp( path, sp->path ) )
            return sp;
    }

    /*
     * Not found...
     */
    if ( create_type == NETSNMP_FS_FIND_EXIST ) {
        DEBUG_MSGTL( ( "fsys:path", "No such filesystem entry\n" ) );
        return NULL;
    }

    /*
     * ... so let's create a new one
     */
    sp = _fsys_create_entry();
    if ( sp )
        String_copyTruncate( sp->path, path, sizeof( sp->path ) );
    return sp;
}

/*
 * Retrieve a filesystem entry based on the hardware device,
 *   (or exported path for remote mounts).
 * (Optionally) insert a new one into the container.
 */
netsnmp_fsys_info*
netsnmp_fsys_by_device( char* device, int create_type )
{
    netsnmp_fsys_info* sp;

    DEBUG_MSGTL( ( "fsys:device", "Get filesystem entry (%s)\n", device ) );

    /*
     *  Look through the list for a matching entry
     */
    /* .. or use a secondary index container ?? */
    for ( sp = ( netsnmp_fsys_info* )CONTAINER_FIRST( _fsys_container );
          sp;
          sp = ( netsnmp_fsys_info* )CONTAINER_NEXT( _fsys_container, sp ) ) {

        if ( !strcmp( device, sp->device ) )
            return sp;
    }

    /*
     * Not found...
     */
    if ( create_type == NETSNMP_FS_FIND_EXIST ) {
        DEBUG_MSGTL( ( "fsys:device", "No such filesystem entry\n" ) );
        return NULL;
    }

    /*
     * ... so let's create a new one
     */
    sp = _fsys_create_entry();
    if ( sp )
        String_copyTruncate( sp->device, device, sizeof( sp->device ) );
    return sp;
}

netsnmp_fsys_info*
_fsys_create_entry( void )
{
    netsnmp_fsys_info* sp;

    sp = MEMORY_MALLOC_TYPEDEF( netsnmp_fsys_info );
    if ( sp ) {
        /*
         * Set up the index value.
         *  
         * All this trouble, just for a simple integer.
         * Surely there must be a better way?
         */
        sp->idx.len = 1;
        sp->idx.oids = MEMORY_MALLOC_TYPEDEF( oid );
        sp->idx.oids[ 0 ] = ++_fsys_idx;
    }

    DEBUG_MSGTL( ( "fsys:new", "Create filesystem entry (index = %d)\n", _fsys_idx ) );
    CONTAINER_INSERT( _fsys_container, sp );
    return sp;
}

/*
 *  Convert fsys size information to 1K units
 *    (attempting to avoid 32-bit overflow!)
 */
unsigned long long
_fsys_to_K( unsigned long long size, unsigned long long units )
{
    int factor = 1;

    if ( units == 0 ) {
        return 0; /* XXX */
    } else if ( units == 1024 ) {
        return size;
    } else if ( units == 512 ) { /* To avoid unnecessary division */
        return size / 2;
    } else if ( units < 1024 ) {
        factor = 1024 / units; /* Assuming power of two */
        return ( size / factor );
    } else {
        factor = units / 1024; /* Assuming multiple of 1K */
        return ( size * factor );
    }
}

unsigned long long
netsnmp_fsys_size_ull( netsnmp_fsys_info* f )
{
    if ( !f ) {
        return 0;
    }
    return _fsys_to_K( f->size, f->units );
}

unsigned long long
netsnmp_fsys_used_ull( netsnmp_fsys_info* f )
{
    if ( !f ) {
        return 0;
    }
    return _fsys_to_K( f->used, f->units );
}

unsigned long long
netsnmp_fsys_avail_ull( netsnmp_fsys_info* f )
{
    if ( !f ) {
        return 0;
    }
    return _fsys_to_K( f->avail, f->units );
}

int netsnmp_fsys_size( netsnmp_fsys_info* f )
{
    unsigned long long v = netsnmp_fsys_size_ull( f );
    return ( int )v;
}

int netsnmp_fsys_used( netsnmp_fsys_info* f )
{
    unsigned long long v = netsnmp_fsys_used_ull( f );
    return ( int )v;
}

int netsnmp_fsys_avail( netsnmp_fsys_info* f )
{
    unsigned long long v = netsnmp_fsys_avail_ull( f );
    return ( int )v;
}

/* recalculate f->size_32, used_32, avail_32 and units_32 from f->size & comp.*/
void netsnmp_fsys_calculate32( netsnmp_fsys_info* f )
{
    unsigned long long s = f->size;
    unsigned shift = 0;

    while ( s > INT32_MAX ) {
        s = s >> 1;
        shift++;
    }

    f->size_32 = s;
    f->units_32 = f->units << shift;
    f->avail_32 = f->avail >> shift;
    f->used_32 = f->used >> shift;

    DEBUG_MSGTL( ( "fsys", "Results of 32-bit conversion: size %" PRIu64 " -> %lu;"
                           " units %" PRIu64 " -> %lu; avail %" PRIu64 " -> %lu;"
                           " used %" PRIu64 " -> %lu\n",
        ( uint64_t )f->size, f->size_32, ( uint64_t )f->units, f->units_32,
        ( uint64_t )f->avail, f->avail_32, ( uint64_t )f->used, f->used_32 ) );
}
