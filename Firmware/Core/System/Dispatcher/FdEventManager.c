#include "FdEventManager.h"
#include "System/Util/Logger.h"
#include "System/Util/Trace.h"

/** ============================[ Private Variables ]================== */

/** to store the file descriptor for read fd. */
static int _readFd[ evmNUMBER_OF_EXTERNAL_FDS ], _readFdLength = 0;

/** to store the file descriptor for write fd. */
static int _writeFd[ evmNUMBER_OF_EXTERNAL_FDS ], _writeFdLength = 0;

/** to store the file descriptor for exception fd. */
static int _exceptionFd[ evmNUMBER_OF_EXTERNAL_FDS ], _exceptionFdLength = 0;

/** to store the callback function for read fd. */
static FdEventCallback_fp _readFdFunction[ evmNUMBER_OF_EXTERNAL_FDS ];

/** to store the callback function for write fd. */
static FdEventCallback_fp _writeFdFunction[ evmNUMBER_OF_EXTERNAL_FDS ];

/** to store the callback function for exception fd. */
static FdEventCallback_fp _exceptionFdFunction[ evmNUMBER_OF_EXTERNAL_FDS ];

/** to store the data for read fd.
 *  the stored data will be passed to the callback function
 */
static void* _readFdData[ evmNUMBER_OF_EXTERNAL_FDS ];

/** to store the data for write fd.
 *  the stored data will be passed to the callback function
 */
static void* _writeFdData[ evmNUMBER_OF_EXTERNAL_FDS ];

/** to store the data for exception fd.
 *  the stored data will be passed to the callback function
 */
static void* _exceptionFdData[ evmNUMBER_OF_EXTERNAL_FDS ];

/**
 * if _isFdUnregistered == 1, means that the callback
 * functions have been unregistered or not ready.
 * Then, they will not be called by dispatcher.
 *
 * if _isFdUnregistered == 0, means that the callback
 * functions have been registered and ready.
 * Then, they will be called by dispatcher.
 */
static int _isFdUnregistered = 1;

/** =============================[ Public Functions ]================== */

int FdEventManager_registerReadFD( int fd, FdEventCallback_fp function, void* data )
{
    if ( _readFdLength < evmNUMBER_OF_EXTERNAL_FDS ) {
        _readFd[ _readFdLength ] = fd;
        _readFdFunction[ _readFdLength ] = function;
        _readFdData[ _readFdLength ] = data;
        _readFdLength++;
        DEBUG_MSGTL( ( "fdEventManager:FdEventManager_registerReadFD", "registered fd %d\n", fd ) );
        return evmREGISTERED_OK;
    } else {
        Logger_log( LOGGER_PRIORITY_CRIT, "FdEventManager_registerReadFD: too many file descriptors\n" );
        return evmREGISTRATION_FAILED;
    }
}

int FdEventManager_registerWriteFD( int fd, FdEventCallback_fp function, void* data )
{
    if ( _writeFdLength < evmNUMBER_OF_EXTERNAL_FDS ) {
        _writeFd[ _writeFdLength ] = fd;
        _writeFdFunction[ _writeFdLength ] = function;
        _writeFdData[ _writeFdLength ] = data;
        _writeFdLength++;
        DEBUG_MSGTL( ( "fdEventManager:FdEventManager_registerWriteFD", "registered fd %d\n", fd ) );
        return evmREGISTERED_OK;
    } else {
        Logger_log( LOGGER_PRIORITY_CRIT,
            "FdEventManager_registerWriteFD: too many file descriptors\n" );
        return evmREGISTRATION_FAILED;
    }
}

int FdEventManager_registerExceptionFD( int fd, FdEventCallback_fp function, void* data )
{
    if ( _exceptionFdLength < evmNUMBER_OF_EXTERNAL_FDS ) {
        _exceptionFd[ _exceptionFdLength ] = fd;
        _exceptionFdFunction[ _exceptionFdLength ] = function;
        _exceptionFdData[ _exceptionFdLength ] = data;
        _exceptionFdLength++;
        DEBUG_MSGTL( ( "fdEventManager:FdEventManager_registerExceptionFD", "registered fd %d\n", fd ) );
        return evmREGISTERED_OK;
    } else {
        Logger_log( LOGGER_PRIORITY_CRIT,
            "FdEventManager_registerExceptionFD: too many file descriptors\n" );
        return evmREGISTRATION_FAILED;
    }
}

int FdEventManager_unregisterReadFD( int fd )
{
    int i, j;

    for ( i = 0; i < _readFdLength; i++ ) {
        if ( _readFd[ i ] == fd ) {
            _readFdLength--;
            for ( j = i; j < _readFdLength; j++ ) {
                _readFd[ j ] = _readFd[ j + 1 ];
                _readFdFunction[ j ] = _readFdFunction[ j + 1 ];
                _readFdData[ j ] = _readFdData[ j + 1 ];
            }
            DEBUG_MSGTL( ( "fdEventManager:FdEventManager_unregisterReadFD", "unregistered fd %d\n", fd ) );
            _isFdUnregistered = 1;
            return evmUNREGISTERED_OK;
        }
    }
    return evmNO_SUCH_REGISTRATION;
}

int FdEventManager_unregisterWriteFD( int fd )
{
    int i, j;

    for ( i = 0; i < _writeFdLength; i++ ) {
        if ( _writeFd[ i ] == fd ) {
            _writeFdLength--;
            for ( j = i; j < _writeFdLength; j++ ) {
                _writeFd[ j ] = _writeFd[ j + 1 ];
                _writeFdFunction[ j ] = _writeFdFunction[ j + 1 ];
                _writeFdData[ j ] = _writeFdData[ j + 1 ];
            }
            DEBUG_MSGTL( ( "fdEventManager:FdEventManager_unregisterWriteFD", "unregistered fd %d\n", fd ) );
            _isFdUnregistered = 1;
            return evmUNREGISTERED_OK;
        }
    }
    return evmNO_SUCH_REGISTRATION;
}

int FdEventManager_unregisterExceptionFD( int fd )
{
    int i, j;

    for ( i = 0; i < _exceptionFdLength; i++ ) {
        if ( _exceptionFd[ i ] == fd ) {
            _exceptionFdLength--;
            for ( j = i; j < _exceptionFdLength; j++ ) {
                _exceptionFd[ j ] = _exceptionFd[ j + 1 ];
                _exceptionFdFunction[ j ] = _exceptionFdFunction[ j + 1 ];
                _exceptionFdData[ j ] = _exceptionFdData[ j + 1 ];
            }
            DEBUG_MSGTL( ( "fdEventManager:FdEventManager_unregisterExceptionFD", "unregistered fd %d\n",
                fd ) );
            _isFdUnregistered = 1;
            return evmUNREGISTERED_OK;
        }
    }
    return evmNO_SUCH_REGISTRATION;
}

void FdEventManager_fdSetEventInfo( int* numfds, fd_set* readfds, fd_set* writefds, fd_set* exceptfds )
{
    LargeFdSet_t lreadfds;
    LargeFdSet_t lwritefds;
    LargeFdSet_t lexceptfds;

    LargeFdSet_init( &lreadfds, FD_SETSIZE );
    LargeFdSet_init( &lwritefds, FD_SETSIZE );
    LargeFdSet_init( &lexceptfds, FD_SETSIZE );

    LargeFdSet_copyFromFdSet( &lreadfds, readfds );
    LargeFdSet_copyFromFdSet( &lwritefds, writefds );
    LargeFdSet_copyFromFdSet( &lexceptfds, exceptfds );

    FdEventManager_largeFdSetEventInfo( numfds, &lreadfds, &lwritefds, &lexceptfds );

    if ( LargeFdSet_copyToFdSet( readfds, &lreadfds ) < 0
        || LargeFdSet_copyToFdSet( writefds, &lwritefds ) < 0
        || LargeFdSet_copyToFdSet( exceptfds, &lexceptfds ) < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "Use FdEventManager_fdSetEventInfo() for processing"
            " large file descriptors\n" );
    }

    LargeFdSet_cleanup( &lreadfds );
    LargeFdSet_cleanup( &lwritefds );
    LargeFdSet_cleanup( &lexceptfds );
}

void FdEventManager_largeFdSetEventInfo( int* numfds,
    LargeFdSet_t* readfds,
    LargeFdSet_t* writefds,
    LargeFdSet_t* exceptfds )
{
    int i;

    _isFdUnregistered = 0;

    for ( i = 0; i < _readFdLength; i++ ) {
        largeFD_SET( _readFd[ i ], readfds );
        if ( _readFd[ i ] >= *numfds )
            *numfds = _readFd[ i ] + 1;
    }
    for ( i = 0; i < _writeFdLength; i++ ) {
        largeFD_SET( _writeFd[ i ], writefds );
        if ( _writeFd[ i ] >= *numfds )
            *numfds = _writeFd[ i ] + 1;
    }
    for ( i = 0; i < _exceptionFdLength; i++ ) {
        largeFD_SET( _exceptionFd[ i ], exceptfds );
        if ( _exceptionFd[ i ] >= *numfds )
            *numfds = _exceptionFd[ i ] + 1;
    }
}

void FdEventManager_dispatchFdSetEvents( int* count, fd_set* readfds, fd_set* writefds, fd_set* exceptfds )
{
    LargeFdSet_t lreadfds;
    LargeFdSet_t lwritefds;
    LargeFdSet_t lexceptfds;

    LargeFdSet_init( &lreadfds, FD_SETSIZE );
    LargeFdSet_init( &lwritefds, FD_SETSIZE );
    LargeFdSet_init( &lexceptfds, FD_SETSIZE );

    LargeFdSet_copyFromFdSet( &lreadfds, readfds );
    LargeFdSet_copyFromFdSet( &lwritefds, writefds );
    LargeFdSet_copyFromFdSet( &lexceptfds, exceptfds );

    FdEventManager_dispatchLargeFdSetEvents( count, &lreadfds, &lwritefds, &lexceptfds );

    if ( LargeFdSet_copyToFdSet( readfds, &lreadfds ) < 0
        || LargeFdSet_copyToFdSet( writefds, &lwritefds ) < 0
        || LargeFdSet_copyToFdSet( exceptfds, &lexceptfds ) < 0 ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "Use FdEventManager_dispatchFdSetEvents() for processing"
            " large file descriptors\n" );
    }

    LargeFdSet_cleanup( &lreadfds );
    LargeFdSet_cleanup( &lwritefds );
    LargeFdSet_cleanup( &lexceptfds );
}

void FdEventManager_dispatchLargeFdSetEvents( int* count,
    LargeFdSet_t* readfds,
    LargeFdSet_t* writefds,
    LargeFdSet_t* exceptfds )
{
    int i;
    for ( i = 0; *count && ( i < _readFdLength ) && !_isFdUnregistered; i++ ) {
        if ( largeFD_ISSET( _readFd[ i ], readfds ) ) {
            DEBUG_MSGTL( ( "fdEventManager:FdEventManager_dispatchLargeFdSetEvents",
                "readfd[%d] = %d\n", i, _readFd[ i ] ) );
            _readFdFunction[ i ]( _readFd[ i ],
                _readFdData[ i ] );
            largeFD_CLR( _readFd[ i ], readfds );
            ( *count )--;
        }
    }
    for ( i = 0; *count && ( i < _writeFdLength ) && !_isFdUnregistered; i++ ) {
        if ( largeFD_ISSET( _writeFd[ i ], writefds ) ) {
            DEBUG_MSGTL( ( "fdEventManager:FdEventManager_dispatchLargeFdSetEvents",
                "writefd[%d] = %d\n", i, _writeFd[ i ] ) );
            _writeFdFunction[ i ]( _writeFd[ i ],
                _writeFdData[ i ] );
            largeFD_CLR( _writeFd[ i ], writefds );
            ( *count )--;
        }
    }
    for ( i = 0; *count && ( i < _exceptionFdLength ) && !_isFdUnregistered; i++ ) {
        if ( largeFD_ISSET( _exceptionFd[ i ], exceptfds ) ) {
            DEBUG_MSGTL( ( "fdEventManager:FdEventManager_dispatchLargeFdSetEvents",
                "exceptfd[%d] = %d\n", i, _exceptionFd[ i ] ) );
            _exceptionFdFunction[ i ]( _exceptionFd[ i ],
                _exceptionFdData[ i ] );
            largeFD_CLR( _exceptionFd[ i ], exceptfds );
            ( *count )--;
        }
    }
}
