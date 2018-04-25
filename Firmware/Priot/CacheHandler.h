#ifndef CACHEHANDLER_H
#define CACHEHANDLER_H

#include "AgentHandler.h"
#include "Tools.h"
/*
 * This caching helper provides a generalised (SNMP-manageable) caching
 * mechanism.  Individual SNMP table and scalar/scalar group MIB
 * implementations can use data caching in a consistent manner, without
 * needing to handle the generic caching details themselves.
 */

#define CACHE_NAME "cacheInfo"

/*
 * Flags affecting cache handler operation
 */
enum CacheOperation_e {
    CacheOperation_DONT_INVALIDATE_ON_SET = 0x0001,
    CacheOperation_DONT_FREE_BEFORE_LOAD = 0x0002,
    CacheOperation_DONT_FREE_EXPIRED = 0x0004,
    CacheOperation_DONT_AUTO_RELEASE = 0x0008,
    CacheOperation_PRELOAD = 0x0010,
    CacheOperation_AUTO_RELOAD = 0x0020,
    CacheOperation_RESET_TIMER_ON_USE = 0x0040,
    CacheOperation_HINT_HANDLER_ARGS = 0x1000
};

typedef struct Cache_s Cache;

typedef int( CacheLoadFT )( Cache*, void* );

typedef void( CacheFreeFT )( Cache*, void* );

struct Cache_s {
    /** Number of handlers whose myvoid member points at this structure. */
    int refcnt;
    /*
    * For operation of the data caches
    */
    int flags;
    int enabled;
    int valid;
    char expired;
    int timeout; /* Length of time the cache is valid (in s) */
    markerT timestampM; /* When the cache was last loaded */
    u_long timer_id; /* periodic timer id */

    CacheLoadFT* load_cache;
    CacheFreeFT* free_cache;

    /*
    * void pointer for the user that created the cache.
    * You never know when it might not come in useful ....
    */
    void* magic;

    /*
    * hint from the cache helper. contains the standard
    * handler arguments.
    */
    HandlerArgs* cache_hint;

    /*
 * For SNMP-management of the data caches
 */
    Cache *next, *prev;
    oid* rootoid;
    int rootoid_len;
};

void CacheHandler_reqinfoInsert( Cache* cache,
    AgentRequestInfo* reqinfo,
    const char* name );
Cache*
CacheHandler_reqinfoExtract( AgentRequestInfo* reqinfo,
    const char* name );

Cache*
CacheHandler_extractCacheInfo( AgentRequestInfo* );

int CacheHandler_checkAndReload( Cache* cache );

int CacheHandler_checkExpired( Cache* cache );

int CacheHandler_isValid( AgentRequestInfo*,
    const char* name );

/** for backwards compat */
int CacheHandler_isCacheValid( AgentRequestInfo* );

MibHandler*
CacheHandler_getCacheHandler( int,
    CacheLoadFT*,
    CacheFreeFT*,
    const oid*,
    int );

int CacheHandler_registerCacheHandler( HandlerRegistration* reginfo,
    int,
    CacheLoadFT*,
    CacheFreeFT* );

NodeHandlerFT CacheHandler_helperHandler;

Cache*
CacheHandler_create( int timeout,
    CacheLoadFT* load_hook,
    CacheFreeFT* free_hook,
    const oid* rootoid,
    int rootoid_len );

int CacheHandler_remove( Cache* cache );

int CacheHandler_free( Cache* cache );

MibHandler*
CacheHandler_handlerGet( Cache* cache );

void CacheHandler_handlerOwnsCache( MibHandler* handler );

Cache*
CacheHandler_findByOid( const oid* rootoid,
    int rootoid_len );

unsigned int
CacheHandler_timerStart( Cache* cache );

void CacheHandler_timerStop( Cache* cache );

int CacheHandler_helperHandler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests );

Cache*
CacheHandler_getHead( void );

#endif // CACHEHANDLER_H
