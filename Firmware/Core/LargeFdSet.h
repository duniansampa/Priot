#ifndef LARGEFDSET_H
#define LARGEFDSET_H

#include "Generals.h"
#include "Types.h"

#include <sys/select.h>



/**
 * Add socket fd to the set *fdset if not yet present.
 * Enlarges the set if necessary.
 */
#define LARGEFDSET_FD_SET(fd, fdset) LargeFdSet_setfd(fd, fdset)


/**
 * Remove socket fd from the set *fdset.
 * Do nothing if fd is not present in *fdset.
 * Do nothing if fd >= fdset->lfs_setsize.
 */
#define LARGEFDSET_FD_CLR(fd, fdset) LargeFdSet_clr(fd, fdset)

/**
 * Test whether set *fdset contains socket fd.
 * Evaluates to zero (false) if fd >= fdset->lfs_setsize.
 */
#define LARGEFDSET_FD_ISSET(fd, fdset) LargeFdSet_isSet(fd, fdset)



/**
 * Size of a single element of the array with file descriptor bitmasks.
 *
 * According to SUSv2, this array must have the name fds_bits. See also
 * <a href="http://www.opengroup.org/onlinepubs/007908775/xsh/systime.h.html">The Single UNIX Specification, Version 2, &lt;sys/time.h&gt;</a>.
 */
#define LARGEFDSET_FD_MASK_SIZE sizeof(((fd_set*)0)->fds_bits[0])

/** Number of bits in one element of the fd_set.fds_bits array. */
#define LARGEFDSET_BITS_PER_FD_MASK (8 * LARGEFDSET_FD_MASK_SIZE)

/** Number of elements needed for the fds_bits array. */
#define LARGEFDSET_FD_SET_ELEM_COUNT(setsize) (setsize + LARGEFDSET_BITS_PER_FD_MASK - 1) / LARGEFDSET_BITS_PER_FD_MASK

/**
 * Number of bytes needed to store a number of file descriptors as a
 * struct fd_set.
 */
#define NETSNMP_FD_SET_BYTES(setsize)                                       \
                       (sizeof(fd_set) + ((setsize) > FD_SETSIZE ?          \
                       LARGEFDSET_FD_SET_ELEM_COUNT((setsize) - FD_SETSIZE) \
                       * LARGEFDSET_FD_MASK_SIZE : 0))

/** Remove all file descriptors from the set *fdset. */
#define NETSNMP_LARGE_FD_ZERO(fdset)                            \
    do {                                                        \
        memset((fdset)->lfs_setptr, 0,                          \
               NETSNMP_FD_SET_BYTES((fdset)->lfs_setsize));     \
    } while (0)


void   LargeFdSet_setfd( int fd, Types_LargeFdSet *fdset);
void   LargeFdSet_clr(   int fd, Types_LargeFdSet *fdset);
int    LargeFdSet_isSet( int fd, Types_LargeFdSet *fdset);



/**
 * Initialize a netsnmp_large_fd_set structure.
 *
 * Note: this function only initializes the lfs_setsize and lfs_setptr
 * members of netsnmp_large_fd_set, not the file descriptor set itself.
 * The file descriptor set must be initialized separately, e.g. via
 * NETSNMP_LARGE_FD_CLR().
 */

void   LargeFdSet_init( Types_LargeFdSet *fdset, int setsize);

/**
 * Modify the size of a file descriptor set and preserve the first
 * min(fdset->lfs_setsize, setsize) file descriptors.
 *
 * Returns 1 upon success or 0 if memory allocation failed.
 */
int    LargeFdSet_resize( Types_LargeFdSet *fdset, int setsize);

/**
 * Synchronous I/O multiplexing for large file descriptor sets.
 *
 * On POSIX systems, any file descriptor set with size below numfds will be
 * resized before invoking select().
 *
 * @see See also select(2) for more information.
 */

int    LargeFdSet_select(int numfds, Types_LargeFdSet *readfds,
                         Types_LargeFdSet *writefds,
                         Types_LargeFdSet *exceptfds,
                        struct timeval *timeout);

/** Deallocate the memory allocated by LargeFdSet_init. */

void   LargeFdSet_cleanup(Types_LargeFdSet *fdset);

/**
 * Copy an fd_set to a netsnmp_large_fd_set structure.
 *
 * @note dst must have been initialized before this function is called.
 */
void   LargeFdSet_copyFdSetToLargeFdSet(Types_LargeFdSet *dst, const fd_set *src);

/**
 * Copy a netsnmp_large_fd_set structure into an fd_set.
 *
 * @return 0 upon success, -1 when copying fails because *src is too large to
 *         fit into *dst.
 */
int    LargeFdSet_copyLargeFdSetToFdSet(fd_set *dst, const Types_LargeFdSet *src);


#endif // LARGEFDSET_H
