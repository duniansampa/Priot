/** \file Callback.c
 *
 *  \note the inline callback methods use major/minor to index into matrix.
 *        all users in this function do range checking before calling these
 *        functions, so it is redundant for them to check again.
 *
 *  \author Dunian Coutinho Sampa (duniansampa)
 *  \bug    No known bugs.
 */

#include "Callback.h"
#include "System/Util/Assert.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"

/** ============================[ Private Variables ]================== */

/**
 *  if _callback_needInit == 1, then Callback is not initialized.
 *  if _callback_needInit == 0, then Callback is initialized.
 */
static int _callback_needInit = 1;

/** The matrix of callbaks list */
static struct Callback_s* _callback_theCallbacks[ CALLBACK_MAX_CALLBACK_IDS ][ CALLBACK_MAX_CALLBACK_SUBIDS ];

/*
 * Extremely simplistic locking, just to find problems were the
 * callback list is modified while being traversed. Not intended
 * to do any real protection, or in any way imply that this code
 * has been evaluated for use in a multi-threaded environment.
 * It has been updated to a lock per callback, since a particular callback may trigger
 * registration/unregistration of other callbacks (eg AgentX subagents do this).
 */
static int _callback_locks[ CALLBACK_MAX_CALLBACK_IDS ][ CALLBACK_MAX_CALLBACK_SUBIDS ];

static const char* _callback_types[ CALLBACK_MAX_CALLBACK_IDS ] = { "LIB", "APP" };

static const char* _callback_lib[ CALLBACK_MAX_CALLBACK_SUBIDS ] = {
    "POST_READ_CONFIG", /* 0 */
    "STORE_DATA", /* 1 */
    "SHUTDOWN", /* 2 */
    "POST_PREMIB_READ_CONFIG", /* 3 */
    "LOGGING", /* 4 */
    "SESSION_INIT", /* 5 */
    NULL, /* 6 */
    NULL, /* 7 */
    NULL, /* 8 */
    NULL, /* 9 */
    NULL, /* 10 */
    NULL, /* 11 */
    NULL, /* 12 */
    NULL, /* 13 */
    NULL, /* 14 */
    NULL /* 15 */
};

/** ==================[ Private Functions Prototypes ]================== */

static int _Callback_lock( int major, int minor, const char* warn, int do_assert );

/**
 * Register a callback function.
 *
 * @param major        - Major callback event type.
 * @param minor        - Minor callback event type.
 * @param callbackFunc - Callback function being registered.
 * @param callbackFuncArg  - Argument that will be passed to the callback function.
 * @param priority     - Handler invocation priority. When multiple handlers have
 *                       been registered for the same (major, minor) callback event type, handlers
 *                       with the numerically lowest priority will be invoked first. Handlers with
 *                       identical priority are invoked in the order they have been registered.
 *
 * \see Callback_register
 */
static int _Callback_register( int major, int minor, Callback_f* callbackFunc, void* callbackFuncArg, int priority );

static int _Callback_lock( int major, int minor, const char* warn, int doAssert );

static void _Callback_unlock( int major, int minor );

/** ============================[ Macros ============================ */

#define CALLBACK_LOCK( maj, min ) ++_callback_locks[ maj ][ min ]
#define CALLBACK_UNLOCK( maj, min ) --_callback_locks[ maj ][ min ]
#define CALLBACK_LOCK_COUNT( maj, min ) _callback_locks[ maj ][ min ]

/** =============================[ Public Functions ]================== */

void Callback_init()
{
    /*
     * (poses a problem if you put Callback_initCallbacks() inside of
     * Api_init() and then want the app to register a callback before
     * Api_init() is called in the first place.  -- Wes
     */
    if ( 0 == _callback_needInit )
        return;

    _callback_needInit = 0;

    memset( _callback_theCallbacks, 0, sizeof( _callback_theCallbacks ) );
    memset( _callback_locks, 0, sizeof( _callback_locks ) );

    DEBUG_MSGTL( ( "callback", "initialized\n" ) );
}

int Callback_register( int major, int minor, Callback_f* callbackFun, void* callbackFuncArg )
{
    return _Callback_register( major, minor, callbackFun, callbackFuncArg,
        CALLBACK_PRIORITY_DEFAULT );
}

int Callback_call( int major, int minor, void* extraArgument )
{
    struct Callback_s* scp;
    unsigned int count = 0;

    if ( major >= CALLBACK_MAX_CALLBACK_IDS || minor >= CALLBACK_MAX_CALLBACK_SUBIDS ) {
        return ErrorCode_GENERR;
    }

    if ( _callback_needInit )
        Callback_init();

    _Callback_lock( major, minor, "Callback_callCallbacks", 1 );

    DEBUG_MSGTL( ( "callback", "START calling callbacks for maj=%d min=%d\n",
        major, minor ) );

    /*
     * for each registered callback of type major and minor
     */
    for ( scp = _callback_theCallbacks[ major ][ minor ]; scp != NULL; scp = scp->next ) {

        /*
         * skip unregistered callbacks
         */
        if ( NULL == scp->callbackFun )
            continue;

        DEBUG_MSGTL( ( "callback", "calling a callback for maj=%d min=%d\n",
            major, minor ) );

        /*
         * call them
         */
        ( *( scp->callbackFun ) )( major, minor, extraArgument,
            scp->callbackFuncArg );
        count++;
    }

    DEBUG_MSGTL( ( "callback",
        "END calling callbacks for maj=%d min=%d (%d called)\n",
        major, minor, count ) );

    _Callback_unlock( major, minor );
    return ErrorCode_SUCCESS;
}

int Callback_count( int major, int minor )
{
    int count = 0;
    struct Callback_s* scp;

    if ( major >= CALLBACK_MAX_CALLBACK_IDS || minor >= CALLBACK_MAX_CALLBACK_SUBIDS ) {
        return ErrorCode_GENERR;
    }

    if ( _callback_needInit )
        Callback_init();

    for ( scp = _callback_theCallbacks[ major ][ minor ]; scp != NULL; scp = scp->next ) {
        count++;
    }

    return count;
}

int Callback_isRegistered( int major, int minor )
{
    if ( major >= CALLBACK_MAX_CALLBACK_IDS || minor >= CALLBACK_MAX_CALLBACK_SUBIDS ) {
        return ErrorCode_GENERR;
    }

    if ( _callback_needInit )
        Callback_init();

    if ( _callback_theCallbacks[ major ][ minor ] != NULL ) {
        return ErrorCode_SUCCESS;
    }

    return ErrorCode_GENERR;
}

int Callback_unregister( int major, int minor, Callback_f* callbackFun, void* callbackFuncArg, int matchArg )
{
    struct Callback_s* scp = _callback_theCallbacks[ major ][ minor ];
    struct Callback_s** prevNext = &( _callback_theCallbacks[ major ][ minor ] );
    int count = 0;

    if ( major >= CALLBACK_MAX_CALLBACK_IDS || minor >= CALLBACK_MAX_CALLBACK_SUBIDS )
        return ErrorCode_GENERR;

    if ( _callback_needInit )
        Callback_init();

    _Callback_lock( major, minor, "Callback_unregisterCallback", 1 );

    while ( scp != NULL ) {
        if ( ( scp->callbackFun == callbackFun ) && ( !matchArg || ( scp->callbackFuncArg == callbackFuncArg ) ) ) {
            DEBUG_MSGTL( ( "callback", "unregistering (%d,%d) at %p\n", major,
                minor, scp ) );
            if ( 1 == CALLBACK_LOCK_COUNT( major, minor ) ) {
                *prevNext = scp->next;
                MEMORY_FREE( scp );
                scp = *prevNext;
            } else {
                scp->callbackFun = NULL;
                /** set cleanup flag? */
            }
            count++;
        } else {
            prevNext = &( scp->next );
            scp = scp->next;
        }
    }

    _Callback_unlock( major, minor );
    return count;
}

int Callback_clearCallbackFuncArg( void* callbackFuncArg, int i, int j )
{
    struct Callback_s* scp = NULL;
    int rc = 0;

    /*
     * don't init i and j before loop, since the caller specified
     * the starting point explicitly. But *after* the i loop has
     * finished executing once, init j to 0 for the next pass
     * through the subids.
     */
    for ( ; i < CALLBACK_MAX_CALLBACK_IDS; i++, j = 0 ) {
        for ( ; j < CALLBACK_MAX_CALLBACK_SUBIDS; j++ ) {
            scp = _callback_theCallbacks[ i ][ j ];
            while ( scp != NULL ) {
                if ( ( NULL != scp->callbackFun ) && ( scp->callbackFuncArg != NULL ) && ( scp->callbackFuncArg == callbackFuncArg ) ) {
                    DEBUG_MSGTL( ( "9:callback", "  clearing %p at [%d,%d]\n", callbackFuncArg, i, j ) );
                    scp->callbackFuncArg = NULL;
                    ++rc;
                }
                scp = scp->next;
            }
        }
    }

    if ( 0 != rc ) {
        DEBUG_MSGTL( ( "callback", "removed %d callback function args\n", rc ) );
    }

    return rc;
}

void Callback_clear( void )
{
    unsigned int i = 0, j = 0;
    struct Callback_s* scp = NULL;

    if ( _callback_needInit )
        Callback_init();

    DEBUG_MSGTL( ( "callback", "clear callback\n" ) );
    for ( i = 0; i < CALLBACK_MAX_CALLBACK_IDS; i++ ) {
        for ( j = 0; j < CALLBACK_MAX_CALLBACK_SUBIDS; j++ ) {
            _Callback_lock( i, j, "clear_callback", 1 );
            scp = _callback_theCallbacks[ i ][ j ];
            while ( scp != NULL ) {
                _callback_theCallbacks[ i ][ j ] = scp->next;
                /*
                 * if there is a callback function arg, check for duplicates
                 * and then free it.
                 */
                if ( ( NULL != scp->callbackFun ) && ( scp->callbackFuncArg != NULL ) ) {
                    void* tmp_arg;
                    /*
                     * save the callback function arg, then set it to null so that it
                     * won't look like a duplicate, then check for duplicates
                     * starting at the current i,j (earlier dups should have
                     * already been found) and free the pointer.
                     */
                    tmp_arg = scp->callbackFuncArg;
                    scp->callbackFuncArg = NULL;
                    DEBUG_MSGTL( ( "9:callback", "  freeing %p at [%d,%d]\n", tmp_arg, i, j ) );
                    ( void )Callback_clearCallbackFuncArg( tmp_arg, i, j );
                    free( tmp_arg );
                }
                MEMORY_FREE( scp );
                scp = _callback_theCallbacks[ i ][ j ];
            }
            _Callback_unlock( i, j );
        }
    }
}

Callback* Callback_getList( int major, int minor )
{
    if ( _callback_needInit )
        Callback_init();

    return ( _callback_theCallbacks[ major ][ minor ] );
}

/** =============================[ Private Functions ]================== */

static int _Callback_register( int major, int minor, Callback_f* callbackFunc, void* callbackFuncArg, int priority )
{
    struct Callback_s *newscp = NULL, *scp = NULL;
    struct Callback_s** prevNext = &( _callback_theCallbacks[ major ][ minor ] );

    if ( major >= CALLBACK_MAX_CALLBACK_IDS || minor >= CALLBACK_MAX_CALLBACK_SUBIDS ) {
        return ErrorCode_GENERR;
    }

    if ( _callback_needInit )
        Callback_init();

    _Callback_lock( major, minor, "Callback_registerCallback2", 1 );

    if ( ( newscp = MEMORY_MALLOC_STRUCT( Callback_s ) ) == NULL ) {
        _Callback_unlock( major, minor );
        return ErrorCode_GENERR;
    } else {
        newscp->priority = priority;
        newscp->callbackFuncArg = callbackFuncArg;
        newscp->callbackFun = callbackFunc;
        newscp->next = NULL;

        for ( scp = _callback_theCallbacks[ major ][ minor ]; scp != NULL;
              scp = scp->next ) {
            if ( newscp->priority < scp->priority ) {
                newscp->next = scp;
                break;
            }
            prevNext = &( scp->next );
        }

        *prevNext = newscp;

        DEBUG_MSGTL( ( "callback", "registered (%d,%d) at %p with priority %d\n",
            major, minor, newscp, priority ) );
        _Callback_unlock( major, minor );
        return ErrorCode_SUCCESS;
    }
}

static int _Callback_lock( int major, int minor, const char* warn, int doAssert )
{
    int lock_holded = 0;
    struct timeval lock_time = { 0, 1000 };

    DEBUG_MSGTL( ( "9:callback:lock", "locked (%s,%s)\n",
        _callback_types[ major ], ( CallbackMajor_LIBRARY == major ) ? UTILITIES_STRING_OR_NULL( _callback_lib[ minor ] ) : "null" ) );

    while ( CALLBACK_LOCK_COUNT( major, minor ) >= 1 && ++lock_holded < 100 )
        select( 0, NULL, NULL, NULL, &lock_time );

    if ( lock_holded >= 100 ) {
        if ( NULL != warn )
            Logger_log( LOGGER_PRIORITY_WARNING,
                "lock in Callback_lock sleeps more than 100 milliseconds in %s\n", warn );
        if ( doAssert )
            Assert_assert( lock_holded < 100 );

        return 1;
    }

    CALLBACK_LOCK( major, minor );
    return 0;
}

static void _Callback_unlock( int major, int minor )
{

    CALLBACK_UNLOCK( major, minor );

    DEBUG_MSGTL( ( "9:callback:lock", "unlocked (%s,%s)\n",
        _callback_types[ major ], ( CallbackMajor_LIBRARY == major ) ? UTILITIES_STRING_OR_NULL( _callback_lib[ minor ] ) : "null" ) );
}
