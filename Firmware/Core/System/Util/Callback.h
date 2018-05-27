#ifndef IOT_CALLBACK_H
#define IOT_CALLBACK_H
#include "Generals.h"

/** ============================[ Macros ]============================ */

#define CALLBACK_MAX_CALLBACK_IDS 2
#define CALLBACK_MAX_CALLBACK_SUBIDS 16

/*
 * Callback priority (lower priority numbers called first)
 */
#define CALLBACK_PRIORITY_HIGHEST -1024
#define CALLBACK_PRIORITY_DEFAULT 0
#define CALLBACK_PRIORITY_LOWEST 1024

/** ============================[ Types ]================== */

/*
 * Callback Major Types
 */
enum CallbackMajor_e {
    CallbackMajor_LIBRARY,
    CallbackMajor_APPLICATION
};

/*
 * CallbackMajor_LIBRARY minor types
 */
enum CallbackMinor_e {
    CallbackMinor_POST_READ_CONFIG,
    CallbackMinor_STORE_DATA,
    CallbackMinor_SHUTDOWN,
    CallbackMinor_POST_PREMIB_READ_CONFIG,
    CallbackMinor_LOGGING,
    CallbackMinor_SESSION_INIT,
    CallbackMinor_PRE_READ_CONFIG,
    CallbackMinor_PRE_PREMIB_READ_CONFIG
};

/** callback function type */
typedef int( Callback_f )( int majorID, int minorID, void* extraArgument, void* callbackFuncArg );

typedef struct Callback_s {
    Callback_f* callbackFun;
    void* callbackFuncArg;
    int priority;
    struct Callback_s* next;
} Callback;

/** =============================[ Functions Prototypes ]================== */

/** It initializes the internal variables of the Callback class. */
void Callback_init( void );

/**
 * This function registers a generic callback function.  The major and
 * minor values are used to set the newCallback function into a global
 * static multi-dimensional matrix of type struct Callback_s.
 * The function makes sure to append this callback function at the end
 * of the link list, Callback_s->next.
 *
 * @param major - is the callback major type used.
 *              - CALLBACK_LIBRARY
 *              - CallbackMajor_APPLICATION
 *
 * @param minor - is the callback minor type used.
 *              - CALLBACK_POST_READ_CONFIG
 *              - CALLBACK_STORE_DATA
 *              - CALLBACK_SHUTDOWN
 *              - CALLBACK_POST_PREMIB_READ_CONFIG
 *              - CALLBACK_LOGGING
 *              - CALLBACK_SESSION_INIT
 *
 * @param callbackFun - is the callback function that is registered.
 *
 * @param callbackFuncArg - when not NULL is a void pointer used whenever callbackFun
 *                          function is exercised. Ownership is transferred to the twodimensional
 *                          _callback_theCallbacks[][] array. The function Callback_clear()
 *                          will deallocate the memory pointed at by calling free().
 *
 * @return  Returns ErrorCode_GENERR if major is >= CALLBACK_MAX_CALLBACK_IDS or minor is >= CALLBACK_MAX_CALLBACK_SUBIDS
 *          or a Callback_s pointer could not be allocated, otherwise ErrorCode_SUCCESS is returned.
 *          - \#define CALLBACK_MAX_CALLBACK_IDS    2
 *          - \#define CALLBACK_MAX_CALLBACK_SUBIDS 16
 *
 * \see Callback_call
 * \see Callback_unregister
 */
int Callback_register( int major, int minor, Callback_f* callbackFun, void* callbackFuncArg );

/**
 * This function calls the callback function for each registered callback of
 * type major and minor.
 *
 * @param major is the callback major type used
 *
 * @param minor is the callback minor type used
 * @param extraArgument - is a void pointer which is sent in callbackFun as the callback's
 *                        extraArgument parameter, if needed.
 *
 * @return Returns ErrorCode_GENERR if major is >= CALLBACK_MAX_CALLBACK_IDS or
 *         minor is >= CALLBACK_MAX_CALLBACK_SUBIDS, otherwise ErrorCode_SUCCESS is returned.
 *
 * \see Callback_register
 * \see Callback_unregister
 */
int Callback_call( int major, int minor, void* extraArgument );

/**
 * It checks to see if the callback is registered.
 *
 * @param major - is the callback major type
 * @param minor - is the callback minor type
 *
 * @return ErrorCode_SUCCESS if it is registered, Otherwise ErrorCode_GENERR.
 */
int Callback_isRegistered( int major, int minor );

/**
 * It counts the number of Callback registered in a given major and minor.
 *
 * @param major - is the callback major type
 * @param minor - is the callback minor type
 *
 * @return the number of Callback registered
 */
int Callback_count( int major, int minor );

/**
 * This function unregisters a specified callback function given a major
 * and minor type.

 * Note: no bound checking on major and minor.
 *
 * @param major - is the callback major type used
 * @param minor - is the callback minor type used
 * @param callbackFun - is the callback function that will be unregistered.
 * @param callbackFuncArg - is a void pointer used for comparison against the registered
 *                          callback's callbackFuncArg variable.
 *
 * @param matchArg - is an integer used to bypass the comparison of callbackFuncArg and the
 *                   callback's callbackFuncArg variable only when matchArg is set to 0.
 *
 * @return the number of callbacks that were unregistered.
 *
 * \see Callback_register
 * \see Callback_call
 */
int Callback_unregister( int major, int minor, Callback_f* callbackFun, void* callbackFuncArg, int matchArg );

/**
 * Removes all callbacks from the matrix of callbacks list (_callback_theCallbacks)
 * and deallocates the memory allocated.
 */
void Callback_clear( void );

/**
 * find and clear callback function argument that match callbackFuncArg
 *
 * @param callbackFuncArg - pointer to search for
 * @param i - callback id to start at
 * @param j - callback subid to start at
 */
int Callback_clearCallbackFuncArg( void* callbackFuncArg, int i, int j );

/**
 * Returns the list of callbacks given a major and minor type.
 *
 * @param major - is the callback major type
 * @param minor - is the callback minor type
 *
 * @return the list of callbacks
 */
Callback* Callback_getList( int major, int minor );

#endif // IOT_CALLBACK_H
