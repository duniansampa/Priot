
#ifndef STRING_H_
#define STRING_H_

#include "Generals.h"

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

#include "Generals.h"

struct String_Compare_s{
    //Either the value of the first character that does not match is lower in the compared string,
    //or all compared characters match but the compared string is shorter.
    int LOWER;

    //They compare equal
    int EQUAL;

    //EiSTRING_COMPAREther the value of the first character that does not match is greater in the compared string,
    //or all compared characters match but the compared string is longer.
    int GREATER;
};

extern const struct String_Compare_s STRING_COMPARE;

//Tests if this string begin with the specified prefix.
bool String_beginsWith(const char * s, const char * prefix);

//Appends a copy of the source string to the destination string.
//The terminating null character in destination is overwritten by the first character of source,
//and a null-character is included at the end of the new string formed by the concatenation of
//both in destination
char * String_concat(char * destination, const char * source);

//Compares two strings lexicographically.
int String_compare(const char * s1, const char * s2);

//Tests if this string ends with the specified suffix.
bool String_endsWith(const char * s, const char * suffix);

//Compares this String to another String, ignoring case considerations.
bool String_equalsIgnoreCase(const char * s1, const char * s2);

//Compares this String to another String
bool String_equals(const char * s1, const char * s2);

//Returns a formatted string using the specified locale, format string, and arguments.
char * String_format(const char * format, ...);

//Converts all of the characters in this string to lower case.
char * String_toLowerCase(const char * s);

//Converts all of the characters in this String to upper;
char * String_toUpperCase(const char * s);

//Returns a string representation of the string argument as an string in base 16.
char * String_toHexString(const char * s);

//Returns the string, with leading and trailing whitespace omitted.
// returned string is equal to s after function call.
char * String_trim(char * s);

//Returns the string representation of the int64 argument.
char * String_valueOfS64(int64 b);

//Returns the string representation of the boolean argument.
char * String_valueOfB(bool b);

//Returns the string representation of the byte argument.
char * String_valueOfBt(ubyte b);


//Returns the string representation of the double argument.
char * String_valueOfD(double d);

//Returns the string representation of the float argument.
char * String_valueOfF(float f);

//Returns the string representation of the int argument.
char * String_valueOfI32(int i);

//Returns the string representation of the long argument.
char * String_valueOfS32(long l);

//Returns the string representation of the short argument.
char * String_valueOfS16(short i);


#endif /* STRING_H_ */
