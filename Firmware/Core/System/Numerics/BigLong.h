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


#include "../../Config.h"
#include "../../Generals.h"
#include "../String.h"



#define  BIGLONG_MAXVALUE    LLONG_MAX
#define  BIGLONG_MINVALUE    LLONG_MIN
#define  BIGLONG_SIZE        sizeof(int64)



//Parses the string argument as a signed decimal int64.
int64 BigLong_parseLong1(const char * s);

//Parses the string argument as a signed int64 in the 'base' specified by the second argument.
int64 BigLong_parseLong2(const char * s, uchar base);

//Returns a string representation of the int64 argument as an unsigned integer in base 2.
char * BigLong_toBinaryString(int64 i);

//Returns a string representation of the int64 argument as an unsigned integer in base 16.
char * BigLong_toHexString(int64 i);

//Returns a string object representing the specified int64.
char * BigLong_toString(int64 i);

//Returns a string representation of the int64 argument as an unsigned integer in base 8.
char * BigLong_toOctalString1(int64 i);

//Returns a string representation of the first argument in the 'base' specified by the second argument.
char * BigLong_toString2(int64 i, uchar base);

#endif /* BIG_LONG_H_ */
