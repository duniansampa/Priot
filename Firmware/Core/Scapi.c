#include "Scapi.h"
#include "Api.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"
#include "Usm.h"
#include <openssl/aes.h>
#include <openssl/des.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#define SCAPI_QUITFUN( e, l )                \
    if ( e != ErrorCode_SUCCESS ) {          \
        rval = ErrorCode_SC_GENERAL_FAILURE; \
        goto l;                              \
    }

/*
 * All functions devolve to the following block if we can't do cryptography
 */
#define SCAPI_NOT_CONFIGURED                                                    \
    {                                                                           \
        Logger_log( LOGGER_PRIORITY_ERR, "Encryption support not enabled.\n" ); \
        DEBUG_MSGTL( ( "scapi", "SCAPI not configured" ) );                     \
        return ErrorCode_SC_NOT_CONFIGURED;                                     \
    }

/*
 * Scapi_getProperLength(oid *hashtype, u_int hashtype_len):
 *
 * Given a hashing type ("hashtype" and its length hashtype_len), return
 * the length of the hash result.
 *
 * Returns either the length or ErrorCode_GENERR for an unknown hashing type.
 */
int Scapi_getProperLength( const oid* hashtype, u_int hashtype_len )
{
    DEBUG_TRACE;
    /*
     * Determine transform type hash length.
     */
    if ( UTILITIES_ISTRANSFORM( hashtype, hMACMD5Auth ) ) {
        return UTILITIES_BYTE_SIZE( SCAPI_TRANS_AUTHLEN_HMACMD5 );
    } else if ( UTILITIES_ISTRANSFORM( hashtype, hMACSHA1Auth ) ) {
        return UTILITIES_BYTE_SIZE( SCAPI_TRANS_AUTHLEN_HMACSHA1 );
    }
    return ErrorCode_GENERR;
}

int Scapi_getProperPrivLength( const oid* privtype, u_int privtype_len )
{
    int properlength = 0;
    if ( UTILITIES_ISTRANSFORM( privtype, dESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( SCAPI_TRANS_PRIVLEN_1DES );
    }
    if ( UTILITIES_ISTRANSFORM( privtype, aESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( SCAPI_TRANS_PRIVLEN_AES );
    }
    return properlength;
}

/*******************************************************************-o-******
 * sc_init
 *
 * Returns:
 *	ErrorCode_SUCCESS			Success.
 */
int Scapi_init( void )
{
    int rval = ErrorCode_SUCCESS;

    return rval;
} /* end sc_init() */

/*******************************************************************-o-******
 * sc_random
 *
 * Parameters:
 *	*buf		Pre-allocated buffer.
 *	*buflen 	Size of buffer.
 *
 * Returns:
 *	ErrorCode_SUCCESS			Success.
 */
int Scapi_random( u_char* buf, size_t* buflen )
{
    int rval = ErrorCode_SUCCESS;

    DEBUG_TRACE;

    RAND_bytes( buf, *buflen ); /* will never fail */

    return rval;

} /* end sc_random() */

/*******************************************************************-o-******
 * Scapi_generateKeyedHash
 *
 * Parameters:
 *	 authtype	Type of authentication transform.
 *	 authtypelen
 *	*key		Pointer to key (Kul) to use in keyed hash.
 *	 keylen		Length of key in bytes.
 *	*message	Pointer to the message to hash.
 *	 msglen		Length of the message.
 *	*MAC		Will be returned with allocated bytes containg hash.
 *	*maclen		Length of the hash buffer in bytes; also indicates
 *				whether the MAC should be truncated.
 *
 * Returns:
 *	ErrorCode_SUCCESS			Success.
 *	ErrorCode_GENERR			All errs
 *
 *
 * A hash of the first msglen bytes of message using a keyed hash defined
 * by authtype is created and stored in MAC.  MAC is ASSUMED to be a buffer
 * of at least maclen bytes.  If the length of the hash is greater than
 * maclen, it is truncated to fit the buffer.  If the length of the hash is
 * less than maclen, maclen set to the number of hash bytes generated.
 *
 * ASSUMED that the number of hash bits is a multiple of 8.
 */
int Scapi_generateKeyedHash( const oid* authtype, size_t authtypelen,
    const u_char* key, u_int keylen,
    const u_char* message, u_int msglen,
    u_char* MAC, size_t* maclen )
{
    int rval = ErrorCode_SUCCESS;
    int iproperlength;
    size_t properlength;

    u_char buf[ UTILITIES_MAX_BUFFER_SMALL ];
    unsigned int buf_len = sizeof( buf );

    DEBUG_TRACE;

    /*
     * Sanity check.
     */
    if ( !authtype || !key || !message || !MAC || !maclen
        || ( keylen <= 0 ) || ( msglen <= 0 ) || ( *maclen <= 0 )
        || ( authtypelen != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_generateKeyedHashQuit );
    }

    iproperlength = Scapi_getProperLength( authtype, authtypelen );
    if ( iproperlength == ErrorCode_GENERR )
        return ErrorCode_GENERR;
    properlength = ( size_t )iproperlength;
    if ( keylen < properlength ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_generateKeyedHashQuit );
    }
    /*
     * Determine transform type.
     */
    if ( UTILITIES_ISTRANSFORM( authtype, hMACMD5Auth ) )
        HMAC( EVP_md5(), key, keylen, message, msglen, buf, &buf_len );
    else if ( UTILITIES_ISTRANSFORM( authtype, hMACSHA1Auth ) )
        HMAC( EVP_sha1(), key, keylen, message, msglen, buf, &buf_len );
    else {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_generateKeyedHashQuit );
    }
    if ( buf_len != properlength ) {
        SCAPI_QUITFUN( rval, goto_generateKeyedHashQuit );
    }
    if ( *maclen > buf_len )
        *maclen = buf_len;
    memcpy( MAC, buf, *maclen );

goto_generateKeyedHashQuit:
    memset( buf, 0, UTILITIES_MAX_BUFFER_SMALL );
    return rval;
} /* end Scapi_generateKeyedHash() */

/*
 * sc_hash(): a generic wrapper around whatever hashing package we are using.
 *
 * IN:
 * hashtype    - oid pointer to a hash type
 * hashtypelen - length of oid pointer
 * buf         - u_char buffer to be hashed
 * buf_len     - integer length of buf data
 * MAC_len     - length of the passed MAC buffer size.
 *
 * OUT:
 * MAC         - pre-malloced space to store hash output.
 * MAC_len     - length of MAC output to the MAC buffer.
 *
 * Returns:
 * ErrorCode_SUCCESS              Success.
 * SNMP_SC_GENERAL_FAILURE      Any error.
 * ErrorCode_SC_NOT_CONFIGURED    Hash type not supported.
 */
int Scapi_hash( const oid* hashtype, size_t hashtypelen, const u_char* buf,
    size_t buf_len, u_char* MAC, size_t* MAC_len )
{
    int rval = ErrorCode_SUCCESS;
    unsigned int tmp_len;
    int ret;
    const EVP_MD* hashfn;
    EVP_MD_CTX* cptr;

    DEBUG_TRACE;

    if ( hashtype == NULL || buf == NULL || buf_len <= 0 || MAC == NULL || MAC_len == NULL )
        return ( ErrorCode_GENERR );
    ret = Scapi_getProperLength( hashtype, hashtypelen );
    if ( ( ret < 0 ) || ( *MAC_len < ( size_t )ret ) )
        return ( ErrorCode_GENERR );

    /*
     * Determine transform type.
     */
    if ( UTILITIES_ISTRANSFORM( hashtype, hMACMD5Auth ) ) {
        hashfn = ( const EVP_MD* )EVP_md5();
    } else if ( UTILITIES_ISTRANSFORM( hashtype, hMACSHA1Auth ) ) {
        hashfn = ( const EVP_MD* )EVP_sha1();
    } else {
        return ( ErrorCode_GENERR );
    }

    /** initialize the pointer */
    cptr = EVP_MD_CTX_create();

    if ( !EVP_DigestInit( cptr, hashfn ) ) {
        /* requested hash function is not available */
        return ErrorCode_SC_NOT_CONFIGURED;
    }

    /** pass the data */
    EVP_DigestUpdate( cptr, buf, buf_len );

    /** do the final pass */
    EVP_DigestFinal( cptr, MAC, &tmp_len );
    *MAC_len = tmp_len;
    EVP_MD_CTX_destroy( cptr );

    return ( rval );
}

/*******************************************************************-o-******
 * sc_check_keyed_hash
 *
 * Parameters:
 *	 authtype	Transform type of authentication hash.
 *	*key		Key bits in a string of bytes.
 *	 keylen		Length of key in bytes.
 *	*message	Message for which to check the hash.
 *	 msglen		Length of message.
 *	*MAC		Given hash.
 *	 maclen		Length of given hash; indicates truncation if it is
 *				shorter than the normal size of output for
 *				given hash transform.
 * Returns:
 *	ErrorCode_SUCCESS		Success.
 *	SNMP_SC_GENERAL_FAILURE	Any error
 *
 *
 * Check the hash given in MAC against the hash of message.  If the length
 * of MAC is less than the length of the transform hash output, only maclen
 * bytes are compared.  The length of MAC cannot be greater than the
 * length of the hash transform output.
 */
int Scapi_checkKeyedHash( const oid* authtype, size_t authtypelen,
    const u_char* key, u_int keylen,
    const u_char* message, u_int msglen,
    const u_char* MAC, u_int maclen )
{
    int rval = ErrorCode_SUCCESS;
    size_t buf_len = UTILITIES_MAX_BUFFER_SMALL;

    u_char buf[ UTILITIES_MAX_BUFFER_SMALL ];

    DEBUG_TRACE;

    /*
     * Sanity check.
     */
    if ( !authtype || !key || !message || !MAC
        || ( keylen <= 0 ) || ( msglen <= 0 ) || ( maclen <= 0 )
        || ( authtypelen != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_checkKeyedHashQuit );
    }

    if ( maclen != USM_MD5_AND_SHA_AUTH_LEN ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_checkKeyedHashQuit );
    }

    /*
     * Generate a full hash of the message, then compare
     * the result with the given MAC which may shorter than
     * the full hash length.
     */
    rval = Scapi_generateKeyedHash( authtype, authtypelen,
        key, keylen,
        message, msglen, buf, &buf_len );
    SCAPI_QUITFUN( rval, goto_checkKeyedHashQuit );

    if ( maclen > msglen ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_checkKeyedHashQuit );

    } else if ( memcmp( buf, MAC, maclen ) != 0 ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_checkKeyedHashQuit );
    }

goto_checkKeyedHashQuit:
    memset( buf, 0, UTILITIES_MAX_BUFFER_SMALL );

    return rval;

} /* end sc_check_keyed_hash() */

/*******************************************************************-o-******
 * sc_encrypt
 *
 * Parameters:
 *	 privtype	Type of privacy cryptographic transform.
 *	*key		Key bits for crypting.
 *	 keylen		Length of key (buffer) in bytes.
 *	*iv		IV bits for crypting.
 *	 ivlen		Length of iv (buffer) in bytes.
 *	*plaintext	Plaintext to crypt.
 *	 ptlen		Length of plaintext.
 *	*ciphertext	Ciphertext to crypt.
 *	*ctlen		Length of ciphertext.
 *
 * Returns:
 *	ErrorCode_SUCCESS			Success.
 *	ErrorCode_SC_NOT_CONFIGURED	Encryption is not supported.
 *	ErrorCode_SC_GENERAL_FAILURE	Any other error
 *
 *
 * Encrypt plaintext into ciphertext using key and iv.
 *
 * ctlen contains actual number of crypted bytes in ciphertext upon
 * successful return.
 */
int Scapi_encrypt( const oid* privtype, size_t privtypelen,
    u_char* key, u_int keylen,
    u_char* iv, u_int ivlen,
    const u_char* plaintext, u_int ptlen,
    u_char* ciphertext, size_t* ctlen )
{
    int rval = ErrorCode_SUCCESS;
    u_int properlength = 0, properlength_iv = 0;
    u_char pad_block[ 128 ]; /* bigger than anything I need */
    u_char my_iv[ 128 ]; /* ditto */
    int pad, plast, pad_size = 0;
    int have_trans;

    DES_key_schedule key_sched_store;
    DES_key_schedule* key_sch = &key_sched_store;

    DES_cblock key_struct;

    AES_KEY aes_key;
    int new_ivlen = 0;

    DEBUG_TRACE;

    /*
     * Sanity check.
     */

    if ( !privtype || !key || !iv || !plaintext || !ciphertext || !ctlen
        || ( keylen <= 0 ) || ( ivlen <= 0 ) || ( ptlen <= 0 ) || ( *ctlen <= 0 )
        || ( privtypelen != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_scEncryptQuit );
    } else if ( ptlen > *ctlen ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_scEncryptQuit );
    }

    /*
     * Determine privacy transform.
     */
    have_trans = 0;
    if ( UTILITIES_ISTRANSFORM( privtype, dESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( SCAPI_TRANS_PRIVLEN_1DES );
        properlength_iv = UTILITIES_BYTE_SIZE( SCAPI_TRANS_PRIVLEN_1DES_IV );
        pad_size = properlength;
        have_trans = 1;
    }
    if ( UTILITIES_ISTRANSFORM( privtype, aESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( SCAPI_TRANS_PRIVLEN_AES );
        properlength_iv = UTILITIES_BYTE_SIZE( SCAPI_TRANS_PRIVLEN_AES_IV );
        have_trans = 1;
    }
    if ( !have_trans ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_scEncryptQuit );
    }

    if ( ( keylen < properlength ) || ( ivlen < properlength_iv ) ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_scEncryptQuit );
    }

    memset( my_iv, 0, sizeof( my_iv ) );

    if ( UTILITIES_ISTRANSFORM( privtype, dESPriv ) ) {

        /*
         * now calculate the padding needed
         */
        pad = pad_size - ( ptlen % pad_size );
        plast = ( int )ptlen - ( pad_size - pad );
        if ( pad == pad_size )
            pad = 0;
        if ( ptlen + pad > *ctlen ) {
            SCAPI_QUITFUN( ErrorCode_GENERR, goto_scEncryptQuit ); /* not enough space */
        }
        if ( pad > 0 ) { /* copy data into pad block if needed */
            memcpy( pad_block, plaintext + plast, pad_size - pad );
            memset( &pad_block[ pad_size - pad ], pad, pad ); /* filling in padblock */
        }

        memcpy( key_struct, key, sizeof( key_struct ) );
        ( void )DES_key_sched( &key_struct, key_sch );

        memcpy( my_iv, iv, ivlen );
        /*
         * encrypt the data
         */
        DES_ncbc_encrypt( plaintext, ciphertext, plast, key_sch,
            ( DES_cblock* )my_iv, DES_ENCRYPT );
        if ( pad > 0 ) {
            /*
             * then encrypt the pad block
             */
            DES_ncbc_encrypt( pad_block, ciphertext + plast, pad_size,
                key_sch, ( DES_cblock* )my_iv, DES_ENCRYPT );
            *ctlen = plast + pad_size;
        } else {
            *ctlen = plast;
        }
    }
    if ( UTILITIES_ISTRANSFORM( privtype, aESPriv ) ) {
        ( void )AES_set_encrypt_key( key, properlength * 8, &aes_key );

        memcpy( my_iv, iv, ivlen );
        /*
         * encrypt the data
         */
        AES_cfb128_encrypt( plaintext, ciphertext, ptlen,
            &aes_key, my_iv, &new_ivlen, AES_ENCRYPT );
        *ctlen = ptlen;
    }
goto_scEncryptQuit:
    /*
     * clear memory just in case
     */
    memset( my_iv, 0, sizeof( my_iv ) );
    memset( pad_block, 0, sizeof( pad_block ) );
    memset( key_struct, 0, sizeof( key_struct ) );

    memset( &key_sched_store, 0, sizeof( key_sched_store ) );
    memset( &aes_key, 0, sizeof( aes_key ) );
    return rval;

} /* end sc_encrypt() */

/*******************************************************************-o-******
 * sc_decrypt
 *
 * Parameters:
 *	 privtype
 *	*key
 *	 keylen
 *	*iv
 *	 ivlen
 *	*ciphertext
 *	 ctlen
 *	*plaintext
 *	*ptlen
 *
 * Returns:
 *	ErrorCode_SUCCESS			Success.
 *	ErrorCode_SC_NOT_CONFIGURED	Encryption is not supported.
 *      ErrorCode_SC_GENERAL_FAILURE      Any other error
 *
 *
 * Decrypt ciphertext into plaintext using key and iv.
 *
 * ptlen contains actual number of plaintext bytes in plaintext upon
 * successful return.
 */
int Scapi_decrypt( const oid* privtype, size_t privtypelen,
    u_char* key, u_int keylen,
    u_char* iv, u_int ivlen,
    u_char* ciphertext, u_int ctlen,
    u_char* plaintext, size_t* ptlen )
{

    int rval = ErrorCode_SUCCESS;
    u_char my_iv[ 128 ];

    DES_key_schedule key_sched_store;
    DES_key_schedule* key_sch = &key_sched_store;
    DES_cblock key_struct;
    u_int properlength = 0, properlength_iv = 0;
    int have_transform;
    int new_ivlen = 0;
    AES_KEY aes_key;

    DEBUG_TRACE;

    if ( !privtype || !key || !iv || !plaintext || !ciphertext || !ptlen
        || ( ctlen <= 0 ) || ( *ptlen <= 0 ) || ( *ptlen < ctlen )
        || ( privtypelen != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_scDecryptQuit );
    }

    /*
     * Determine privacy transform.
     */
    have_transform = 0;
    if ( UTILITIES_ISTRANSFORM( privtype, dESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( SCAPI_TRANS_PRIVLEN_1DES );
        properlength_iv = UTILITIES_BYTE_SIZE( SCAPI_TRANS_PRIVLEN_1DES_IV );
        have_transform = 1;
    }
    if ( UTILITIES_ISTRANSFORM( privtype, aESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( SCAPI_TRANS_PRIVLEN_AES );
        properlength_iv = UTILITIES_BYTE_SIZE( SCAPI_TRANS_PRIVLEN_AES_IV );
        have_transform = 1;
    }
    if ( !have_transform ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_scDecryptQuit );
    }

    if ( ( keylen < properlength ) || ( ivlen < properlength_iv ) ) {
        SCAPI_QUITFUN( ErrorCode_GENERR, goto_scDecryptQuit );
    }

    memset( my_iv, 0, sizeof( my_iv ) );
    if ( UTILITIES_ISTRANSFORM( privtype, dESPriv ) ) {
        memcpy( key_struct, key, sizeof( key_struct ) );
        ( void )DES_key_sched( &key_struct, key_sch );

        memcpy( my_iv, iv, ivlen );
        DES_cbc_encrypt( ciphertext, plaintext, ctlen, key_sch,
            ( DES_cblock* )my_iv, DES_DECRYPT );
        *ptlen = ctlen;
    }
    if ( UTILITIES_ISTRANSFORM( privtype, aESPriv ) ) {
        ( void )AES_set_encrypt_key( key, properlength * 8, &aes_key );

        memcpy( my_iv, iv, ivlen );
        /*
         * encrypt the data
         */
        AES_cfb128_encrypt( ciphertext, plaintext, ctlen,
            &aes_key, my_iv, &new_ivlen, AES_DECRYPT );
        *ptlen = ctlen;
    }

/*
     * exit cond
     */
goto_scDecryptQuit:

    memset( &key_sched_store, 0, sizeof( key_sched_store ) );
    memset( key_struct, 0, sizeof( key_struct ) );
    memset( my_iv, 0, sizeof( my_iv ) );
    return rval;
} /* USE OPEN_SSL */
