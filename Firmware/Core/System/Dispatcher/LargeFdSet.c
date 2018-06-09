#include "LargeFdSet.h"
#include "System/Util/Assert.h"
#include "System/Util/Memory.h"

/*
 * Recent versions of glibc trigger abort() if FD_SET(), FD_CLR() or
 * FD_ISSET() is invoked with n >= FD_SETSIZE. Hence these replacement macros.
 */
#define largeFD_BIT_SET( n, p ) ( ( p )->fds_bits[ ( n ) / NFDBITS ] |= ( 1ULL << ( ( n ) % NFDBITS ) ) )
#define largeFD_BIT_CLR( n, p ) ( ( p )->fds_bits[ ( n ) / NFDBITS ] &= ~( 1ULL << ( ( n ) % NFDBITS ) ) )
#define largeFD_BIT_ISSET( n, p ) ( ( p )->fds_bits[ ( n ) / NFDBITS ] & ( 1ULL << ( ( n ) % NFDBITS ) ) )

void LargeFdSet_set( int fd, LargeFdSet_t* largeFdSet )
{
    Assert_assert( fd >= 0 );

    while ( fd >= ( int )largeFdSet->fdSetSize )
        LargeFdSet_resize( largeFdSet, 2 * ( largeFdSet->fdSetSize + 1 ) );

    largeFD_BIT_SET( fd, largeFdSet->fdSetPtr );
}

void LargeFdSet_clr( int fd, LargeFdSet_t* largeFdSet )
{
    Assert_assert( fd >= 0 );

    if ( ( unsigned )fd < largeFdSet->fdSetSize )
        largeFD_BIT_CLR( fd, largeFdSet->fdSetPtr );
}

int LargeFdSet_isSet( int fd, LargeFdSet_t* largeFdSet )
{
    Assert_assert( fd >= 0 );

    return ( ( unsigned )fd < largeFdSet->fdSetSize && largeFD_BIT_ISSET( fd, largeFdSet->fdSetPtr ) );
}

void LargeFdSet_init( LargeFdSet_t* largeFdSet, int setSize )
{
    largeFdSet->fdSetSize = 0;
    largeFdSet->fdSetPtr = NULL;
    LargeFdSet_resize( largeFdSet, setSize );
}

int LargeFdSet_select( int numfds, LargeFdSet_t* readfds,
    LargeFdSet_t* writefds,
    LargeFdSet_t* exceptfds,
    struct timeval* timeout )
{
    /* Bit-set representation: make sure all fds have at least size 'numfds'. */
    if ( readfds && readfds->fdSetSize < numfds )
        LargeFdSet_resize( readfds, numfds );
    if ( writefds && writefds->fdSetSize < numfds )
        LargeFdSet_resize( writefds, numfds );
    if ( exceptfds && exceptfds->fdSetSize < numfds )
        LargeFdSet_resize( exceptfds, numfds );

    return select( numfds,
        readfds ? readfds->fdSetPtr : NULL,
        writefds ? writefds->fdSetPtr : NULL,
        exceptfds ? exceptfds->fdSetPtr : NULL,
        timeout );
}

int LargeFdSet_resize( LargeFdSet_t* largeFdSet, int setSize )
{
    int fd_set_bytes;

    if ( largeFdSet->fdSetSize == setSize )
        goto goto_success;

    if ( setSize > FD_SETSIZE ) {
        fd_set_bytes = largeFD_SET_BYTES( setSize );
        if ( largeFdSet->fdSetSize > FD_SETSIZE ) {
            largeFdSet->fdSetPtr = ( fd_set* )Memory_realloc( largeFdSet->fdSetPtr, fd_set_bytes );
            if ( !largeFdSet->fdSetPtr )
                goto goto_outOfMem;
        } else {
            largeFdSet->fdSetPtr = ( fd_set* )malloc( fd_set_bytes );
            if ( !largeFdSet->fdSetPtr )
                goto goto_outOfMem;
            *largeFdSet->fdSetPtr = largeFdSet->fdSet;
        }
    } else {
        if ( largeFdSet->fdSetSize > FD_SETSIZE ) {
            largeFdSet->fdSet = *largeFdSet->fdSetPtr;
            free( largeFdSet->fdSetPtr );
        }
        largeFdSet->fdSetPtr = &largeFdSet->fdSet;
    }

    /*
     * Unix: when enlarging, clear the file descriptors defined in the
     * resized *fdset but that were not defined in the original *fdset.
     */
    if ( largeFdSet->fdSetSize == 0 && setSize == FD_SETSIZE ) {
        /* In this case we can use the OS's FD_ZERO */
        FD_ZERO( largeFdSet->fdSetPtr );
    } else {
        int i;

        for ( i = largeFdSet->fdSetSize; i < setSize; i++ )
            largeFD_BIT_CLR( i, largeFdSet->fdSetPtr );
    }
    largeFdSet->fdSetSize = setSize;
goto_success:
    return 1;

goto_outOfMem:
    largeFdSet->fdSetSize = 0;

    return 0;
}

void LargeFdSet_cleanup( LargeFdSet_t* fdset )
{
    LargeFdSet_resize( fdset, 0 );
    fdset->fdSetSize = 0;
    fdset->fdSetPtr = NULL;
}

void LargeFdSet_copyFromFdSet( LargeFdSet_t* dst, const fd_set* src )
{
    LargeFdSet_resize( dst, FD_SETSIZE );
    *dst->fdSetPtr = *src;
}

int LargeFdSet_copyToFdSet( fd_set* dst, const LargeFdSet_t* src )
{
    /* Report failure if *src is larger than FD_SETSIZE. */
    if ( src->fdSetSize > FD_SETSIZE ) {
        FD_ZERO( dst );
        return -1;
    }

    *dst = *src->fdSetPtr;

    int i;

    /* Unix: clear any file descriptors defined in *dst but not in *src. */
    for ( i = src->fdSetSize; i < FD_SETSIZE; ++i )
        FD_CLR( i, dst );

    return 0;
}
