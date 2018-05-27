#ifndef IOT_INTEGER64_H
#define IOT_INTEGER64_H

#include "Generals.h"

typedef struct Integer64_s {
    uint32_t high;
    uint32_t low;
} Integer64;

typedef Integer64 Counter64;

/** length of the char buffer that will hold the converted Integer64 value */
#define INTEGER64_STRING_SIZE 21

/**
 * Add an unsigned 16-bit int to an unsigned 64-bit integer.
 *
 * @param[in,out] number - Number to be incremented.
 * @param[in]     amount -  the Amount to add.
 *
 */
void Integer64_addUInt16( Integer64* number, uint16_t amount );

/**
 * Add an unsigned 32-bit int to an unsigned 64-bit integer.
 *
 * @param[in,out] number - Number to be incremented.
 * @param[in]     amount - Amount to add.
 *
 */
void Integer64_addUInt32( Integer64* number, uint32_t amount );

/**
 * Set an unsigned 64-bit number to zero.
 *
 * @param[in] number - Number to be zeroed.
 */
void Integer64_zero( Integer64* number );

/**
 * Check if an unsigned 64-bit number is zero.
 *
 * @param[in] pu64 - Number to be checked.
 */
int Integer64_isZero( const Integer64* number );

/** Convert an unsigned 64-bit number to ASCII.
 *
 *  \note the size of the buffer must be INTEGER64_STRING_SIZE + 1:
 *        buffer[INTEGER64_STRING_SIZE + 1].
 *
 *  @param buffer - the result of conversion
 *  @param number - Number to be converted.
 */
void Integer64_uint64ToString( char* buffer, const Integer64* number );

/** Convert a signed 64-bit number to ASCII.
 *
 *  \note the size of the buffer must be INTEGER64_STRING_SIZE + 1:
 *        buffer[INTEGER64_STRING_SIZE + 1].
 *
 *  @param buffer - the result of conversion
 *  @param number - Number to be converted.
 */
void Integer64_int64ToString( char* buffer, const Integer64* number );

/** Convert a signed 64-bit integer from ASCII to Integer64.
 *
 *  @param number - the result of conversion.
 *  @param s - the string to be converted
 */
int Integer64_stringToInt64( Integer64* number, const char* s );

/**
 * Subtract two 64-bit numbers.
 *
 * @param[in] number1 - Number to start from.
 * @param[in] number2 - Amount to subtract.
 * @param[out] result - the result: number1 - number2.
 */
void Integer64_subtract( const Integer64* number1, const Integer64* number2, Integer64* result );

/**
 * Add two 64-bit numbers.
 *
 * @param[in] amount - the amount to add.
 * @param[in,out] number - the result is: number += amount.
 */
void Integer64_addInt64( Integer64* number, const Integer64* amount );

/**
 * Add the difference of two 64-bit numbers to a 64-bit counter.
 *
 * @param[in] number1 - Number to start from.
 * @param[in] number2 - Amount to subtract.
 * @param[out] result - the result is:  result += (number1 - number2)
 */
void Integer64_diffAddInt64( Integer64* result, const Integer64* number1, const Integer64* number2 );

/**
 * Copy a 64-bit number.
 *
 * @param[in] source - number to be copied.
 * @param[out] destination - where to store the copy: *destination = *source.
 */
void Integer64_copy( Integer64* destination, const Integer64* source );

/**
 * update a 64 bit value with the difference between two (possibly) 32 bit values
 *
 * @param prevVal         - the 64 bit current counter
 * @param oldPrevVal      - the (possibly 32 bit) previous value
 * @param newVal          - the (possible 32bit) new value
 * @param needWrapCheck   - pointer to integer indicating if wrap check is needed.
 *                          flag may be cleared if 64 bit counter is detected
 *
 * \note The oldPrevVal and newVal values must be from within a time
 *       period which would only allow the 32bit portion of the counter to
 *       wrap once. i.e. if the 32bit portion of the counter could
 *       wrap every 60 seconds, the old and new values should be compared
 *       at least every 59 seconds (though I'd recommend at least every
 *       50 seconds to allow for timer inaccuracies).
 *
 * Suggested use:
 *
 *   static needWrapCheck = 1;
 *   static Integer64 current, prevVal, newVal;
 *
 *   your_functions_to_update_new_value(&newVal);
 *   if (0 == needWrapCheck)
 *      memcpy(current, newVal, sizeof(newVal));
 *   else {
 *      Integer64_check32AndUpdate(&current,&new,&prev,&needWrapCheck);
 *      memcpy(prevVal, newVal, sizeof(newVal));
 *   }
 *
 *
 * @retval  0 : success
 * @retval -1 : error checking for 32 bit wrap
 * @retval -2 : look like we have 64 bit values, but sums aren't consistent
 */
int Integer64_check32AndUpdate( Integer64* prevVal, Integer64* newVal, Integer64* oldPrevVal, int* needWrapCheck );

#endif // IOT_INTEGER64_H
