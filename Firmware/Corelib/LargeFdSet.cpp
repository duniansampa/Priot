#include "LargeFdSet.h"
#include "Assert.h"

#include <stdio.h>
#include <string.h> /* memset(), which is invoked by FD_ZERO() */
#include <stdlib.h>




/*
 * Recent versions of glibc trigger abort() if FD_SET(), FD_CLR() or
 * FD_ISSET() is invoked with n >= FD_SETSIZE. Hence these replacement macros.
 */
#define LARGEFDSET_LFD_SET(n, p)    ((p)->fds_bits[(n)/NFDBITS] |=  (1ULL << ((n) % NFDBITS)))
#define LARGEFDSET_LFD_CLR(n, p)    ((p)->fds_bits[(n)/NFDBITS] &= ~(1ULL << ((n) % NFDBITS)))
#define LARGEFDSET_LFD_ISSET(n, p)  ((p)->fds_bits[(n)/NFDBITS] &   (1ULL << ((n) % NFDBITS)))

void LargeFdSet_setfd(int fd, Types_LargeFdSet * fdset)
{
    Assert_assert(fd >= 0);

    while (fd >= (int)fdset->lfs_setsize)
        LargeFdSet_resize(fdset, 2 * (fdset->lfs_setsize + 1));

    LARGEFDSET_LFD_SET(fd, fdset->lfs_setptr);
}

void LargeFdSet_clr(int fd, Types_LargeFdSet * fdset)
{
    Assert_assert(fd >= 0);

    if ((unsigned)fd < fdset->lfs_setsize)
        LARGEFDSET_LFD_CLR(fd, fdset->lfs_setptr);
}

int LargeFdSet_isSet(int fd, Types_LargeFdSet * fdset)
{
    Assert_assert(fd >= 0);

    return ((unsigned)fd < fdset->lfs_setsize &&
            LARGEFDSET_LFD_ISSET(fd, fdset->lfs_setptr));
}


void LargeFdSet_init(Types_LargeFdSet * fdset, int setsize)
{
    fdset->lfs_setsize = 0;
    fdset->lfs_setptr  = NULL;
    LargeFdSet_resize(fdset, setsize);
}


int LargeFdSet_select(int numfds, Types_LargeFdSet *readfds,
                     Types_LargeFdSet *writefds,
                     Types_LargeFdSet *exceptfds,
                     struct timeval *timeout)
{
    /* Bit-set representation: make sure all fds have at least size 'numfds'. */
    if (readfds && readfds->lfs_setsize < numfds)
        LargeFdSet_resize(readfds, numfds);
    if (writefds && writefds->lfs_setsize < numfds)
        LargeFdSet_resize(writefds, numfds);
    if (exceptfds && exceptfds->lfs_setsize < numfds)
        LargeFdSet_resize(exceptfds, numfds);

    return select(numfds,
            readfds ? readfds->lfs_setptr : NULL,
            writefds ? writefds->lfs_setptr : NULL,
            exceptfds ? exceptfds->lfs_setptr : NULL,
            timeout);
}


int LargeFdSet_resize(Types_LargeFdSet * fdset, int setsize)
{
    int             fd_set_bytes;

    if (fdset->lfs_setsize == setsize)
        goto success;

    if (setsize > FD_SETSIZE) {
        fd_set_bytes = NETSNMP_FD_SET_BYTES(setsize);
        if (fdset->lfs_setsize > FD_SETSIZE) {
            fdset->lfs_setptr = (fd_set *)realloc(fdset->lfs_setptr, fd_set_bytes);
            if (!fdset->lfs_setptr)
                goto out_of_mem;
        } else {
            fdset->lfs_setptr = (fd_set *)malloc(fd_set_bytes);
            if (!fdset->lfs_setptr)
                goto out_of_mem;
            *fdset->lfs_setptr = fdset->lfs_set;
        }
    } else {
        if (fdset->lfs_setsize > FD_SETSIZE) {
            fdset->lfs_set = *fdset->lfs_setptr;
            free(fdset->lfs_setptr);
        }
        fdset->lfs_setptr = &fdset->lfs_set;
    }

    /*
     * Unix: when enlarging, clear the file descriptors defined in the
     * resized *fdset but that were not defined in the original *fdset.
     */
    if ( fdset->lfs_setsize == 0 && setsize == FD_SETSIZE ) {
        /* In this case we can use the OS's FD_ZERO */
        FD_ZERO(fdset->lfs_setptr);
    } else {
        int             i;

        for (i = fdset->lfs_setsize; i < setsize; i++)
            LARGEFDSET_LFD_CLR(i, fdset->lfs_setptr);
    }
    fdset->lfs_setsize = setsize;
success:
    return 1;

out_of_mem:
    fdset->lfs_setsize = 0;

    return 0;
}


void LargeFdSet_cleanup(Types_LargeFdSet * fdset)
{
    LargeFdSet_resize(fdset, 0);
    fdset->lfs_setsize = 0;
    fdset->lfs_setptr  = NULL;
}

void LargeFdSet_copyFdSetToLargeFdSet(Types_LargeFdSet * dst, const fd_set * src)
{
    LargeFdSet_resize(dst, FD_SETSIZE);
    *dst->lfs_setptr = *src;
}

int LargeFdSet_copyLargeFdSetToFdSet(fd_set * dst, const Types_LargeFdSet * src)
{
    /* Report failure if *src is larger than FD_SETSIZE. */
    if (src->lfs_setsize > FD_SETSIZE) {
        FD_ZERO(dst);
        return -1;
    }

    *dst = *src->lfs_setptr;


    int             i;

    /* Unix: clear any file descriptors defined in *dst but not in *src. */
    for (i = src->lfs_setsize; i < FD_SETSIZE; ++i)
        FD_CLR(i, dst);


    return 0;
}
