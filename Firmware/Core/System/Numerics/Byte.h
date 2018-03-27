#ifndef BYTE_H_
#define BYTE_H_

#include "Generals.h"

/*!
 * \brief     Byte class.
 * \details   A class to define Byte operations.
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
#include "Double.h"



#define  BYTE_MAXVALUE  UCHAR_MAX
#define  BYTE_MINVALUE  0
#define  BYTE_SIZE      sizeof(ubyte)

//Parses the string argument as a decimal ubyte.
ubyte Byte_parseByte1(const char * s);

//Parses the string argument as a ubyte in the base specified by the second argument.
ubyte  Byte_parseByte2(const char * s, uchar base);

//Returns a string representation of the 'ubyte' argument as an unsigned 'ubyte' in base 2.
char *  Byte_toBinaryString(ubyte b);

//Returns a string representation of the 'ubyte' argument as an unsigned 'ubyte' in base 16.
char *  Byte_toHexString(ubyte b);

//Returns a new String object representing the specified ubyte.
char *  Byte_toString1(ubyte b);

//Returns a string representation of the first argument in the 'base' specified by the second argument.
char * 	Byte_toString2(ubyte b, uchar base);

// Returns a Byte whose value is equivalent to this Byte with the designated bit cleared.
ubyte Byte_clearBit(ubyte b, uchar n);

// Returns a Byte whose value is equivalent to this Byte with the designated bit set.
ubyte Byte_setBit(ubyte b, uchar n);

// Returns true if and only if the designated bit is set.
bool Byte_testBit(ubyte b, uchar n);

double Byte_getValue(ubyte * beginByte, ubyte startBit, ubyte numBit, double resolution, bool isSigned);

void Byte_setValue(double value, ubyte * beginByte, ubyte startBit, ubyte numBit, double resolution);

void Byte_reflectValue(ubyte * beginByte, ubyte startBit, ubyte numBit);


#endif /* BYTE_H_ */
