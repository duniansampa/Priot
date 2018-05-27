#include "Integer64.h"
#include "System/Util/Assert.h"
#include "System/Util/Trace.h"

/** ==================[ Private Functions Prototypes ]================== */

/**
 * Divide an unsigned 64-bit integer by 10.
 *
 * @param[in]  number - Number to be divided.
 * @param[out] quotient - the quotient.
 * @param[out] remainder - the remainder.
 */
static void _Integer64_divBy10( Integer64 number, Integer64* quotient, unsigned int* remainder );

/**
 * Multiply an unsigned 64-bit integer by 10.
 *
 * @param[in]  number - Number to be multiplied.
 * @param[out] result - the result
 */
static void _Integer64_multBy10( Integer64 number, Integer64* result );

/**
 * check the old and new values of a Integer64 for 32bit wrapping
 *
 * @param adjust : set to 1 to auto-increment new_val->high
 *                 if a 32bit wrap is detected.
 *
 * @param oldValue
 * @param newValue
 *
 * \note The old and new values must be be from within a time period
 *       which would only allow the 32bit portion of the counter to
 *       wrap once. i.e. if the 32bit portion of the counter could
 *       wrap every 60 seconds, the old and new values should be compared
 *       at least every 59 seconds (though I'd recommend at least every
 *       50 seconds to allow for timer inaccuracies).
 *
 * @retval 64 : 64bit wrap
 * @retval 32 : 32bit wrap
 * @retval  0 : did not wrap
 * @retval -1 : bad parameter
 * @retval -2 : unexpected high value (changed by more than 1)
 */
static int _Integer64_checkFor32bitWrap( Integer64* oldValue, Integer64* newValue, int adjust );

/** =============================[ Public Functions ]================== */

void Integer64_addUInt16( Integer64* number, uint16_t amount )
{
    Integer64_addUInt32( number, amount );
}

void Integer64_addUInt32( Integer64* number, uint32_t amount )
{
    uint32_t tmp;

    tmp = number->low;
    number->low = ( uint32_t )( tmp + amount );
    if ( number->low < tmp )
        number->high = ( uint32_t )( number->high + 1 );
}

void Integer64_zero( Integer64* number )
{
    number->low = 0;
    number->high = 0;
}

int Integer64_isZero( const Integer64* number )
{
    return number->low == 0 && number->high == 0;
}

void Integer64_subtract( const Integer64* number1, const Integer64* number2, Integer64* result )
{
    int carry;

    carry = number1->low < number2->low;
    result->low = ( uint32_t )( number1->low - number2->low );
    result->high = ( uint32_t )( number1->high - number2->high - carry );
}

void Integer64_addInt64( Integer64* number, const Integer64* amount )
{
    number->high = ( uint32_t )( number->high + amount->high );
    Integer64_addUInt32( number, amount->low );
}

void Integer64_diffAddInt64( Integer64* result, const Integer64* number1, const Integer64* number2 )
{
    Integer64 tmp;

    Integer64_subtract( number1, number2, &tmp );
    Integer64_addInt64( result, &tmp );
}

void Integer64_copy( Integer64* destination, const Integer64* source )
{
    *destination = *source;
}

int Integer64_check32AndUpdate( Integer64* prevVal, Integer64* newVal,
    Integer64* oldPrevVal, int* needWrapCheck )
{
    int rc;

    /*
     * counters are 32bit or unknown (which we'll treat as 32bit).
     * update the prev values with the difference between the
     * new stats and the prev old_stats:
     *    prev->stats += (new->stats - prev->old_stats)
     */
    if ( ( NULL == needWrapCheck ) || ( 0 != *needWrapCheck ) ) {
        rc = _Integer64_checkFor32bitWrap( oldPrevVal, newVal, 1 );
        if ( rc < 0 ) {
            DEBUG_MSGTL( ( "c64", "32 bit check failed\n" ) );
            return -1;
        }
    } else
        rc = 0;

    /*
     * update previous values
     */
    ( void )Integer64_diffAddInt64( prevVal, newVal, oldPrevVal );

    /*
     * if wrap check was 32 bit, undo adjust, now that prev is updated
     */
    if ( 32 == rc ) {
        /*
         * check wrap incremented high, so reset it. (Because having
         * high set for a 32 bit counter will confuse us in the next update).
         */
        Assert_assert( 1 == newVal->high );
        newVal->high = 0;
    } else if ( 64 == rc ) {
        /*
         * if we really have 64 bit counters, the summing we've been
         * doing for prev values should be equal to the new values.
         */
        if ( ( prevVal->low != newVal->low ) || ( prevVal->high != newVal->high ) ) {
            DEBUG_MSGTL( ( "c64", "looks like a 64bit wrap, but prev!=new\n" ) );
            return -2;
        } else if ( NULL != needWrapCheck )
            *needWrapCheck = 0;
    }

    return 0;
}

void Integer64_uint64ToString( char* buffer, const Integer64* number )
{
    Integer64 u64a;
    Integer64 u64b;

    char aRes[ INTEGER64_STRING_SIZE + 1 ];
    unsigned int u;
    int j;

    u64a = *number;
    aRes[ INTEGER64_STRING_SIZE ] = 0;
    for ( j = 0; j < INTEGER64_STRING_SIZE; j++ ) {
        _Integer64_divBy10( u64a, &u64b, &u );
        aRes[ ( INTEGER64_STRING_SIZE - 1 ) - j ] = ( char )( '0' + u );
        u64a = u64b;
        if ( Integer64_isZero( &u64a ) )
            break;
    }
    strcpy( buffer, &aRes[ ( INTEGER64_STRING_SIZE - 1 ) - j ] );
}

void Integer64_int64ToString( char* buffer, const Integer64* number )
{
    Integer64 u64a;

    if ( number->high & 0x80000000 ) {
        u64a.high = ( uint32_t )~number->high;
        u64a.low = ( uint32_t )~number->low;
        Integer64_addUInt32( &u64a, 1 ); /* bit invert and incr by 1 to print 2s complement */
        buffer[ 0 ] = '-';
        Integer64_uint64ToString( buffer + 1, &u64a );
    } else {
        Integer64_uint64ToString( buffer, number );
    }
}

int Integer64_stringToInt64( Integer64* number, const char* s )
{
    Integer64 i64p;
    unsigned int u;
    int sign = 0;
    int ok = 0;

    Integer64_zero( number );
    if ( *s == '-' ) {
        sign = 1;
        s++;
    }

    while ( *s && isdigit( ( unsigned char )( *s ) ) ) {
        ok = 1;
        u = *s - '0';
        _Integer64_multBy10( *number, &i64p );
        *number = i64p;
        Integer64_addUInt16( number, u );
        s++;
    }
    if ( sign ) {
        number->high = ( uint32_t )~number->high;
        number->low = ( uint32_t )~number->low;
        Integer64_addUInt16( number, 1 );
    }
    return ok;
}

/** =============================[ Private Functions ]================== */

static void _Integer64_divBy10( Integer64 number, Integer64* quotient, unsigned int* remainder )
{
    unsigned long ulT;
    unsigned long ulQ;
    unsigned long ulR;

    /*
     * top 16 bits
     */
    ulT = ( number.high >> 16 ) & 0x0ffff;
    ulQ = ulT / 10;
    ulR = ulT % 10;
    quotient->high = ulQ << 16;

    /*
     * next 16
     */
    ulT = ( number.high & 0x0ffff );
    ulT += ( ulR << 16 );
    ulQ = ulT / 10;
    ulR = ulT % 10;
    quotient->high = quotient->high | ulQ;

    /*
     * next 16
     */
    ulT = ( ( number.low >> 16 ) & 0x0ffff ) + ( ulR << 16 );
    ulQ = ulT / 10;
    ulR = ulT % 10;
    quotient->low = ulQ << 16;

    /*
     * final 16
     */
    ulT = ( number.low & 0x0ffff );
    ulT += ( ulR << 16 );
    ulQ = ulT / 10;
    ulR = ulT % 10;
    quotient->low = quotient->low | ulQ;

    *remainder = ( unsigned int )( ulR );
}

static void _Integer64_multBy10( Integer64 number, Integer64* result )
{
    unsigned long ulT;
    unsigned long ulP;
    unsigned long ulK;

    /*
     * lower 16 bits
     */
    ulT = number.low & 0x0ffff;
    ulP = ulT * 10;
    ulK = ulP >> 16;
    result->low = ulP & 0x0ffff;

    /*
     * next 16
     */
    ulT = ( number.low >> 16 ) & 0x0ffff;
    ulP = ( ulT * 10 ) + ulK;
    ulK = ulP >> 16;
    result->low = ( ulP & 0x0ffff ) << 16 | result->low;

    /*
     * next 16 bits
     */
    ulT = number.high & 0x0ffff;
    ulP = ( ulT * 10 ) + ulK;
    ulK = ulP >> 16;
    result->high = ulP & 0x0ffff;

    /*
     * final 16
     */
    ulT = ( number.high >> 16 ) & 0x0ffff;
    ulP = ( ulT * 10 ) + ulK;
    ulK = ulP >> 16;
    result->high = ( ulP & 0x0ffff ) << 16 | result->high;
}

static int _Integer64_checkFor32bitWrap( Integer64* oldValue, Integer64* newValue, int adjust )
{
    if ( ( NULL == oldValue ) || ( NULL == newValue ) )
        return -1;

    DEBUG_MSGTL( ( "9:c64:checkWrap", "check wrap 0x%0lx.0x%0lx 0x%0lx.0x%0lx\n",
        oldValue->high, oldValue->low, newValue->high, newValue->low ) );

    /*
     * check for wraps
     */
    if ( ( newValue->low >= oldValue->low ) && ( newValue->high == oldValue->high ) ) {
        DEBUG_MSGTL( ( "9:c64:checkWrap", "no wrap\n" ) );
        return 0;
    }

    /*
     * low wrapped. did high change?
     */
    if ( newValue->high == oldValue->high ) {
        DEBUG_MSGTL( ( "c64:checkWrap", "32 bit wrap\n" ) );
        if ( adjust )
            newValue->high = ( uint32_t )( newValue->high + 1 );
        return 32;
    } else if ( newValue->high == ( uint32_t )( oldValue->high + 1 ) ) {
        DEBUG_MSGTL( ( "c64:checkWrap", "64 bit wrap\n" ) );
        return 64;
    }

    return -2;
}
