#ifndef BIG_LONG_H_
#define BIG_LONG_H_

/*!
 * \brief     A class to define Number operations. class.
 * \details   A class to define BigLong operations.
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



#define  BIGLONG_MAXVALUE    LLONG_MAX
#define  BIGLONG_MINVALUE    LLONG_MIN
#define  BIGLONG_SIZE        sizeof(_s64)



//Parses the string argument as a signed decimal _s64.
_s64 BigLong_parseLong(const _s8 * s);

//Parses the string argument as a signed _s64 in the 'base' specified by the second argument.
_s64 BigLong_parseLong(const _s8 * s, _u8 base);

//Returns a string representation of the _s64 argument as an unsigned integer in base 2.
_s8 * BigLong_toBinaryString(_s64 i);

//Returns a string representation of the _s64 argument as an unsigned integer in base 16.
_s8 * BigLong_toHexString(_s64 i);

//Returns a string representation of the _s64 argument as an unsigned integer in base 8.
_s8 * BigLong_toOctalString(_s64 i);

//Returns a string object representing the specified _s64.
_s8 * BigLong_toString(_s64 i);

//Returns a string representation of the first argument in the 'base' specified by the second argument.
_s8 * BigLong_toString(_s64 i, _u8 base);

#endif /* BIG_LONG_H_ */
