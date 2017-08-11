#ifndef DOUBLE_H_
#define DOUBLE_H_


/*!
 * \brief     Double class.
 * \details   A class to define Double operations.
 * \author    Dunian Coutinho Sampa
 * \version   1.0
 * \date      2015-2016
 * \pre       Have a Linux and C/C++ environment configured.
 * \bug       No bug.
 * \warning   N/A
 * \copyright DSPX LTDA.
 */

#include "../Settings.h"
#include "../Generals.h"
#include "String.h"


//A constant holding the largest positive finite value of type double.
#define  DOUBLE_MAXVALUE  DBL_MAX

//A constant holding the smallest positive nonzero value of type double.
#define  DOUBLE_MINVALUE  DBL_MIN

//The number of bits used to represent a double value.
#define  DOUBLE_SIZE      sizeof(double)

//Difference between 1 and the least value greater than 1 that is representable.
#define  DOUBLE_EPSILON   DBL_EPSILON



//Returns the value of the specified number as a _s64.
_s64  Double_bigLongValue(double value);

//Returns the value of this Double as a _ubyte (by casting to a _ubyte).
_ubyte  Double_byteValue(double value);

//Compares the two specified double values.
_i32 Double_compare(double d1, double d2);

//Compares the two specified double values using epsilon
_i32	Double_compare(double d1, double d2, double epsilon);

////Compares two Double objects numerically.
//int	Double_compareTo(const Double & anotherDouble);

//Returns the value of the specified number as a char.
char Double_charValue(double d1);

//Returns the float value of this Double object.
float	Double_floatValue(double d1);

//Returns the value of this Double as an int (by casting to type int).
_i32	Double_intValue(double d1);

//Returns true if this Double value is infinitely large in magnitude, false otherwise.
_bool Double_isInfinite(double d1);


//Returns true if the specified number is a Not-a-Number (NaN) value, false otherwise.
_bool Double_isNaN(double v);

//Returns the value of this Double as a long (by casting to type long).
_s32 Double_longValue(double d1);

//Returns a new double initialized to the value represented by the specified String, as performed by the valueOf method of class Double.
double Double_parseDouble(const string & s);

//Returns the value of this Double as a short (by casting to a short).
_s16 Double_shortValue(double d1);

//Returns a string representation of the double argument as an Hexadecimal floating point
_s8 * Double_toHexString(double d);

//Returns a string representation of the double argument.
_s8 * 	Double_toString(double d);

#endif /* DOUBLE_H_ */
