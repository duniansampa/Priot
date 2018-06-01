#ifndef IOT_LARGEFDSET_H
#define IOT_LARGEFDSET_H

#include "Types.h"

/** ============================[ Macros ============================ */

/**
 * Add socket fd to the set *largeFdSet if not yet present.
 * Enlarges the set if necessary.
 */
#define largeFD_SET( fd, largeFdSet ) LargeFdSet_set( fd, largeFdSet )

/**
 * Remove socket fd from the set *largeFdSet.
 * Do nothing if fd is not present in *fdset.
 * Do nothing if fd >= largeFdSet->fdSetPtr.
 */
#define largeFD_CLR( fd, largeFdSet ) LargeFdSet_clr( fd, largeFdSet )

/**
 * Test whether set *largeFdSet contains socket fd.
 * Evaluates to zero (false) if fd >= largeFdSet->fdSetPtr.
 */
#define largeFD_ISSET( fd, largeFdSet ) LargeFdSet_isSet( fd, largeFdSet )

/**
 * Size of a single element of the array with file descriptor bitmasks.
 *
 * According to SUSv2, this array must have the name fds_bits. See also
 * <a href="http://www.opengroup.org/onlinepubs/007908775/xsh/systime.h.html">The Single UNIX Specification, Version 2, &lt;sys/time.h&gt;</a>.
 */
#define largeFD_MASK_SIZE sizeof( ( ( fd_set* )0 )->fds_bits[ 0 ] )

/** Number of bits in one element of the fd_set.fds_bits array. */
#define largeBITS_PER_FD_MASK ( 8 * largeFD_MASK_SIZE )

/** Number of elements needed for the fds_bits array. */
#define largeFD_SET_ELEM_COUNT( setSize ) ( setSize + largeBITS_PER_FD_MASK - 1 ) / largeBITS_PER_FD_MASK

/**
 * Number of bytes needed to store a number of file descriptors as a
 * struct fd_set.
 */
#define largeFD_SET_BYTES( setSize ) \
    ( sizeof( fd_set ) + ( ( setSize ) > FD_SETSIZE ? largeFD_SET_ELEM_COUNT( ( setSize )-FD_SETSIZE ) * largeFD_MASK_SIZE : 0 ) )

/** Remove all file descriptors from the set *largeFdSet. */
#define largeFD_ZERO( largeFdSet )                            \
    do {                                                      \
        memset( ( largeFdSet )->fdSetPtr, 0,                  \
            largeFD_SET_BYTES( ( largeFdSet )->fdSetSize ) ); \
    } while ( 0 )

/** ============================[ Types ]================== */

/**
 * Structure for holding a set of file descriptors, similar to fd_set.
 *
 * This structure however can hold so-called large file descriptors
 * (>= FD_SETSIZE or 1024) on Unix systems or more than FD_SETSIZE (64)
 * sockets on Windows systems.
 *
 * It is safe to allocate this structure on the stack.
 *
 * This structure must be initialized by calling LargeFdSet_init()
 * and must be cleaned up via LargeFdSet_cleanup(). If this last
 * function is not called this may result in a memory leak.
 *
 * The members of this structure are:
 * fdSetPtr: maximum set size.
 * lsf_setptr:  points to lfs_set if fdSetPtr <= FD_SETSIZE, and otherwise
 *              to dynamically allocated memory.
 * lfs_set:     file descriptor / socket set data if fdSetPtr <= FD_SETSIZE.
 */

/**
 * >>> IMPORTANT <<<
 * _GNU_SOURCE defined in Config.h. It's defined by the user.
 * _XOPEN_SOURCE define in feature.h and depend on _GNU_SOURCE
 * __USE_XOPEN defined in feature.h and depend on _XOPEN_SOURCE
 * fd_set defined in sys/select.h and depend on __USE_XOPEN
 */
typedef struct LargeFdSet_s {
    unsigned fdSetSize;
    fd_set* fdSetPtr;
    fd_set fdSet;
} LargeFdSet_t;

/** =============================[ Functions Prototypes ]================== */

/**
 * @brief LargeFdSet_setFd
 *        Sets the bit for the file descriptor fd in the file descriptor set fdset.
 * @param fd - the file descriptor
 * @param largeFdSet - the file descriptor set
 */
void LargeFdSet_set( int fd, LargeFdSet_t* largeFdSet );

/**
 * @brief LargeFdSet_clr
 *        Clears the bit for the file descriptor fd in the file descriptor set largeFdSet.
 *
 * @param fd - the file descriptor
 * @param largeFdSet - the file descriptor set
 */
void LargeFdSet_clr( int fd, LargeFdSet_t* largeFdSet );

/**
 * @brief LargeFdSet_isSet
 *        Returns a non-zero value if the bit for the file descriptor fd
 *        is set in the file descriptor set pointed to by largeFdSet, and 0 otherwise.
 *
 * @param fd - the file descriptor
 * @param largeFdSet - the file descriptor set
 * @returns 1 : if the bit for the file descriptor fd is set
 *          0 : if the bit is not
 */
int LargeFdSet_isSet( int fd, LargeFdSet_t* largeFdSet );

/**
 * @brief LargeFdSet_init
 *        Initialize a LargeFdSet_s structure.
 *
 * Note: this function only initializes the fdSetPtr and lfs_setptr
 * members of LargeFdSet_s, not the file descriptor set itself.
 * The file descriptor set must be initialized separately, e.g. via
 * largeFD_CLR().
 *
 * @param largeFdSet - the file descriptor set
 * @param setSize - the initial size
 */
void LargeFdSet_init( LargeFdSet_t* largeFdSet, int setSize );

/**
 * @brief LargeFdSet_resize
 *        Modify the size of a file descriptor set and preserve the first
 *        min(fdset->fdSetPtr, setSize) file descriptors.
 *
 * Returns 1 upon success or 0 if memory allocation failed.
 */
int LargeFdSet_resize( LargeFdSet_t* largeFdSet, int setSize );

/**
 * Synchronous I/O multiplexing for large file descriptor sets.
 *
 * On POSIX systems, any file descriptor set with size below numfds will be
 * resized before invoking select().
 *
 * @see See also select(2) for more information.
 */
int LargeFdSet_select( int numfds, LargeFdSet_t* readfds,
    LargeFdSet_t* writefds,
    LargeFdSet_t* exceptfds,
    struct timeval* timeout );

/** Deallocate the memory allocated by LargeFdSet_init. */
void LargeFdSet_cleanup( LargeFdSet_t* fdset );

/**
 * Copy an fd_set to a LargeFdSet_s structure.
 *
 * @note dst must have been initialized before this function is called.
 */
void LargeFdSet_copyFromFdSet( LargeFdSet_t* dst, const fd_set* src );

/**
 * Copy a LargeFdSet_s structure into an fd_set.
 *
 * @return 0 upon success, -1 when copying fails because *src is too large to
 *         fit into *dst.
 */
int LargeFdSet_copyToFdSet( fd_set* dst, const LargeFdSet_t* src );

#endif // IOT_LARGEFDSET_H
