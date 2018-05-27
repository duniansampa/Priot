#ifndef IOT_FLOAT_H
#define IOT_FLOAT_H

/**
 * @brief     Float class.
 * \details   A class to define Float operations.
 * \author    Dunian Coutinho Sampa
 * \version   1.0
 * \date      2015-2016
 * \pre       Have a Linux and C/C++ environment configured.
 * \bug       No bug.
 * \warning   N/A
 * \copyright DSPX LTDA.
 */

#include "Config.h"
#include "Generals.h"
#include "System/String.h"

/** ============================[ Macros ]============================ */

/** A constant holding the largest positive finite value of type float */
#define FLOAT_MAXVALUE FLT_MAX

/** A constant holding the smallest positive nonzero value of type float */
#define FLOAT_MINVALUE FLT_MIN

/** The number of bits used to represent a float value. */
#define FLOAT_SIZE sizeof( float )

/** Difference between 1 and the least value greater than 1 that is representable. */
#define FLOAT_EPSILON FLT_EPSILON

/** =============================[ Functions Prototypes ]================== */

/** Compares the two specified float values. */
int Float_compare( float f1, float f2 );

/** Compares the two specified float values using epsilon */
int Float_compareEpsilon( float f1, float f2, float epsilon );

/** Returns true if the specified number is infinitely large in magnitude, false otherwise. */
bool Float_isInfinite( float v );

/** Returns true if the specified number is a Not-a-Number (NaN) value, false otherwise. */
bool Float_isNaN( float v );

#endif /* IOT_FLOAT_H */
