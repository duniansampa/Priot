#ifndef SHORT_H_
#define SHORT_H_

/*!
 * \brief     Short class.
 * \details   A class to define Short operations.
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


#define SHORT_MAXVALUE    SHRT_MAX
#define SHORT_MIN_VALUE   SHRT_MIN
#define SHORT_SIZE        sizeof(short)



//Parses the string argument as a signed decimal short.
short Short_parseShort(const _s8 * s);

//Parses the string argument as a signed short in the base specified by the second argument.
short Short_parseShort(const _s8 * s, int base);


//Returns a string representation of the 'short' argument as an unsigned '_ubyte' in base 16.
string Short_toHexString(short sh);


//Returns a new String object representing the specified short.
string Short_toString(short sh);


#endif /* SHORT_H_ */
