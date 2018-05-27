#ifndef IOT_DOUBLE_H
#define IOT_DOUBLE_H

/**
 * @brief     Double class.
 * \details   A class to define Double operations.
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

/** A constant holding the largest positive finite value of type double. */
#define DOUBLE_MAXVALUE DBL_MAX

/** A constant holding the smallest positive nonzero value of type double. */
#define DOUBLE_MINVALUE DBL_MIN

/** The number of bits used to represent a double value. */
#define DOUBLE_SIZE sizeof( double )

/** Difference between 1 and the least value greater than 1 that is representable. */
#define DOUBLE_EPSILON DBL_EPSILON

/** =============================[ Functions Prototypes ]================== */

/** Compares the two specified double values. */
int Double_compare( double d1, double d2 );

/** Compares the two specified double values using epsilon */
int Double_compareEpsilon( double d1, double d2, double epsilon );

/** Returns true if this Double value is infinitely large in magnitude, false otherwise. */
bool Double_isInfinite( double d1 );

/** Returns true if the specified number is a Not-a-Number (NaN) value, false otherwise. */
bool Double_isNaN( double v );

#endif /* IOT_DOUBLE_H */
