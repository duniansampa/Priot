#ifndef IOT_CONVERT_H
#define IOT_CONVERT_H

#include "Generals.h"

/** ============================[ Macros ]============================ */

/** Converts \p c to upper case */
#define CONVERT_TO_UPPER( c ) ( c >= 'a' && c <= 'z' ? c - ( 'a' - 'A' ) : c )

/** Converts \p c to lower case */
#define CONVERT_TO_LOWER( c ) ( c >= 'A' && c <= 'Z' ? c + ( 'a' - 'A' ) : c )

/** Converts the hex (0..F) to value (0..15) */
#define CONVERT_HEX_TO_VALUE( s ) ( ( isalpha( s ) ? ( CONVERT_TO_LOWER( s ) - 'a' + 10 ) : ( CONVERT_TO_LOWER( s ) - '0' ) ) & 0xf )

/** Converts the value (0..15) to hex (0..F) */
#define CONVERT_VALUE_TO_HEX( s ) ( ( s ) + ( ( ( s ) >= 10 ) ? ( 'a' - 10 ) : '0' ) )

/** \def CONVERT_MACRO_VALUE_TO_STRING(s)
 *  Expands to string with value of the s.
 *  If s is macro, the resulting string is value of the macro.
 *  Example:
 *  \#define TEST 1234
 *  CONVERT_MACRO_VALUE_TO_STRING(TEST) expands to "1234"
 *  CONVERT_MACRO_VALUE_TO_STRING(TEST+1) expands to "1234+1"
 */
#define CONVERT_MACRO_VALUE_TO_STRING( s ) #s

/** =============================[ Functions Prototypes ]================== */

/** Parses the string argument as a decimal int8_t. */
int8_t Convert_stringToInt8( const char* s );

/** Parses the string argument as a ubyte in the base specified by the second argument. */
int8_t Convert_stringBaseToInt8( const char* s, uint8_t base );

/** @brief  Returns a string representation of the string argument as an string in base 16.
 *
 *  @param  s: the string
 *  @return string, converted to base 16.
 */
char* Convert_stringToHexString( const char* s );

/** Parses the string argument as a signed decimal Int16_t. */
int16_t Convert_stringToInt16( const char* s );

/** Parses the string argument as a signed int16_t in the base specified by the second argument. */
int16_t Convert_stringBaseToInt16( const char* s, uint8_t base );

/** Parses the string argument as a signed decimal int32_t. */
int32_t Convert_stringToInt32( const char* s );

/** Parses the string argument as a signed integer in the base specified by the second argument. */
int32_t Convert_stringBaseToInt32( const char* s, uint8_t base );

/** Parses the string argument as a signed decimal int64_t. */
int64_t Convert_stringToInt64( const char* s );

/** Parses the string argument as a signed int64_t in the 'base' specified by the second argument. */
int64_t Convert_stringBaseToInt64( const char* s, uint8_t base );

/** Returns a new double initialized to the value represented by
 *  the specified String, as performed by the valueOf method of class Double.
 */
double Convert_stringToDouble( const char* s );

/** Returns a new float initialized to the value represented by
 * the specified String, as performed by the valueOf method of class Float.
 */
float Convert_stringToFloat( const char* s );

/** Returns a string representation of the 'int8_t' argument as an unsigned 'int8_t' in base 2. */
char* Convert_int8ToBinaryString( int8_t b );

/** Returns a string representation of the 'int8_t' argument as an unsigned 'int8_t' in base 16. */
char* Convert_int8ToHexString( int8_t b );

/** Returns a new String object representing the specified int8_t. */
char* Convert_int8ToString( int8_t b );

/** Returns a string representation of the first argument in the 'base' specified by the second argument. */
char* Convert_int8ToStringBase( uint8_t b, uint8_t base );

/** Returns a string representation of the 'int16_t' argument as an 'int16_t' in base 16. */
char* Convert_int16ToHexString( int16_t sh );

/** Returns a new String object representing the specified int16_t. */
char* Convert_int16ToString( int16_t sh );

/** Returns a string representation of the int32_t argument as an int32_t in base 2. */
char* Convert_int32ToBinaryString( int32_t i );

/** Returns a string representation of the int32_t argument as an unsigned int32_t in base 16. */
char* Convert_int32ToHexString( int32_t i );

/** Returns a string representation of the int32_t argument as an unsigned int32_t in base 8. */
char* Convert_int32ToOctalString( int32_t i );

/** Returns a String object representing the specified int32_t. */
char* Convert_int32ToString( int32_t i );

/** Returns a string representation of the first argument in the base specified by the second argument. */
char* Convert_int32ToStringBase( int32_t i, uint8_t base );

/** Returns a string representation of the int64_t argument as an unsigned int64_t in base 2. */
char* Convert_int64ToBinaryString( int64_t i );

/** Returns a string representation of the int64_t argument as an unsigned int64_t in base 16. */
char* Convert_int64ToHexString( int64_t i );

/** Returns a string object representing the specified int64_t. */
char* Convert_int64ToString( int64_t i );

/** Returns a string representation of the int64_t argument as an unsigned int64_t in base 8. */
char* Convert_int64ToOctalString( int64_t i );

/** Returns a string representation of the first argument in the 'base' specified by the second argument. */
char* Convert_int64ToStringBase( int64_t i, uint8_t base );

/** Returns the value of the specified number as a int64_t. */
int64_t Convert_doubleToInt64( double value );

/** Returns the value of this Double as a int8_t (by casting to a int8_t). */
int8_t Convert_doubleToInt8( double value );

/** Returns the value of the specified number as a char. */
char Convert_doubleToChar( double d1 );

/** Returns the float value of this Double object. */
float Convert_doubleToFloat( double d1 );

/** Returns the value of this Double as an int32_t (by casting to type int32_t). */
int32_t Convert_doubleToInt32( double d1 );

/** Returns the value of this Double as a int16_t (by casting to a int16_t). */
int16_t Convert_doubleToInt16( double d1 );

/** Returns a string representation of the double argument as an Hexadecimal floating point */
char* Convert_doubleToHexString( double d );

/** Returns a string representation of the double argument. */
char* Convert_doubleToString( double d );

/** Returns the value of the specified number as a int64_t. */
int64_t Convert_floatToInt64( float value );

/** Returns the value of this Float as a int8_t (by casting to a int8_t). */
int8_t Convert_floatToInt8( float value );

/** Returns the value of the specified number as a char. */
char Convert_floatToChar( float value );

/** Returns the double value of this Float object. */
double Convert_floatToDouble( float value );

/** Returns the value of this Float as an int32_t (by casting to type int32_t). */
int32_t Convert_floatToInt32( float value );

/** Returns the value of this Float as a int16_t (by casting to a int16_t). */
int16_t Convert_floatToInt16( float value );

/** Returns a string representation of the float argument as an Hexadecimal floating point. */
char* Convert_floatToHexString( float f );

/** Returns a string representation of the float argument. */
char* Convert_floatToString( float f );

/**
 * Converts \p numBit bits within string to double, using a given resolution and flag \p isSigned.
 *
 * @param beginByte - the first byte of the string where the bits should be picked up.
 * @param startBit - the position of the first bit (which must be picked up) within the first byte.
 * @param numBit - the number of bits that must be picked up.
 * @param resolution - the resolution that should be used.
 * @param isSigned  - it tells you if value is signaled or not signaled.
 * @return a double value
*/
double Convert_bitsToDouble( uint8_t* beginByte, uint8_t startBit, uint8_t numBit, double resolution, bool isSigned );

/**
 * Converts double value to \p numBit bits within string, using a given resolution.
 *
 * @param value - the value that must be converted.
 * @param beginByte - the first byte of the string where the bits should be placed.
 * @param startBit - the position of the first bit (where the value should be placed) within the first byte.
 * @param numBit - the number of bits that must be placed.
 * @param resolution - the resolution that should be used.
*/
void Convert_doubleToBits( double value, uint8_t* beginByte, uint8_t startBit, uint8_t numBit, double resolution );

/**
 * It makes the reflection \p numBit bits within a string, starting at the
 * \p startBit position of the first byte of the string.
 *
 * @param beginByte - the first byte of the string where the bits should be reflected.
 * @param startBit - the position of the first bit (where the value should reflected) within the first byte.
 * @param numBit - the number of bits that must be reflected.
*/
void Convert_valueToReflectedValue( uint8_t* beginByte, uint8_t startBit, uint8_t numBit );

/**
 * Takes a time string like 4h and converts it to seconds.
 * The string time given may end in 's' for seconds (the default
 * anyway if no suffix is specified),
 * 'm' for minutes, 'h' for hours, 'd' for days, or 'w' for weeks.  The
 * upper case versions are also accepted.
 *
 * @param timeString The time string to convert.
 *
 * @return seconds converted from the string
 * @return -1  : on failure
 */
uint32_t Convert_stringTimeToSeconds( const char* time_string );

/** converts binary to hexidecimal
 *
 *     @param *input            Binary data.
 *     @param inputLen          Length of binary data.
 *     @param **dest            NULL terminated string equivalent in hex.
 *     @param *destLen         size of destination buffer
 *     @param allowRealloc     flag indicating if buffer can be realloc'd
 *
 * @return length of output string not including NULL terminator.
 */
u_int Convert_binaryStringToHexString( u_char** dest, size_t* destLen, int allowRealloc, const u_char* input, size_t inputLen );

/**
 * convert an ASCII hex string (with specified delimiters) to binary
 *
 * @param buffer  address of a pointer (pointer to pointer) for the output buffer.
 *                If allowRealloc is set, the buffer may be grown via Memory_realloc
 *                to accomodate the data.
 *
 * @param bufferLen pointer to a size_t containing the initial size of buf.
 *
 * @param offset  On input, a pointer to a size_t indicating an offset into buffer.
 *                The  binary data will be stored at this offset.
 *                On output, this pointer will have updated the offset to be
 *                the first byte after the converted data.
 *
 * @param allowRealloc  If true, the buffer can be reallocated. If false, and
 *                      the buffer is not large enough to contain the string,
 *                      an error will be returned.
 *
 * @param hexString    pointer to hex string to be converted. May be prefixed by
 *                     "0x" or "0X".
 *
 * @param delimiters   point to a string of allowed delimiters between bytes.
 *                     If not specified, any non-hex characters will be an error.
 *
 * @retval 1  success
 * @retval 0  error
 */
int Convert_hexStringToBinaryString( u_char** buffer, size_t* bufferLen, size_t* offset, int allowRealloc, const char* hexString, const char* delimiters );

/**
 * convert an ASCII hex string to binary
 *
 * \note This is a wrapper which calls Tools_hexToBinary with a
 * delimiter string of " ".
 *
 * See Tools_hexToBinary for parameter descriptions.
 *
 * @retval 1  success
 * @retval 0  error
 */
int Convert_hexStringToBinaryStringWrapper( u_char** buffer, size_t* bufferLen, size_t* offset, int allowRealloc, const char* hexString );

/**
 *	@param *input		Printable data in base16.
 *	@param inputLen		Length in bytes of data.
 *	@param **output     Binary data equivalent to input.
 *
 * @return ErrorCode_GENERR on failure, otherwise length of allocated string.
 *
 * Input of an odd length is right aligned.
 *
 *  FIX	Another version of "hex-to-binary" which takes odd length input
 *	strings.  It also allocates the memory to hold the binary data.
 *	Should be integrated with the official Convert_hexStringToBinaryString() function.
 */
int Convert_hexStringToBinaryString2( const u_char* input, size_t inputLen, char** output );

/**
 * convert an integer string to binary
 *
 * @param buffer - address of a pointer (pointer to pointer) for the output buffer.
 *
 * @param bufferLen - pointer to a size_t containing the initial size of buf.
 *
 * @param outLen - the length of the buffer. Refers to the amount of data in the buffer.
 *
 * @param allowRealloc - If true, the buffer can be reallocated. If false, and
 *                      the buffer is not large enough to contain the string,
 *                      an error will be returned.
 *
 * @param integerString - pointer to integer string to be converted.
 *
 * @retval 1  success
 * @retval 0  error
 */
int Convert_integerStringToBinaryString( u_char** buffer, size_t* bufferLen, size_t* outLen, int allowRealloc, const char* integerString );

/**
 * @brief Convert_variablesToDateAndTimeString
 *        is the wrapping of the Time_convertVariablesToDateAndTimeString
 *
 * @see Time_convertVariablesToDateAndTimeString
 */
int Convert_variablesToDateAndTimeString( uint8_t* buffer, size_t* bufferSize,
    uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minutes, uint8_t seconds,
    uint8_t deciSeconds, int utcOffsetDirection,
    uint8_t utcOffsetHours, uint8_t utcOffsetMinutes );

/**
 * @brief Convert_dateAndTimeToString
 *        is the wrapping of the Time_convertDateAndTimeToString
 *
 * @see Time_convertDateAndTimeToString
 */
u_char* Convert_dateAndTimeToString( const time_t* when, size_t* length );

/**
 * @brief Convert_ctimeStringToTimet
 *        is the wrapping of the Time_convertCtimeStringToTimet
 *
 * @see Time_convertCtimeStringToTimet
 */
time_t Convert_ctimeStringToTimet( const char* ctimeString );

#endif // IOT_CONVERT_H
