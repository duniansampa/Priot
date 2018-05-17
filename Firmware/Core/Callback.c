#include "Callback.h"

#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"
#include "System/Util/Assert.h"
#include "Api.h"
#include "System/Util/Debug.h"

/*
 * the inline callback methods use major/minor to index into arrays.
 * all users in this function do range checking before calling these
 * functions, so it is redundant for them to check again. But if you
 * want to be paranoid, define this var, and additional range checks
 * will be performed.
 * #define NETSNMP_PARANOID_LEVEL_HIGH 1
 */

static int _callback_needInit = 1;
static struct Callback_GenCallback_s *_callback_thecallbacks[CALLBACK_MAX_CALLBACK_IDS][CALLBACK_MAX_CALLBACK_SUBIDS];

#define CALLBACK_NAME_LOGGING 1
static const char *_callback_types[CALLBACK_MAX_CALLBACK_IDS] = { "LIB", "APP" };
static const char *_callback_lib[CALLBACK_MAX_CALLBACK_SUBIDS] = {
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

/*
 * extremely simplistic locking, just to find problems were the
 * callback list is modified while being traversed. Not intended
 * to do any real protection, or in any way imply that this code
 * has been evaluated for use in a multi-threaded environment.
 * In 5.2, it was a single lock. For 5.3, it has been updated to
 * a lock per callback, since a particular callback may trigger
 * registration/unregistration of other callbacks (eg AgentX
 * subagents do this).
 */
#define LOCK_PER_CALLBACK_SUBID 1
static int _callback_locks[CALLBACK_MAX_CALLBACK_IDS][CALLBACK_MAX_CALLBACK_SUBIDS];
#define CALLBACK_LOCK(maj,min) ++_callback_locks[maj][min]
#define CALLBACK_UNLOCK(maj,min) --_callback_locks[maj][min]
#define CALLBACK_LOCK_COUNT(maj,min) _callback_locks[maj][min]


static int _Callback_lock(int major, int minor, const char* warn, int do_assert)
{
    int lock_holded=0;
    struct timeval lock_time = { 0, 1000 };

    DEBUG_MSGTL(("9:callback:lock", "locked (%s,%s)\n",
                _callback_types[major], (CALLBACK_LIBRARY == major) ?
                UTILITIES_STRING_OR_NULL(_callback_lib[minor]) : "null"));

    while (CALLBACK_LOCK_COUNT(major,minor) >= 1 && ++lock_holded < 100)
    select(0, NULL, NULL, NULL, &lock_time);

    if(lock_holded >= 100) {
        if (NULL != warn)
            Logger_log(LOGGER_PRIORITY_WARNING,
                     "lock in Callback_lock sleeps more than 100 milliseconds in %s\n", warn);
        if (do_assert)
            Assert_assert(lock_holded < 100);

        return 1;
    }

    CALLBACK_LOCK(major,minor);
    return 0;
}

void
static _Callback_unlock(int major, int minor)
{

    CALLBACK_UNLOCK(major,minor);

    DEBUG_MSGTL(("9:callback:lock", "unlocked (%s,%s)\n",
                _callback_types[major], (CALLBACK_LIBRARY == major) ?
                UTILITIES_STRING_OR_NULL(_callback_lib[minor]) : "null"));
}


/*
 * the chicken. or the egg.  You pick.
 */
void
Callback_initCallbacks()
{
    /*
     * (poses a problem if you put init_callbacks() inside of
     * init_snmp() and then want the app to register a callback before
     * init_snmp() is called in the first place.  -- Wes
     */
    if (0 == _callback_needInit)
        return;

    _callback_needInit = 0;

    memset(_callback_thecallbacks, 0, sizeof(_callback_thecallbacks));
    memset(_callback_locks, 0, sizeof(_callback_locks));


    DEBUG_MSGTL(("callback", "initialized\n"));
}

/**
 * This function registers a generic callback function.  The major and
 * minor values are used to set the new_callback function into a global
 * static multi-dimensional array of type struct Callback_GenCallback_s.
 * The function makes sure to append this callback function at the end
 * of the link list, snmp_gen_callback->next.
 *
 * @param major is the SNMP callback major type used
 * 		- CALLBACK_LIBRARY
 *              - CALLBACK_APPLICATION
 *
 * @param minor is the SNMP callback minor type used
 *		- CALLBACK_POST_READ_CONFIG
 *		- CALLBACK_STORE_DATA
 *		- CALLBACK_SHUTDOWN
 *		- CALLBACK_POST_PREMIB_READ_CONFIG
 *		- CALLBACK_LOGGING
 *		- CALLBACK_SESSION_INIT
 *
 * @param new_callback is the callback function that is registered.
 *
 * @param arg when not NULL is a void pointer used whenever new_callback
 *	function is exercised. Ownership is transferred to the twodimensional
 *      callback_thecallbacks[][] array. The function clear_callback() will deallocate
 *      the memory pointed at by calling free().
 *
 * @return
 *	Returns ErrorCode_GENERR if major is >= MAX_CALLBACK_IDS or minor is >=
 *	MAX_CALLBACK_SUBIDS or a snmp_gen_callback pointer could not be
 *	allocated, otherwise ErrorCode_SUCCESS is returned.
 * 	- \#define MAX_CALLBACK_IDS    2
 *	- \#define MAX_CALLBACK_SUBIDS 16
 *
 * @see snmp_call_callbacks
 * @see Callback_unregisterCallback
 */
int
Callback_registerCallback(int major, int minor, Callback_CallbackFT * new_callback,
                       void *arg)
{
    return Callback_registerCallback2( major, minor, new_callback, arg,
                                      CALLBACK_DEFAULT_PRIORITY);
}

/**
 * Register a callback function.
 *
 * @param major        Major callback event type.
 * @param minor        Minor callback event type.
 * @param new_callback Callback function being registered.
 * @param arg          Argument that will be passed to the callback function.
 * @param priority     Handler invocation priority. When multiple handlers have
 *   been registered for the same (major, minor) callback event type, handlers
 *   with the numerically lowest priority will be invoked first. Handlers with
 *   identical priority are invoked in the order they have been registered.
 *
 * @see Callback_registerCallback
 */
int
Callback_registerCallback2(int major, int minor, Callback_CallbackFT * new_callback,
                          void *arg, int priority)
{
    struct Callback_GenCallback_s *newscp = NULL, *scp = NULL;
    struct Callback_GenCallback_s **prevNext = &(_callback_thecallbacks[major][minor]);

    if (major >= CALLBACK_MAX_CALLBACK_IDS || minor >= CALLBACK_MAX_CALLBACK_SUBIDS) {
        return ErrorCode_GENERR;
    }

    if (_callback_needInit)
        Callback_initCallbacks();

    _Callback_lock(major,minor, "Callback_registerCallback2", 1);

    if ((newscp = MEMORY_MALLOC_STRUCT(Callback_GenCallback_s)) == NULL) {
        _Callback_unlock(major,minor);
        return ErrorCode_GENERR;
    } else {
        newscp->priority = priority;
        newscp->sc_client_arg = arg;
        newscp->sc_callback = new_callback;
        newscp->next = NULL;

        for (scp = _callback_thecallbacks[major][minor]; scp != NULL;
             scp = scp->next) {
            if (newscp->priority < scp->priority) {
                newscp->next = scp;
                break;
            }
            prevNext = &(scp->next);
        }

        *prevNext = newscp;

        DEBUG_MSGTL(("callback", "registered (%d,%d) at %p with priority %d\n",
                    major, minor, newscp, priority));
        _Callback_unlock(major,minor);
        return ErrorCode_SUCCESS;
    }
}

/**
 * This function calls the callback function for each registered callback of
 * type major and minor.
 *
 * @param major is the SNMP callback major type used
 *
 * @param minor is the SNMP callback minor type used
 *
 * @param caller_arg is a void pointer which is sent in as the callback's
 *	serverarg parameter, if needed.
 *
 * @return Returns ErrorCode_GENERR if major is >= MAX_CALLBACK_IDS or
 * minor is >= MAX_CALLBACK_SUBIDS, otherwise ErrorCode_SUCCESS is returned.
 *
 * @see Callback_registerCallback
 * @see Callback_unregisterCallback
 */
int
Callback_callCallbacks(int major, int minor, void *caller_arg)
{
    struct Callback_GenCallback_s *scp;
    unsigned int    count = 0;

    if (major >= CALLBACK_MAX_CALLBACK_IDS || minor >= CALLBACK_MAX_CALLBACK_SUBIDS) {
        return ErrorCode_GENERR;
    }

    if (_callback_needInit)
        Callback_initCallbacks();

    _Callback_lock(major,minor,"Callback_callCallbacks", 1);


    DEBUG_MSGTL(("callback", "START calling callbacks for maj=%d min=%d\n",
                major, minor));

    /*
     * for each registered callback of type major and minor
     */
    for (scp = _callback_thecallbacks[major][minor]; scp != NULL; scp = scp->next) {

        /*
         * skip unregistered callbacks
         */
        if(NULL == scp->sc_callback)
            continue;

        DEBUG_MSGTL(("callback", "calling a callback for maj=%d min=%d\n",
                    major, minor));

        /*
         * call them
         */
        (*(scp->sc_callback)) (major, minor, caller_arg,
                               scp->sc_client_arg);
        count++;
    }

    DEBUG_MSGTL(("callback",
                "END calling callbacks for maj=%d min=%d (%d called)\n",
                major, minor, count));

    _Callback_unlock(major,minor);
    return ErrorCode_SUCCESS;
}

int
Callback_countCallbacks(int major, int minor)
{
    int             count = 0;
    struct Callback_GenCallback_s *scp;

    if (major >= CALLBACK_MAX_CALLBACK_IDS || minor >= CALLBACK_MAX_CALLBACK_SUBIDS) {
        return ErrorCode_GENERR;
    }

    if (_callback_needInit)
        Callback_initCallbacks();

    for (scp = _callback_thecallbacks[major][minor]; scp != NULL; scp = scp->next) {
        count++;
    }

    return count;
}

int
Callback_available(int major, int minor)
{
    if (major >= CALLBACK_MAX_CALLBACK_IDS || minor >= CALLBACK_MAX_CALLBACK_SUBIDS) {
        return ErrorCode_GENERR;
    }

    if (_callback_needInit)
        Callback_initCallbacks();

    if (_callback_thecallbacks[major][minor] != NULL) {
        return ErrorCode_SUCCESS;
    }

    return ErrorCode_GENERR;
}

/**
 * This function unregisters a specified callback function given a major
 * and minor type.
 *
 * Note: no bound checking on major and minor.
 *
 * @param major is the SNMP callback major type used
 *
 * @param minor is the SNMP callback minor type used
 *
 * @param target is the callback function that will be unregistered.
 *
 * @param arg is a void pointer used for comparison against the registered
 *	callback's sc_client_arg variable.
 *
 * @param matchargs is an integer used to bypass the comparison of arg and the
 *	callback's sc_client_arg variable only when matchargs is set to 0.
 *
 *
 * @return
 *        Returns the number of callbacks that were unregistered.
 *
 * @see Callback_registerCallback
 * @see snmp_call_callbacks
 */

int
Callback_unregisterCallback(int major, int minor, Callback_CallbackFT * target,
                         void *arg, int matchargs)
{
    struct Callback_GenCallback_s *scp = _callback_thecallbacks[major][minor];
    struct Callback_GenCallback_s **prevNext = &(_callback_thecallbacks[major][minor]);
    int             count = 0;

    if (major >= CALLBACK_MAX_CALLBACK_IDS || minor >= CALLBACK_MAX_CALLBACK_SUBIDS)
        return ErrorCode_GENERR;

    if (_callback_needInit)
        Callback_initCallbacks();

    _Callback_lock(major,minor,"Callback_unregisterCallback", 1);


    while (scp != NULL) {
        if ((scp->sc_callback == target) &&
            (!matchargs || (scp->sc_client_arg == arg))) {
            DEBUG_MSGTL(("callback", "unregistering (%d,%d) at %p\n", major,
                        minor, scp));
            if(1 == CALLBACK_LOCK_COUNT(major,minor)) {
                *prevNext = scp->next;
                MEMORY_FREE(scp);
                scp = *prevNext;
            }
            else {
                scp->sc_callback = NULL;
                /** set cleanup flag? */
            }
            count++;
        } else {
            prevNext = &(scp->next);
            scp = scp->next;
        }
    }

    _Callback_unlock(major,minor);
    return count;
}

/**
 * find and clear client args that match ptr
 *
 * @param ptr  pointer to search for
 * @param i    callback id to start at
 * @param j    callback subid to start at
 */
int Callback_clearClientArg(void *ptr, int i, int j)
{
    struct Callback_GenCallback_s *scp = NULL;
    int rc = 0;

    /*
     * don't init i and j before loop, since the caller specified
     * the starting point explicitly. But *after* the i loop has
     * finished executing once, init j to 0 for the next pass
     * through the subids.
     */
    for (; i < CALLBACK_MAX_CALLBACK_IDS; i++,j=0) {
        for (; j < CALLBACK_MAX_CALLBACK_SUBIDS; j++) {
            scp = _callback_thecallbacks[i][j];
            while (scp != NULL) {
                if ((NULL != scp->sc_callback) &&
                    (scp->sc_client_arg != NULL) &&
                    (scp->sc_client_arg == ptr)) {
                    DEBUG_MSGTL(("9:callback", "  clearing %p at [%d,%d]\n", ptr, i, j));
                    scp->sc_client_arg = NULL;
                    ++rc;
                }
                scp = scp->next;
            }
        }
    }

    if (0 != rc) {
        DEBUG_MSGTL(("callback", "removed %d client args\n", rc));
    }

    return rc;
}

void Callback_clearCallback(void)
{
    unsigned int i = 0, j = 0;
    struct Callback_GenCallback_s *scp = NULL;

    if (_callback_needInit)
        Callback_initCallbacks();

    DEBUG_MSGTL(("callback", "clear callback\n"));
    for (i = 0; i < CALLBACK_MAX_CALLBACK_IDS; i++) {
        for (j = 0; j < CALLBACK_MAX_CALLBACK_SUBIDS; j++) {
            _Callback_lock(i,j, "clear_callback", 1);
            scp = _callback_thecallbacks[i][j];
            while (scp != NULL) {
                _callback_thecallbacks[i][j] = scp->next;
                /*
                 * if there is a client arg, check for duplicates
                 * and then free it.
                 */
                if ((NULL != scp->sc_callback) &&
                    (scp->sc_client_arg != NULL)) {
                    void *tmp_arg;
                    /*
                     * save the client arg, then set it to null so that it
                     * won't look like a duplicate, then check for duplicates
                     * starting at the current i,j (earlier dups should have
                     * already been found) and free the pointer.
                     */
                    tmp_arg = scp->sc_client_arg;
                    scp->sc_client_arg = NULL;
                    DEBUG_MSGTL(("9:callback", "  freeing %p at [%d,%d]\n", tmp_arg, i, j));
                    (void)Callback_clearClientArg(tmp_arg, i, j);
                    free(tmp_arg);
                }
                MEMORY_FREE(scp);
                scp = _callback_thecallbacks[i][j];
            }
            _Callback_unlock(i,j);
        }
    }
}

struct Callback_GenCallback_s *
Callback_list(int major, int minor)
{
    if (_callback_needInit)
        Callback_initCallbacks();

    return (_callback_thecallbacks[major][minor]);
}
