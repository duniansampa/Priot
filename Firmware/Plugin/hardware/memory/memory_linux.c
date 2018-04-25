

#include "siglog/agent/hardware/memory.h"
#include "CacheHandler.h"
#include "Debug.h"
#include "Logger.h"
#include "System.h"

/*
 * Try to use an initial size that will cover default cases. We aren't talking
 * about huge files, so why fiddle about with reallocs?
 * I checked /proc/meminfo sizes on 3 different systems: 598, 644, 654
 * 
 * On newer systems, the size is up to around 930 (2.6.27 kernel)
 *   or 1160  (2.6.28 kernel)
 */
#define MEMINFO_INIT_SIZE 1279
#define MEMINFO_STEP_SIZE 256
#define MEMINFO_FILE "/proc/meminfo"

/*
     * Load the latest memory usage statistics
     */
int netsnmp_mem_arch_load( Cache* cache, void* magic )
{
    int statfd;
    static char* buff = NULL;
    static int bsize = 0;
    static int first = 1;
    ssize_t bytes_read;
    char* b;
    unsigned long memtotal = 0, memfree = 0, memshared = 0,
                  buffers = 0, cached = 0,
                  swaptotal = 0, swapfree = 0;

    netsnmp_memory_info* mem;

    /*
     * Retrieve the memory information from the underlying O/S...
     */
    if ( ( statfd = open( MEMINFO_FILE, O_RDONLY, 0 ) ) == -1 ) {
        Logger_logPerror( MEMINFO_FILE );
        return -1;
    }
    if ( bsize == 0 ) {
        bsize = MEMINFO_INIT_SIZE;
        buff = ( char* )malloc( bsize + 1 );
        if ( NULL == buff ) {
            Logger_log( LOGGER_PRIORITY_ERR, "malloc failed\n" );
            close( statfd );
            return -1;
        }
    }
    while ( ( bytes_read = read( statfd, buff, bsize ) ) == bsize ) {
        b = ( char* )realloc( buff, bsize + MEMINFO_STEP_SIZE + 1 );
        if ( NULL == b ) {
            Logger_log( LOGGER_PRIORITY_ERR, "malloc failed\n" );
            close( statfd );
            return -1;
        }
        buff = b;
        bsize += MEMINFO_STEP_SIZE;
        DEBUG_MSGTL( ( "mem", "/proc/meminfo buffer increased to %d\n", bsize ) );
        close( statfd );
        statfd = open( MEMINFO_FILE, O_RDONLY, 0 );
        if ( statfd == -1 ) {
            Logger_logPerror( MEMINFO_FILE );
            return -1;
        }
    }
    close( statfd );
    if ( bytes_read <= 0 ) {
        Logger_logPerror( MEMINFO_FILE );
    } else {
        buff[ bytes_read ] = '\0';
    }

    /*
     * ... parse this into a more useable form...
     */
    b = strstr( buff, "MemTotal: " );
    if ( b )
        sscanf( b, "MemTotal: %lu", &memtotal );
    else {
        if ( first )
            Logger_log( LOGGER_PRIORITY_ERR, "No MemTotal line in /proc/meminfo\n" );
    }
    b = strstr( buff, "MemFree: " );
    if ( b )
        sscanf( b, "MemFree: %lu", &memfree );
    else {
        if ( first )
            Logger_log( LOGGER_PRIORITY_ERR, "No MemFree line in /proc/meminfo\n" );
    }
    if ( 0 == System_osPrematch( "Linux", "2.4" ) ) {
        b = strstr( buff, "MemShared: " );
        if ( b )
            sscanf( b, "MemShared: %lu", &memshared );
        else if ( first )
            Logger_log( LOGGER_PRIORITY_ERR, "No MemShared line in /proc/meminfo\n" );
    } else {
        b = strstr( buff, "Shmem: " );
        if ( b )
            sscanf( b, "Shmem: %lu", &memshared );
        else if ( first )
            Logger_log( LOGGER_PRIORITY_ERR, "No Shmem line in /proc/meminfo\n" );
    }
    b = strstr( buff, "Buffers: " );
    if ( b )
        sscanf( b, "Buffers: %lu", &buffers );
    else {
        if ( first )
            Logger_log( LOGGER_PRIORITY_ERR, "No Buffers line in /proc/meminfo\n" );
    }
    b = strstr( buff, "Cached: " );
    if ( b )
        sscanf( b, "Cached: %lu", &cached );
    else {
        if ( first )
            Logger_log( LOGGER_PRIORITY_ERR, "No Cached line in /proc/meminfo\n" );
    }
    b = strstr( buff, "SwapTotal: " );
    if ( b )
        sscanf( b, "SwapTotal: %lu", &swaptotal );
    else {
        if ( first )
            Logger_log( LOGGER_PRIORITY_ERR, "No SwapTotal line in /proc/meminfo\n" );
    }
    b = strstr( buff, "SwapFree: " );
    if ( b )
        sscanf( b, "SwapFree: %lu", &swapfree );
    else {
        if ( first )
            Logger_log( LOGGER_PRIORITY_ERR, "No SwapFree line in /proc/meminfo\n" );
    }
    first = 0;

    /*
     * ... and save this in a standard form.
     */
    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 1 );
    if ( !mem ) {
        Logger_logPerror( "No Physical Memory info entry" );
    } else {
        if ( !mem->descr )
            mem->descr = strdup( "Physical memory" );
        mem->units = 1024;
        mem->size = memtotal;
        mem->free = memfree;
        mem->other = -1;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_VIRTMEM, 1 );
    if ( !mem ) {
        Logger_logPerror( "No Virtual Memory info entry" );
    } else {
        if ( !mem->descr )
            mem->descr = strdup( "Virtual memory" );
        mem->units = 1024;
        mem->size = memtotal + swaptotal;
        mem->free = memfree + swapfree;
        mem->other = -1;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SHARED, 1 );
    if ( !mem ) {
        Logger_logPerror( "No Shared Memory info entry" );
    } else {
        if ( !mem->descr )
            mem->descr = strdup( "Shared memory" );
        mem->units = 1024;
        mem->size = memshared;
        mem->free = 0; /* All in use */
        mem->other = -1;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_CACHED, 1 );
    if ( !mem ) {
        Logger_logPerror( "No Cached Memory info entry" );
    } else {
        if ( !mem->descr )
            mem->descr = strdup( "Cached memory" );
        mem->units = 1024;
        mem->size = cached;
        mem->free = 0; /* Report cached size/used as equal */
        mem->other = -1;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 1 );
    if ( !mem ) {
        Logger_logPerror( "No Swap info entry" );
    } else {
        if ( !mem->descr )
            mem->descr = strdup( "Swap space" );
        mem->units = 1024;
        mem->size = swaptotal;
        mem->free = swapfree;
        mem->other = -1;
    }

    mem = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_MBUF, 1 );
    if ( !mem ) {
        Logger_logPerror( "No Buffer, etc info entry" );
    } else {
        if ( !mem->descr )
            mem->descr = strdup( "Memory buffers" );
        mem->units = 1024;
        mem->size = memtotal; /* Traditionally we've always regarded
                                   all memory as potentially available
                                   for memory buffers. */
        mem->free = memtotal - buffers;
        mem->other = -1;
    }

    return 0;
}
