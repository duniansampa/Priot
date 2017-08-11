#ifndef LONG_H_
#define LONG_H_

/*!
 * \brief     Long class.
 * \details   A class to define Long operations.
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


#define  LONG_MAXVALUE     LONG_MAX
#define  LONG_MINVALUE     LONG_MIN
#define  LONG_SIZE         sizeof(long)
#define  LONG_TYPE         CLASS_NAME(Long)


//Parses the string argument as a signed decimal long.
long   Long_parseLong(const _s8 * s);

//Parses the string argument as a signed long in the 'base' specified by the second argument.
long   Long_parseLong(const _s8 *  s, int base);

//Returns a string representation of the long argument as an unsigned integer in base 2.
string Long_toBinaryString(long i);

//Returns a string representation of the long argument as an unsigned integer in base 16.
string Long_toHexString(long i);

//Returns a string representation of the long argument as an unsigned integer in base 8.
string Long_toOctalString(long i);


//Returns a string object representing the specified long.
string Long_toString(long i);

//Returns a string representation of the first argument in the 'base' specified by the second argument.
string	Long_toString(unsigned long i, int base);


#endif /* LONG_H_ */
