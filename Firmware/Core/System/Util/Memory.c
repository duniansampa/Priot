#include "Memory.h"

char* Memory_strdup( const char* ptr )
{
    return strdup( ptr );
}

void* Memory_calloc( size_t numElements, size_t sizeElement )
{
    return calloc( numElements, sizeElement );
}

void* Memory_malloc( size_t size )
{
    return malloc( size );
}

void* Memory_realloc( void* ptr, size_t size )
{
    return realloc( ptr, size );
}

void Memory_free( void* ptr )
{
    if ( ptr )
        free( ptr );
}

int Memory_reallocIncrease( u_char** buf, size_t* bufLen )
{
    u_char* new_buf = NULL;
    size_t new_buf_len = 0;

    if ( buf == NULL ) {
        return 0;
    }

    if ( *bufLen <= 255 ) {
        new_buf_len = *bufLen + 256;
    } else if ( *bufLen > 255 && *bufLen <= 8191 ) {
        new_buf_len = *bufLen * 2;
    } else if ( *bufLen > 8191 ) {
        new_buf_len = *bufLen + 8192;
    }

    if ( *buf == NULL ) {
        new_buf = ( u_char* )malloc( new_buf_len );
    } else {
        new_buf = ( u_char* )realloc( *buf, new_buf_len );
    }

    if ( new_buf != NULL ) {
        *buf = new_buf;
        *bufLen = new_buf_len;
        return 1;
    } else {
        return 0;
    }
}

void Memory_freeZero( void* buf, size_t size )
{
    if ( buf ) {
        memset( buf, 0, size );
        free( buf );
    }
}

void* Memory_memdup( const void* from, size_t size )
{
    void* to = NULL;

    if ( from ) {
        to = malloc( size );
        if ( to )
            memcpy( to, from, size );
    }
    return to;
}

char* Memory_strdupAndNull( const u_char* from, size_t from_len )
{
    char* ret;

    if ( from_len == 0 || from[ from_len - 1 ] != '\0' ) {
        ret = ( char* )malloc( from_len + 1 );
        if ( !ret )
            return NULL;
        ret[ from_len ] = '\0';
    } else {
        ret = ( char* )malloc( from_len );
        if ( !ret )
            return NULL;
        ret[ from_len - 1 ] = '\0';
    }
    memcpy( ret, from, from_len );
    return ret;
}
