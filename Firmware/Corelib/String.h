
#ifndef STRING_H_
#define STRING_H_

/*!
 * \brief     String class.
 * \details   A class to define String operations.
 * \author    Dunian Coutinho Sampa
 * \version   1.0
 * \date      2015-2016
 * \pre       Have a Linux and C/C++ environment configured.
 * \bug       No bug.
 * \warning   N/A
 * \copyright DSPX LTDA.
 */

#include "Settings.h"
#include "Generals.h"


//They compare equal
const _i32 STRING_COMPARE_EQUAL = 0;

//Either the value of the first character that does not match is lower in the compared string,
//or all compared characters match but the compared string is shorter.
const _i32 STRING_COMPARE_LOWER = -1;

//Either the value of the first character that does not match is greater in the compared string,
//or all compared characters match but the compared string is longer.
const _i32 STRING_COMPARE_GREATER = 1;

//Tests if this string begin with the specified prefix.
_bool String_beginsWith(const _s8 * s, const _s8 * prefix);

//Appends a copy of the source string to the destination string.
//The terminating null character in destination is overwritten by the first character of source,
//and a null-character is included at the end of the new string formed by the concatenation of
//both in destination
_s8 * String_concat(_s8 * destination, const _s8 * source);

//Compares two strings lexicographically.
_i32 String_compare(const _s8 * s1, const _s8 * s2);

//Tests if this string ends with the specified suffix.
_bool String_endsWith(const _s8 * s, const _s8 * suffix);

//Compares this String to another String, ignoring case considerations.
_bool String_equalsIgnoreCase(const _s8 * s1, const _s8 * s2);

//Compares this String to another String
_bool String_equals(const _s8 * s1, const _s8 * s2);

//Returns a formatted string using the specified locale, format string, and arguments.
_s8 * String_format(const _s8 * format, ...);

//Converts all of the characters in this string to lower case.
_s8 * String_toLowerCase(const _s8 * s);

//Converts all of the characters in this String to upper;
_s8 * String_toUpperCase(const _s8 * s);

//Returns a string representation of the string argument as an string in base 16.
_s8 * String_toHexString(const _s8 * s);

//Returns the string, with leading and trailing whitespace omitted.
// returned string is equal to s after function call.
_s8 * String_trim(_s8 * s);

//Returns the string representation of the _s64 argument.
_s8 * String_valueOf(_s64 b);

//Returns the string representation of the boolean argument.
_s8 * String_valueOf(_bool b);

//Returns the string representation of the byte argument.
_s8 * String_valueOf(_ubyte b);


//Returns the string representation of the double argument.
_s8 * String_valueOf(double d);

//Returns the string representation of the float argument.
_s8 * String_valueOf(float f);

//Returns the string representation of the int argument.
_s8 * String_valueOf(_i32 i);

//Returns the string representation of the long argument.
_s8 * String_valueOf(_s32 l);

//Returns the string representation of the short argument.
_s8 * String_valueOf(_s16 i);


#endif /* STRING_H_ */
