#include "CacheHandler.h"
#include "System/Util/Alarm.h"
#include "Api.h"
#include "System/Util/DefaultStore.h"
#include "DsAgent.h"
#include "Mib.h"
#include "System/Containers/Map.h"
#include "System/Util/Assert.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"

static Cache* _cacheHandler_head = NULL;
static int _cacheHandler_outstandingValid = 0;
static int _CacheHandler_load( Cache* cache );

#define CACHE_RELEASE_FREQUENCY 60 /* Check for expired caches every 60s */

void CacheHandler_releaseCachedResources( unsigned int regNo, void* clientargs );

/** @defgroup cache_handler cache_handler
 *  Maintains a cache of data for use by lower level handlers.
 *  @ingroup utilities
 *  This helper checks to see whether the data has been loaded "recently"
 *  (according to the timeout for that particular cache) and calls the
 *  registered "load_cache" routine if necessary.
 *  The lower handlers can then work with this local cached data.
 *
 *  A timeout value of -1 will cause CacheHandler_checkExpired() to
 *  always return true, and thus the cache will be reloaded for every
 *  request.
 *
 *  To minimze resource use by the agent, a periodic callback checks for
 *  expired caches, and will call the free_cache function for any expired
 *  cache.
 *
 *  The load_cache routine should return a negative number if the cache
 *  was not successfully loaded. 0 or any positive number indicates successs.
 *
 *
 *  Several flags can be set to affect the operations on the cache.
 *
 *  If CacheOperation_DONT_INVALIDATE_ON_SET is set, the free_cache method
 *  will not be called after a set request has processed. It is assumed that
 *  the lower mib handler using the cache has maintained cache consistency.
 *
 *  If CacheOperation_DONT_FREE_BEFORE_LOAD is set, the free_cache method
 *  will not be called before the load_cache method is called. It is assumed
 *  that the load_cache routine will properly deal with being called with a
 *  valid cache.
 *
 *  If CacheOperation_DONT_FREE_EXPIRED is set, the free_cache method will
 *  not be called with the cache expires. The expired flag will be set, but
 *  the valid flag will not be cleared. It is assumed that the load_cache
 *  routine will properly deal with being called with a valid cache.
 *
 *  If CacheOperation_PRELOAD is set when a the cache handler is created,
 *  the cache load routine will be called immediately.
 *
 *  If CacheOperation_DONT_AUTO_RELEASE is set, the periodic callback that
 *  checks for expired caches will skip the cache. The cache will only be
 *  checked for expiration when a request triggers the cache handler. This
 *  is useful if the cache has it's own periodic callback to keep the cache
 *  fresh.
 *
 *  If CacheOperation_AUTO_RELOAD is set, a timer will be set up to reload
 *  the cache when it expires. This is useful for keeping the cache fresh,
 *  even in the absence of incoming snmp requests.
 *
 *  If CacheOperation_RESET_TIMER_ON_USE is set, the expiry timer will be
 *  reset on each cache access. In practice the 'timeout' becomes a timer
 *  which triggers when the cache is no longer needed. This is useful
 *  if the cache is automatically kept synchronized: e.g. by receiving
 *  change notifications from Netlink, inotify or similar. This should
 *  not be used if cache is not synchronized automatically as it would
 *  result in stale cache information when if polling happens too fast.
 *
 *
 *  Here are some suggestions for some common situations.
 *
 *  Cached File:
 *      If your table is based on a file that may periodically change,
 *      you can test the modification date to see if the file has
 *      changed since the last cache load. To get the cache helper to call
 *      the load function for every request, set the timeout to -1, which
 *      will cause the cache to always report that it is expired. This means
 *      that you will want to prevent the agent from flushing the cache when
 *      it has expired, and you will have to flush it manually if you
 *      detect that the file has changed. To accomplish this, set the
 *      following flags:
 *
 *          CacheOperation_DONT_FREE_EXPIRED
 *          CacheOperation_DONT_AUTO_RELEASE
 *
 *
 *  Constant (periodic) reload:
 *      If you want the cache kept up to date regularly, even if no requests
 *      for the table are received, you can have your cache load routine
 *      called periodically. This is very useful if you need to monitor the
 *      data for changes (eg a <i>LastChanged</i> object). You will need to
 *      prevent the agent from flushing the cache when it expires. Set the
 *      cache timeout to the frequency, in seconds, that you wish to
 *      reload your cache, and set the following flags:
 *
 *          CacheOperation_DONT_FREE_EXPIRED
 *          CacheOperation_DONT_AUTO_RELEASE
 *          CacheOperation_AUTO_RELOAD
 *
 *  Dynamically updated, unloaded after timeout:
 *      If the cache is kept up to date dynamically by listening for
 *      change notifications somehow, but it should not be in memory
 *      if it's not needed. Set the following flag:
 *
 *          CacheOperation_RESET_TIMER_ON_USE
 *
 *  @{
 */

static void
_CacheHandler_free2( Cache* cache );

/** get cache head
 * @internal
 * unadvertised function to get cache head. You really should not
 * do this, since the internal storage mechanism might change.
 */
Cache*
CacheHandler_getHead( void )
{
    return _cacheHandler_head;
}

/** find existing cache
 */
Cache*
CacheHandler_findByOid( const oid* rootoid, int rootoid_len )
{
    Cache* cache;

    for ( cache = _cacheHandler_head; cache; cache = cache->next ) {
        if ( 0 == Api_oidEquals( cache->rootoid, cache->rootoid_len,
                      rootoid, rootoid_len ) )
            return cache;
    }

    return NULL;
}

/** returns a cache
 */
Cache*
CacheHandler_create( int timeout, CacheLoadFT* load_hook,
    CacheFreeFT* free_hook,
    const oid* rootoid, int rootoid_len )
{
    Cache* cache = NULL;

    cache = MEMORY_MALLOC_TYPEDEF( Cache );
    if ( NULL == cache ) {
        Logger_log( LOGGER_PRIORITY_ERR, "malloc error in CacheHandler_create\n" );
        return NULL;
    }
    cache->timeout = timeout;
    cache->load_cache = load_hook;
    cache->free_cache = free_hook;
    cache->enabled = 1;

    if ( 0 == cache->timeout )
        cache->timeout = DefaultStore_getInt( DsStore_APPLICATION_ID,
            DsAgentInterger_CACHE_TIMEOUT );

    /*
     * Add the registered OID information, and tack
     * this onto the list for cache SNMP management
     *
     * Note that this list is not ordered.
     *    table_iterator rules again!
     */
    if ( rootoid ) {
        cache->rootoid = Api_duplicateObjid( rootoid, rootoid_len );
        cache->rootoid_len = rootoid_len;
        cache->next = _cacheHandler_head;
        if ( _cacheHandler_head )
            _cacheHandler_head->prev = cache;
        _cacheHandler_head = cache;
    }

    return cache;
}

static Cache*
_CacheHandler_ref( Cache* cache )
{
    cache->refcnt++;
    return cache;
}

static void
_CacheHandler_deref( Cache* cache )
{
    if ( --cache->refcnt == 0 ) {
        CacheHandler_remove( cache );
        CacheHandler_free( cache );
    }
}

/** frees a cache
 */
int CacheHandler_free( Cache* cache )
{
    Cache* pos;

    if ( NULL == cache )
        return ErrorCode_SUCCESS;

    for ( pos = _cacheHandler_head; pos; pos = pos->next ) {
        if ( pos == cache ) {
            size_t out_len = 0;
            size_t buf_len = 0;
            char* buf = NULL;

            Mib_sprintReallocObjid2( ( u_char** )&buf, &buf_len, &out_len,
                1, pos->rootoid, pos->rootoid_len );
            Logger_log( LOGGER_PRIORITY_WARNING,
                "not freeing cache with root OID %s (still in list)\n",
                buf );
            free( buf );
            return PRIOT_ERR_GENERR;
        }
    }

    if ( 0 != cache->timer_id )
        CacheHandler_timerStop( cache );

    if ( cache->valid )
        _CacheHandler_free2( cache );

    if ( cache->timestampM )
        free( cache->timestampM );

    if ( cache->rootoid )
        free( cache->rootoid );

    free( cache );

    return ErrorCode_SUCCESS;
}

/** removes a cache
 */
int CacheHandler_remove( Cache* cache )
{
    Cache *cur, *prev;

    if ( !cache || !_cacheHandler_head )
        return -1;

    if ( cache == _cacheHandler_head ) {
        _cacheHandler_head = _cacheHandler_head->next;
        if ( _cacheHandler_head )
            _cacheHandler_head->prev = NULL;
        return 0;
    }

    prev = _cacheHandler_head;
    cur = _cacheHandler_head->next;
    for ( ; cur; prev = cur, cur = cur->next ) {
        if ( cache == cur ) {
            prev->next = cur->next;
            if ( cur->next )
                cur->next->prev = cur->prev;
            return 0;
        }
    }
    return -1;
}

/** callback function to call cache load function */
static void
_CacheHandler_timerReload( unsigned int regNo, void* clientargs )
{
    Cache* cache = ( Cache* )clientargs;

    DEBUG_MSGT( ( "cache_timer:start", "loading cache %p\n", cache ) );

    cache->expired = 1;

    _CacheHandler_load( cache );
}

/** starts the recurring cache_load callback */
unsigned int
CacheHandler_timerStart( Cache* cache )
{
    if ( NULL == cache )
        return 0;

    DEBUG_MSGTL( ( "cache_timer:start", "OID: " ) );
    DEBUG_MSGOID( ( "cache_timer:start", cache->rootoid, cache->rootoid_len ) );
    DEBUG_MSG( ( "cache_timer:start", "\n" ) );

    if ( 0 != cache->timer_id ) {
        Logger_log( LOGGER_PRIORITY_WARNING, "cache has existing timer id.\n" );
        return cache->timer_id;
    }

    if ( !( cache->flags & CacheOperation_AUTO_RELOAD ) ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "cache_timer_start called but auto_reload not set.\n" );
        return 0;
    }

    cache->timer_id = Alarm_register( cache->timeout, AlarmFlag_REPEAT,
        _CacheHandler_timerReload, cache );
    if ( 0 == cache->timer_id ) {
        Logger_log( LOGGER_PRIORITY_ERR, "could not register alarm\n" );
        return 0;
    }

    cache->flags &= ~CacheOperation_AUTO_RELOAD;
    DEBUG_MSGT( ( "cache_timer:start",
        "starting timer %lu for cache %p\n", cache->timer_id, cache ) );
    return cache->timer_id;
}

/** stops the recurring cache_load callback */
void CacheHandler_timerStop( Cache* cache )
{
    if ( NULL == cache )
        return;

    if ( 0 == cache->timer_id ) {
        Logger_log( LOGGER_PRIORITY_WARNING, "cache has no timer id.\n" );
        return;
    }

    DEBUG_MSGT( ( "cache_timer:stop",
        "stopping timer %lu for cache %p\n", cache->timer_id, cache ) );

    Alarm_unregister( cache->timer_id );
    cache->flags |= CacheOperation_AUTO_RELOAD;
}

/** returns a cache handler that can be injected into a given handler chain.
 */
MibHandler*
CacheHandler_handlerGet( Cache* cache )
{
    MibHandler* ret = NULL;

    ret = AgentHandler_createHandler( "cacheHandler",
        CacheHandler_helperHandler );
    if ( ret ) {
        ret->flags |= MIB_HANDLER_AUTO_NEXT;
        ret->myvoid = ( void* )cache;

        if ( NULL != cache ) {
            if ( ( cache->flags & CacheOperation_PRELOAD ) && !cache->valid ) {
                /*
                 * load cache, ignore rc
                 * (failed load doesn't affect registration)
                 */
                ( void )_CacheHandler_load( cache );
            }
            if ( cache->flags & CacheOperation_AUTO_RELOAD )
                CacheHandler_timerStart( cache );
        }
    }
    return ret;
}

/** Makes sure that memory allocated for the cache is freed when the handler
 *  is unregistered.
 */
void CacheHandler_handlerOwnsCache( MibHandler* handler )
{
    Assert_assert( handler->myvoid );
    ( ( Cache* )handler->myvoid )->refcnt++;
    handler->data_clone = ( void* ( * )( void* ))_CacheHandler_ref;
    handler->data_free = ( void ( * )( void* ) )_CacheHandler_deref;
}

/** returns a cache handler that can be injected into a given handler chain.
 */
MibHandler*
CacheHandler_getCacheHandler( int timeout, CacheLoadFT* load_hook,
    CacheFreeFT* free_hook,
    const oid* rootoid, int rootoid_len )
{
    MibHandler* ret = NULL;
    Cache* cache = NULL;

    ret = CacheHandler_handlerGet( NULL );
    if ( ret ) {
        cache = CacheHandler_create( timeout, load_hook, free_hook,
            rootoid, rootoid_len );
        ret->myvoid = ( void* )cache;
        CacheHandler_handlerOwnsCache( ret );
    }
    return ret;
}

/** functionally the same as calling AgentHandler_registerHandler() but also
 * injects a cache handler at the same time for you. */
int CacheHandler_handlerRegister( HandlerRegistration* reginfo,
    Cache* cache )
{
    MibHandler* handler = NULL;
    handler = CacheHandler_handlerGet( cache );

    AgentHandler_injectHandler( reginfo, handler );
    return AgentHandler_registerHandler( reginfo );
}

/** functionally the same as calling AgentHandler_registerHandler() but also
 * injects a cache handler at the same time for you. */
int CacheHandler_registerCacheHandler( HandlerRegistration* reginfo,
    int timeout, CacheLoadFT* load_hook,
    CacheFreeFT* free_hook )
{
    MibHandler* handler = NULL;
    handler = CacheHandler_getCacheHandler( timeout, load_hook, free_hook,
        reginfo->rootoid,
        reginfo->rootoid_len );

    AgentHandler_injectHandler( reginfo, handler );
    return AgentHandler_registerHandler( reginfo );
}

static char*
_CacheHandler_buildCacheName( const char* name )
{
    char* dup = ( char* )malloc( strlen( name ) + strlen( CACHE_NAME ) + 2 );
    if ( NULL == dup )
        return NULL;
    sprintf( dup, "%s:%s", CACHE_NAME, name );
    return dup;
}

/** Insert the cache information for a given request (PDU) */
void CacheHandler_reqinfoInsert( Cache* cache,
    AgentRequestInfo* reqinfo,
    const char* name )
{
    char* cache_name = _CacheHandler_buildCacheName( name );
    if ( NULL == Agent_getListData( reqinfo, cache_name ) ) {
        DEBUG_MSGTL( ( "verbose:helper:cache_handler", " adding '%s' to %p\n",
            cache_name, reqinfo ) );
        Agent_addListData( reqinfo,
            Map_newElement( cache_name,
                               cache, NULL ) );
    }
    MEMORY_FREE( cache_name );
}

/** Extract the cache information for a given request (PDU) */
Cache*
CacheHandler_reqinfoExtract( AgentRequestInfo* reqinfo,
    const char* name )
{
    Cache* result;
    char* cache_name = _CacheHandler_buildCacheName( name );
    result = ( Cache* )Agent_getListData( reqinfo, cache_name );
    MEMORY_FREE( cache_name );
    return result;
}

/** Extract the cache information for a given request (PDU) */
Cache*
CacheHandler_extractCacheInfo( AgentRequestInfo* reqinfo )
{
    return CacheHandler_reqinfoExtract( reqinfo, CACHE_NAME );
}

/** Check if the cache timeout has passed. Sets and return the expired flag. */
int CacheHandler_checkExpired( Cache* cache )
{
    if ( NULL == cache )
        return 0;
    if ( cache->expired )
        return 1;
    if ( !cache->valid || ( NULL == cache->timestampM ) || ( -1 == cache->timeout ) )
        cache->expired = 1;
    else
        cache->expired = Time_readyMonotonic( cache->timestampM,
            1000 * cache->timeout );

    return cache->expired;
}

/** Reload the cache if required */
int CacheHandler_checkAndReload( Cache* cache )
{
    if ( !cache ) {
        DEBUG_MSGT( ( "helper:cache_handler", " no cache\n" ) );
        return 0; /* ?? or -1 */
    }
    if ( !cache->valid || CacheHandler_checkExpired( cache ) )
        return _CacheHandler_load( cache );
    else {
        DEBUG_MSGT( ( "helper:cache_handler", " cached (%d)\n",
            cache->timeout ) );
        return 0;
    }
}

/** Is the cache valid for a given request? */
int CacheHandler_isValid( AgentRequestInfo* reqinfo,
    const char* name )
{
    Cache* cache = CacheHandler_reqinfoExtract( reqinfo, name );
    return ( cache && cache->valid );
}

/** Is the cache valid for a given request?
 * for backwards compatability. CacheHandler_isValid() is preferred.
 */
int CacheHandler_isCacheValid( AgentRequestInfo* reqinfo )
{
    return CacheHandler_isValid( reqinfo, CACHE_NAME );
}

/** Implements the cache handler */
int CacheHandler_helperHandler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    char addrstr[ 32 ];

    Cache* cache = NULL;
    HandlerArgs cache_hint;

    DEBUG_MSGTL( ( "helper:cache_handler", "Got request (%d) for %s: ",
        reqinfo->mode, reginfo->handlerName ) );
    DEBUG_MSGOID( ( "helper:cache_handler", reginfo->rootoid,
        reginfo->rootoid_len ) );
    DEBUG_MSG( ( "helper:cache_handler", "\n" ) );

    Assert_assert( handler->flags & MIB_HANDLER_AUTO_NEXT );

    cache = ( Cache* )handler->myvoid;
    if ( DefaultStore_getBoolean( DsStore_APPLICATION_ID,
             DsAgentBoolean_NO_CACHING )
        || !cache || !cache->enabled || !cache->load_cache ) {
        DEBUG_MSGT( ( "helper:cache_handler", " caching disabled or "
                                              "cache not found, disabled or had no load method\n" ) );
        return PRIOT_ERR_NOERROR;
    }
    snprintf( addrstr, sizeof( addrstr ), "%ld", ( long int )cache );
    DEBUG_MSGTL( ( "helper:cache_handler", "using cache %s: ", addrstr ) );
    DEBUG_MSGOID( ( "helper:cache_handler", cache->rootoid, cache->rootoid_len ) );
    DEBUG_MSG( ( "helper:cache_handler", "\n" ) );

    /*
     * Make the handler-chain parameters available to
     * the cache_load hook routine.
     */
    cache_hint.handler = handler;
    cache_hint.reginfo = reginfo;
    cache_hint.reqinfo = reqinfo;
    cache_hint.requests = requests;
    cache->cache_hint = &cache_hint;

    switch ( reqinfo->mode ) {

    case MODE_GET:
    case MODE_GETNEXT:
    case MODE_GETBULK:
    case MODE_SET_RESERVE1:

        /*
         * only touch cache once per pdu request, to prevent a cache
         * reload while a module is using cached data.
         *
         * XXX: this won't catch a request reloading the cache while
         * a previous (delegated) request is still using the cache.
         * maybe use a reference counter?
         */
        if ( CacheHandler_isValid( reqinfo, addrstr ) )
            break;

        /*
         * call the load hook, and update the cache timestamp.
         * If it's not already there, add to reqinfo
         */
        CacheHandler_checkAndReload( cache );
        CacheHandler_reqinfoInsert( cache, reqinfo, addrstr );
        /** next handler called automatically - 'AUTO_NEXT' */
        break;

    case MODE_SET_RESERVE2:
    case MODE_SET_FREE:
    case MODE_SET_ACTION:
    case MODE_SET_UNDO:
        Assert_assert( CacheHandler_isValid( reqinfo, addrstr ) );
        /** next handler called automatically - 'AUTO_NEXT' */
        break;

    /*
         * A (successful) SET request wouldn't typically trigger a reload of
         *  the cache, but might well invalidate the current contents.
         * Only do this on the last pass through.
         */
    case MODE_SET_COMMIT:
        if ( cache->valid && !( cache->flags & CacheOperation_DONT_INVALIDATE_ON_SET ) ) {
            cache->free_cache( cache, cache->magic );
            cache->valid = 0;
        }
        /** next handler called automatically - 'AUTO_NEXT' */
        break;

    default:
        Logger_log( LOGGER_PRIORITY_WARNING, "cache_handler: Unrecognised mode (%d)\n",
            reqinfo->mode );
        Agent_requestSetErrorAll( requests, PRIOT_ERR_GENERR );
        return PRIOT_ERR_GENERR;
    }
    if ( cache->flags & CacheOperation_RESET_TIMER_ON_USE )
        Time_setMonotonicMarker( &cache->timestampM );
    return PRIOT_ERR_NOERROR;
}

static void
_CacheHandler_free2( Cache* cache )
{
    if ( NULL != cache->free_cache ) {
        cache->free_cache( cache, cache->magic );
        cache->valid = 0;
    }
}

static int
_CacheHandler_load( Cache* cache )
{
    int ret = -1;

    /*
     * If we've got a valid cache, then release it before reloading
     */
    if ( cache->valid && ( !( cache->flags & CacheOperation_DONT_FREE_BEFORE_LOAD ) ) )
        _CacheHandler_free2( cache );

    if ( cache->load_cache )
        ret = cache->load_cache( cache, cache->magic );
    if ( ret < 0 ) {
        DEBUG_MSGT( ( "helper:cache_handler", " load failed (%d)\n", ret ) );
        cache->valid = 0;
        return ret;
    }
    cache->valid = 1;
    cache->expired = 0;

    /*
     * If we didn't previously have any valid caches outstanding,
     *   then schedule a pass of the auto-release routine.
     */
    if ( ( !_cacheHandler_outstandingValid ) && ( !( cache->flags & CacheOperation_DONT_FREE_EXPIRED ) ) ) {
        Alarm_register( CACHE_RELEASE_FREQUENCY,
            0, CacheHandler_releaseCachedResources, NULL );
        _cacheHandler_outstandingValid = 1;
    }
    Time_setMonotonicMarker( &cache->timestampM );
    DEBUG_MSGT( ( "helper:cache_handler", " loaded (%d)\n", cache->timeout ) );

    return ret;
}

/** run regularly to automatically release cached resources.
 * xxx - method to prevent cache from expiring while a request
 *     is being processed (e.g. delegated request). proposal:
 *     set a flag, which would be cleared when request finished
 *     (which could be acomplished by a dummy data list item in
 *     agent req info & custom free function).
 */
void CacheHandler_releaseCachedResources( unsigned int regNo, void* clientargs )
{
    Cache* cache = NULL;

    _cacheHandler_outstandingValid = 0;
    DEBUG_MSGTL( ( "helper:cache_handler", "running auto-release\n" ) );
    for ( cache = _cacheHandler_head; cache; cache = cache->next ) {
        DEBUG_MSGTL( ( "helper:cache_handler", " checking %p (flags 0x%x)\n",
            cache, cache->flags ) );
        if ( cache->valid && !( cache->flags & CacheOperation_DONT_AUTO_RELEASE ) ) {
            DEBUG_MSGTL( ( "helper:cache_handler", "  releasing %p\n", cache ) );
            /*
             * Check to see if this cache has timed out.
             * If so, release the cached resources.
             * Otherwise, note that we still have at
             *   least one active cache.
             */
            if ( CacheHandler_checkExpired( cache ) ) {
                if ( !( cache->flags & CacheOperation_DONT_FREE_EXPIRED ) )
                    _CacheHandler_free2( cache );
            } else {
                _cacheHandler_outstandingValid = 1;
            }
        }
    }
    /*
     * If there are any caches still valid & active,
     *   then schedule another pass.
     */
    if ( _cacheHandler_outstandingValid ) {
        Alarm_register( CACHE_RELEASE_FREQUENCY,
            0, CacheHandler_releaseCachedResources, NULL );
    }
}
