#ifndef INTEGER_H_
#define INTEGER_H_

#include "Generals.h"
/*!
 * \brief     Integer class.
 * \details   A class to define Integer operations.
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

#define  INTEGER_MAXVALUE    INT_MAX
#define  INTEGER_MIN_VALUE   INT_MIN
#define  INTEGER_SIZE        sizeof(int)



//Parses the string argument as a signed decimal integer.
int	Integer_parse1(const char * s);

//Parses the string argument as a signed integer in the base specified by the second argument.
int	Integer_parseInt2(const  char * s, int base);

//Parses the string argument as signed integer in the base specified by the third argument.
//sub is an object of type string, whose value is set by the function to the substring that starts to next character
//in s after the numerical value.
int	Integer_parseInt3(const  char * s,  char * sub, int base);


//Returns a string representation of the integer argument as an unsigned integer in base 2.
char * Integer_toBinaryString(int i);

//Returns a string representation of the integer argument as an unsigned integer in base 16.
char * Integer_toHexString(int i);

//Returns a string representation of the integer argument as an unsigned integer in base 8.
char * Integer_toOctalString(int i);


//Returns a String object representing the specified integer.
char * 	Integer_toString1(int i);

// Returns a string representation of the first argument in the base specified by the second argument.
char * 	Integer_toString2(unsigned int i, int base);


// Returns a Integer whose value is equivalent to this Integer with the designated bit cleared.
int Integer_clearBit(int value, int n);

// Returns a Integer whose value is equivalent to this Integer with the designated bit set.
int Integer_setBit(int value, int n);

// Returns true if and only if the designated bit is set.
bool Integer_testBit(int value, int n);


#endif /* INTEGER_H_ */
