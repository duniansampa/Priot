#include "String.h"
#include "Util/Memory.h"

/** ==================[ Private Functions Prototypes ]================== */

static void _String_freeFunction( void* value );

/** =============================[ Public Functions ]================== */

char* String_new( const char* s )
{
    return Memory_strdup( s );
}

bool String_beginsWith( const char* s, const char* prefix )
{

    int lp, ls;

    if ( String_isNull( s ) || String_isNull( prefix ) )
        return 0;

    lp = String_length( prefix );
    ls = String_length( s );

    if ( lp > ls )
        return 0;

    return strncmp( s, prefix, lp ) == 0;
}

char* String_append( char* destination, const char* source )
{
    return String_appendLittle( destination, source, String_length( source ) );
}

char* String_appendLittle( char* destination, const char* source, int num )
{
    /** do the concat */
    strncat( destination, source, num );

    return destination;
}

int String_appendRealloc( u_char** buffer, size_t* bufferLen, size_t* outLen,
    int allowRealloc, const u_char* s )
{
    if ( buffer == NULL || bufferLen == NULL || outLen == NULL ) {
        return 0;
    }

    if ( s == NULL ) {
        /*
         * Appending a NULL string always succeeds since it is a NOP.
         */
        return 1;
    }

    while ( ( *outLen + strlen( ( const char* )s ) + 1 ) >= *bufferLen ) {
        if ( !( allowRealloc && Memory_reallocIncrease( buffer, bufferLen ) ) ) {
            return 0;
        }
    }

    if ( !*buffer )
        return 0;

    strcpy( ( char* )( *buffer + *outLen ), ( const char* )s );
    *outLen += strlen( ( char* )( *buffer + *outLen ) );
    return 1;
}

char* String_appendAlloc( const char* s1, const char* s2, int num )
{
    int newSize;

    /** Determine new size */
    newSize = String_length( s1 ) + num + 1;

    /** Allocate new buffer */
    char* newBuffer = ( char* )Memory_malloc( newSize );

    /** do the copy and concat */
    strcpy( newBuffer, s1 );
    String_appendLittle( newBuffer, s2, num );

    return newBuffer;
}

char* String_appendTruncate( char* destination, const char* source, int destinationMaxSize )
{

    int len, diff;

    len = String_length( destination );

    diff = destinationMaxSize - len - 1;

    if ( diff <= 0 )
        return destination;

    String_appendLittle( destination, source, diff );

    return destination;
}

char* String_copyTruncate( char* destination, const char* source, int destinationMaxSize )
{

    String_fill( destination, 0, 1 );
    return String_appendTruncate( destination, source, destinationMaxSize );
}

int String_compare( const char* s1, const char* s2 )
{

    return strcmp( s1, s2 );
}

bool String_endsWith( const char* s, const char* suffix )
{

    int lp, ls, diff;

    if ( String_isNull( s ) || String_isNull( suffix ) )
        return 0;

    lp = strlen( suffix );
    ls = strlen( s );

    if ( lp > ls )
        return 0;

    diff = ls - lp;

    return String_equals( s + diff, suffix );
}

bool String_equalsIgnoreCase( const char* s1, const char* s2 )
{
    return strcasecmp( s1, s2 ) == 0;
}

bool String_equals( const char* s1, const char* s2 )
{
    return String_compare( s1, s2 ) == 0;
}

bool String_isEmpty( const char* s1 )
{
    return String_length( s1 ) == 0;
}

bool String_isNull( const char* s1 )
{
    return s1 == NULL;
}

int String_length( const char* s1 )
{
    return strlen( s1 );
}

char* String_format( const char* format, ... )
{
    char buffer[ 512 ];
    int size;
    va_list args;
    char* s;
    va_start( args, format );
    if ( ( size = vsprintf( buffer, format, args ) ) < 0 ) {
        s = NULL;
    } else {
        buffer[ size ] = 0;
        s = String_new( buffer );
    }
    va_end( args );

    return s;
}

char* String_toLowerCase( const char* s )
{
    char* sA = String_new( s );
    while ( *sA != 0 ) {
        *sA = tolower( *sA );
        sA++;
    }
    return sA;
}

char* String_toUpperCase( const char* s )
{
    char* sA = String_new( s );
    while ( *sA != 0 ) {
        *sA = toupper( *sA );
        sA++;
    }
    return sA;
}

char* String_trim( const char* s )
{

    int left = 0, right = 0, len = 0;

    /** Trim leading space */
    while ( isspace( ( unsigned char )s[ left ] ) )
        left++;

    if ( s[ left ] == 0 ) // All spaces?
        return String_new( "" );

    /** Trim trailing space */
    len = String_length( s );
    right = len - 1;
    while ( right > left && isspace( ( unsigned char )s[ right ] ) )
        right--;

    return String_subString( s, left, right - left + 1 );
}

bool String_contains( const char* s, const char* sequence )
{
    return strstr( s, sequence ) != NULL;
}

char* String_subStringBeginAt( const char* s, int beginIndex )
{
    return String_subString( s, beginIndex, String_length( s ) );
}

char* String_subString( const char* s, int beginIndex, int length )
{
    int l;

    if ( String_isNull( s ) || beginIndex > ( l = String_length( s ) ) )
        return NULL;

    length = length < ( l - beginIndex ) ? length : ( l - beginIndex );

    /** Allocate new buffer */
    char* newBuffer = ( char* )Memory_malloc( length + 1 );
    const char* ptr = s + beginIndex;
    l = length;
    while ( ( *ptr ) != 0 && length > 0 ) {
        *newBuffer++ = *ptr++;
        length--;
    }

    *newBuffer = 0;

    return newBuffer - l;
}

char* String_replaceChar( const char* s, char oldChar, char newChar )
{
    if ( String_isNull( s ) )
        return NULL;

    char *ret, *tmp;

    ret = String_new( s );
    tmp = ret;

    while ( ( *tmp ) != 0 ) {
        if ( ( *tmp ) == oldChar ) {
            *tmp = newChar;
        }
        tmp++;
    }
    return ret;
}

char* String_replace( const char* s, const char* target, const char* replacement )
{
    int lr, lt, ls, size, count = 0;
    char *tmp, *ptr;

    if ( String_isNull( s ) || String_isNull( target ) || String_isNull( replacement ) )
        return String_new( s );

    lt = String_length( target );
    lr = String_length( replacement );
    ls = String_length( s );

    if ( ls < lt )
        return String_new( s );

    tmp = ( char* )s;

    /** Counts the number of occurrences of \e targer in \e s. */
    while ( ( ptr = strstr( tmp, target ) ) != 0 ) {
        count++;
        tmp = ptr + lt;
    }

    /** computes the size of the new string and allocs the memory space for it */
    size = ls - lt * count + lr * count + 1;
    printf( "==> %d\n", size );
    char* newString = ( char* )Memory_malloc( size );
    tmp = ( char* )s;

    /** makes the replaces. */
    while ( ( ptr = strstr( tmp, target ) ) != 0 ) {
        size = ptr - tmp;
        printf( "==> %c\n", *ptr );
        if ( size > 0 )
            String_appendLittle( newString, tmp, size );
        newString = String_append( newString, replacement );
        tmp = ptr + lt;
    }
    return newString;
}

char* String_setInt64( int64_t i )
{

    return String_format( "%lld", i );
}

char* String_setBool( bool b )
{
    return String_new( ( b == 0 ) ? "false" : "true" );
}

char* String_setInt8( int8_t b )
{
    return String_format( "%hhd", b );
}

char* String_setDouble( double d )
{
    return String_format( "%lf", d );
}

char* String_setFloat( float f )
{
    return String_format( "%f", f );
}

char* String_setInt32( int32_t i )
{
    return String_format( "%ld", i );
}

char* String_setInt16( int16_t i )
{
    return String_format( "%hd", i );
}

int String_indexOf( const char* s, const char* substring, int fromIndex )
{
    int len;
    const char* ptr;

    len = String_length( s );
    if ( fromIndex > len )
        return -1;
    ptr = strstr( s + fromIndex, substring );

    return ( ptr ) ? ( ptr - s ) : -1;
}

int String_indexOfChar( const char* s, const char ch, int fromIndex )
{
    int len;
    const char* ptr;

    len = String_length( s );
    if ( fromIndex > len )
        return -1;
    ptr = strchr( s + fromIndex, ch );

    return ( ptr ) ? ( ptr - s ) : -1;
}

List* String_split( const char* s, const char* delimiters )
{
    int count = 0;
    char *pch, *tmp;
    List* list = NULL;
    tmp = String_new( s );
    pch = strtok( tmp, delimiters );

    while ( pch != NULL ) {
        List_emplace( &list, String_new( pch ), _String_freeFunction );
        pch = strtok( NULL, delimiters );
        count++;
    }
    Memory_free( tmp );
    return list;
}

char* String_fill( char* s, char ch, int num )
{
    memset( s, ( int )ch, num );
    return s;
}

/** =============================[ Private Functions ]================== */

static void _String_freeFunction( void* data )
{
    Memory_free( data );
}
