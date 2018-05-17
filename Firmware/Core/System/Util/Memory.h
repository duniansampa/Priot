#ifndef MEMORY_H_
#define MEMORY_H_

#include "Generals.h"

/** @def MEMORY_FREE(s)
 * Frees a pointer only if it is !NULL
 * and sets its value to NULL
 */
#define MEMORY_FREE( s )        \
    do {                        \
        if ( s ) {              \
            free( ( void* )s ); \
            s = NULL;           \
        }                       \
    } while ( 0 )

/** @def MEMORY_SWIPE_MEMORY(n, s)
 * Frees pointer n only if it is !NULL,
 * sets n to s and sets s to NULL
 */
#define MEMORY_SWIPE_MEMORY( n, s ) \
    do {                            \
        if ( n )                    \
            free( ( void* )n );     \
        n = s;                      \
        s = NULL;                   \
    } while ( 0 )

/** @def MEMORY_MALLOC_STRUCT(s)
 * Mallocs memory of sizeof(struct s),
 * zeros it and returns a pointer to it.
 */
#define MEMORY_MALLOC_STRUCT( s ) ( struct s* )calloc( 1, sizeof( struct s ) )

/** @def MEMORY_MALLOC_TYPEDEF(t)
 *  Mallocs memory of sizeof(t), zeros it and returns a pointer to it.
 */
#define MEMORY_MALLOC_TYPEDEF( td ) ( td* )calloc( 1, sizeof( td ) )

/** @def MEMORY_FILL_ZERO(s,l)
 * Zeros l bytes of memory starting at s.
 */
#define MEMORY_FILL_ZERO( s, l ) \
    do {                         \
        if ( s )                 \
            memset( s, 0, l );   \
    } while ( 0 )

/**
 * This function is a wrapper for the strdup function.
 */
char* Memory_strdup( const char* ptr );

/**
 * This function is a wrapper for the calloc function.
 */
void* Memory_calloc( size_t numElements, size_t sizeElement );

/**
 * This function is a wrapper for the malloc function.
 */
void* Memory_malloc( size_t size );

/**
 * This function is a wrapper for the realloc function.
 */
void* Memory_realloc( void* ptr, size_t size );

/**
 * This function is a wrapper for the free function.
 * It calls free only if the calling parameter has a non-zero value.
 */
void Memory_free( void* ptr );

/**
 * This function increase the size of the buffer pointed at by *buf, which is
 * initially of size *bufLen.  Contents are preserved **AT THE BOTTOM END OF
 * THE BUFFER**.  If memory can be (re-)allocated then it returns 1, else it
 * returns 0.
 *
 * \param buf  pointer to a buffer pointer
 * \param bufLen      pointer to current size of buffer in bytes
 *
 * \note
 * The current re-allocation algorithm is to increase the buffer size by
 * whichever is the greater of 256 bytes or the current buffer size, up to
 * a maximum increase of 8192 bytes.
 */
int Memory_reallocIncrease( u_char** buffer, size_t* bufLen );

/** zeros memory before freeing it.
 *
 *	\param *buf	Pointer at bytes to free.
 *	\param size	Number of bytes in buf.
 */
void Memory_freeZero( void* buf, size_t size );

/**
 * Duplicates a memory block.
 *
 * @param[in] from Pointer to copy memory from.
 * @param[in] size Size of the data to be copied.
 *
 * @return Pointer to the duplicated memory block, or NULL if memory allocation
 * failed.
 */
void* Memory_memdup( const void* from, size_t size );

/** copies a (possible) unterminated string of a given length into a
 *  new buffer and null terminates it as well (new buffer MAY be one
 *  byte longer to account for this */
char* Memory_strdupAndNull( const u_char* from, size_t fromLen );

#endif // MEMORY_H_
