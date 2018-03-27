#ifndef FLOAT_H_
#define FLOAT_H_

/*!
 * \brief     Float class.
 * \details   A class to define Float operations.
 * \author    Dunian Coutinho Sampa
 * \version   1.0
 * \date      2015-2016
 * \pre       Have a Linux and C/C++ environment configured.
 * \bug       No bug.
 * \warning   N/A
 * \copyright DSPX LTDA.
 */


#include "../../Config.h"
#include "../../Generals.h"
#include "../String.h"


//A constant holding the largest positive finite value of type float
#define  FLOAT_MAXVALUE     FLT_MAX

//A constant holding the smallest positive nonzero value of type float
#define  FLOAT_MINVALUE    FLT_MIN

//The number of bits used to represent a float value.
#define  FLOAT_SIZE        sizeof(float)

//Difference between 1 and the least value greater than 1 that is representable.
#define  FLOAT_EPSILON     FLT_EPSILON



//Returns the value of the specified number as a int64.
int64 Float_bigLongValue(float value);

//Returns the value of this Float as a ubyte (by casting to a ubyte).
ubyte Float_byteValue(float value);

//Compares the two specified float values.
int	Float_compare1(float f1, float f2);

//Compares the two specified float values using epsilon
int	Float_compare2(float f1, float f2, float epsilon);


//Returns the value of the specified number as a char.
char Float_charValue(float value);

//Returns the double value of this Float object.
double	Float_doubleValue(float value);


//Returns the value of this Float as an int (by casting to type int).
int Float_intValue(float value);


//Returns true if the specified number is infinitely large in magnitude, false otherwise.
bool	Float_isInfinite(float v);

//Returns true if the specified number is a Not-a-Number (NaN) value, false otherwise.
bool	Float_isNaN(float v);

//Returns value of this Float as a long (by casting to type long).
long Float_longValue(float value);

//Returns a new float initialized to the value represented by the specified String, as performed by the valueOf method of class Float.
float Float_parseFloat(const char  * s);

//Returns the value of this Float as a short (by casting to a short).
short Float_shortValue(float value);

//Returns a string representation of the float argument as an Hexadecimal floating point.
char * Float_toHexString(float f);

//Returns a string representation of the float argument.
char * Float_toString(float f);


#endif /* FLOAT_H_ */
