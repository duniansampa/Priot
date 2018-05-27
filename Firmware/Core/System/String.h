#ifndef IOT_STRING_H
#define IOT_STRING_H

/**
 * @brief     String class.
 * @details   A class to define String operations.
 * @author    Dunian Coutinho Sampa
 * @version   1.0
 * @date      2015-2016
 * @pre       Have a Linux and C/C++ environment configured.
 * @bug       No bug.
 * @warning   N/A
 * @copyright AdianteX LTDA.
 */

#include "Containers/List.h"

/** ============================[ Macros ]============================ */

#define STRING_appendRealloc( b, l, o, a, s ) String_appendRealloc( b, l, o, a, ( const u_char* )s )

/** ============================[ Types ]================== */

enum StringCompare_e {
    /** Either the value of the first character that does not match is lower in the compared string,
        or all compared characters match but the compared string is shorter. */
    StringCompare_LOWER = -1,

    /** They compare equal */
    StringCompare_EQUAL = 0,

    /** Either the value of the first character that does not match is greater in the compared string,
        or all compared characters match but the compared string is longer. */
    StringCompare_GREATER = 1
};

/** =============================[ Functions Prototypes ]================== */

/** @brief  This function returns a pointer to a new string which is a duplicate of
 *          the string s. Memory for the new string can be freed with free().
 *
 * @param   s - the string
 * @return  returns a pointer to a new string which.
 */
char* String_new( const char* s );

/** @brief  Tests if this string begin with the specified prefix.
 *
 * @param   s - the string
 * @param   prefix - the prefix
 * @return  returns true if the \p s starts with \p prefix; otherwise returns false.
 */
bool String_beginsWith( const char* s, const char* prefix );

/** @brief  appends a copy of the \p source string to the \p destination string.
*           The terminating null character in \p destination is overwritten by
*           the first character of \p source,  and a null-character is included
*           at the end of the new string formed by the concatenation of
*           both in \p destination.
*
*   @param  destination - Pointer to the destination array, which should
*                         contain a C string, and be large enough
*                         to contain the concatenated resulting string.
*
*   @param  source - a string to be appended. This should not overlap destination.
*   @return \p destination is returned.
*/
char* String_append( char* destination, const char* source );

/** @brief  appends a copy of the first \p num characters of \p source string to
*           the \p destination string. The terminating null character in \p destination
*           is overwritten by the first character of \p source,  and a null-character is included
*           at the end of the new string formed by the concatenation of both in \p destination.
*
*   @param  destination - Pointer to the destination array, which should
*                         contain a C string, and be large enough
*                         to contain the concatenated resulting string.
*
*   @param  source - a string to be appended. This should not overlap destination.
*   @param  num - maximum number of characters to be appended.
*   @return \p destination is returned.
*/
char* String_appendLittle( char* destination, const char* source, int num );

/** @brief  Concatenates a copy of the first \p num characters of \p s1 string and the \p s2 string.
 *          The size of the new string is the sum of the size of \p s1 and \p s2.
*           Note: this function does not truncate anything.
*
*   @param  s1 - a string
*   @param  s2 - a string
*   @param  num - maximum number of characters of s2 to be concatenated.
*   @return \p a new string.
*/
char* String_appendAlloc( const char* s1, const char* s2, int num );

/**
 * Concatenates the content of the \p s to the buffer
 *
 * @param buffer - address of a pointer (pointer to pointer) for the output buffer.
 *
 * @param bufferLen - pointer to a size_t containing the initial size of buf.
 *
 * @param outLen - the length of the buffer. Refers to the amount of data in the buffer.
 *
 * @param allowRealloc - If true, the buffer can be reallocated. If false, and
 *                       the buffer is not large enough to contain the string,
 *                       an error will be returned.
 *
 * @param s - pointer to data string to be appended.
 *
 * @retval 1  success
 * @retval 0  error
 */
int String_appendRealloc( u_char** buffer, size_t* buffferLen, size_t* outLen, int allowRealloc, const u_char* s );

/** @brief  Appends \p source to string \p destination of size \p destinationMaxSize
 *          (unlike String_appendLittle, \p destinationMaxSize is the full size of \p destination,
 *          not space left). At most destinationMaxSize-1 characters will be copied.
 *          If destinationMaxSize <= String_length(destination),  destination is returned.
 *          If destination length + source length >= destinationMaxSize, truncation occurred.
 *
 *  @param  destination - the destination buffer
 *  @param  source - the source
 *  @param  destinationMaxSize - the size of the \p destination buffer
 *  @return the \p destination buffer
 */
char* String_appendTruncate( char* destination, const char* source, int destinationMaxSize );

/** @brief  Copies src to the \p destination buffer. The copy will never overflow the
 *          \p destination buffer and \p destination will always be null terminated,
 *          \p destinationMaxSize is the size of the \p destination buffer.
 *
 *  @param  destination - the destination buffer
 *  @param  source - the source
 *  @param  destinationMaxSize - the size of the \p destination buffer
 *  \retrun the \p destination buffer
 */
char* String_copyTruncate( char* destination, const char* source, int destinationMaxSize );

/** @brief  Compares \p s1 with \p s2 and returns an integer less than,
 *          equal to, or greater than zero if \p s1 is less than,
 *          equal to, or greater than \p s2.
 *
 *  @param  s1 - string to be compared.
 *  @param  s2 - string to be compared.
 *
 *  @retval <0  - the first character that does not match has a lower value in s1 than in \p s2
 *  @retval  0  - the contents of both strings are equal
 *  @retval >0  - the first character that does not match has a greater value in \p s1 than in \p s2
 */
int String_compare( const char* s1, const char* s2 );

/** @brief  Tests if this string ends with the specified \p suffix.
 *
 * @param   s - the string
 * @param   suffix - the suffix
 * @return  returns true if the string ends with \p suffix; otherwise returns false.
 *          Note that the result will be true if the \p suffix' is the empty
 *          string or is equal to this String \p s.
 */
bool String_endsWith( const char* s, const char* suffix );

/** @brief  Compares this String to another String, ignoring case
 *          considerations. Two strings are considered equal ignoring
 *          case if they are of the same length and corresponding characters
 *          in the two strings are equal ignoring case.
 *
 *  @param  s1 - string to be compared.
 *  @param  s2 - string to be compared.
 *
 *  @retval 0  - the contents of both strings are defferent (s1 != s2)
 *  @retval 1  - the contents of both strings are equal (s1 == s2)
 */
bool String_equalsIgnoreCase( const char* s1, const char* s2 );

/** @brief  Compares this String to another String. Two strings are
 *          considered equal if they are of the same length and corresponding
 *          characters in the two strings are equal considering case.
 *
 *  @param  s1 - string to be compared.
 *  @param  s2 - string to be compared.
 *
 *  @retval 0  - the contents of both strings are defferent (s1 != s2)
 *  @retval 1  - the contents of both strings are equal (s1 == s2)
 */
bool String_equals( const char* s1, const char* s2 );

/** @brief  Returns true if, and only if, length() is 0.
 *
 *  @param  s1 - the string.
 *  @return true if length() is 0, otherwise false
*/
bool String_isEmpty( const char* s1 );

/** @brief  Returns true if this string is null; otherwise returns false.
 *
 *  @param  s1 - the string.
 *  @return true if \p s is NULL, otherwise false
*/
bool String_isNull( const char* s1 );

/** @brief  Returns the length of this string. The length
 *          is equal to the number of characters in this string
 *
 *  @param  s1 - the string.
 *  @return the length of the sequence of characters.
 */
int String_length( const char* s1 );

/** @brief  Returns a formatted string using the specified format string and arguments.
 *
 *  @param  format - A format string
 *  @param  args - Arguments referenced by the format specifiers in the format string.
 *                 If there are more arguments than format specifiers, the extra arguments
 *                 are ignored.
 *
 *  @return A formatted string
 */
char* String_format( const char* format, ... );

/** @brief  Converts all of the characters in this String to lower case.
 *
 *  @param  s - the string
 *  @return the string, converted to lowercase.
 */
char* String_toLowerCase( const char* s );

/** @brief  Converts all of the characters in this string to upper case
 *
 *  @param  s - the string
 *  @return the string, converted to uppercase.
 */
char* String_toUpperCase( const char* s );

/** @brief  Returns a new string, with leading and trailing whitespace omitted.
 *
 *  @param  s - the string
 *  @return a new string with leading and trailing whitespace removed.
 */
char* String_trim( const char* s );

/** @brief  Returns true if and only if this string contains the
 *          specified sequence of char values.
 *
 *  @param  s - the string
 *  @param  sequence - the sequence to search for
 *  @return true if this string contains sequence, false otherwise
 */
bool String_contains( const char* s, const char* sequence );

/** @brief  Returns a new string that is a substring of this string.
 *          The substring begins with the character at the specified
 *          index and extends to the end of this string.
 *
 *  @param  s - the string
 *  @param  beginIndex - the beginning index, inclusive.
 *  @return the specified substring.
 */
char* String_subStringBeginAt( const char* s, int beginIndex );

/** @brief  Returns a new string that is a substring of this string.
 *          The substring begins at the specified beginIndex and extends
 *          to the character at index beginIndex + length - 1. Thus the length of the
 *          substring is \p length.
 *
 *  @param  s - the string
 *  @param  beginIndex - the beginning index, inclusive.
 *  @param  length - Number of characters to include in the substring
 *  @return the specified substring.
 */
char* String_subString( const char* s, int beginIndex, int length );

/** @brief  Returns a new string resulting from replacing all occurrences of oldChar
 *          in this string with newChar.
 *
 *  @param  s - the string
 *  @param  oldChar - the old character.
 *  @param  newChar - the new character.
 *  @return a string derived from this string by replacing every
 *          occurrence of oldChar with newChar.
 */
char* String_replaceChar( const char* s, char oldChar, char newChar );

/** @brief  Replaces each substring of this string that matches the literal
 *          target sequence with the specified literal replacement sequence.
 *          The replacement proceeds from the beginning of the string to the end,
 *          for example, replacing "aa" with "b" in the string "aaa" will result
 *          in "ba" rather than "ab".
 *
 *  @param  s - the string
 *  @param  target - The sequence of char values to be replaced
 *  @param  replacement - The replacement sequence of char values
 *  @return The resulting string.
 */
char* String_replace( const char* s, const char* target, const char* replacement );

/** Returns the string representation of the int64_t argument.
 *
 *  @param i - an int64_t.
 *  @return a string representation of int64_t argument.
 */
char* String_setInt64( int64_t i );

/** @brief  Returns the string representation of the boolean argument.
 *
 *  @param  b - a boolean
 *  @return if the argument is true, a string equal to "true" is returned;
 *          otherwise, a string equal to "false" is returned.
 */
char* String_setBool( bool b );

/** @brief  Returns the string representation of the int8_t argument.
 *
 *  @param b - an int8_t.
 *  @return a string representation of int8_t argument.
 */
char* String_setInt8( int8_t b );

/** @brief  Returns the string representation of the double argument.
 *
 *  @param  d - an double.
 *  @return a string representation of double argument.
 */
char* String_setDouble( double d );

/** @brief  Returns the string representation of the float argument.
 *
 *  @param  f - an float.
 *  @return a string representation of float argument.
 */
char* String_setFloat( float f );

/** @brief  Returns the string representation of the int32_t argument.
 *
 *  @param  i - an int32_t.
 *  @return a string representation of int32_t argument.
 */
char* String_setInt32( int32_t i );

/** @brief  Returns the string representation of the int16_t argument.
 *
 *  @param  s - an int16_t.
 *  @return a string representation of int16_t argument.
 */
char* String_setInt16( int16_t i );

/** @brief  Returns the index within this string of the first occurrence
 *          of the specified substring, starting at the specified index.

 *
 *  @param  s - the string.
 *  @param  substring - the substring to search for.
 *  @param  fromIndex - the index to start the search from.
 *
 *  @return the index of the first occurrence of the specified
 *          substring, starting at the specified index,
 *          or -1 if there is no such occurrence.
 */
int String_indexOf( const char* s, const char* substring, int fromIndex );

/** @brief  Returns the index within this string of the last occurrence of the specified
 *          character, searching backward starting at the specified index.
 *
 *  @param  s - the string.
 *  @param  ch - a character
 *  @param  fromIndex - the index to start the search from.
 *
 *  @return the index of the first occurrence of the character in the character
 *          sequence represented by this string that is greater than or equal to
 *          fromIndex, or -1 if the character does not occur.
 */
int String_indexOfChar( const char* s, const char ch, int fromIndex );

/** @brief  Splits the string into substrings wherever sep occurs, and returns
 *          the map of those strings. If sep does not match anywhere in the string,
 *          split() returns a single-element list containing this string.
 *
 *  @param  s - the string.
 *  @param  delimiters - C string containing the delimiter characters.
 *
 *  @return a map of substrings where \e key is the index and \e value is the substrings
 */
List* String_split( const char* s, const char* delimiters );

/** @brief  Sets the first \p num character in the string to character ch.
 *
 *  @param  s - the string.
 *  @param  ch - the character to be set.
 *  @param  num - number of character to be set to the \p ch.
 *  @return \p s
 */
char* String_fill( char* s, char ch, int num );

#endif /* IOT_STRING_H */
