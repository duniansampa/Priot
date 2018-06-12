#ifndef IOT_FDEVENTMANAGER_H
#define IOT_FDEVENTMANAGER_H

#include "System/Dispatcher/LargeFdSet.h"

#define evmNUMBER_OF_EXTERNAL_FDS 32

#define evmREGISTERED_OK 0
#define evmREGISTRATION_FAILED -2
#define evmUNREGISTERED_OK 0
#define evmNO_SUCH_REGISTRATION -1

/**
 * @details
 * Here are the key functions of this unit. Use
 * FdEventManager_registerXXXFD to register a callback
 * to be called when there is XXX activity on the register fd.
 * XXX can be read, write, or except (for exception). When registering,
 * you can pass in a pointer to some data that you have allocated that
 * you would like to have back when the callback is called.
 */

/**
 * function type: a callback to be called when there is
 * read, or write, or except activity on the register fd.
 */
typedef void ( *FdEventCallback_fp )( int fd, void* data );

/**
 * @brief FdEventManager_registerReadFD
 *        Register a given fd for read events.
 *        Call callback when events
 *        are received.
 *
 * @param fd - the file descriptor
 * @param function - a callback to be called
 * @param data - the data you would like to have back when the callback is called.
 *
 * @returns FD_REGISTERED_OK : on success
 *          FD_REGISTRATION_FAILED : in case of error
 */
int FdEventManager_registerReadFD( int fd, FdEventCallback_fp function, void* data );

/**
 * @brief FdEventManager_registerWriteFD
 *        Register a given fd for write events.
 *        Call callback when events
 *        are received.
 *
 * @param fd - the file descriptor
 * @param function - a callback to be called
 * @param data - the data you would like to have back when the callback is called.
 *
 * @returns FD_REGISTERED_OK : on success
 *          FD_REGISTRATION_FAILED : in case of error
 */
int FdEventManager_registerWriteFD( int fd, FdEventCallback_fp function, void* data );

/**
 * @brief FdEventManager_registerExceptionFD
 *        Register a given fd for exception events.
 *        Call callback when events
 *        are received.
 *
 * @param fd - the file descriptor
 * @param function - a callback to be called
 * @param data - the data you would like to have back when the callback is called.
 *
 * @returns FD_REGISTERED_OK : on success
 *          FD_REGISTRATION_FAILED : in case of error
 */
int FdEventManager_registerExceptionFD( int fd, FdEventCallback_fp function, void* data );

/**
 * @brief FdEventManager_unregisterReadFD]
 *        Unregister a given fd for read events.
 *
 * @param fd - the file descriptor
 *
 * @returns FD_UNREGISTERED_OK : on success
 *          FD_NO_SUCH_REGISTRATION : in case of error
 */
int FdEventManager_unregisterReadFD( int fd );

/**
 * @brief FdEventManager_unregisterWriteFD
 *        Unregister a given fd for read events.
 *
 * @param fd - the file descriptor
 *
 * @returns FD_UNREGISTERED_OK : on success
 *          FD_NO_SUCH_REGISTRATION : in case of error
 */
int FdEventManager_unregisterWriteFD( int fd );

/**
 * @brief FdEventManager_unregisterExceptionFD
 *        Unregister a given fd for exception events.
 *
 * @param fd - the file descriptor
 *
 * @returns FD_UNREGISTERED_OK : on success
 *          FD_NO_SUCH_REGISTRATION : in case of error
 */
int FdEventManager_unregisterExceptionFD( int fd );

/**
 * @brief FdEventManager_fdSetEventInfo
 *        Call this function to add an external event fds to your read, write,
 *        exception fds that your application already has.  When this function
 *        returns, your fd_sets will be ready for select().  It returns the
 *        biggest fd in the fd_sets so far.
 *
 * @param numfds - The biggest fd so far.  On exit to this function, numfds
 *                 could of changed since we pass out the new biggest fd.
 * @param readfds - Set of read FDs that we are monitoring.  This function
 *                  can modify this set to have more FDs that we are monitoring.
 * @param writefds - Set of write FDs that we are monitoring.  This function
 *                   can modify this set to have more FDs that we are monitoring.
 * @param exceptfds - Set of exception FDs that we are monitoring.  This function
 *                    can modify this set to have more FDs that we are monitoring.
 */
void FdEventManager_fdSetEventInfo( int* numfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds );

/**
 * @brief FdEventManager_largeFdSetEventInfo
 *        Call this function to add an external event fds to your read, write,
 *        exception fds that your application already has.  When this function
 *        returns, your LargeFdSet_t will be ready for select().  It returns the
 *        biggest fd in the LargeFdSet_t so far.
 *
 * @param numfds - The biggest fd so far.  On exit to this function, numfds
 *                 could of changed since we pass out the new biggest fd.
 * @param readfds - Set of read FDs that we are monitoring.  This function
 *                  can modify this set to have more FDs that we are monitoring.
 * @param writefds - Set of write FDs that we are monitoring.  This function
 *                   can modify this set to have more FDs that we are monitoring.
 * @param exceptfds - Set of exception FDs that we are monitoring.  This function
 *                    can modify this set to have more FDs that we are monitoring.
 */
void FdEventManager_largeFdSetEventInfo( int* numfds, LargeFdSet_t* readfds,
    LargeFdSet_t* writefds, LargeFdSet_t* exceptfds );

/**
 * @brief FdEventManager_dispatchFdSetEvents
 *        Dispatch External Events. Call this function after select returns
 *        with pending events.  If any of them were PRIOT external events,
 *        the registered callback will be called. The corresponding fd_set
 *        will have the FD cleared after the event is dispatched.
 *
 * @param count - Number of FDs that have activity.
 *                In this function, we decrement
 *                count as we dispatch an event.
 * @param readfds - Set of read FDs that have activity
 * @param writefds - Set of write FDs that have activity
 * @param exceptfds - Set of exception FDs that have activity
 */
void FdEventManager_dispatchFdSetEvents( int* count, fd_set* readfds, fd_set* writefds, fd_set* exceptfds );

/**
 * @brief FdEventManager_dispatchLargeFdSetEvents
 *        Dispatch External Events. Call this function after select returns
 *        with pending events.  If any of them were PRIOT external events,
 *        the registered callback will be called. The corresponding LargeFdSet_t
 *        will have the FD cleared after the event is dispatched.
 *
 * @param count - Number of FDs that have activity.
 *                In this function, we decrement
 *                count as we dispatch an event.
 * @param readfds - Set of read FDs that have activity
 * @param writefds - Set of write FDs that have activity
 * @param exceptfds - Set of exception FDs that have activity
 */
void FdEventManager_dispatchLargeFdSetEvents( int* count, LargeFdSet_t* readfds, LargeFdSet_t* writefds, LargeFdSet_t* exceptfds );

#endif // IOT_FDEVENTMANAGER_H
