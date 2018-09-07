#ifndef IOT_ASN_H
#define IOT_ASN_H

#include "Generals.h"
#include "System/Numerics/Integer64.h"

#define asnPARSE_PACKET 0
#define asnDUMP_PACKET 1

/**
 * @note Definitions for Abstract Syntax Notation One, ASN.1
 *       As defined in ISO/IS 8824 and ISO/IS 8825
 */

/** ============================[ Macros ]============================ */

#define asnMIN_OID_LEN 2
/** max subid's in an oid */
#define asnMAX_OID_LEN 128
#define asnOID_LENGTH( x ) ( sizeof( x ) / sizeof( oid ) )

#define asnBOOLEAN ( ( u_char )0x01 )
#define asnINTEGER ( ( u_char )0x02 )
#define asnBIT_STR ( ( u_char )0x03 )
#define asnOCTET_STR ( ( u_char )0x04 )
#define asnNULL ( ( u_char )0x05 )
#define asnOBJECT_ID ( ( u_char )0x06 )
#define asnSEQUENCE ( ( u_char )0x10 )
#define asnSET ( ( u_char )0x11 )

#define asnUNIVERSAL ( ( u_char )0x00 )
#define asnAPPLICATION ( ( u_char )0x40 )
#define asnCONTEXT ( ( u_char )0x80 )
#define asnPRIVATE ( ( u_char )0xC0 )

#define asnPRIMITIVE ( ( u_char )0x00 )
#define asnCONSTRUCTOR ( ( u_char )0x20 )

#define asnLONG_LEN ( 0x80 )
#define asnEXTENSION_ID ( 0x1F )
#define asnBIT8 ( 0x80 )

#define asnIS_CONSTRUCTOR( byte ) ( ( byte )&asnCONSTRUCTOR )
#define asnIS_EXTENSION_ID( byte ) ( ( ( byte )&asnEXTENSION_ID ) == asnEXTENSION_ID )

/**
 * defined types (from the SMI, RFC 1157)
 */
#define asnIPADDRESS ( asnAPPLICATION | 0 )
#define asnCOUNTER ( asnAPPLICATION | 1 )
#define asnGAUGE ( asnAPPLICATION | 2 )
/* RFC 1902 - same as GAUGE */
#define asnUNSIGNED ( asnAPPLICATION | 2 )
#define asnTIMETICKS ( asnAPPLICATION | 3 )
/* changed so no conflict with other includes */
#define asnOPAQUE ( asnAPPLICATION | 4 )

/**
 * defined types (from the SMI, RFC 1442)
 */
/** historic - don't use */
#define asnNSAP ( asnAPPLICATION | 5 )
#define asnCOUNTER64 ( asnAPPLICATION | 6 )
/** historic - don't use */
#define asnUINTEGER ( asnAPPLICATION | 7 )

/**
 * defined types from draft-perkins-opaque-01.txt
 */
#define asnFLOAT ( asnAPPLICATION | 8 )
#define asnDOUBLE ( asnAPPLICATION | 9 )
#define asnINTEGER64 ( asnAPPLICATION | 10 )
#define asnUNSIGNED64 ( asnAPPLICATION | 11 )

/**
 * @note The BER inside an OPAQUE is an context specific with a value of 48 (0x30)
 * plus the "normal" tag. For a Counter64, the tag is 0x46 (i.e., an
 * applications specific tag with value 6). So the value for a 64 bit
 * counter is 0x46 + 0x30, or 0x76 (118 base 10). However, values
 * greater than 30 can not be encoded in one octet. So the first octet
 * has the class, in this case context specific (AsnCONTEXT), and
 * the special value (i.e., 31) to indicate that the real value follows
 * in one or more octets. The high order bit of each following octet
 * indicates if the value is encoded in additional octets. A high order
 * bit of zero, indicates the last. For this "hack", only one octet
 * will be used for the value.
 */

/** first octet of the tag */
#define asnOPAQUE_TAG1 ( asnCONTEXT | asnEXTENSION_ID )

/**
 * base value for the second octet of the tag - the
 * second octet was the value for the tag
 */
#define asnOPAQUE_TAG2 ( ( u_char )0x30 )

/** second octet of tag for union */
#define asnOPAQUE_TAG2U ( ( u_char )0x2f )

/**
 * All the ASN.1 types for PRIOT "should have been" defined in this file,
 * but they were not. (They are defined in Impl.h)  Thus, the tag for
 * Opaque and Counter64 is defined, again, here with a different names.
 */
#define asnAPP_OPAQUE ( asnAPPLICATION | 4 )
#define asnAPP_COUNTER64 ( asnAPPLICATION | 6 )
#define asnAPP_FLOAT ( asnAPPLICATION | 8 )
#define asnAPP_DOUBLE ( asnAPPLICATION | 9 )
#define asnAPP_I64 ( asnAPPLICATION | 10 )
#define asnAPP_U64 ( asnAPPLICATION | 11 )
#define asnAPP_UNION ( asnPRIVATE | 1 ) /* or ASN_PRIV_UNION ? */

/** value for Counter64 */
#define asnOPAQUE_COUNTER64 ( asnOPAQUE_TAG2 + asnAPP_COUNTER64 )

/** max size of BER encoding of Counter64 */
#define asnOPAQUE_COUNTER64_MX_BER_LEN 12

/** value for Float */
#define asnOPAQUE_FLOAT ( asnOPAQUE_TAG2 + asnAPP_FLOAT )

/** size of BER encoding of Float */
#define asnOPAQUE_FLOAT_BER_LEN 7

/** value for Double */
#define asnOPAQUE_DOUBLE ( asnOPAQUE_TAG2 + asnAPP_DOUBLE )

/** size of BER encoding of Double */
#define asnOPAQUE_DOUBLE_BER_LEN 11

/** value for Integer64 */
#define asnOPAQUE_I64 ( asnOPAQUE_TAG2 + asnAPP_I64 )

/** max size of BER encoding of Integer64 */
#define asnOPAQUE_I64_MX_BER_LEN 11

/** value for Unsigned64 */
#define asnOPAQUE_U64 ( asnOPAQUE_TAG2 + asnAPP_U64 )

/** max size of BER encoding of Unsigned64 */
#define asnOPAQUE_U64_MX_BER_LEN 12

#define asnPRIV_INCL_RANGE ( asnPRIVATE | 2 )
#define asnPRIV_EXCL_RANGE ( asnPRIVATE | 3 )
#define asnPRIV_DELEGATED ( asnPRIVATE | 5 )
/** 4 */
#define asnPRIV_IMPLIED_OCTET_STR ( asnPRIVATE | asnOCTET_STR )
/** 6 */
#define asnPRIV_IMPLIED_OBJECT_ID ( asnPRIVATE | asnOBJECT_ID )
/** 199 */
#define asnPRIV_RETRY ( asnPRIVATE | 7 )
#define asnIS_DELEGATED( x ) ( ( x ) == asnPRIV_DELEGATED )

/** =============================[ Functions Prototypes ]================== */

/**
 * @brief Asn_checkPacket
 *        checks the incoming packet for validity and returns its size or 0
 *
 * @param packet - The packet
 * @param length - The length to check
 *
 * @return The size of the packet if valid; 0 otherwise
 */
int Asn_checkPacket( u_char* packet, size_t length );

/**
 * @brief Asn_parseInt
 *        pulls a long out of an int type.
 *
 * @details On entry, dataLength is input as the number of valid bytes following
 *          `data`.  On exit, it is returned as the number of valid bytes
 *          following the end of this object.
 *
 * @param[in] data - pointer to start of object
 * @param[in,out] dataLength - number of valid bytes left in buffer
 * @param[in] type - asn type of object
 * @param[in,out] outputBuffer - pointer to start of output buffer
 * @param[in] outputBufferSize - size of output buffer
 *
 * @return pointer to the first byte past the end
 *         of this object (i.e. the start of the next object).
 *         Returns NULL on any error
 */
u_char* Asn_parseInt( u_char* data, size_t* dataLength, u_char* type,
    long* outputBuffer, size_t outputBufferSize );

/**
 * @brief Asn_buildInt
 *        builds an ASN object containing an integer.
 *
 * @details On entry, dataLength is input as the number of valid bytes following
 *          `data`.  On exit, it is returned as the number of valid bytes
 *          following the end of this object.
 *
 * @param[in] data - pointer to start of output buffer
 * @param[in,out] dataLength - number of valid bytes left in buffer
 * @param[in] type - asn type of objec
 * @param[in] intPointer - pointer to start of long integer
 * @param[in] intSize - size of input buffer
 *
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn_buildInt( u_char* data, size_t* dataLength,
    u_char type, const long* intPointer, size_t intSize );

/**
 * @internal
 * asn_parse_unsigned_int - pulls an unsigned long out of an ASN int type.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data       IN - pointer to start of object
 * @param datalength IN/OUT - number of valid bytes left in buffer
 * @param type       OUT - asn type of object
 * @param intp       IN/OUT - pointer to start of output buffer
 * @param intsize    IN - size of output buffer
 *
 * @return pointer to the first byte past the end
 *   of this object (i.e. the start of the next object) Returns NULL on any error
 */
u_char* Asn01_parseUnsignedInt( u_char*, size_t*, u_char*, u_long*, size_t );

/**
 * @internal
 * asn_build_unsigned_int - builds an ASN object containing an integer.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 *
 * @param data         IN - pointer to start of output buffer
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         IN  - asn type of objec
 * @param intp         IN - pointer to start of long integer
 * @param intsize      IN - size of input buffer
 *
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn01_buildUnsignedInt( u_char*, size_t*, u_char, const u_long*, size_t );

/**
 * @internal
 * asn_parse_string - pulls an octet string out of an ASN octet string type.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  "string" is filled with the octet string.
 * ASN.1 octet string   ::=      primstring | cmpdstring
 * primstring           ::= 0x04 asnlength byte {byte}*
 * cmpdstring           ::= 0x24 asnlength string {string}*
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data        IN - pointer to start of object
 * @param datalength  IN/OUT - number of valid bytes left in buffer
 * @param type        OUT - asn type of object
 * @param string      IN/OUT - pointer to start of output buffer
 * @param strlength   IN/OUT - size of output buffer
 *
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn01_parseString( u_char*, size_t*, u_char*, u_char*, size_t* );

/**
 * @internal
 * asn_build_string - Builds an ASN octet string object containing the input string.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         IN - asn type of object
 * @param string       IN - pointer to start of input buffer
 * @param strlength    IN - size of input buffer
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn01_buildString( u_char*, size_t*, u_char, const u_char*, size_t );

/**
 * @internal
 * Asn01_parseHeader - interprets the ID and length of the current object.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   in this object following the id and length.
 *
 *  Returns a pointer to the first byte of the contents of this object.
 *  Returns NULL on any error.
 *
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         OUT - asn type of object
 * @return  Returns a pointer to the first byte of the contents of this object.
 *          Returns NULL on any error.
 *
 */
u_char* Asn01_parseHeader( u_char*, size_t*, u_char* );

/**
 * @brief Asn01_parseLength
 *        interprets the string `data` and converts its contents to a long value.
 *
 * @note On exit, length contains the value of this length field.
 *       In case of error, calls `IMPL_ERROR_MSG` to store this error
 *       message in global error detail storage and return NULL.
 *
 * @note the first byte of `data` contains two information: the data
 *       type (in this case the long) and the data size. Bit 7
 *       represents the long type and the remainder bits represent
 *       the size (Z) of the long type. If there is no error in the first
 *       byte of `data`, the Z bytes of the `data` will be converted to
 *       a long value (`length`).
 *
 * @param[in] data - pointer to start of length field
 * @param[out] length - value of length field
 *
 * @return Returns a pointer to the first byte after this length
 *         field (aka: the start of the data field).
 *         Returns NULL on any error.
 */
u_char* Asn01_parseLength( u_char* data, u_long* length );

/**
 * @internal
 * same as Asn01_parseHeader with test for expected type
 *
 * @see Asn01_parseHeader
 *
 * @param data          IN - pointer to start of object
 * @param datalength    IN/OUT - number of valid bytes left in buffer
 * @param type          OUT - asn type of object
 * @param expected_type IN expected type
 * @return  Returns a pointer to the first byte of the contents of this object.
 *          Returns NULL on any error.
 *
 */
u_char* Asn01_parseSequence( u_char*, size_t*, u_char*, u_char expected_type, /* must be this type */
    const char* estr ); /* error message prefix */

/**
 * @brief Asn01_buildHeader
 *        builds an ASN header for an object with the ID and
 *        length specified.
 *
 *  On entry, `dataLength` is input as the number of valid bytes following
 *  `data`.  On exit, it is returned as the number of valid bytes
 *  in this object following the id and length.
 *
 *  This only works on data types < 30, i.e. no extension octets.
 *  The maximum length is 0xFFFF;
 *
 *  Returns a pointer to the first byte of the contents of this object.
 *  Returns NULL on any error.
 *
 * @param[in] data - pointer to start of object
 * @param[in,out] dataLength - number of valid bytes left in buffer
 * @param[in] type - asn type of object
 * @param[in] length - length of object
 *
 * @return Returns a pointer to the first byte of the
 *         contents of this object. Returns NULL on any error.
 */
u_char* Asn01_buildHeader( u_char* data, size_t* dataLength, u_char type, size_t length );

/**
 * @internal
 * asn_build_sequence - builds an ASN header for a sequence with the ID and
 *
 * length specified.
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   in this object following the id and length.
 *
 *  This only works on data types < 30, i.e. no extension octets.
 *  The maximum length is 0xFFFF;
 *
 *  Returns a pointer to the first byte of the contents of this object.
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         IN - asn type of object
 * @param length       IN - length of object
 *
 * @return Returns a pointer to the first byte of the contents of this object.
 *         Returns NULL on any error.
 */
u_char* Asn01_buildSequence( u_char*, size_t*, u_char, size_t );

/**
 * @internal
 * Asn01_buildLength - builds an ASN header for a length with
 * length specified.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   in this object following the length.
 *
 *
 *  Returns a pointer to the first byte of the contents of this object.
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param length       IN - length of object
 *
 * @return Returns a pointer to the first byte of the contents of this object.
 *         Returns NULL on any error.
 */
u_char* Asn01_buildLength( u_char*, size_t*, size_t );

/**
 * @internal
 * asn_parse_objid - pulls an object indentifier out of an ASN object identifier type.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  "objid" is filled with the object identifier.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         OUT - asn type of object
 * @param objid        IN/OUT - pointer to start of output buffer
 * @param objidlength  IN/OUT - number of sub-id's in objid
 *
 *  @return Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 */
u_char* Asn01_parseObjid( u_char*, size_t*, u_char*, oid*, size_t* );

u_char* Asn01_buildObjid( u_char*, size_t*, u_char, oid*, size_t );

/**
 * @internal
 * asn_parse_null - Interprets an ASN null type.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         OUT - asn type of object
 *  @return Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn01_parseNull( u_char*, size_t*, u_char* );

/**
 * @internal
 * asn_build_null - Builds an ASN null object.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         IN - asn type of object
 * @retun  Returns a pointer to the first byte past the end
 *         of this object (i.e. the start of the next object).
 *         Returns NULL on any error.
 *
 */
u_char* Asn01_buildNull( u_char*, size_t*, u_char );

/**
 * @internal
 * asn_parse_bitstring - pulls a bitstring out of an ASN bitstring type.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  "string" is filled with the bit string.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         OUT - asn type of object
 * @param string       IN/OUT - pointer to start of output buffer
 * @param strlength    IN/OUT - size of output buffer
 * @return Returns a pointer to the first byte past the end
 *         of this object (i.e. the start of the next object).
 *         Returns NULL on any error.
 */
u_char* Asn01_parseBitstring( u_char*, size_t*, u_char*, u_char*, size_t* );

/**
 * @internal
 * asn_build_bitstring - Builds an ASN bit string object containing the
 * input string.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the beginning of the next object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         IN - asn type of object
 * @param string       IN - pointer to start of input buffer
 * @param strlength    IN - size of input buffer
 * @return Returns a pointer to the first byte past the end
 *         of this object (i.e. the start of the next object).
 *         Returns NULL on any error.
 */
u_char* Asn01_buildBitstring( u_char*, size_t*, u_char, const u_char*, size_t );

/**
 * @internal
 * asn_parse_unsigned_int64 - pulls a 64 bit unsigned long out of an ASN int
 * type.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         OUT - asn type of object
 * @param cp           IN/OUT - pointer to counter struct
 * @param countersize  IN - size of output buffer
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn01_parseUnsignedInt64( u_char*, size_t*, u_char*, Counter64*, size_t );

/**
 * @internal
 * asn_build_unsigned_int64 - builds an ASN object containing a 64 bit integer.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of output buffer
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         IN  - asn type of object
 * @param cp           IN - pointer to counter struct
 * @param countersize  IN - size of input buffer
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn01_buildUnsignedInt64( u_char*, size_t*, u_char,
    const Counter64*, size_t );

/**
 * @internal
 * asn_parse_signed_int64 - pulls a 64 bit signed long out of an ASN int
 * type.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.

 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         OUT - asn type of object
 * @param cp           IN/OUT - pointer to counter struct
 * @param countersize  IN - size of output buffer
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn01_parseSignedInt64( u_char*, size_t*, u_char*, Counter64*, size_t );

/**
 * @internal
 * asn_build_signed_int64 - builds an ASN object containing a 64 bit integer.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of output buffer
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         IN  - asn type of object
 * @param cp           IN - pointer to counter struct
 * @param countersize  IN - size of input buffer
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn01_buildSignedInt64( u_char*, size_t*, u_char, const Counter64*, size_t );

/**
 * @internal
 * asn_build_float - builds an ASN object containing a single precision floating-point
 *                    number in an Opaque value.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         IN - asn type of object
 * @param floatp       IN - pointer to float
 * @param floatsize    IN - size of input buffer
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.

 */
u_char* Asn01_buildFloat( u_char*, size_t*, u_char, const float*, size_t );

/**
 * @internal
 * asn_parse_float - pulls a single precision floating-point out of an opaque type.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         OUT - asn type of object
 * @param floatp       IN/OUT - pointer to float
 * @param floatsize    IN - size of output buffer
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn01_parseFloat( u_char*, size_t*, u_char*, float*, size_t );

/**
 * @internal
 * asn_build_double - builds an ASN object containing a double
 *                    number in an Opaque value.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         IN - asn type of object
 * @param doublep      IN - pointer to double
 * @param doublesize   IN - size of input buffer
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn01_buildDouble( u_char*, size_t*, u_char, const double*, size_t );

/**
 * @internal
 * asn_parse_double - pulls a double out of an opaque type.
 *
 *  On entry, datalength is input as the number of valid bytes following
 *   "data".  On exit, it is returned as the number of valid bytes
 *   following the end of this object.
 *
 *  Returns a pointer to the first byte past the end
 *   of this object (i.e. the start of the next object).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of object
 * @param datalength   IN/OUT - number of valid bytes left in buffer
 * @param type         OUT - asn type of object
 * @param doublep       IN/OUT - pointer to double
 * @param doublesize    IN - size of output buffer
 * @return  Returns a pointer to the first byte past the end
 *          of this object (i.e. the start of the next object).
 *          Returns NULL on any error.
 */
u_char* Asn01_parseDouble( u_char*, size_t*, u_char*, double*, size_t );

/**
 * @internal
 * This function increases the size of the buffer pointed to by *pkt, which
 * is initially of size *pkt_len.  Contents are preserved **AT THE TOP END OF
 * THE BUFFER** (hence making this function useful for reverse encoding).
 * You can change the reallocation scheme, but you **MUST** guarantee to
 * allocate **AT LEAST** one extra byte.  If memory cannot be reallocated,
 * then return 0; otherwise return 1.
 *
 * @param pkt     buffer to increase
 * @param pkt_len initial buffer size
 *
 * @return 1 on success 0 on error (memory cannot be reallocated)
 */
int Asn01_realloc( u_char**, size_t* );

/*
 * Re-allocating reverse ASN.1 encoder functions.  Synopsis:
 *
 * u_char *buf = (u_char*)malloc(100);
 * u_char type = (ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER);
 * size_t buf_len = 100, offset = 0;
 * long data = 12345;
 * int allow_realloc = 1;
 *
 * if (asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
 * type, &data, sizeof(long)) == 0) {
 * error;
 * }
 *
 * NOTE WELL: after calling one of these functions with allow_realloc
 * non-zero, buf might have moved, buf_len might have grown and
 * offset will have increased by the size of the encoded data.
 * You should **NEVER** do something like this:
 *
 * u_char *buf = (u_char *)malloc(100), *ptr;
 * u_char type = (ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER);
 * size_t buf_len = 100, offset = 0;
 * long data1 = 1234, data2 = 5678;
 * int rc = 0, allow_realloc = 1;
 *
 * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
 * type, &data1, sizeof(long));
 * ptr = buf[buf_len - offset];   / * points at encoding of data1 * /
 * if (rc == 0) {
 * error;
 * }
 * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
 * type, &data2, sizeof(long));
 * make use of ptr here;
 *
 *
 * ptr is **INVALID** at this point.  In general, you should store the
 * offset value and compute pointers when you need them:
 *
 *
 *
 * u_char *buf = (u_char *)malloc(100), *ptr;
 * u_char type = (ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER);
 * size_t buf_len = 100, offset = 0, ptr_offset;
 * long data1 = 1234, data2 = 5678;
 * int rc = 0, allow_realloc = 1;
 *
 * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
 * type, &data1, sizeof(long));
 * ptr_offset = offset;
 * if (rc == 0) {
 * error;
 * }
 * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
 * type, &data2, sizeof(long));
 * ptr = buf + buf_len - ptr_offset
 * make use of ptr here;
 *
 *
 *
 * Here, you can see that ptr will be a valid pointer even if the block of
 * memory has been moved, as it may well have been.  Plenty of examples of
 * usage all over asn1.c, snmp_api.c, snmpusm.c.
 *
 * The other thing you should **NEVER** do is to pass a pointer to a buffer
 * on the stack as the first argument when allow_realloc is non-zero, unless
 * you really know what you are doing and your machine/compiler allows you to
 * free non-heap memory.  There are rumours that such things exist, but many
 * consider them no more than the wild tales of a fool.
 *
 * Of course, you can pass allow_realloc as zero, to indicate that you do not
 * wish the packet buffer to be reallocated for some reason; perhaps because
 * it is on the stack.  This may be useful to emulate the functionality of
 * the old API:
 *
 * u_char my_static_buffer[100], *cp = NULL;
 * size_t my_static_buffer_len = 100;
 * float my_pi = (float)22/(float)7;
 *
 * cp = asn_rbuild_float(my_static_buffer, &my_static_buffer_len,
 * ASN_OPAQUE_FLOAT, &my_pi, sizeof(float));
 * if (cp == NULL) {
 * error;
 * }
 *
 *
 * IS EQUIVALENT TO:
 *
 *
 * u_char my_static_buffer[100];
 * size_t my_static_buffer_len = 100, my_offset = 0;
 * float my_pi = (float)22/(float)7;
 * int rc = 0;
 *
 * rc = asn_realloc_rbuild_float(&my_static_buffer, &my_static_buffer_len,
 * &my_offset, 0,
 * ASN_OPAQUE_FLOAT, &my_pi, sizeof(float));
 * if (rc == 0) {
 * error;
 * }
 *
 *
 */

/**
 * @internal
 * builds an ASN object containing an int.
 *
 * @see asn_build_int
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type    IN - type of object
 * @param intp    IN - pointer to start of long integer
 * @param intsize IN - size of input buffer
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildInt( u_char** pkt, size_t* pkt_len,
    size_t* offset,
    int allow_realloc, u_char type,
    const long* data, size_t data_size );

/**
 * @internal
 * builds an ASN object containing an string.
 *
 * @see asn_build_string
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type    IN - type of object
 * @param string    IN - pointer to start of the string
 * @param strlength IN - size of input buffer
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildString( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    const u_char* data,
    size_t data_size );

/**
 * @internal
 * builds an ASN object containing an unsigned int.
 *
 * @see asn_build_unsigned_int
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type    IN - type of object
 * @param intp    IN - pointer to start of unsigned int
 * @param intsize IN - size of input buffer
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildUnsignedInt( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    const u_long* data,
    size_t data_size );

/**
 * @internal
 * builds an ASN header for an object with the ID and
 * length specified.
 *
 * @see asn_build_header
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type   IN - type of object
 * @param length   IN - length of object
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocBuildHeader( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    size_t data_size );

/**
 * @internal
 * builds an ASN object containing an sequence.
 *
 * @see asn_build_sequence
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type    IN - type of object
 * @param length IN - length of object
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildSequence( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    size_t data_size );

/**
 * @internal
 * reverse  builds an ASN header for a length with
 * length specified.
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param length  IN - length of object
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildLength( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    size_t data_size );

/**
 * @internal
 * builds an ASN object containing an objid.
 *
 * @see asn_build_objid
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type    IN - type of object
 * @param objid   IN - pointer to the object id
 * @param objidlength  IN - length of the input
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildObjid( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type, const oid*,
    size_t );

/**
 * @internal
 * builds an ASN object containing an null object.
 *
 * @see asn_build_null
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type    IN - type of object
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildNull( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type );

/**
 * @internal
 * builds an ASN object containing an bitstring.
 *
 * @see asn_build_bitstring
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type    IN - type of object
 * @param string   IN - pointer to the string
 * @param strlength  IN - length of the input
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildBitstring( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    const u_char* data,
    size_t data_size );

/**
 * @internal
 * builds an ASN object containing an unsigned int64.
 *
 * @see asn_build_unsigned_int64
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type    IN - type of object
 * @param cp           IN - pointer to counter struct
 * @param countersize  IN - size of input buffer
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildUnsignedInt64( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    Counter64 const* data, size_t );

/**
 * @internal
 * builds an ASN object containing an signed int64.
 *
 * @see asn_build_signed_int64
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type    IN - type of object
 * @param cp           IN - pointer to counter struct
 * @param countersize  IN - size of input buffer
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildSignedInt64( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    const Counter64* data,
    size_t );

/**
 * @internal
 * builds an ASN object containing an float.
 *
 * @see asn_build_float
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type       IN - type of object
 * @param floatp     IN - pointer to the float
 * @param floatsize  IN - size of input buffer
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildFloat( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type, const float* data,
    size_t data_size );

/**
 * @internal
 * builds an ASN object containing an double.
 *
 * @see asn_build_double
 *
 * @param pkt     IN/OUT address of the begining of the buffer.
 * @param pkt_len IN/OUT address to an integer containing the size of pkt.
 * @param offset  IN/OUT offset to the start of the buffer where to write
 * @param r       IN if not zero reallocate the buffer to fit the
 *                needed size.
 * @param type    IN - type of object
 * @param doublep           IN - pointer to double
 * @param doublesize  IN - size of input buffer
 *
 * @return 1 on success, 0 on error
 */
int Asn01_reallocRbuildDouble( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type, const double* data,
    size_t data_size );

#endif // IOT_ASN_H
