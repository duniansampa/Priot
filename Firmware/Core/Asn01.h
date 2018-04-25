#ifndef ASN01_H
#define ASN01_H

#include "Generals.h"

#define ASN01_PARSE_PACKET 0
#define ASN01_DUMP_PACKET 1

/*
 * Definitions for Abstract Syntax Notation One, ASN.1
 * As defined in ISO/IS 8824 and ISO/IS 8825
 *
 *
 */

#define ASN01_MIN_OID_LEN 2
#define ASN01_MAX_OID_LEN 128 /* max subid's in an oid */
#define ASN01_MAX_NAME_LEN ASN01_MAX_OID_LEN /* obsolete. use ASN01_MAX_OID_LEN */

#define ASN01_OID_LENGTH( x ) ( sizeof( x ) / sizeof( oid ) )

#define ASN01_BOOLEAN ( ( u_char )0x01 )
#define ASN01_INTEGER ( ( u_char )0x02 )
#define ASN01_BIT_STR ( ( u_char )0x03 )
#define ASN01_OCTET_STR ( ( u_char )0x04 )
#define ASN01_NULL ( ( u_char )0x05 )
#define ASN01_OBJECT_ID ( ( u_char )0x06 )
#define ASN01_SEQUENCE ( ( u_char )0x10 )
#define ASN01_SET ( ( u_char )0x11 )

#define ASN01_UNIVERSAL ( ( u_char )0x00 )
#define ASN01_APPLICATION ( ( u_char )0x40 )
#define ASN01_CONTEXT ( ( u_char )0x80 )
#define ASN01_PRIVATE ( ( u_char )0xC0 )

#define ASN01_PRIMITIVE ( ( u_char )0x00 )
#define ASN01_CONSTRUCTOR ( ( u_char )0x20 )

#define ASN01_LONG_LEN ( 0x80 )
#define ASN01_EXTENSION_ID ( 0x1F )
#define ASN01_BIT8 ( 0x80 )

#define ASN01_IS_CONSTRUCTOR( byte ) ( ( byte )&ASN01_CONSTRUCTOR )
#define ASN01_IS_EXTENSION_ID( byte ) ( ( ( byte )&ASN01_EXTENSION_ID ) == ASN01_EXTENSION_ID )

typedef struct Asn01_Counter64_s {
    uint high;
    uint low;
} Asn01_Counter64;

typedef Asn01_Counter64 Asn01_Integer64;
typedef Asn01_Counter64 Asn01_Unsigned64;

/*
 * defined types (from the SMI, RFC 1157)
 */
#define ASN01_IPADDRESS ( ASN01_APPLICATION | 0 )
#define ASN01_COUNTER ( ASN01_APPLICATION | 1 )
#define ASN01_GAUGE ( ASN01_APPLICATION | 2 )
#define ASN01_UNSIGNED ( ASN01_APPLICATION | 2 ) /* RFC 1902 - same as GAUGE */
#define ASN01_TIMETICKS ( ASN01_APPLICATION | 3 )
#define ASN01_OPAQUE ( ASN01_APPLICATION | 4 ) /* changed so no conflict with other includes */

/*
 * defined types (from the SMI, RFC 1442)
 */
#define ASN01_NSAP ( ASN01_APPLICATION | 5 ) /* historic - don't use */
#define ASN01_COUNTER64 ( ASN01_APPLICATION | 6 )
#define ASN01_UINTEGER ( ASN01_APPLICATION | 7 ) /* historic - don't use */

/*
 * defined types from draft-perkins-opaque-01.txt
 */
#define ASN01_FLOAT ( ASN01_APPLICATION | 8 )
#define ASN01_DOUBLE ( ASN01_APPLICATION | 9 )
#define ASN01_INTEGER64 ( ASN01_APPLICATION | 10 )
#define ASN01_UNSIGNED64 ( ASN01_APPLICATION | 11 )

/*
 * The BER inside an OPAQUE is an context specific with a value of 48 (0x30)
 * plus the "normal" tag. For a Counter64, the tag is 0x46 (i.e., an
 * applications specific tag with value 6). So the value for a 64 bit
 * counter is 0x46 + 0x30, or 0x76 (118 base 10). However, values
 * greater than 30 can not be encoded in one octet. So the first octet
 * has the class, in this case context specific (ASN_CONTEXT), and
 * the special value (i.e., 31) to indicate that the real value follows
 * in one or more octets. The high order bit of each following octet
 * indicates if the value is encoded in additional octets. A high order
 * bit of zero, indicates the last. For this "hack", only one octet
 * will be used for the value.
 */

/*
 * first octet of the tag
 */
#define ASN01_OPAQUE_TAG1 ( ASN01_CONTEXT | ASN01_EXTENSION_ID )
/*
 * base value for the second octet of the tag - the
 * second octet was the value for the tag
 */
#define ASN01_OPAQUE_TAG2 ( ( u_char )0x30 )

#define ASN01_OPAQUE_TAG2U ( ( u_char )0x2f ) /* second octet of tag for union */

/*
 * All the ASN.1 types for SNMP "should have been" defined in this file,
 * but they were not. (They are defined in snmp_impl.h)  Thus, the tag for
 * Opaque and Counter64 is defined, again, here with a different names.
 */
#define ASN01_APP_OPAQUE ( ASN01_APPLICATION | 4 )
#define ASN01_APP_COUNTER64 ( ASN01_APPLICATION | 6 )
#define ASN01_APP_FLOAT ( ASN01_APPLICATION | 8 )
#define ASN01_APP_DOUBLE ( ASN01_APPLICATION | 9 )
#define ASN01_APP_I64 ( ASN01_APPLICATION | 10 )
#define ASN01_APP_U64 ( ASN01_APPLICATION | 11 )
#define ASN01_APP_UNION ( ASN01_PRIVATE | 1 ) /* or ASN_PRIV_UNION ? */

/*
 * value for Counter64
 */
#define ASN01_OPAQUE_COUNTER64 ( ASN01_OPAQUE_TAG2 + ASN01_APP_COUNTER64 )
/*
 * max size of BER encoding of Counter64
 */
#define ASN01_OPAQUE_COUNTER64_MX_BER_LEN 12

/*
 * value for Float
 */
#define ASN01_OPAQUE_FLOAT ( ASN01_OPAQUE_TAG2 + ASN01_APP_FLOAT )
/*
 * size of BER encoding of Float
 */
#define ASN01_OPAQUE_FLOAT_BER_LEN 7

/*
 * value for Double
 */
#define ASN01_OPAQUE_DOUBLE ( ASN01_OPAQUE_TAG2 + ASN01_APP_DOUBLE )
/*
 * size of BER encoding of Double
 */
#define ASN01_OPAQUE_DOUBLE_BER_LEN 11

/*
 * value for Integer64
 */
#define ASN01_OPAQUE_I64 ( ASN01_OPAQUE_TAG2 + ASN01_APP_I64 )
/*
 * max size of BER encoding of Integer64
 */
#define ASN01_OPAQUE_I64_MX_BER_LEN 11

/*
 * value for Unsigned64
 */
#define ASN01_OPAQUE_U64 ( ASN01_OPAQUE_TAG2 + ASN01_APP_U64 )
/*
 * max size of BER encoding of Unsigned64
 */
#define ASN01_OPAQUE_U64_MX_BER_LEN 12

#define ASN01_PRIV_INCL_RANGE ( ASN01_PRIVATE | 2 )
#define ASN01_PRIV_EXCL_RANGE ( ASN01_PRIVATE | 3 )
#define ASN01_PRIV_DELEGATED ( ASN01_PRIVATE | 5 )
#define ASN01_PRIV_IMPLIED_OCTET_STR ( ASN01_PRIVATE | ASN01_OCTET_STR ) /* 4 */
#define ASN01_PRIV_IMPLIED_OBJECT_ID ( ASN01_PRIVATE | ASN01_OBJECT_ID ) /* 6 */
#define ASN01_PRIV_RETRY ( ASN01_PRIVATE | 7 ) /* 199 */
#define ASN01_IS_DELEGATED( x ) ( ( x ) == ASN01_PRIV_DELEGATED )

int Asn01_checkPacket( u_char*, size_t );

u_char* Asn01_parseInt( u_char*, size_t*, u_char*, long*, size_t );

u_char* Asn01_buildInt( u_char*, size_t*, u_char, const long*, size_t );

u_char* Asn01_parseUnsignedInt( u_char*, size_t*, u_char*, u_long*, size_t );

u_char* Asn01_buildUnsignedInt( u_char*, size_t*, u_char, const u_long*, size_t );

u_char* Asn01_parseString( u_char*, size_t*, u_char*, u_char*, size_t* );

u_char* Asn01_buildString( u_char*, size_t*, u_char, const u_char*, size_t );

u_char* Asn01_parseHeader( u_char*, size_t*, u_char* );

u_char* Asn01_parseSequence( u_char*, size_t*, u_char*, u_char expected_type, /* must be this type */
    const char* estr ); /* error message prefix */

u_char* Asn01_buildHeader( u_char*, size_t*, u_char, size_t );

u_char* Asn01_buildSequence( u_char*, size_t*, u_char, size_t );

u_char* Asn01_parseLength( u_char*, u_long* );

u_char* Asn01_buildLength( u_char*, size_t*, size_t );

u_char* Asn01_parseObjid( u_char*, size_t*, u_char*, oid*, size_t* );

u_char* Asn01_buildObjid( u_char*, size_t*, u_char, oid*, size_t );

u_char* Asn01_parseNull( u_char*, size_t*, u_char* );

u_char* Asn01_buildNull( u_char*, size_t*, u_char );

u_char* Asn01_parseBitstring( u_char*, size_t*, u_char*, u_char*, size_t* );

u_char* Asn01_buildBitstring( u_char*, size_t*, u_char, const u_char*, size_t );

u_char* Asn01_parseUnsignedInt64( u_char*, size_t*, u_char*, struct Asn01_Counter64_s*, size_t );

u_char* Asn01_buildUnsignedInt64( u_char*, size_t*, u_char,
    const struct Asn01_Counter64_s*, size_t );

u_char* Asn01_parseSignedInt64( u_char*, size_t*, u_char*, struct Asn01_Counter64_s*, size_t );

u_char* Asn01_buildSignedInt64( u_char*, size_t*, u_char, const struct Asn01_Counter64_s*, size_t );

u_char* Asn01_buildFloat( u_char*, size_t*, u_char, const float*, size_t );

u_char* Asn01_parseFloat( u_char*, size_t*, u_char*, float*, size_t );

u_char* Asn01_buildDouble( u_char*, size_t*, u_char, const double*, size_t );

u_char* Asn01_parseDouble( u_char*, size_t*, u_char*, double*, size_t );

/*
 * Re-allocator function for below.
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

int Asn01_reallocRbuildInt( u_char** pkt, size_t* pkt_len,
    size_t* offset,
    int allow_realloc, u_char type,
    const long* data, size_t data_size );

int Asn01_reallocRbuildString( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    const u_char* data,
    size_t data_size );

int Asn01_reallocRbuildUnsignedInt( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    const u_long* data,
    size_t data_size );

int Asn01_reallocRbuildHeader( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    size_t data_size );

int Asn01_reallocRbuildSequence( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    size_t data_size );

int Asn01_reallocRbuildLength( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    size_t data_size );

int Asn01_reallocRbuildObjid( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type, const oid*,
    size_t );

int Asn01_reallocRbuildNull( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type );

int Asn01_reallocRbuildBitstring( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    const u_char* data,
    size_t data_size );

int Asn01_reallocRbuildUnsignedInt64( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    struct Asn01_Counter64_s const* data, size_t );

int Asn01_reallocRbuildSignedInt64( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type,
    const struct Asn01_Counter64_s* data,
    size_t );

int Asn01_reallocRbuildFloat( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type, const float* data,
    size_t data_size );

int Asn01_reallocRbuildDouble( u_char** pkt,
    size_t* pkt_len,
    size_t* offset,
    int allow_realloc,
    u_char type, const double* data,
    size_t data_size );
#endif // ASN01_H
