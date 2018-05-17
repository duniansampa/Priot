#ifndef INTEGER_H_
#define INTEGER_H_

/*!
 * \brief     Number class.
 * \details   A class to define Number operations.
 * \author    Dunian Coutinho Sampa
 * \version   1.0
 * \date      2015-2016
 * \pre       Have a Linux and C/C++ environment configured.
 * \bug       No bug.
 * \warning   N/A
 * \copyright DSPX LTDA.
 */

#include "Generals.h"

#define INTEGER_INT8_MAXVALUE UCHAR_MAX
#define INTEGER_INT8_MINVALUE 0
#define INTEGER_INT8_SIZE sizeof( int8_t )

#define INTEGER_INT16_MAXVALUE SHRT_MAX
#define INTEGER_INT16_MIN_VALUE SHRT_MIN
#define INTEGER_INT16_SIZE sizeof( int16_t )

#define INTEGER_INT32_MAXVALUE INT_MAX
#define INTEGER_INT32_MIN_VALUE INT_MIN
#define INTEGER_INT32_SIZE sizeof( int32_t )

#define INTEGER_INT64_MAXVALUE LLONG_MAX
#define INTEGER_INT64_MINVALUE LLONG_MIN
#define INTEGER_INT64_SIZE sizeof( int64_t )

/** Returns a int8_t whose value is equivalent to this int8_t with the designated bit cleared. */
int8_t Integer_clearBitInt8( int8_t b, uint8_t n );

/** Returns a int8_t whose value is equivalent to this int8_t with the designated bit set. */
int8_t Integer_setBitInt8( int8_t b, uint8_t n );

/** Returns true if and only if the designated bit is set. */
bool Integer_testBitInt8( int8_t b, uint8_t n );

/** Returns a int32_t whose value is equivalent to this int32_t with the designated bit cleared. */
int32_t Integer_clearBitInt32( int32_t value, uint8_t n );

/** Returns a int32_t whose value is equivalent to this int32_t with the designated bit set. */
int32_t Integer_setBitInt32( int32_t value, uint8_t n );

/** Returns true if and only if the designated bit is set. */
bool Integer_testBitInt32( int32_t value, uint8_t n );

#endif /* INTEGER_H_ */
