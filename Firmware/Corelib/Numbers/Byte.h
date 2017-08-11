#ifndef BYTE_H_
#define BYTE_H_

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


#include "../Settings.h"
#include "../Generals.h"
#include "String.h"
#include "Double.h"



#define  BYTE_MAXVALUE  UCHAR_MAX
#define  BYTE_MINVALUE  0
#define  BYTE_SIZE      sizeof(_ubyte)

//Parses the string argument as a decimal _ubyte.
_ubyte Byte_parseByte(const _s8 * s);

//Parses the string argument as a _ubyte in the base specified by the second argument.
_ubyte  Byte_parseByte(const _s8 * s, _u8 base);

//Returns a string representation of the '_ubyte' argument as an unsigned '_ubyte' in base 2.
_s8 *  Byte_toBinaryString(_ubyte b);

//Returns a string representation of the '_ubyte' argument as an unsigned '_ubyte' in base 16.
_s8 *  Byte_toHexString(_ubyte b);

//Returns a new String object representing the specified _ubyte.
_s8 *  Byte_toString(_ubyte b);

//Returns a string representation of the first argument in the 'base' specified by the second argument.
_s8 * 	Byte_toString(_ubyte b, _u8 base);

// Returns a Byte whose value is equivalent to this Byte with the designated bit cleared.
_ubyte Byte_clearBit(_ubyte b, _u8 n);

// Returns a Byte whose value is equivalent to this Byte with the designated bit set.
_ubyte Byte_setBit(_ubyte b, _u8 n);

// Returns true if and only if the designated bit is set.
_bool Byte_testBit(_ubyte b, _u8 n);

double Byte_getValue(_ubyte * beginByte, _ubyte startBit, _ubyte numBit, double resolution, bool isSigned = false);

void Byte_setValue(double value, _ubyte * beginByte, _ubyte startBit, _ubyte numBit, double resolution);

void Byte_reflectValue(_ubyte * beginByte, _ubyte startBit, _ubyte numBit);


#endif /* BYTE_H_ */
