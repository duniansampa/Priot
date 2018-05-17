#include "Integer.h"

int8_t Integer_clearBitInt8( int8_t b, uint8_t n )
{
    return ( int8_t )( b & ~( 1 << n ) );
}

int8_t Integer_setBitInt8( int8_t b, uint8_t n )
{
    return ( int8_t )( b | ( 1 << n ) );
}

bool Integer_testBitInt8( int8_t b, uint8_t n )
{
    return ( b & ( 1 << n ) ) != 0;
}

int32_t Integer_clearBitInt32( int32_t value, uint8_t n )
{
    return value & ~( 1 << n );
}

int32_t Integer_setBitInt32( int32_t value, uint8_t n )
{
    return value | ( 1 << n );
}

bool Integer_testBitInt32( int32_t value, uint8_t n )
{
    return ( value & ( 1 << n ) ) != 0;
}
