#include "Convert.h"
#include "Numerics/Double.h"
#include "Numerics/Float.h"
#include "System/Util/Logger.h"
#include "System/Util/Memory.h"

/** =============================[ Public Functions ]================== */

/** ==> String to... */

int8_t Convert_stringToInt8( const char* s )
{
    return ( int8_t )Convert_stringToInt32( s );
}

int8_t Convert_stringBaseToInt8( const char* s, uint8_t base )
{
    return ( int8_t )Convert_stringBaseToInt32( s, base );
}

char* Convert_stringToHexString( const char* s )
{

    int size = 0;
    char buffer[ 512 ];
    while ( *s != 0 ) {
        size += sprintf( buffer + size, "%02X", *s );
        s++;
    }
    return strndup( buffer, size );
}

int16_t Convert_stringToInt16( const char* s )
{
    return ( int16_t )Convert_stringToInt32( s );
}

int16_t Convert_stringBaseToInt16( const char* s, uint8_t base )
{
    return ( int16_t )Convert_stringBaseToInt32( s, base );
}

int32_t Convert_stringToInt32( const char* s )
{
    return ( int32_t )atol( s );
}

int32_t Convert_stringBaseToInt32( const char* s, uint8_t base )
{
    return ( ( int32_t )strtol( s, NULL, base ) );
}

int64_t Convert_stringToInt64( const char* s )
{
    return ( int64_t )atoll( s );
}

int64_t Convert_stringBaseToInt64( const char* s, uint8_t base )
{
    return ( ( int64_t )strtoll( s, NULL, base ) );
}

/** ==> Int8 to... */

char* Convert_int8ToBinaryString( int8_t b )
{
    return Convert_int8ToStringBase( b, 2 );
}

char* Convert_int8ToHexString( int8_t b )
{
    return Convert_int32ToHexString( b );
}

char* Convert_int8ToString( int8_t b )
{
    return String_setInt8( b );
}

char* Convert_int8ToStringBase( uint8_t i, uint8_t base )
{
    return Convert_int64ToStringBase( i, base );
}

/** ==> Int16 to... */

char* Convert_int16ToHexString( int16_t sh )
{
    return Convert_int32ToHexString( sh );
}

char* Convert_int16ToString( int16_t sh )
{
    return String_setInt16( sh );
}

/** ==> Int32 to... */

char* Convert_int32ToBinaryString( int32_t i )
{
    return Convert_int64ToStringBase( i, 2 );
}

char* Convert_int32ToHexString( int32_t i )
{
    return String_format( "%X", i );
}

char* Convert_int32ToOctalString( int32_t i )
{
    return String_format( "%o", i );
}

char* Convert_int32ToString( int32_t i )
{
    return String_setInt32( i );
}

char* Convert_int32ToStringBase( int32_t i, uint8_t base )
{
    return Convert_int64ToStringBase( i, base );
}

char* Convert_int64ToBinaryString( int64_t i )
{
    return Convert_int64ToStringBase( i, 2 );
}

char* Convert_int64ToHexString( int64_t i )
{
    return String_format( "%X", i );
}

char* Convert_int64ToOctalString( int64_t i )
{
    return String_format( "%o", i );
}

char* Convert_int64ToString( int64_t i )
{
    return String_setInt64( i );
}

char* Convert_int64ToStringBase( int64_t i, uint8_t base )
{
    int64_t q;
    int16_t r;
    const uint16_t BUF_SIZE = 512;
    char buffer[ BUF_SIZE ];
    uint16_t index = 0;
    if ( base < 1 )
        return NULL;

    do {
        q = i / base;
        r = i % base;
        i = q;

        buffer[ BUF_SIZE - 1 - index++ ] = ( char )( ( r < 10 ) ? ( '0' + r ) : ( 'A' + r - 10 ) );
    } while ( i > 0 );

    return strndup( buffer + ( BUF_SIZE - index ), index );
}

int64_t Convert_doubleToInt64( double value )
{
    if ( value >= 0 )
        return ( int64_t )( value + DOUBLE_EPSILON * 1E+1 );
    else
        return ( int64_t )( value - DOUBLE_EPSILON * 1E+1 );
}

int8_t Convert_doubleToInt8( double value )
{
    if ( value >= 0 )
        return ( int8_t )( value + DOUBLE_EPSILON * 1E+1 );
    else
        return ( int8_t )( value - DOUBLE_EPSILON * 1E+1 );
}

char Convert_doubleToChar( double value )
{
    if ( value >= 0 )
        return ( char )( value + DOUBLE_EPSILON * 1E+1 );
    else
        return ( char )( value - DOUBLE_EPSILON * 1E+1 );
}

float Convert_doubleToFloat( double value )
{
    return ( float )value;
}

int32_t Convert_doubleToInt32( double value )
{
    if ( value >= 0 )
        return ( int32_t )( value + DOUBLE_EPSILON * 1E+1 );
    else
        return ( int32_t )( value - DOUBLE_EPSILON * 1E+1 );
}

double Convert_stringToDouble( const char* s )
{
    return ( double )atof( s );
}

int16_t Convert_doubleToInt16( double value )
{
    if ( value >= 0 )
        return ( int16_t )( value + DOUBLE_EPSILON * 1E+1 );
    else
        return ( int16_t )( value - DOUBLE_EPSILON * 1E+1 );
}

char* Convert_doubleToHexString( double d )
{
    return String_format( "%A", d );
}

char* Convert_doubleToString( double d )
{
    return String_setDouble( d );
}

int64_t Convert_floatToInt64( float value )
{
    if ( value >= 0 )
        return ( int64_t )( value + FLOAT_EPSILON * 1E+1 );
    else
        return ( int64_t )( value - FLOAT_EPSILON * 1E+1 );
}

int8_t Convert_floatToInt8( float value )
{
    if ( value >= 0 )
        return ( int8_t )( value + FLOAT_EPSILON * 1E+1 );
    else
        return ( int8_t )( value - FLOAT_EPSILON * 1E+1 );
}

char Convert_floatToChar( float value )
{
    if ( value >= 0 )
        return ( char )( value + FLOAT_EPSILON * 1E+1 );
    else
        return ( char )( value - FLOAT_EPSILON * 1E+1 );
}

double Convert_floatToDouble( float value )
{
    return ( double )value;
}

int32_t Convert_floatToInt32( float value )
{
    if ( value >= 0 )
        return ( int32_t )( value + FLOAT_EPSILON * 1E+1 );
    else
        return ( int32_t )( value - FLOAT_EPSILON * 1E+1 );
}

float Convert_stringToFloat( const char* s )
{
    return ( float )atof( s );
}

int16_t Convert_floatToInt16( float value )
{
    if ( value >= 0 )
        return ( int16_t )( value + FLOAT_EPSILON * 1E+1 );
    else
        return ( int16_t )( value - FLOAT_EPSILON * 1E+1 );
}

char* Convert_floatToHexString( float f )
{
    return String_format( "%A", f );
}

char* Convert_floatToString( float f )
{
    return String_setFloat( f );
}

double Convert_bitsToDouble( uint8_t* beginByte, uint8_t startBit, uint8_t numBit, double resolution, bool isSigned )
{

    int64_t val = 0;
    int64_t up;
    int64_t test = 1;
    int numBytes;

    if ( numBit > 31 ) {
        up = ( ~( ( int64_t )0 ) ) << 31;
        up <<= ( numBit - 31 );
        test <<= 31;
        test <<= ( numBit - 31 - 1 );
    } else {
        up = ( ~( ( int64_t )0 ) ) << numBit;
        test = test << ( numBit - 1 );
    }
    numBytes = Convert_doubleToInt32( ( numBit + startBit ) / 8.0 );
    if ( Double_compare( ( numBit + startBit ) / 8.0, numBytes ) != 0 ) {
        numBytes = numBytes + 1;
    }
    for ( int i = numBytes; i != 0; i-- ) {
        val <<= 8;
        val |= beginByte[ i - 1 ];
    }
    val >>= startBit;
    val &= ( ~up );
    if ( isSigned ) {
        if ( ( val & test ) != 0 ) {
            val = val | up;
        }
    }
    return ( double )( val * resolution );
}

void Convert_doubleToBits( double value, uint8_t* beginByte, uint8_t startBit, uint8_t numBit, double resolution )
{
    int64_t val = Convert_doubleToInt32( value / resolution );
    int64_t up;
    int64_t mask;
    int numBytes;
    mask = ( ~( ( int64_t )0 ) );
    if ( numBit > 31 ) {
        up = ( ~( ( int64_t )0 ) ) << 31;
        up <<= ( numBit - 31 );

    } else {
        up = ( ~( ( int64_t )0 ) ) << numBit;
    }
    val &= ( ~up );
    mask &= ( ~up );
    val <<= startBit;
    mask <<= startBit;
    mask = ~mask;
    numBytes = Convert_doubleToInt32( ( numBit + startBit ) / 8.0 );
    if ( Double_compare( ( numBit + startBit ) / 8.0, numBytes ) != 0 ) {
        numBytes = numBytes + 1;
    }
    for ( int i = 0; i != numBytes; i++ ) {

        beginByte[ i ] &= ( uint8_t )mask;
        beginByte[ i ] |= ( uint8_t )val;
        val >>= 8;
        mask >>= 8;
    }
}
void Convert_valueToReflectedValue( uint8_t* beginByte, uint8_t startBit, uint8_t numBit )
{

    double d = Convert_bitsToDouble( beginByte, startBit, numBit, 1, 0 );

    uint64_t value = d;
    uint64_t reflect = 0;
    uint64_t j = 1;

    for ( int i = 0; i < numBit; i++ ) {
        reflect <<= 1;
        if ( value & j )
            reflect |= 1;
        j <<= 1;
    }
    d = reflect;

    Convert_doubleToBits( reflect, beginByte, startBit, numBit, 1 );
}

uint32_t Convert_stringTimeToSeconds( const char* timeString )
{
    uint32_t secs = -1;
    if ( !timeString || !timeString[ 0 ] )
        return secs;

    secs = Convert_stringToInt32( timeString );

    if ( isdigit( ( unsigned char )timeString[ strlen( timeString ) - 1 ] ) )
        return secs; /* no letter specified, it's already in seconds */

    switch ( timeString[ strlen( timeString ) - 1 ] ) {
    case 's':
    case 'S':
        /* already in seconds */
        break;

    case 'm':
    case 'M':
        secs = secs * 60;
        break;

    case 'h':
    case 'H':
        secs = secs * 60 * 60;
        break;

    case 'd':
    case 'D':
        secs = secs * 60 * 60 * 24;
        break;

    case 'w':
    case 'W':
        secs = secs * 60 * 60 * 24 * 7;
        break;

    default:
        Logger_log( LOGGER_PRIORITY_ERR, "time string %s contains an invalid suffix letter\n",
            timeString );
        return -1;
    }

    return secs;
}

u_int Convert_binaryStringToHexString( u_char** dest, size_t* destLen, int allowRealloc,
    const u_char* input, size_t inputLen )
{
    u_int olen = ( inputLen * 2 ) + 1;
    u_char *s, *op;
    const u_char* ip = input;

    if ( dest == NULL || destLen == NULL || input == NULL )
        return 0;

    if ( NULL == *dest ) {
        s = ( unsigned char* )calloc( 1, olen );
        *destLen = olen;
    } else
        s = *dest;

    if ( *destLen < olen ) {
        if ( !allowRealloc )
            return 0;
        *destLen = olen;
        if ( Memory_reallocIncrease( dest, destLen ) )
            return 0;
    }

    op = s;
    while ( ip - input < ( int )inputLen ) {
        *op++ = CONVERT_VALUE_TO_HEX( ( *ip >> 4 ) & 0xf );
        *op++ = CONVERT_VALUE_TO_HEX( *ip & 0xf );
        ip++;
    }
    *op = '\0';

    if ( s != *dest )
        *dest = s;
    *destLen = olen;

    return olen;
}

int Convert_hexStringToBinaryString( u_char** buffer, size_t* bufferLen, size_t* offset,
    int allowRealloc, const char* hexString, const char* delimiters )
{
    unsigned int subid = 0;
    const char* cp = hexString;

    if ( buffer == NULL || bufferLen == NULL || offset == NULL || hexString == NULL ) {
        return 0;
    }

    if ( ( *cp == '0' ) && ( ( *( cp + 1 ) == 'x' ) || ( *( cp + 1 ) == 'X' ) ) ) {
        cp += 2;
    }

    while ( *cp != '\0' ) {
        if ( !isxdigit( ( int )*cp ) || !isxdigit( ( int )*( cp + 1 ) ) ) {
            if ( ( NULL != delimiters ) && ( NULL != strchr( delimiters, *cp ) ) ) {
                cp++;
                continue;
            }
            return 0;
        }
        if ( sscanf( cp, "%2x", &subid ) == 0 ) {
            return 0;
        }
        /*
         * if we dont' have enough space, realloc.
         * (snmp_realloc will adjust buf_len to new size)
         */
        if ( ( *offset >= *bufferLen ) && !( allowRealloc && Memory_reallocIncrease( buffer, bufferLen ) ) ) {
            return 0;
        }
        *( *buffer + *offset ) = ( u_char )subid;
        ( *offset )++;
        if ( *++cp == '\0' ) {
            /*
             * Odd number of hex digits is an error.
             */
            return 0;
        } else {
            cp++;
        }
    }
    return 1;
}

int Convert_hexStringToBinaryStringWrapper( u_char** buffer, size_t* bufferLen, size_t* offset,
    int allowRealloc, const char* hexString )
{
    return Convert_hexStringToBinaryString( buffer, bufferLen, offset, allowRealloc, hexString, " " );
}

int Convert_integerStringToBinaryString( u_char** buffer, size_t* bufferLen, size_t* outLen,
    int allowRealloc, const char* integerString )
{
    int subid = 0;
    const char* cp = integerString;

    if ( buffer == NULL || bufferLen == NULL || outLen == NULL
        || integerString == NULL ) {
        return 0;
    }

    while ( *cp != '\0' ) {
        if ( isspace( ( int )*cp ) || *cp == '.' ) {
            cp++;
            continue;
        }
        if ( !isdigit( ( int )*cp ) ) {
            return 0;
        }
        if ( ( subid = atoi( cp ) ) > 255 ) {
            return 0;
        }
        if ( ( *outLen >= *bufferLen ) && !( allowRealloc && Memory_reallocIncrease( buffer, bufferLen ) ) ) {
            return 0;
        }
        *( *buffer + *outLen ) = ( u_char )subid;
        ( *outLen )++;
        while ( isdigit( ( int )*cp ) ) {
            cp++;
        }
    }
    return 1;
}

int Convert_hexStringToBinaryString2( const u_char* input, size_t inputLen, char** output )
{
    u_int olen = ( inputLen / 2 ) + ( inputLen % 2 );
    char *s = ( char * )calloc( 1, olen ? olen : 1 ), *op = s;
    const u_char* ip = input;

    *output = NULL;
    if ( !s )
        goto goto_hexToBinaryQuit;

    *op = 0;
    if ( inputLen % 2 ) {
        if ( !isxdigit( *ip ) )
            goto goto_hexToBinaryQuit;
        *op++ = CONVERT_HEX_TO_VALUE( *ip );
        ip++;
    }

    while ( ip < input + inputLen ) {
        if ( !isxdigit( *ip ) )
            goto goto_hexToBinaryQuit;
        *op = CONVERT_HEX_TO_VALUE( *ip ) << 4;
        ip++;

        if ( !isxdigit( *ip ) )
            goto goto_hexToBinaryQuit;
        *op++ += CONVERT_HEX_TO_VALUE( *ip );
        ip++;
    }

    *output = s;
    return olen;

goto_hexToBinaryQuit:
    Memory_freeZero( s, olen );
    return -1;
}
