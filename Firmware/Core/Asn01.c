#include "Asn01.h"

#include "Impl.h"
#include "Mib.h"
#include "System/Numerics/Integer64.h"
#include "System/Util/Trace.h"
#include "System/Util/Utilities.h"

/**
 * @defgroup asn1_packet_parse asn1 parsing and datatype manipulation routines.
 * @ingroup library
 *
 * @{
 *
 * Note on
 *
 * Re-allocating reverse ASN.1 encoder functions.  Synopsis:
 *
 * \code
 *
 * u_char *buf = (u_char*)malloc(100);
 * u_char type = (ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER);
 * size_t buf_len = 100, offset = 0;
 * long data = 12345;
 * int allow_realloc = 1;
 *
 * if (asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
 *                            type, &data, sizeof(long)) == 0) {
 *     error;
 * }
 *
 * \endcode
 *
 * NOTE WELL: after calling one of these functions with allow_realloc
 * non-zero, buf might have moved, buf_len might have grown and
 * offset will have increased by the size of the encoded data.
 * You should **NEVER** do something like this:
 *
 * \code
 *
 * u_char *buf = (u_char *)malloc(100), *ptr;
 * u_char type = (ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER);
 * size_t buf_len = 100, offset = 0;
 * long data1 = 1234, data2 = 5678;
 * int rc = 0, allow_realloc = 1;
 *
 * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
 *                                type, &data1, sizeof(long));
 * ptr = buf[buf_len - offset];   / * points at encoding of data1 * /
 * if (rc == 0) {
 *      error;
 * }
 * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
 *                              type, &data2, sizeof(long));
 * make use of ptr here;
 *
 * \endcode
 *
 * ptr is **INVALID** at this point.  In general, you should store the
 * offset value and compute pointers when you need them:
 *
 *
 * \code
 *
 * u_char *buf = (u_char *)malloc(100), *ptr;
 * u_char type = (ASN_UNIVERSAL | ASN_PRIMITIVE | ASN_INTEGER);
 * size_t buf_len = 100, offset = 0, ptr_offset;
 * long data1 = 1234, data2 = 5678;
 * int rc = 0, allow_realloc = 1;
 *
 * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
 *                              type, &data1, sizeof(long));
 * ptr_offset = offset;
 * if (rc == 0) {
 *      error;
 * }
 * rc  = asn_realloc_rbuild_int(&buf, &buf_len, &offset, allow_realloc,
 *                              type, &data2, sizeof(long));
 * ptr = buf + buf_len - ptr_offset
 * make use of ptr here;
 *
 * \endcode
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
 * \code
 *
 * u_char my_static_buffer[100], *cp = NULL;
 * size_t my_static_buffer_len = 100;
 * float my_pi = (float)22/(float)7;
 *
 * cp = asn_rbuild_float(my_static_buffer, &my_static_buffer_len,
 *                       ASN_OPAQUE_FLOAT, &my_pi, sizeof(float));
 * if (cp == NULL) {
 * error;
 * }
 *
 * \endcode
 *
 * IS EQUIVALENT TO:
 *
 * \code
 *
 * u_char my_static_buffer[100];
 * size_t my_static_buffer_len = 100, my_offset = 0;
 * float my_pi = (float)22/(float)7;
 * int rc = 0;
 *
 * rc = asn_realloc_rbuild_float(&my_static_buffer, &my_static_buffer_len,
 *                               &my_offset, 0,
 *                               ASN_OPAQUE_FLOAT, &my_pi, sizeof(float));
 * if (rc == 0) {
 *   error;
 * }
 * \endcode
 *
 */

#include <netinet/in.h>

#define CHECK_OVERFLOW_S( x, y )                                                                       \
    do {                                                                                               \
        if ( x > INT32_MAX ) {                                                                         \
            DEBUG_MSG( ( "asn", "truncating signed value %ld to 32 bits (%d)\n", ( long )( x ), y ) ); \
            x &= 0xffffffff;                                                                           \
        } else if ( x < INT32_MIN ) {                                                                  \
            DEBUG_MSG( ( "asn", "truncating signed value %ld to 32 bits (%d)\n", ( long )( x ), y ) ); \
            x = 0 - ( x & 0xffffffff );                                                                \
        }                                                                                              \
    } while ( 0 )

#define CHECK_OVERFLOW_U( x, y )                                                      \
    do {                                                                              \
        if ( x > UINT32_MAX ) {                                                       \
            x &= 0xffffffff;                                                          \
            DEBUG_MSG( ( "asn", "truncating unsigned value to 32 bits (%d)\n", y ) ); \
        }                                                                             \
    } while ( 0 )

/**
 * @internal
 * output an error for a wrong size
 *
 * @param str        error string
 * @param wrongsize  wrong size
 * @param rightsize  expected size
 */
static void _Asn01_sizeErr( const char* str, size_t wrongsize, size_t rightsize )
{
    char ebuf[ 128 ];

    snprintf( ebuf, sizeof( ebuf ),
        "%s size %lu: s/b %lu", str,
        ( unsigned long )wrongsize, ( unsigned long )rightsize );
    ebuf[ sizeof( ebuf ) - 1 ] = 0;
    IMPL_ERROR_MSG( ebuf );
}

/**
 * @internal
 * output an error for a wrong type
 *
 * @param str        error string
 * @param wrongtype  wrong type
 */
static void _Asn01_typeErr( const char* str, int wrongtype )
{
    char ebuf[ 128 ];

    snprintf( ebuf, sizeof( ebuf ), "%s type %d", str, wrongtype );
    ebuf[ sizeof( ebuf ) - 1 ] = 0;
    IMPL_ERROR_MSG( ebuf );
}

/**
 * @internal
 * output an error for a wrong length
 *
 * @param str        error string
 * @param wrongsize  wrong  length
 * @param rightsize  expected length
 */
static void _Asn01_lengthErr( const char* str, size_t wrongsize, size_t rightsize )
{
    char ebuf[ 128 ];

    snprintf( ebuf, sizeof( ebuf ),
        "%s length %lu too large: exceeds %lu", str,
        ( unsigned long )wrongsize, ( unsigned long )rightsize );
    ebuf[ sizeof( ebuf ) - 1 ] = 0;
    IMPL_ERROR_MSG( ebuf );
}

/**
 * @internal
 * call after Asn01_parseLength to verify result.
 *
 * @param str  error string
 * @param bufp start of buffer
 * @param data start of data
 * @param plen  ?
 * @param dlen  ?
 *
 * @return 1 on error 0 on success
 */
static int _Asn01_parseLengthCheck( const char* str,
    const u_char* bufp, const u_char* data,
    u_long plen, size_t dlen )
{
    char ebuf[ 128 ];
    size_t header_len;

    if ( bufp == NULL ) {
        /*
         * error message is set
         */
        return 1;
    }
    header_len = bufp - data;
    if ( plen > 0x7fffffff || header_len > 0x7fffffff || ( ( size_t )plen + header_len ) > dlen ) {
        snprintf( ebuf, sizeof( ebuf ),
            "%s: message overflow: %d len + %d delta > %d len",
            str, ( int )plen, ( int )header_len, ( int )dlen );
        ebuf[ sizeof( ebuf ) - 1 ] = 0;
        IMPL_ERROR_MSG( ebuf );
        return 1;
    }
    return 0;
}

/**
 * @internal
 * call after asn_build_header to verify result.
 *
 * @param str     error string to output
 * @param data    data pointer to verify (NULL => error )
 * @param datalen  data len to check
 * @param typedlen  type length
 *
 * @return 0 on success, 1 on error
 */
static int _Asn01_buildHeaderCheck( const char* str, const u_char* data,
    size_t datalen, size_t typedlen )
{
    char ebuf[ 128 ];

    if ( data == NULL ) {
        /*
         * error message is set
         */
        return 1;
    }
    if ( datalen < typedlen ) {
        snprintf( ebuf, sizeof( ebuf ),
            "%s: bad header, length too short: %lu < %lu", str,
            ( unsigned long )datalen, ( unsigned long )typedlen );
        ebuf[ sizeof( ebuf ) - 1 ] = 0;
        IMPL_ERROR_MSG( ebuf );
        return 1;
    }
    return 0;
}

/**
 * @internal
 * call after asn_build_header to verify result.
 *
 * @param str       error string
 * @param pkt       packet to check
 * @param pkt_len  length of the packet
 * @param typedlen length of the type
 *
 * @return 0 on success 1 on error
 */
static int _Asn01_reallocBuildHeaderCheck( const char* str,
    u_char** pkt,
    const size_t* pkt_len, size_t typedlen )
{
    char ebuf[ 128 ];

    if ( pkt == NULL || *pkt == NULL ) {
        /*
         * Error message is set.
         */
        return 1;
    }

    if ( *pkt_len < typedlen ) {
        snprintf( ebuf, sizeof( ebuf ),
            "%s: bad header, length too short: %lu < %lu", str,
            ( unsigned long )*pkt_len, ( unsigned long )typedlen );
        ebuf[ sizeof( ebuf ) - 1 ] = 0;
        IMPL_ERROR_MSG( ebuf );
        return 1;
    }
    return 0;
}

/**
 * @internal
 * checks the incoming packet for validity and returns its size or 0
 *
 * @param pkt The packet
 * @param len The length to check
 *
 * @return The size of the packet if valid; 0 otherwise
 */
int Asn01_checkPacket( u_char* pkt, size_t len )
{
    u_long asn_length;

    if ( len < 2 )
        return 0; /* always too short */

    if ( *pkt != ( u_char )( ASN01_SEQUENCE | ASN01_CONSTRUCTOR ) )
        return -1; /* wrong type */

    if ( *( pkt + 1 ) & 0x80 ) {
        /*
         * long length
         */
        if ( ( int )len < ( int )( *( pkt + 1 ) & ~0x80 ) + 2 )
            return 0; /* still to short, incomplete length */
        Asn01_parseLength( pkt + 1, &asn_length );
        return ( asn_length + 2 + ( *( pkt + 1 ) & ~0x80 ) );
    } else {
        /*
         * short length
         */
        return ( *( pkt + 1 ) + 2 );
    }
}

static int _Asn01_bitstringCheck( const char* str, size_t asn_length, u_char datum )
{
    char ebuf[ 128 ];

    if ( asn_length < 1 ) {
        snprintf( ebuf, sizeof( ebuf ),
            "%s: length %d too small", str, ( int )asn_length );
        ebuf[ sizeof( ebuf ) - 1 ] = 0;
        IMPL_ERROR_MSG( ebuf );
        return 1;
    }
    /*
     * if (datum > 7){
     * sprintf(ebuf,"%s: datum %d >7: too large", str, (int)(datum));
     * IMPL_ERROR_MSG(ebuf);
     * return 1;
     * }
     */
    return 0;
}

/**
 * @internal
 * asn_parse_int - pulls a long out of an int type.
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
u_char* Asn01_parseInt( u_char* data,
    size_t* datalength,
    u_char* type, long* intp, size_t intsize )
{
    /*
     * ASN.1 integer ::= 0x02 asnlength byte {byte}*
     */
    static const char* errpre = "parse int";
    register u_char* bufp = data;
    u_long asn_length;
    register long value = 0;

    if ( intsize != sizeof( long ) ) {
        _Asn01_sizeErr( errpre, intsize, sizeof( long ) );
        return NULL;
    }
    *type = *bufp++;
    if ( *type != ASN01_INTEGER ) {
        _Asn01_typeErr( errpre, *type );
        return NULL;
    }

    bufp = Asn01_parseLength( bufp, &asn_length );
    if ( _Asn01_parseLengthCheck( errpre, bufp, data, asn_length, *datalength ) )
        return NULL;

    if ( ( size_t )asn_length > intsize || ( int )asn_length == 0 ) {
        _Asn01_lengthErr( errpre, ( size_t )asn_length, intsize );
        return NULL;
    }

    *datalength -= ( int )asn_length + ( bufp - data );
    if ( *bufp & 0x80 )
        value = -1; /* integer is negative */

    DEBUG_DUMPSETUP( "recv", data, bufp - data + asn_length );

    while ( asn_length-- )
        value = ( value << 8 ) | *bufp++;

    CHECK_OVERFLOW_S( value, 1 );

    DEBUG_MSG( ( "dumpvRecv", "  Integer:\t%ld (0x%.2lX)\n", value, value ) );

    *intp = value;
    return bufp;
}

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
u_char* Asn01_parseUnsignedInt( u_char* data,
    size_t* datalength,
    u_char* type, u_long* intp, size_t intsize )
{
    /*
     * ASN.1 integer ::= 0x02 asnlength byte {byte}*
     */
    static const char* errpre = "parse uint";
    register u_char* bufp = data;
    u_long asn_length;
    register u_long value = 0;

    if ( intsize != sizeof( long ) ) {
        _Asn01_sizeErr( errpre, intsize, sizeof( long ) );
        return NULL;
    }
    *type = *bufp++;
    if ( *type != ASN01_COUNTER && *type != ASN01_GAUGE && *type != ASN01_TIMETICKS
        && *type != ASN01_UINTEGER ) {
        _Asn01_typeErr( errpre, *type );
        return NULL;
    }
    bufp = Asn01_parseLength( bufp, &asn_length );
    if ( _Asn01_parseLengthCheck( errpre, bufp, data, asn_length, *datalength ) )
        return NULL;

    if ( ( asn_length > ( intsize + 1 ) ) || ( ( int )asn_length == 0 ) || ( ( asn_length == intsize + 1 ) && *bufp != 0x00 ) ) {
        _Asn01_lengthErr( errpre, ( size_t )asn_length, intsize );
        return NULL;
    }
    *datalength -= ( int )asn_length + ( bufp - data );
    if ( *bufp & 0x80 )
        value = ~value; /* integer is negative */

    DEBUG_DUMPSETUP( "recv", data, bufp - data + asn_length );

    while ( asn_length-- )
        value = ( value << 8 ) | *bufp++;

    CHECK_OVERFLOW_U( value, 2 );

    DEBUG_MSG( ( "dumpvRecv", "  UInteger:\t%ld (0x%.2lX)\n", value, value ) );

    *intp = value;
    return bufp;
}

/**
 * @internal
 * asn_build_int - builds an ASN object containing an integer.
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
u_char* Asn01_buildInt( u_char* data,
    size_t* datalength, u_char type, const long* intp, size_t intsize )
{
    /*
     * ASN.1 integer ::= 0x02 asnlength byte {byte}*
     */
    static const char* errpre = "build int";
    register long integer;
    register u_long mask;
    u_char* initdatap = data;

    if ( intsize != sizeof( long ) ) {
        _Asn01_sizeErr( errpre, intsize, sizeof( long ) );
        return NULL;
    }
    integer = *intp;
    CHECK_OVERFLOW_S( integer, 3 );
    /*
     * Truncate "unnecessary" bytes off of the most significant end of this
     * 2's complement integer.  There should be no sequence of 9
     * consecutive 1's or 0's at the most significant end of the
     * integer.
     */
    mask = ( ( u_long )0x1FF ) << ( ( 8 * ( sizeof( long ) - 1 ) ) - 1 );
    /*
     * mask is 0xFF800000 on a big-endian machine
     */
    while ( ( ( ( integer & mask ) == 0 ) || ( ( integer & mask ) == mask ) )
        && intsize > 1 ) {
        intsize--;
        integer <<= 8;
    }
    data = Asn01_buildHeader( data, datalength, type, intsize );
    if ( _Asn01_buildHeaderCheck( errpre, data, *datalength, intsize ) )
        return NULL;

    *datalength -= intsize;
    mask = ( ( u_long )0xFF ) << ( 8 * ( sizeof( long ) - 1 ) );
    /*
     * mask is 0xFF000000 on a big-endian machine
     */
    while ( intsize-- ) {
        *data++ = ( u_char )( ( integer & mask ) >> ( 8 * ( sizeof( long ) - 1 ) ) );
        integer <<= 8;
    }
    DEBUG_DUMPSETUP( "send", initdatap, data - initdatap );
    DEBUG_MSG( ( "dumpvSend", "  Integer:\t%ld (0x%.2lX)\n", *intp, *intp ) );
    return data;
}

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
u_char* Asn01_buildUnsignedInt( u_char* data,
    size_t* datalength,
    u_char type, const u_long* intp, size_t intsize )
{
    /*
     * ASN.1 integer ::= 0x02 asnlength byte {byte}*
     */
    static const char* errpre = "build uint";
    register u_long integer;
    register u_long mask;
    int add_null_byte = 0;
    u_char* initdatap = data;

    if ( intsize != sizeof( long ) ) {
        _Asn01_sizeErr( errpre, intsize, sizeof( long ) );
        return NULL;
    }
    integer = *intp;
    CHECK_OVERFLOW_U( integer, 4 );

    mask = ( ( u_long )0xFF ) << ( 8 * ( sizeof( long ) - 1 ) );
    /*
     * mask is 0xFF000000 on a big-endian machine
     */
    if ( ( u_char )( ( integer & mask ) >> ( 8 * ( sizeof( long ) - 1 ) ) ) & 0x80 ) {
        /*
         * if MSB is set
         */
        add_null_byte = 1;
        intsize++;
    } else {
        /*
         * Truncate "unnecessary" bytes off of the most significant end of this 2's complement integer.
         * There should be no sequence of 9 consecutive 1's or 0's at the most significant end of the
         * integer.
         */
        mask = ( ( u_long )0x1FF ) << ( ( 8 * ( sizeof( long ) - 1 ) ) - 1 );
        /*
         * mask is 0xFF800000 on a big-endian machine
         */
        while ( ( ( ( integer & mask ) == 0 ) || ( ( integer & mask ) == mask ) )
            && intsize > 1 ) {
            intsize--;
            integer <<= 8;
        }
    }
    data = Asn01_buildHeader( data, datalength, type, intsize );
    if ( _Asn01_buildHeaderCheck( errpre, data, *datalength, intsize ) )
        return NULL;

    *datalength -= intsize;
    if ( add_null_byte == 1 ) {
        *data++ = '\0';
        intsize--;
    }
    mask = ( ( u_long )0xFF ) << ( 8 * ( sizeof( long ) - 1 ) );
    /*
     * mask is 0xFF000000 on a big-endian machine
     */
    while ( intsize-- ) {
        *data++ = ( u_char )( ( integer & mask ) >> ( 8 * ( sizeof( long ) - 1 ) ) );
        integer <<= 8;
    }
    DEBUG_DUMPSETUP( "send", initdatap, data - initdatap );
    DEBUG_MSG( ( "dumpvSend", "  UInteger:\t%ld (0x%.2lX)\n", *intp, *intp ) );
    return data;
}

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

u_char* Asn01_parseString( u_char* data,
    size_t* datalength,
    u_char* type, u_char* str, size_t* strlength )
{
    static const char* errpre = "parse string";
    u_char* bufp = data;
    u_long asn_length;

    *type = *bufp++;
    if ( *type != ASN01_OCTET_STR && *type != ASN01_IPADDRESS && *type != ASN01_OPAQUE
        && *type != ASN01_NSAP ) {
        _Asn01_typeErr( errpre, *type );
        return NULL;
    }

    bufp = Asn01_parseLength( bufp, &asn_length );
    if ( _Asn01_parseLengthCheck( errpre, bufp, data, asn_length, *datalength ) ) {
        return NULL;
    }

    if ( asn_length > *strlength ) {
        _Asn01_lengthErr( errpre, ( size_t )asn_length, *strlength );
        return NULL;
    }

    DEBUG_DUMPSETUP( "recv", data, bufp - data + asn_length );

    memmove( str, bufp, asn_length );
    if ( *strlength > asn_length )
        str[ asn_length ] = 0;
    *strlength = asn_length;
    *datalength -= asn_length + ( bufp - data );

    DEBUG_IF( "dumpvRecv" )
    {
        u_char* buf = ( u_char* )malloc( 1 + asn_length );
        size_t l = ( buf != NULL ) ? ( 1 + asn_length ) : 0, ol = 0;

        if ( Mib_sprintReallocAsciiString( &buf, &l, &ol, 1, str, asn_length ) ) {
            DEBUG_MSG( ( "dumpvRecv", "  String:\t%s\n", buf ) );
        } else {
            if ( buf == NULL ) {
                DEBUG_MSG( ( "dumpvRecv", "  String:\t[TRUNCATED]\n" ) );
            } else {
                DEBUG_MSG( ( "dumpvRecv", "  String:\t%s [TRUNCATED]\n",
                    buf ) );
            }
        }
        if ( buf != NULL ) {
            free( buf );
        }
    }

    return bufp + asn_length;
}

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

u_char* Asn01_buildString( u_char* data,
    size_t* datalength,
    u_char type, const u_char* str, size_t strlength )
{
    /*
     * ASN.1 octet string ::= primstring | cmpdstring
     * primstring ::= 0x04 asnlength byte {byte}*
     * cmpdstring ::= 0x24 asnlength string {string}*
     * This code will never send a compound string.
     */
    u_char* initdatap = data;
    data = Asn01_buildHeader( data, datalength, type, strlength );
    if ( _Asn01_buildHeaderCheck( "build string", data, *datalength, strlength ) )
        return NULL;

    if ( strlength ) {
        if ( str == NULL ) {
            memset( data, 0, strlength );
        } else {
            memmove( data, str, strlength );
        }
    }
    *datalength -= strlength;
    DEBUG_DUMPSETUP( "send", initdatap, data - initdatap + strlength );
    DEBUG_IF( "dumpvSend" )
    {
        u_char* buf = ( u_char* )malloc( 1 + strlength );
        size_t l = ( buf != NULL ) ? ( 1 + strlength ) : 0, ol = 0;

        if ( Mib_sprintReallocAsciiString( &buf, &l, &ol, 1, str, strlength ) ) {
            DEBUG_MSG( ( "dumpvSend", "  String:\t%s\n", buf ) );
        } else {
            if ( buf == NULL ) {
                DEBUG_MSG( ( "dumpvSend", "  String:\t[TRUNCATED]\n" ) );
            } else {
                DEBUG_MSG( ( "dumpvSend", "  String:\t%s [TRUNCATED]\n",
                    buf ) );
            }
        }
        if ( buf != NULL ) {
            free( buf );
        }
    }
    return data + strlength;
}

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
u_char* Asn01_parseHeader( u_char* data, size_t* datalength, u_char* type )
{
    register u_char* bufp;
    u_long asn_length;

    if ( !data || !datalength || !type ) {
        IMPL_ERROR_MSG( "parse header: NULL pointer" );
        return NULL;
    }
    bufp = data;
    /*
     * this only works on data types < 30, i.e. no extension octets
     */
    if ( ASN01_IS_EXTENSION_ID( *bufp ) ) {
        IMPL_ERROR_MSG( "can't process ID >= 30" );
        return NULL;
    }
    *type = *bufp;
    bufp = Asn01_parseLength( bufp + 1, &asn_length );

    if ( _Asn01_parseLengthCheck( "parse header", bufp, data, asn_length, *datalength ) )
        return NULL;

    if ( ( *type == ASN01_OPAQUE ) && ( *bufp == ASN01_OPAQUE_TAG1 ) ) {

        /*
         * check if 64-but counter
         */
        switch ( *( bufp + 1 ) ) {
        case ASN01_OPAQUE_COUNTER64:
        case ASN01_OPAQUE_U64:
        case ASN01_OPAQUE_FLOAT:
        case ASN01_OPAQUE_DOUBLE:
        case ASN01_OPAQUE_I64:
            *type = *( bufp + 1 );
            break;

        default:
            /*
             * just an Opaque
             */
            *datalength = ( int )asn_length;
            return bufp;
        }
        /*
         * value is encoded as special format
         */
        bufp = Asn01_parseLength( bufp + 2, &asn_length );
        if ( _Asn01_parseLengthCheck( "parse opaque header", bufp, data,
                 asn_length, *datalength ) )
            return NULL;
    }

    *datalength = ( int )asn_length;

    return bufp;
}

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
u_char* Asn01_parseSequence( u_char* data, size_t* datalength, u_char* type, u_char expected_type, /* must be this type */
    const char* estr )
{ /* error message prefix */
    data = Asn01_parseHeader( data, datalength, type );
    if ( data && ( *type != expected_type ) ) {
        char ebuf[ 128 ];
        snprintf( ebuf, sizeof( ebuf ),
            "%s header type %02X: s/b %02X", estr,
            ( u_char )*type, ( u_char )expected_type );
        ebuf[ sizeof( ebuf ) - 1 ] = 0;
        IMPL_ERROR_MSG( ebuf );
        return NULL;
    }
    return data;
}

/**
 * @internal
 * asn_build_header - builds an ASN header for an object with the ID and
 * length specified.
 *
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
 * @return Returns a pointer to the first byte of the contents of this object.
 *          Returns NULL on any error.
 */
u_char* Asn01_buildHeader( u_char* data,
    size_t* datalength, u_char type, size_t length )
{
    char ebuf[ 128 ];

    if ( *datalength < 1 ) {
        snprintf( ebuf, sizeof( ebuf ),
            "bad header length < 1 :%lu, %lu",
            ( unsigned long )*datalength, ( unsigned long )length );
        ebuf[ sizeof( ebuf ) - 1 ] = 0;
        IMPL_ERROR_MSG( ebuf );
        return NULL;
    }
    *data++ = type;
    ( *datalength )--;
    return Asn01_buildLength( data, datalength, length );
}

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
u_char* Asn01_buildSequence( u_char* data,
    size_t* datalength, u_char type, size_t length )
{
    static const char* errpre = "build seq";
    char ebuf[ 128 ];

    if ( *datalength < 4 ) {
        snprintf( ebuf, sizeof( ebuf ),
            "%s: length %d < 4: PUNT", errpre,
            ( int )*datalength );
        ebuf[ sizeof( ebuf ) - 1 ] = 0;
        IMPL_ERROR_MSG( ebuf );
        return NULL;
    }
    *datalength -= 4;
    *data++ = type;
    *data++ = ( u_char )( 0x02 | ASN01_LONG_LEN );
    *data++ = ( u_char )( ( length >> 8 ) & 0xFF );
    *data++ = ( u_char )( length & 0xFF );
    return data;
}

/**
 * @internal
 * Asn01_parseLength - interprets the length of the current object.
 *
 *  On exit, length contains the value of this length field.
 *
 *  Returns a pointer to the first byte after this length
 *  field (aka: the start of the data field).
 *  Returns NULL on any error.
 *
 * @param data         IN - pointer to start of length field
 * @param length       OUT - value of length field
 *
 *  @return Returns a pointer to the first byte after this length
 *          field (aka: the start of the data field).
 *          Returns NULL on any error.
 */
u_char* Asn01_parseLength( u_char* data, u_long* length )
{
    static const char* errpre = "parse length";
    char ebuf[ 128 ];
    register u_char lengthbyte;

    if ( !data || !length ) {
        IMPL_ERROR_MSG( "parse length: NULL pointer" );
        return NULL;
    }
    lengthbyte = *data;

    if ( lengthbyte & ASN01_LONG_LEN ) {
        lengthbyte &= ~ASN01_LONG_LEN; /* turn MSb off */
        if ( lengthbyte == 0 ) {
            snprintf( ebuf, sizeof( ebuf ),
                "%s: indefinite length not supported", errpre );
            ebuf[ sizeof( ebuf ) - 1 ] = 0;
            IMPL_ERROR_MSG( ebuf );
            return NULL;
        }
        if ( lengthbyte > sizeof( long ) ) {
            snprintf( ebuf, sizeof( ebuf ),
                "%s: data length %d > %lu not supported", errpre,
                lengthbyte, ( unsigned long )sizeof( long ) );
            ebuf[ sizeof( ebuf ) - 1 ] = 0;
            IMPL_ERROR_MSG( ebuf );
            return NULL;
        }
        data++;
        *length = 0; /* protect against short lengths */
        while ( lengthbyte-- ) {
            *length <<= 8;
            *length |= *data++;
        }
        if ( ( long )*length < 0 ) {
            snprintf( ebuf, sizeof( ebuf ),
                "%s: negative data length %ld\n", errpre,
                ( long )*length );
            ebuf[ sizeof( ebuf ) - 1 ] = 0;
            IMPL_ERROR_MSG( ebuf );
            return NULL;
        }
        return data;
    } else { /* short asnlength */
        *length = ( long )lengthbyte;
        return data + 1;
    }
}

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
u_char* Asn01_buildLength( u_char* data, size_t* datalength, size_t length )
{
    static const char* errpre = "build length";
    char ebuf[ 128 ];

    u_char* start_data = data;

    /*
     * no indefinite lengths sent
     */
    if ( length < 0x80 ) {
        if ( *datalength < 1 ) {
            snprintf( ebuf, sizeof( ebuf ),
                "%s: bad length < 1 :%lu, %lu", errpre,
                ( unsigned long )*datalength, ( unsigned long )length );
            ebuf[ sizeof( ebuf ) - 1 ] = 0;
            IMPL_ERROR_MSG( ebuf );
            return NULL;
        }
        *data++ = ( u_char )length;
    } else if ( length <= 0xFF ) {
        if ( *datalength < 2 ) {
            snprintf( ebuf, sizeof( ebuf ),
                "%s: bad length < 2 :%lu, %lu", errpre,
                ( unsigned long )*datalength, ( unsigned long )length );
            ebuf[ sizeof( ebuf ) - 1 ] = 0;
            IMPL_ERROR_MSG( ebuf );
            return NULL;
        }
        *data++ = ( u_char )( 0x01 | ASN01_LONG_LEN );
        *data++ = ( u_char )length;
    } else { /* 0xFF < length <= 0xFFFF */
        if ( *datalength < 3 ) {
            snprintf( ebuf, sizeof( ebuf ),
                "%s: bad length < 3 :%lu, %lu", errpre,
                ( unsigned long )*datalength, ( unsigned long )length );
            ebuf[ sizeof( ebuf ) - 1 ] = 0;
            IMPL_ERROR_MSG( ebuf );
            return NULL;
        }
        *data++ = ( u_char )( 0x02 | ASN01_LONG_LEN );
        *data++ = ( u_char )( ( length >> 8 ) & 0xFF );
        *data++ = ( u_char )( length & 0xFF );
    }
    *datalength -= ( data - start_data );
    return data;
}

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
u_char* Asn01_parseObjid( u_char* data,
    size_t* datalength,
    u_char* type, oid* objid, size_t* objidlength )
{
    static const char* errpre = "parse objid";
    /*
     * ASN.1 objid ::= 0x06 asnlength subidentifier {subidentifier}*
     * subidentifier ::= {leadingbyte}* lastbyte
     * leadingbyte ::= 1 7bitvalue
     * lastbyte ::= 0 7bitvalue
     */
    register u_char* bufp = data;
    register oid* oidp = objid + 1;
    register u_long subidentifier;
    register long length;
    u_long asn_length;
    size_t original_length = *objidlength;

    *type = *bufp++;
    if ( *type != ASN01_OBJECT_ID ) {
        _Asn01_typeErr( errpre, *type );
        return NULL;
    }
    bufp = Asn01_parseLength( bufp, &asn_length );
    if ( _Asn01_parseLengthCheck( "parse objid", bufp, data,
             asn_length, *datalength ) )
        return NULL;

    *datalength -= ( int )asn_length + ( bufp - data );

    DEBUG_DUMPSETUP( "recv", data, bufp - data + asn_length );

    /*
     * Handle invalid object identifier encodings of the form 06 00 robustly
     */
    if ( asn_length == 0 )
        objid[ 0 ] = objid[ 1 ] = 0;

    length = asn_length;
    ( *objidlength )--; /* account for expansion of first byte */

    while ( length > 0 && ( *objidlength )-- > 0 ) {
        subidentifier = 0;
        do { /* shift and add in low order 7 bits */
            subidentifier = ( subidentifier << 7 ) + ( *( u_char* )bufp & ~ASN01_BIT8 );
            length--;
        } while ( ( *( u_char* )bufp++ & ASN01_BIT8 ) && ( length > 0 ) ); /* last byte has high bit clear */

        if ( length == 0 ) {
            u_char* last_byte = bufp - 1;
            if ( *last_byte & ASN01_BIT8 ) {
                /* last byte has high bit set -> wrong BER encoded OID */
                IMPL_ERROR_MSG( "subidentifier syntax error" );
                return NULL;
            }
        }
        if ( subidentifier > TYPES_OID_MAX_SUBID ) {
            IMPL_ERROR_MSG( "subidentifier too large" );
            return NULL;
        }
        *oidp++ = ( oid )subidentifier;
    }

    if ( 0 != length ) {
        IMPL_ERROR_MSG( "OID length exceeds buffer size" );
        *objidlength = original_length;
        return NULL;
    }

    /*
     * The first two subidentifiers are encoded into the first component
     * with the value (X * 40) + Y, where:
     *  X is the value of the first subidentifier.
     *  Y is the value of the second subidentifier.
     */
    subidentifier = ( u_long )objid[ 1 ];
    if ( subidentifier == 0x2B ) {
        objid[ 0 ] = 1;
        objid[ 1 ] = 3;
    } else {
        if ( subidentifier < 40 ) {
            objid[ 0 ] = 0;
            objid[ 1 ] = subidentifier;
        } else if ( subidentifier < 80 ) {
            objid[ 0 ] = 1;
            objid[ 1 ] = subidentifier - 40;
        } else {
            objid[ 0 ] = 2;
            objid[ 1 ] = subidentifier - 80;
        }
    }

    *objidlength = ( int )( oidp - objid );

    DEBUG_MSG( ( "dumpvRecv", "  ObjID: " ) );
    DEBUG_MSGOID( ( "dumpvRecv", objid, *objidlength ) );
    DEBUG_MSG( ( "dumpvRecv", "\n" ) );
    return bufp;
}

/**
 * @internal
 * asn_build_objid - Builds an ASN object identifier object containing the
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
 * @param objid        IN - pointer to start of input buffer
 * @param objidlength  IN - number of sub-id's in objid
 *
 * @return   Returns a pointer to the first byte past the end
 *           of this object (i.e. the start of the next object).
 *           Returns NULL on any error.
 */
u_char* Asn01_buildObjid( u_char* data,
    size_t* datalength,
    u_char type, oid* objid, size_t objidlength )
{
    /*
     * ASN.1 objid ::= 0x06 asnlength subidentifier {subidentifier}*
     * subidentifier ::= {leadingbyte}* lastbyte
     * leadingbyte ::= 1 7bitvalue
     * lastbyte ::= 0 7bitvalue
     */
    size_t asnlength;
    register oid* op = objid;
    u_char objid_size[ TYPES_MAX_OID_LEN ];
    register u_long objid_val;
    u_long first_objid_val;
    register int i;
    u_char* initdatap = data;

    /*
     * check if there are at least 2 sub-identifiers
     */
    if ( objidlength == 0 ) {
        /*
         * there are not, so make OID have two with value of zero
         */
        objid_val = 0;
        objidlength = 2;
    } else if ( objid[ 0 ] > 2 ) {
        IMPL_ERROR_MSG( "build objid: bad first subidentifier" );
        return NULL;
    } else if ( objidlength == 1 ) {
        /*
         * encode the first value
         */
        objid_val = ( op[ 0 ] * 40 );
        objidlength = 2;
        op++;
    } else {
        /*
         * combine the first two values
         */
        if ( ( op[ 1 ] > 40 ) && ( op[ 0 ] < 2 ) ) {
            IMPL_ERROR_MSG( "build objid: bad second subidentifier" );
            return NULL;
        }
        objid_val = ( op[ 0 ] * 40 ) + op[ 1 ];
        op += 2;
    }
    first_objid_val = objid_val;

    /*
     * ditch illegal calls now
     */
    if ( objidlength > TYPES_MAX_OID_LEN )
        return NULL;

    /*
     * calculate the number of bytes needed to store the encoded value
     */
    for ( i = 1, asnlength = 0;; ) {

        CHECK_OVERFLOW_U( objid_val, 5 );
        if ( objid_val < ( unsigned )0x80 ) {
            objid_size[ i ] = 1;
            asnlength += 1;
        } else if ( objid_val < ( unsigned )0x4000 ) {
            objid_size[ i ] = 2;
            asnlength += 2;
        } else if ( objid_val < ( unsigned )0x200000 ) {
            objid_size[ i ] = 3;
            asnlength += 3;
        } else if ( objid_val < ( unsigned )0x10000000 ) {
            objid_size[ i ] = 4;
            asnlength += 4;
        } else {
            objid_size[ i ] = 5;
            asnlength += 5;
        }
        i++;
        if ( i >= ( int )objidlength )
            break;
        objid_val = *op++; /* XXX - doesn't handle 2.X (X > 40) */
    }

    /*
     * store the ASN.1 tag and length
     */
    data = Asn01_buildHeader( data, datalength, type, asnlength );
    if ( _Asn01_buildHeaderCheck( "build objid", data, *datalength, asnlength ) )
        return NULL;

    /*
     * store the encoded OID value
     */
    for ( i = 1, objid_val = first_objid_val, op = objid + 2;
          i < ( int )objidlength; i++ ) {
        if ( i != 1 )
            objid_val = ( uint32_t )( *op++ ); /* already logged warning above */
        switch ( objid_size[ i ] ) {
        case 1:
            *data++ = ( u_char )objid_val;
            break;

        case 2:
            *data++ = ( u_char )( ( objid_val >> 7 ) | 0x80 );
            *data++ = ( u_char )( objid_val & 0x07f );
            break;

        case 3:
            *data++ = ( u_char )( ( objid_val >> 14 ) | 0x80 );
            *data++ = ( u_char )( ( objid_val >> 7 & 0x7f ) | 0x80 );
            *data++ = ( u_char )( objid_val & 0x07f );
            break;

        case 4:
            *data++ = ( u_char )( ( objid_val >> 21 ) | 0x80 );
            *data++ = ( u_char )( ( objid_val >> 14 & 0x7f ) | 0x80 );
            *data++ = ( u_char )( ( objid_val >> 7 & 0x7f ) | 0x80 );
            *data++ = ( u_char )( objid_val & 0x07f );
            break;

        case 5:
            *data++ = ( u_char )( ( objid_val >> 28 ) | 0x80 );
            *data++ = ( u_char )( ( objid_val >> 21 & 0x7f ) | 0x80 );
            *data++ = ( u_char )( ( objid_val >> 14 & 0x7f ) | 0x80 );
            *data++ = ( u_char )( ( objid_val >> 7 & 0x7f ) | 0x80 );
            *data++ = ( u_char )( objid_val & 0x07f );
            break;
        }
    }

    /*
     * return the length and data ptr
     */
    *datalength -= asnlength;
    DEBUG_DUMPSETUP( "send", initdatap, data - initdatap );
    DEBUG_MSG( ( "dumpvSend", "  ObjID: " ) );
    DEBUG_MSGOID( ( "dumpvSend", objid, objidlength ) );
    DEBUG_MSG( ( "dumpvSend", "\n" ) );
    return data;
}

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
u_char* Asn01_parseNull( u_char* data, size_t* datalength, u_char* type )
{
    /*
     * ASN.1 null ::= 0x05 0x00
     */
    register u_char* bufp = data;
    u_long asn_length;

    *type = *bufp++;
    bufp = Asn01_parseLength( bufp, &asn_length );
    if ( bufp == NULL ) {
        IMPL_ERROR_MSG( "parse null: bad length" );
        return NULL;
    }
    if ( asn_length != 0 ) {
        IMPL_ERROR_MSG( "parse null: malformed ASN.1 null" );
        return NULL;
    }

    *datalength -= ( bufp - data );

    DEBUG_DUMPSETUP( "recv", data, bufp - data );
    DEBUG_MSG( ( "dumpvRecv", "  NULL\n" ) );

    return bufp + asn_length;
}

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
u_char* Asn01_buildNull( u_char* data, size_t* datalength, u_char type )
{
    /*
     * ASN.1 null ::= 0x05 0x00
     */
    u_char* initdatap = data;
    data = Asn01_buildHeader( data, datalength, type, 0 );
    DEBUG_DUMPSETUP( "send", initdatap, data - initdatap );
    DEBUG_MSG( ( "dumpvSend", "  NULL\n" ) );
    return data;
}

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
u_char* Asn01_parseBitstring( u_char* data,
    size_t* datalength,
    u_char* type, u_char* str, size_t* strlength )
{
    /*
     * bitstring ::= 0x03 asnlength unused {byte}*
     */
    static const char* errpre = "parse bitstring";
    register u_char* bufp = data;
    u_long asn_length;

    *type = *bufp++;
    if ( *type != ASN01_BIT_STR ) {
        _Asn01_typeErr( errpre, *type );
        return NULL;
    }
    bufp = Asn01_parseLength( bufp, &asn_length );
    if ( _Asn01_parseLengthCheck( errpre, bufp, data,
             asn_length, *datalength ) )
        return NULL;

    if ( ( size_t )asn_length > *strlength ) {
        _Asn01_lengthErr( errpre, ( size_t )asn_length, *strlength );
        return NULL;
    }
    if ( _Asn01_bitstringCheck( errpre, asn_length, *bufp ) )
        return NULL;

    DEBUG_DUMPSETUP( "recv", data, bufp - data );
    DEBUG_MSG( ( "dumpvRecv", "  Bitstring: " ) );
    DEBUG_MSGHEX( ( "dumpvRecv", data, asn_length ) );
    DEBUG_MSG( ( "dumpvRecv", "\n" ) );

    memmove( str, bufp, asn_length );
    *strlength = ( int )asn_length;
    *datalength -= ( int )asn_length + ( bufp - data );
    return bufp + asn_length;
}

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
u_char* Asn01_buildBitstring( u_char* data,
    size_t* datalength,
    u_char type, const u_char* str, size_t strlength )
{
    /*
     * ASN.1 bit string ::= 0x03 asnlength unused {byte}*
     */
    static const char* errpre = "build bitstring";
    if ( _Asn01_bitstringCheck( errpre, strlength, ( u_char )( ( str ) ? *str : 0 ) ) )
        return NULL;

    data = Asn01_buildHeader( data, datalength, type, strlength );
    if ( _Asn01_buildHeaderCheck( errpre, data, *datalength, strlength ) )
        return NULL;

    if ( strlength > 0 && str )
        memmove( data, str, strlength );
    else if ( strlength > 0 && !str ) {
        IMPL_ERROR_MSG( "no string passed into Asn01_buildBitstring\n" );
        return NULL;
    }

    *datalength -= strlength;
    DEBUG_DUMPSETUP( "send", data, strlength );
    DEBUG_MSG( ( "dumpvSend", "  Bitstring: " ) );
    DEBUG_MSGHEX( ( "dumpvSend", data, strlength ) );
    DEBUG_MSG( ( "dumpvSend", "\n" ) );
    return data + strlength;
}

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
u_char* Asn01_parseUnsignedInt64( u_char* data,
    size_t* datalength,
    u_char* type,
    Counter64* cp, size_t countersize )
{
    /*
     * ASN.1 integer ::= 0x02 asnlength byte {byte}*
     */
    static const char* errpre = "parse uint64";
    const int uint64sizelimit = ( 4 * 2 ) + 1;
    register u_char* bufp = data;
    u_long asn_length;
    register u_long low = 0, high = 0;

    if ( countersize != sizeof( Counter64 ) ) {
        _Asn01_sizeErr( errpre, countersize, sizeof( Counter64 ) );
        return NULL;
    }
    *type = *bufp++;
    if ( *type != ASN01_COUNTER64
        && *type != ASN01_OPAQUE ) {
        _Asn01_typeErr( errpre, *type );
        return NULL;
    }
    bufp = Asn01_parseLength( bufp, &asn_length );
    if ( _Asn01_parseLengthCheck( errpre, bufp, data, asn_length, *datalength ) )
        return NULL;

    DEBUG_DUMPSETUP( "recv", data, bufp - data );
    /*
     * 64 bit counters as opaque
     */
    if ( ( *type == ASN01_OPAQUE ) && ( asn_length <= ASN01_OPAQUE_COUNTER64_MX_BER_LEN ) && ( *bufp == ASN01_OPAQUE_TAG1 ) && ( ( *( bufp + 1 ) == ASN01_OPAQUE_COUNTER64 ) || ( *( bufp + 1 ) == ASN01_OPAQUE_U64 ) ) ) {
        /*
         * change type to Counter64 or Int64_U64
         */
        *type = *( bufp + 1 );
        /*
         * value is encoded as special format
         */
        bufp = Asn01_parseLength( bufp + 2, &asn_length );
        if ( _Asn01_parseLengthCheck( "parse opaque uint64", bufp, data,
                 asn_length, *datalength ) )
            return NULL;
    }
    /* NETSNMP_WITH_OPAQUE_SPECIAL_TYPES */
    if ( ( ( int )asn_length > uint64sizelimit ) || ( ( ( int )asn_length == uint64sizelimit ) && *bufp != 0x00 ) ) {
        _Asn01_lengthErr( errpre, ( size_t )asn_length, uint64sizelimit );
        return NULL;
    }
    *datalength -= ( int )asn_length + ( bufp - data );
    while ( asn_length-- ) {
        high = ( ( 0x00FFFFFF & high ) << 8 ) | ( ( low & 0xFF000000U ) >> 24 );
        low = ( ( low & 0x00FFFFFF ) << 8 ) | *bufp++;
    }

    CHECK_OVERFLOW_U( high, 6 );
    CHECK_OVERFLOW_U( low, 6 );

    cp->low = low;
    cp->high = high;

    DEBUG_IF( "dumpvRecv" )
    {
        char i64buf[ INTEGER64_STRING_SIZE + 1 ];
        Integer64_uint64ToString( i64buf, cp );
        DEBUG_MSG( ( "dumpvRecv", "Counter64: %s\n", i64buf ) );
    }

    return bufp;
}

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
u_char* Asn01_buildUnsignedInt64( u_char* data,
    size_t* datalength,
    u_char type,
    const Counter64* cp, size_t countersize )
{
    /*
     * ASN.1 integer ::= 0x02 asnlength byte {byte}*
     */

    register u_long low, high;
    register u_long mask, mask2;
    int add_null_byte = 0;
    size_t intsize;
    u_char* initdatap = data;

    if ( countersize != sizeof( Counter64 ) ) {
        _Asn01_sizeErr( "build uint64", countersize,
            sizeof( Counter64 ) );
        return NULL;
    }
    intsize = 8;
    low = cp->low;
    high = cp->high;

    CHECK_OVERFLOW_U( high, 7 );
    CHECK_OVERFLOW_U( low, 7 );

    mask = 0xff000000U;
    if ( high & 0x80000000U ) {
        /*
         * if MSB is set
         */
        add_null_byte = 1;
        intsize++;
    } else {
        /*
         * Truncate "unnecessary" bytes off of the most significant end of this 2's
         * complement integer.
         * There should be no sequence of 9 consecutive 1's or 0's at the most
         * significant end of the integer.
         */
        mask2 = 0xff800000U;
        while ( ( ( ( high & mask2 ) == 0 ) || ( ( high & mask2 ) == mask2 ) )
            && intsize > 1 ) {
            intsize--;
            high = ( ( high & 0x00ffffffu ) << 8 ) | ( ( low & mask ) >> 24 );
            low = ( low & 0x00ffffffu ) << 8;
        }
    }
    /*
     * encode a Counter64 as an opaque (it also works in SNMPv1)
     */
    /*
     * turn into Opaque holding special tagged value
     */
    if ( type == ASN01_OPAQUE_COUNTER64 ) {
        /*
         * put the tag and length for the Opaque wrapper
         */
        data = Asn01_buildHeader( data, datalength, ASN01_OPAQUE, intsize + 3 );
        if ( _Asn01_buildHeaderCheck( "build counter u64", data, *datalength, intsize + 3 ) )
            return NULL;

        /*
         * put the special tag and length
         */
        *data++ = ASN01_OPAQUE_TAG1;
        *data++ = ASN01_OPAQUE_COUNTER64;
        *data++ = ( u_char )intsize;
        *datalength = *datalength - 3;
    } else
        /*
         * Encode the Unsigned int64 in an opaque
         */
        /*
         * turn into Opaque holding special tagged value
         */
        if ( type == ASN01_OPAQUE_U64 ) {
        /*
         * put the tag and length for the Opaque wrapper
         */
        data = Asn01_buildHeader( data, datalength, ASN01_OPAQUE, intsize + 3 );
        if ( _Asn01_buildHeaderCheck( "build opaque u64", data, *datalength, intsize + 3 ) )
            return NULL;

        /*
         * put the special tag and length
         */
        *data++ = ASN01_OPAQUE_TAG1;
        *data++ = ASN01_OPAQUE_U64;
        *data++ = ( u_char )intsize;
        *datalength = *datalength - 3;
    } else {
        data = Asn01_buildHeader( data, datalength, type, intsize );
        if ( _Asn01_buildHeaderCheck( "build uint64", data, *datalength, intsize ) )
            return NULL;
    }
    *datalength -= intsize;
    if ( add_null_byte == 1 ) {
        *data++ = '\0';
        intsize--;
    }
    while ( intsize-- ) {
        *data++ = ( u_char )( high >> 24 );
        high = ( ( high & 0x00ffffff ) << 8 ) | ( ( low & mask ) >> 24 );
        low = ( low & 0x00ffffff ) << 8;
    }
    DEBUG_DUMPSETUP( "send", initdatap, data - initdatap );
    DEBUG_IF( "dumpvSend" )
    {
        char i64buf[ INTEGER64_STRING_SIZE + 1 ];
        Integer64_uint64ToString( i64buf, cp );
        DEBUG_MSG( ( "dumpvSend", "%s", i64buf ) );
    }
    return data;
}

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

u_char* Asn01_parseSignedInt64( u_char* data,
    size_t* datalength,
    u_char* type,
    Counter64* cp,
    size_t countersize )
{
    static const char* errpre = "parse int64";
    const int int64sizelimit = ( 4 * 2 ) + 1;
    char ebuf[ 128 ];
    register u_char* bufp = data;
    u_long asn_length;
    register u_int low = 0, high = 0;

    if ( countersize != sizeof( Counter64 ) ) {
        _Asn01_sizeErr( errpre, countersize, sizeof( Counter64 ) );
        return NULL;
    }
    *type = *bufp++;
    bufp = Asn01_parseLength( bufp, &asn_length );
    if ( _Asn01_parseLengthCheck( errpre, bufp, data, asn_length, *datalength ) )
        return NULL;

    DEBUG_DUMPSETUP( "recv", data, bufp - data );
    if ( ( *type == ASN01_OPAQUE ) && ( asn_length <= ASN01_OPAQUE_COUNTER64_MX_BER_LEN ) && ( *bufp == ASN01_OPAQUE_TAG1 ) && ( *( bufp + 1 ) == ASN01_OPAQUE_I64 ) ) {
        /*
         * change type to Int64
         */
        *type = *( bufp + 1 );
        /*
         * value is encoded as special format
         */
        bufp = Asn01_parseLength( bufp + 2, &asn_length );
        if ( _Asn01_parseLengthCheck( "parse opaque int64", bufp, data,
                 asn_length, *datalength ) )
            return NULL;
    }
    /*
     * this should always have been true until snmp gets int64 PDU types
     */
    else {
        snprintf( ebuf, sizeof( ebuf ),
            "%s: wrong type: %d, len %d, buf bytes (%02X,%02X)",
            errpre, *type, ( int )asn_length, *bufp, *( bufp + 1 ) );
        ebuf[ sizeof( ebuf ) - 1 ] = 0;
        IMPL_ERROR_MSG( ebuf );
        return NULL;
    }
    if ( ( ( int )asn_length > int64sizelimit ) || ( ( ( int )asn_length == int64sizelimit ) && *bufp != 0x00 ) ) {
        _Asn01_lengthErr( errpre, ( size_t )asn_length, int64sizelimit );
        return NULL;
    }
    *datalength -= ( int )asn_length + ( bufp - data );
    if ( *bufp & 0x80 ) {
        low = 0xFFFFFFFFU; /* first byte bit 1 means start the data with 1s */
        high = 0xFFFFFF;
    }

    while ( asn_length-- ) {
        high = ( ( 0x00FFFFFF & high ) << 8 ) | ( ( low & 0xFF000000U ) >> 24 );
        low = ( ( low & 0x00FFFFFF ) << 8 ) | *bufp++;
    }

    CHECK_OVERFLOW_U( high, 8 );
    CHECK_OVERFLOW_U( low, 8 );

    cp->low = low;
    cp->high = high;

    DEBUG_IF( "dumpvRecv" )
    {
        char i64buf[ INTEGER64_STRING_SIZE + 1 ];
        Integer64_int64ToString( i64buf, cp );
        DEBUG_MSG( ( "dumpvRecv", "Integer64: %s\n", i64buf ) );
    }

    return bufp;
}

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
u_char* Asn01_buildSignedInt64( u_char* data,
    size_t* datalength,
    u_char type,
    const Counter64* cp, size_t countersize )
{
    /*
     * ASN.1 integer ::= 0x02 asnlength byte {byte}*
     */

    register u_int mask, mask2;
    u_long low;
    long high; /* MUST be signed because of CHECK_OVERFLOW_S(). */
    size_t intsize;
    u_char* initdatap = data;

    if ( countersize != sizeof( Counter64 ) ) {
        _Asn01_sizeErr( "build int64", countersize,
            sizeof( Counter64 ) );
        return NULL;
    }
    intsize = 8;
    low = cp->low;
    high = cp->high; /* unsigned to signed conversion */

    CHECK_OVERFLOW_S( high, 9 );
    CHECK_OVERFLOW_U( low, 9 );

    /*
     * Truncate "unnecessary" bytes off of the most significant end of this
     * 2's complement integer.  There should be no sequence of 9
     * consecutive 1's or 0's at the most significant end of the
     * integer.
     */
    mask = 0xFF000000U;
    mask2 = 0xFF800000U;
    while ( ( ( ( high & mask2 ) == 0 ) || ( ( high & mask2 ) == mask2 ) )
        && intsize > 1 ) {
        intsize--;
        high = ( ( high & 0x00ffffff ) << 8 ) | ( ( low & mask ) >> 24 );
        low = ( low & 0x00ffffff ) << 8;
    }
    /*
     * until a real int64 gets incorperated into SNMP, we are going to
     * encode it as an opaque instead.  First, we build the opaque
     * header and then the int64 tag type we use to mark it as an
     * int64 in the opaque string.
     */
    data = Asn01_buildHeader( data, datalength, ASN01_OPAQUE, intsize + 3 );
    if ( _Asn01_buildHeaderCheck( "build int64", data, *datalength, intsize + 3 ) )
        return NULL;

    *data++ = ASN01_OPAQUE_TAG1;
    *data++ = ASN01_OPAQUE_I64;
    *data++ = ( u_char )intsize;
    *datalength -= ( 3 + intsize );

    while ( intsize-- ) {
        *data++ = ( u_char )( high >> 24 );
        high = ( ( high & 0x00ffffff ) << 8 ) | ( ( low & mask ) >> 24 );
        low = ( low & 0x00ffffff ) << 8;
    }
    DEBUG_DUMPSETUP( "send", initdatap, data - initdatap );
    DEBUG_IF( "dumpvSend" )
    {
        char i64buf[ INTEGER64_STRING_SIZE + 1 ];
        Integer64_uint64ToString( i64buf, cp );
        DEBUG_MSG( ( "dumpvSend", "%s\n", i64buf ) );
    }
    return data;
}

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
u_char* Asn01_parseFloat( u_char* data,
    size_t* datalength,
    u_char* type, float* floatp, size_t floatsize )
{
    static const char* errpre = "parse float";
    register u_char* bufp = data;
    u_long asn_length;
    union {
        float floatVal;
        long longVal;
        u_char c[ sizeof( float ) ];
    } fu;

    if ( floatsize != sizeof( float ) ) {
        _Asn01_sizeErr( "parse float", floatsize, sizeof( float ) );
        return NULL;
    }
    *type = *bufp++;
    bufp = Asn01_parseLength( bufp, &asn_length );
    if ( _Asn01_parseLengthCheck( "parse float", bufp, data,
             asn_length, *datalength ) )
        return NULL;

    DEBUG_DUMPSETUP( "recv", data, bufp - data + asn_length );
    /*
     * the float is encoded as an opaque
     */
    if ( ( *type == ASN01_OPAQUE ) && ( asn_length == ASN01_OPAQUE_FLOAT_BER_LEN ) && ( *bufp == ASN01_OPAQUE_TAG1 ) && ( *( bufp + 1 ) == ASN01_OPAQUE_FLOAT ) ) {

        /*
         * value is encoded as special format
         */
        bufp = Asn01_parseLength( bufp + 2, &asn_length );
        if ( _Asn01_parseLengthCheck( "parse opaque float", bufp, data,
                 asn_length, *datalength ) )
            return NULL;

        /*
         * change type to Float
         */
        *type = ASN01_OPAQUE_FLOAT;
    }

    if ( *type != ASN01_OPAQUE_FLOAT ) {
        _Asn01_typeErr( errpre, *type );
        return NULL;
    }

    if ( asn_length != sizeof( float ) ) {
        _Asn01_sizeErr( "parse seq float", asn_length, sizeof( float ) );
        return NULL;
    }

    *datalength -= ( int )asn_length + ( bufp - data );
    memcpy( &fu.c[ 0 ], bufp, asn_length );

    /*
     * correct for endian differences
     */
    fu.longVal = ntohl( fu.longVal );

    *floatp = fu.floatVal;

    DEBUG_MSG( ( "dumpvRecv", "Opaque float: %f\n", *floatp ) );
    return bufp;
}

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
u_char* Asn01_buildFloat( u_char* data,
    size_t* datalength,
    u_char type, const float* floatp, size_t floatsize )
{
    union {
        float floatVal;
        int intVal;
        u_char c[ sizeof( float ) ];
    } fu;
    u_char* initdatap = data;

    if ( floatsize != sizeof( float ) ) {
        _Asn01_sizeErr( "build float", floatsize, sizeof( float ) );
        return NULL;
    }
    /*
     * encode the float as an opaque
     */
    /*
     * turn into Opaque holding special tagged value
     */

    /*
     * put the tag and length for the Opaque wrapper
     */
    data = Asn01_buildHeader( data, datalength, ASN01_OPAQUE, floatsize + 3 );
    if ( _Asn01_buildHeaderCheck( "build float", data, *datalength, ( floatsize + 3 ) ) )
        return NULL;

    /*
     * put the special tag and length
     */
    *data++ = ASN01_OPAQUE_TAG1;
    *data++ = ASN01_OPAQUE_FLOAT;
    *data++ = ( u_char )floatsize;
    *datalength = *datalength - 3;

    fu.floatVal = *floatp;
    /*
     * correct for endian differences
     */
    fu.intVal = htonl( fu.intVal );

    *datalength -= floatsize;
    memcpy( data, &fu.c[ 0 ], floatsize );

    DEBUG_DUMPSETUP( "send", initdatap, data - initdatap );
    DEBUG_MSG( ( "dumpvSend", "Opaque float: %f\n", *floatp ) );
    data += floatsize;
    return data;
}

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
u_char* Asn01_parseDouble( u_char* data,
    size_t* datalength,
    u_char* type, double* doublep, size_t doublesize )
{
    static const char* errpre = "parse double";
    register u_char* bufp = data;
    u_long asn_length;
    long tmp;
    union {
        double doubleVal;
        int intVal[ 2 ];
        u_char c[ sizeof( double ) ];
    } fu;

    if ( doublesize != sizeof( double ) ) {
        _Asn01_sizeErr( "parse double", doublesize, sizeof( double ) );
        return NULL;
    }
    *type = *bufp++;
    bufp = Asn01_parseLength( bufp, &asn_length );
    if ( _Asn01_parseLengthCheck( "parse double", bufp, data,
             asn_length, *datalength ) )
        return NULL;

    DEBUG_DUMPSETUP( "recv", data, bufp - data + asn_length );
    /*
     * the double is encoded as an opaque
     */
    if ( ( *type == ASN01_OPAQUE ) && ( asn_length == ASN01_OPAQUE_DOUBLE_BER_LEN ) && ( *bufp == ASN01_OPAQUE_TAG1 ) && ( *( bufp + 1 ) == ASN01_OPAQUE_DOUBLE ) ) {

        /*
         * value is encoded as special format
         */
        bufp = Asn01_parseLength( bufp + 2, &asn_length );
        if ( _Asn01_parseLengthCheck( "parse opaque double", bufp, data,
                 asn_length, *datalength ) )
            return NULL;

        /*
         * change type to Double
         */
        *type = ASN01_OPAQUE_DOUBLE;
    }

    if ( *type != ASN01_OPAQUE_DOUBLE ) {
        _Asn01_typeErr( errpre, *type );
        return NULL;
    }

    if ( asn_length != sizeof( double ) ) {
        _Asn01_sizeErr( "parse seq double", asn_length, sizeof( double ) );
        return NULL;
    }
    *datalength -= ( int )asn_length + ( bufp - data );
    memcpy( &fu.c[ 0 ], bufp, asn_length );

    /*
     * correct for endian differences
     */

    tmp = ntohl( fu.intVal[ 0 ] );
    fu.intVal[ 0 ] = ntohl( fu.intVal[ 1 ] );
    fu.intVal[ 1 ] = tmp;

    *doublep = fu.doubleVal;
    DEBUG_MSG( ( "dumpvRecv", "  Opaque Double:\t%f\n", *doublep ) );

    return bufp;
}

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
u_char* Asn01_buildDouble( u_char* data,
    size_t* datalength,
    u_char type, const double* doublep, size_t doublesize )
{
    long tmp;
    union {
        double doubleVal;
        int intVal[ 2 ];
        u_char c[ sizeof( double ) ];
    } fu;
    u_char* initdatap = data;

    if ( doublesize != sizeof( double ) ) {
        _Asn01_sizeErr( "build double", doublesize, sizeof( double ) );
        return NULL;
    }

    /*
     * encode the double as an opaque
     */
    /*
     * turn into Opaque holding special tagged value
     */

    /*
     * put the tag and length for the Opaque wrapper
     */
    data = Asn01_buildHeader( data, datalength, ASN01_OPAQUE, doublesize + 3 );
    if ( _Asn01_buildHeaderCheck( "build double", data, *datalength, doublesize + 3 ) )
        return NULL;

    /*
     * put the special tag and length
     */
    *data++ = ASN01_OPAQUE_TAG1;
    *data++ = ASN01_OPAQUE_DOUBLE;
    *data++ = ( u_char )doublesize;
    *datalength = *datalength - 3;

    fu.doubleVal = *doublep;
    /*
     * correct for endian differences
     */
    tmp = htonl( fu.intVal[ 0 ] );
    fu.intVal[ 0 ] = htonl( fu.intVal[ 1 ] );
    fu.intVal[ 1 ] = tmp;
    *datalength -= doublesize;
    memcpy( data, &fu.c[ 0 ], doublesize );

    data += doublesize;
    DEBUG_DUMPSETUP( "send", initdatap, data - initdatap );
    DEBUG_MSG( ( "dumpvSend", "  Opaque double: %f\n", *doublep ) );
    return data;
}

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
int Asn01_realloc( u_char** pkt, size_t* pkt_len )
{
    if ( pkt != NULL && pkt_len != NULL ) {
        size_t old_pkt_len = *pkt_len;

        DEBUG_MSGTL( ( "Asn01_realloc", " old_pkt %8p, old_pkt_len %lu\n",
            *pkt, ( unsigned long )old_pkt_len ) );

        if ( Memory_reallocIncrease( pkt, pkt_len ) ) {
            DEBUG_MSGTL( ( "Asn01_realloc", " new_pkt %8p, new_pkt_len %lu\n",
                *pkt, ( unsigned long )*pkt_len ) );
            DEBUG_MSGTL( ( "Asn01_realloc",
                " memmove(%8p + %08x, %8p, %08x)\n",
                *pkt, ( unsigned )( *pkt_len - old_pkt_len ),
                *pkt, ( unsigned )old_pkt_len ) );
            memmove( *pkt + ( *pkt_len - old_pkt_len ), *pkt, old_pkt_len );
            memset( *pkt, ( int )' ', *pkt_len - old_pkt_len );
            return 1;
        } else {
            DEBUG_MSG( ( "Asn01_realloc", " CANNOT REALLOC()\n" ) );
        }
    }
    return 0;
}

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
int Asn01_reallocRbuildLength( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r, size_t length )
{
    static const char* errpre = "build length";
    char ebuf[ 128 ];
    int tmp_int;
    size_t start_offset = *offset;

    if ( length <= 0x7f ) {
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            snprintf( ebuf, sizeof( ebuf ),
                "%s: bad length < 1 :%ld, %lu", errpre,
                ( long )( *pkt_len - *offset ), ( unsigned long )length );
            ebuf[ sizeof( ebuf ) - 1 ] = 0;
            IMPL_ERROR_MSG( ebuf );
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = length;
    } else {
        while ( length > 0xff ) {
            if ( ( ( *pkt_len - *offset ) < 1 )
                && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                snprintf( ebuf, sizeof( ebuf ),
                    "%s: bad length < 1 :%ld, %lu", errpre,
                    ( long )( *pkt_len - *offset ), ( unsigned long )length );
                ebuf[ sizeof( ebuf ) - 1 ] = 0;
                IMPL_ERROR_MSG( ebuf );
                return 0;
            }
            *( *pkt + *pkt_len - ( ++*offset ) ) = length & 0xff;
            length >>= 8;
        }

        while ( ( *pkt_len - *offset ) < 2 ) {
            if ( !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                snprintf( ebuf, sizeof( ebuf ),
                    "%s: bad length < 1 :%ld, %lu", errpre,
                    ( long )( *pkt_len - *offset ), ( unsigned long )length );
                ebuf[ sizeof( ebuf ) - 1 ] = 0;
                IMPL_ERROR_MSG( ebuf );
                return 0;
            }
        }

        *( *pkt + *pkt_len - ( ++*offset ) ) = length & 0xff;
        tmp_int = *offset - start_offset;
        *( *pkt + *pkt_len - ( ++*offset ) ) = tmp_int | 0x80;
    }

    return 1;
}

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
int Asn01_reallocRbuildHeader( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r,
    u_char type, size_t length )
{
    char ebuf[ 128 ];

    if ( Asn01_reallocRbuildLength( pkt, pkt_len, offset, r, length ) ) {
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            snprintf( ebuf, sizeof( ebuf ),
                "bad header length < 1 :%ld, %lu",
                ( long )( *pkt_len - *offset ), ( unsigned long )length );
            ebuf[ sizeof( ebuf ) - 1 ] = 0;
            IMPL_ERROR_MSG( ebuf );
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = type;
        return 1;
    }
    return 0;
}

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
    size_t* offset, int r,
    u_char type, const long* intp, size_t intsize )
{
    static const char* errpre = "build int";
    register long integer = *intp;
    int testvalue;
    size_t start_offset = *offset;

    if ( intsize != sizeof( long ) ) {
        _Asn01_sizeErr( errpre, intsize, sizeof( long ) );
        return 0;
    }

    CHECK_OVERFLOW_S( integer, 10 );
    testvalue = ( integer < 0 ) ? -1 : 0;

    if ( ( ( *pkt_len - *offset ) < 1 ) && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
        return 0;
    }
    *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )integer;
    integer >>= 8;

    while ( integer != testvalue ) {
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )integer;
        integer >>= 8;
    }

    if ( ( *( *pkt + *pkt_len - *offset ) & 0x80 ) != ( testvalue & 0x80 ) ) {
        /*
         * Make sure left most bit is representational of the rest of the bits
         * that aren't encoded.
         */
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = testvalue & 0xff;
    }

    if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r, type,
             ( *offset - start_offset ) ) ) {
        if ( _Asn01_reallocBuildHeaderCheck( errpre, pkt, pkt_len,
                 ( *offset - start_offset ) ) ) {
            return 0;
        } else {
            DEBUG_DUMPSETUP( "send", ( *pkt + *pkt_len - *offset ),
                ( *offset - start_offset ) );
            DEBUG_MSG( ( "dumpvSend", "  Integer:\t%ld (0x%.2lX)\n", *intp,
                *intp ) );
            return 1;
        }
    }

    return 0;
}

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

int Asn01_reallocRbuildString( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r,
    u_char type,
    const u_char* str, size_t strlength )
{
    static const char* errpre = "build string";
    size_t start_offset = *offset;

    while ( ( *pkt_len - *offset ) < strlength ) {
        if ( !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
    }

    *offset += strlength;
    memcpy( *pkt + *pkt_len - *offset, str, strlength );

    if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r, type, strlength ) ) {
        if ( _Asn01_reallocBuildHeaderCheck( errpre, pkt, pkt_len, strlength ) ) {
            return 0;
        } else {
            DEBUG_DUMPSETUP( "send", ( *pkt + *pkt_len - *offset ),
                *offset - start_offset );
            DEBUG_IF( "dumpvSend" )
            {
                if ( strlength == 0 ) {
                    DEBUG_MSG( ( "dumpvSend", "  String: [NULL]\n" ) );
                } else {
                    u_char* buf = ( u_char* )malloc( 2 * strlength );
                    size_t l = ( buf != NULL ) ? ( 2 * strlength ) : 0, ol = 0;

                    if ( Mib_sprintReallocAsciiString( &buf, &l, &ol, 1, str, strlength ) ) {
                        DEBUG_MSG( ( "dumpvSend", "  String:\t%s\n", buf ) );
                    } else {
                        if ( buf == NULL ) {
                            DEBUG_MSG( ( "dumpvSend",
                                "  String:\t[TRUNCATED]\n" ) );
                        } else {
                            DEBUG_MSG( ( "dumpvSend",
                                "  String:\t%s [TRUNCATED]\n", buf ) );
                        }
                    }
                    if ( buf != NULL ) {
                        free( buf );
                    }
                }
            }
        }
        return 1;
    }

    return 0;
}

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
int Asn01_reallocRbuildUnsignedInt( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r,
    u_char type, const u_long* intp, size_t intsize )
{
    static const char* errpre = "build uint";
    register u_long integer = *intp;
    size_t start_offset = *offset;

    if ( intsize != sizeof( unsigned long ) ) {
        _Asn01_sizeErr( errpre, intsize, sizeof( unsigned long ) );
        return 0;
    }

    CHECK_OVERFLOW_U( integer, 11 );

    if ( ( ( *pkt_len - *offset ) < 1 ) && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
        return 0;
    }
    *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )integer;
    integer >>= 8;

    while ( integer != 0 ) {
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )integer;
        integer >>= 8;
    }

    if ( ( *( *pkt + *pkt_len - *offset ) & 0x80 ) != ( 0 & 0x80 ) ) {
        /*
         * Make sure left most bit is representational of the rest of the bits
         * that aren't encoded.
         */
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = 0;
    }

    if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r, type,
             ( *offset - start_offset ) ) ) {
        if ( _Asn01_reallocBuildHeaderCheck( errpre, pkt, pkt_len,
                 ( *offset - start_offset ) ) ) {
            return 0;
        } else {
            DEBUG_DUMPSETUP( "send", ( *pkt + *pkt_len - *offset ),
                ( *offset - start_offset ) );
            DEBUG_MSG( ( "dumpvSend", "  UInteger:\t%lu (0x%.2lX)\n", *intp,
                *intp ) );
            return 1;
        }
    }

    return 0;
}

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

int Asn01_reallocRbuildSequence( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r,
    u_char type, size_t length )
{
    return Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r, type,
        length );
}

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

int Asn01_reallocRbuildObjid( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r,
    u_char type,
    const oid* objid, size_t objidlength )
{
    /*
     * ASN.1 objid ::= 0x06 asnlength subidentifier {subidentifier}*
     * subidentifier ::= {leadingbyte}* lastbyte
     * leadingbyte ::= 1 7bitvalue
     * lastbyte ::= 0 7bitvalue
     */
    register size_t i;
    register oid tmpint;
    size_t start_offset = *offset;
    const char* errpre = "build objid";

    /*
     * Check if there are at least 2 sub-identifiers.
     */
    if ( objidlength == 0 ) {
        /*
         * There are not, so make OID have two with value of zero.
         */
        while ( ( *pkt_len - *offset ) < 2 ) {
            if ( !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                return 0;
            }
        }

        *( *pkt + *pkt_len - ( ++*offset ) ) = 0;
        *( *pkt + *pkt_len - ( ++*offset ) ) = 0;
    } else if ( objid[ 0 ] > 2 ) {
        IMPL_ERROR_MSG( "build objid: bad first subidentifier" );
        return 0;
    } else if ( objidlength == 1 ) {
        /*
         * Encode the first value.
         */
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )objid[ 0 ];
    } else {
        for ( i = objidlength; i > 2; i-- ) {
            tmpint = objid[ i - 1 ];
            CHECK_OVERFLOW_U( tmpint, 12 );

            if ( ( ( *pkt_len - *offset ) < 1 )
                && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                return 0;
            }
            *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )tmpint & 0x7f;
            tmpint >>= 7;

            while ( tmpint > 0 ) {
                if ( ( ( *pkt_len - *offset ) < 1 )
                    && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                    return 0;
                }
                *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )( ( tmpint & 0x7f ) | 0x80 );
                tmpint >>= 7;
            }
        }

        /*
         * Combine the first two values.
         */
        if ( ( objid[ 1 ] > 40 ) && ( objid[ 0 ] < 2 ) ) {
            IMPL_ERROR_MSG( "build objid: bad second subidentifier" );
            return 0;
        }
        tmpint = ( ( objid[ 0 ] * 40 ) + objid[ 1 ] );
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )tmpint & 0x7f;
        tmpint >>= 7;

        while ( tmpint > 0 ) {
            if ( ( ( *pkt_len - *offset ) < 1 )
                && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                return 0;
            }
            *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )( ( tmpint & 0x7f ) | 0x80 );
            tmpint >>= 7;
        }
    }

    tmpint = *offset - start_offset;
    if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r, type,
             ( *offset - start_offset ) ) ) {
        if ( _Asn01_reallocBuildHeaderCheck( errpre, pkt, pkt_len,
                 ( *offset - start_offset ) ) ) {
            return 0;
        } else {
            DEBUG_DUMPSETUP( "send", ( *pkt + *pkt_len - *offset ),
                ( *offset - start_offset ) );
            DEBUG_MSG( ( "dumpvSend", "  ObjID: " ) );
            DEBUG_MSGOID( ( "dumpvSend", objid, objidlength ) );
            DEBUG_MSG( ( "dumpvSend", "\n" ) );
            return 1;
        }
    }

    return 0;
}

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

int Asn01_reallocRbuildNull( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r, u_char type )
{
    /*
     * ASN.1 null ::= 0x05 0x00
     */
    size_t start_offset = *offset;

    if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r, type, 0 ) ) {
        DEBUG_DUMPSETUP( "send", ( *pkt + *pkt_len - *offset ),
            ( *offset - start_offset ) );
        DEBUG_MSG( ( "dumpvSend", "  NULL\n" ) );
        return 1;
    } else {
        return 0;
    }
}

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

int Asn01_reallocRbuildBitstring( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r,
    u_char type,
    const u_char* str, size_t strlength )
{
    /*
     * ASN.1 bit string ::= 0x03 asnlength unused {byte}*
     */
    static const char* errpre = "build bitstring";
    size_t start_offset = *offset;

    while ( ( *pkt_len - *offset ) < strlength ) {
        if ( !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
    }

    *offset += strlength;
    memcpy( *pkt + *pkt_len - *offset, str, strlength );

    if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r, type, strlength ) ) {
        if ( _Asn01_reallocBuildHeaderCheck( errpre, pkt, pkt_len, strlength ) ) {
            return 0;
        } else {
            DEBUG_DUMPSETUP( "send", ( *pkt + *pkt_len - *offset ),
                *offset - start_offset );
            DEBUG_IF( "dumpvSend" )
            {
                if ( strlength == 0 ) {
                    DEBUG_MSG( ( "dumpvSend", "  Bitstring: [NULL]\n" ) );
                } else {
                    u_char* buf = ( u_char* )malloc( 2 * strlength );
                    size_t l = ( buf != NULL ) ? ( 2 * strlength ) : 0, ol = 0;

                    if ( Mib_sprintReallocAsciiString( &buf, &l, &ol, 1, str, strlength ) ) {
                        DEBUG_MSG( ( "dumpvSend", "  Bitstring:\t%s\n",
                            buf ) );
                    } else {
                        if ( buf == NULL ) {
                            DEBUG_MSG( ( "dumpvSend",
                                "  Bitstring:\t[TRUNCATED]\n" ) );
                        } else {
                            DEBUG_MSG( ( "dumpvSend",
                                "  Bitstring:\t%s [TRUNCATED]\n",
                                buf ) );
                        }
                    }
                    if ( buf != NULL ) {
                        free( buf );
                    }
                }
            }
        }
        return 1;
    }

    return 0;
}

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
int Asn01_reallocRbuildUnsignedInt64( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r,
    u_char type,
    const Counter64* cp, size_t countersize )
{
    /*
     * ASN.1 integer ::= 0x02 asnlength byte {byte}*
     */
    register u_long low = cp->low, high = cp->high;
    size_t intsize, start_offset = *offset;
    int count;

    if ( countersize != sizeof( Counter64 ) ) {
        _Asn01_sizeErr( "build uint64", countersize,
            sizeof( Counter64 ) );
        return 0;
    }

    CHECK_OVERFLOW_U( high, 13 );
    CHECK_OVERFLOW_U( low, 13 );

    /*
     * Encode the low 4 bytes first.
     */
    if ( ( ( *pkt_len - *offset ) < 1 ) && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
        return 0;
    }
    *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )low;
    low >>= 8;
    count = 1;

    while ( low != 0 ) {
        count++;
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )low;
        low >>= 8;
    }

    /*
     * Then the high byte if present.
     */
    if ( high ) {
        /*
         * Do the rest of the low byte.
         */
        for ( ; count < 4; count++ ) {
            if ( ( ( *pkt_len - *offset ) < 1 )
                && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                return 0;
            }
            *( *pkt + *pkt_len - ( ++*offset ) ) = 0;
        }

        /*
         * Do high byte.
         */
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )high;
        high >>= 8;

        while ( high != 0 ) {
            if ( ( ( *pkt_len - *offset ) < 1 )
                && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                return 0;
            }
            *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )high;
            high >>= 8;
        }
    }

    if ( ( *( *pkt + *pkt_len - *offset ) & 0x80 ) != ( 0 & 0x80 ) ) {
        /*
         * Make sure left most bit is representational of the rest of the bits
         * that aren't encoded.
         */
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = 0;
    }

    intsize = *offset - start_offset;

    /*
     * Encode a Counter64 as an opaque (it also works in SNMPv1).
     */
    if ( type == ASN01_OPAQUE_COUNTER64 ) {
        while ( ( *pkt_len - *offset ) < 5 ) {
            if ( !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                return 0;
            }
        }

        *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )intsize;
        *( *pkt + *pkt_len - ( ++*offset ) ) = ASN01_OPAQUE_COUNTER64;
        *( *pkt + *pkt_len - ( ++*offset ) ) = ASN01_OPAQUE_TAG1;

        /*
         * Put the tag and length for the Opaque wrapper.
         */
        if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r,
                 ASN01_OPAQUE, intsize + 3 ) ) {
            if ( _Asn01_reallocBuildHeaderCheck( "build counter u64", pkt, pkt_len, intsize + 3 ) ) {
                return 0;
            }
        } else {
            return 0;
        }
    } else if ( type == ASN01_OPAQUE_U64 ) {
        /*
         * Encode the Unsigned int64 in an opaque.
         */
        while ( ( *pkt_len - *offset ) < 5 ) {
            if ( !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                return 0;
            }
        }

        *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )intsize;
        *( *pkt + *pkt_len - ( ++*offset ) ) = ASN01_OPAQUE_U64;
        *( *pkt + *pkt_len - ( ++*offset ) ) = ASN01_OPAQUE_TAG1;

        /*
         * Put the tag and length for the Opaque wrapper.
         */
        if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r,
                 ASN01_OPAQUE, intsize + 3 ) ) {
            if ( _Asn01_reallocBuildHeaderCheck( "build counter u64", pkt, pkt_len, intsize + 3 ) ) {
                return 0;
            }
        } else {
            return 0;
        }
    } else {

        if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r, type, intsize ) ) {
            if ( _Asn01_reallocBuildHeaderCheck( "build uint64", pkt, pkt_len, intsize ) ) {
                return 0;
            }
        } else {
            return 0;
        }
    }

    DEBUG_DUMPSETUP( "send", ( *pkt + *pkt_len - *offset ), intsize );
    DEBUG_MSG( ( "dumpvSend", "  Int64_U64:\t%lu %lu\n", cp->high, cp->low ) );
    return 1;
}

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
int Asn01_reallocRbuildSignedInt64( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r,
    u_char type,
    const Counter64* cp, size_t countersize )
{
    /*
     * ASN.1 integer ::= 0x02 asnlength byte {byte}*
     */
    register int32_t low = cp->low, high = cp->high;
    size_t intsize, start_offset = *offset;
    int count;
    int32_t testvalue = ( high & 0x80000000 ) ? -1 : 0;

    if ( countersize != sizeof( Counter64 ) ) {
        _Asn01_sizeErr( "build uint64", countersize,
            sizeof( Counter64 ) );
        return 0;
    }

    CHECK_OVERFLOW_S( high, 14 );
    CHECK_OVERFLOW_U( low, 14 );

    /*
     * Encode the low 4 bytes first.
     */
    if ( ( ( *pkt_len - *offset ) < 1 ) && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
        return 0;
    }
    *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )low;
    low >>= 8;
    count = 1;

    while ( ( int )low != testvalue && count < 4 ) {
        count++;
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )low;
        low >>= 8;
    }

    /*
     * Then the high byte if present.
     */
    if ( high != testvalue ) {
        /*
         * Do the rest of the low byte.
         */
        for ( ; count < 4; count++ ) {
            if ( ( ( *pkt_len - *offset ) < 1 )
                && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                return 0;
            }
            *( *pkt + *pkt_len - ( ++*offset ) ) = ( testvalue == 0 ) ? 0 : 0xff;
        }

        /*
         * Do high byte.
         */
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )high;
        high >>= 8;

        while ( ( int )high != testvalue ) {
            if ( ( ( *pkt_len - *offset ) < 1 )
                && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
                return 0;
            }
            *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )high;
            high >>= 8;
        }
    }

    if ( ( *( *pkt + *pkt_len - *offset ) & 0x80 ) != ( testvalue & 0x80 ) ) {
        /*
         * Make sure left most bit is representational of the rest of the bits
         * that aren't encoded.
         */
        if ( ( ( *pkt_len - *offset ) < 1 )
            && !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
        *( *pkt + *pkt_len - ( ++*offset ) ) = ( testvalue == 0 ) ? 0 : 0xff;
    }

    intsize = *offset - start_offset;

    while ( ( *pkt_len - *offset ) < 5 ) {
        if ( !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
    }

    *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )intsize;
    *( *pkt + *pkt_len - ( ++*offset ) ) = ASN01_OPAQUE_I64;
    *( *pkt + *pkt_len - ( ++*offset ) ) = ASN01_OPAQUE_TAG1;

    /*
     * Put the tag and length for the Opaque wrapper.
     */
    if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r,
             ASN01_OPAQUE, intsize + 3 ) ) {
        if ( _Asn01_reallocBuildHeaderCheck( "build counter u64", pkt, pkt_len, intsize + 3 ) ) {
            return 0;
        }
    } else {
        return 0;
    }

    DEBUG_DUMPSETUP( "send", ( *pkt + *pkt_len - *offset ), intsize );
    DEBUG_MSG( ( "dumpvSend", "  UInt64:\t%lu %lu\n", cp->high, cp->low ) );
    return 1;
}

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

int Asn01_reallocRbuildFloat( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r,
    u_char type, const float* floatp, size_t floatsize )
{
    size_t start_offset = *offset;
    union {
        float floatVal;
        int intVal;
        u_char c[ sizeof( float ) ];
    } fu;

    /*
     * Floatsize better not be larger than realistic.
     */
    if ( floatsize != sizeof( float ) || floatsize > 122 ) {
        return 0;
    }

    while ( ( *pkt_len - *offset ) < floatsize + 3 ) {
        if ( !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
    }

    /*
     * Correct for endian differences and copy value.
     */
    fu.floatVal = *floatp;
    fu.intVal = htonl( fu.intVal );
    *offset += floatsize;
    memcpy( *pkt + *pkt_len - *offset, &( fu.c[ 0 ] ), floatsize );

    /*
     * Put the special tag and length (3 bytes).
     */
    *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )floatsize;
    *( *pkt + *pkt_len - ( ++*offset ) ) = ASN01_OPAQUE_FLOAT;
    *( *pkt + *pkt_len - ( ++*offset ) ) = ASN01_OPAQUE_TAG1;

    /*
     * Put the tag and length for the Opaque wrapper.
     */
    if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r,
             ASN01_OPAQUE, floatsize + 3 ) ) {
        if ( _Asn01_reallocBuildHeaderCheck( "build float", pkt, pkt_len,
                 floatsize + 3 ) ) {
            return 0;
        } else {
            DEBUG_DUMPSETUP( "send", ( *pkt + *pkt_len - *offset ),
                *offset - start_offset );
            DEBUG_MSG( ( "dumpvSend", "Opaque Float:\t%f\n", *floatp ) );
            return 1;
        }
    }

    return 0;
}

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

int Asn01_reallocRbuildDouble( u_char** pkt, size_t* pkt_len,
    size_t* offset, int r,
    u_char type, const double* doublep, size_t doublesize )
{
    size_t start_offset = *offset;
    long tmp;
    union {
        double doubleVal;
        int intVal[ 2 ];
        u_char c[ sizeof( double ) ];
    } fu;

    /*
     * Doublesize better not be larger than realistic.
     */
    if ( doublesize != sizeof( double ) || doublesize > 122 ) {
        return 0;
    }

    while ( ( *pkt_len - *offset ) < doublesize + 3 ) {
        if ( !( r && Asn01_realloc( pkt, pkt_len ) ) ) {
            return 0;
        }
    }

    /*
     * Correct for endian differences and copy value.
     */
    fu.doubleVal = *doublep;
    tmp = htonl( fu.intVal[ 0 ] );
    fu.intVal[ 0 ] = htonl( fu.intVal[ 1 ] );
    fu.intVal[ 1 ] = tmp;
    *offset += doublesize;
    memcpy( *pkt + *pkt_len - *offset, &( fu.c[ 0 ] ), doublesize );

    /*
     * Put the special tag and length (3 bytes).
     */
    *( *pkt + *pkt_len - ( ++*offset ) ) = ( u_char )doublesize;
    *( *pkt + *pkt_len - ( ++*offset ) ) = ASN01_OPAQUE_DOUBLE;
    *( *pkt + *pkt_len - ( ++*offset ) ) = ASN01_OPAQUE_TAG1;

    /*
     * Put the tag and length for the Opaque wrapper.
     */
    if ( Asn01_reallocRbuildHeader( pkt, pkt_len, offset, r,
             ASN01_OPAQUE, doublesize + 3 ) ) {
        if ( _Asn01_reallocBuildHeaderCheck( "build float", pkt, pkt_len,
                 doublesize + 3 ) ) {
            return 0;
        } else {
            DEBUG_DUMPSETUP( "send", ( *pkt + *pkt_len - *offset ),
                *offset - start_offset );
            DEBUG_MSG( ( "dumpvSend", "  Opaque Double:\t%f\n", *doublep ) );
            return 1;
        }
    }

    return 0;
}
