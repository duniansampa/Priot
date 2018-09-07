#include "Scapi.h"
#include "Api.h"
#include "System/Security/Usm.h"
#include "System/Util/Logger.h"
#include "System/Util/Trace.h"
#include "System/Util/Utilities.h"
#include <openssl/aes.h>
#include <openssl/des.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

/** ============================[ Macros ============================ */

#define scapiQUITFUN( e, l )                 \
    if ( e != ErrorCode_SUCCESS ) {          \
        rval = ErrorCode_SC_GENERAL_FAILURE; \
        goto l;                              \
    }

/**
 * All functions devolve to the following block if we can't do cryptography
 */
#define scapiNOT_CONFIGURED                                                     \
    {                                                                           \
        Logger_log( LOGGER_PRIORITY_ERR, "Encryption support not enabled.\n" ); \
        DEBUG_MSGTL( ( "scapi", "SCAPI not configured" ) );                     \
        return ErrorCode_SC_NOT_CONFIGURED;                                     \
    }

/** =============================[ Public Functions ]================== */

int Scapi_getProperLength( const oid* hashType, u_int hashTypeLength )
{
    DEBUG_TRACE;
    /*
     * Determine transform type hash length.
     */
    if ( UTILITIES_ISTRANSFORM( hashType, hMACMD5Auth ) ) {
        return UTILITIES_BYTE_SIZE( scapiTRANS_AUTHLEN_HMACMD5 );
    } else if ( UTILITIES_ISTRANSFORM( hashType, hMACSHA1Auth ) ) {
        return UTILITIES_BYTE_SIZE( scapiTRANS_AUTHLEN_HMACSHA1 );
    }
    return ErrorCode_GENERR;
}

int Scapi_getProperPrivLength( const oid* privType, u_int privTypeLength )
{
    int properlength = 0;
    if ( UTILITIES_ISTRANSFORM( privType, dESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( scapiTRANS_PRIVLEN_1DES );
    }
    if ( UTILITIES_ISTRANSFORM( privType, aESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( scapiTRANS_PRIVLEN_AES );
    }
    return properlength;
}

int Scapi_init( void )
{
    int rval = ErrorCode_SUCCESS;

    return rval;
}

int Scapi_random( u_char* buffer, size_t* bufferLength )
{
    int rval = ErrorCode_SUCCESS;

    DEBUG_TRACE;

    RAND_bytes( buffer, *bufferLength ); /* will never fail */

    return rval;
}

int Scapi_generateKeyedHash( const oid* authType, size_t authTypeLength,
    const u_char* key, u_int keyLength, const u_char* message,
    u_int messageLength, u_char* mac, size_t* macLength )
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
    if ( !authType || !key || !message || !mac || !macLength
        || ( keyLength <= 0 ) || ( messageLength <= 0 ) || ( *macLength <= 0 )
        || ( authTypeLength != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_generateKeyedHashQuit );
    }

    iproperlength = Scapi_getProperLength( authType, authTypeLength );
    if ( iproperlength == ErrorCode_GENERR )
        return ErrorCode_GENERR;
    properlength = ( size_t )iproperlength;
    if ( keyLength < properlength ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_generateKeyedHashQuit );
    }
    /*
     * Determine transform type.
     */
    if ( UTILITIES_ISTRANSFORM( authType, hMACMD5Auth ) )
        HMAC( EVP_md5(), key, keyLength, message, messageLength, buf, &buf_len );
    else if ( UTILITIES_ISTRANSFORM( authType, hMACSHA1Auth ) )
        HMAC( EVP_sha1(), key, keyLength, message, messageLength, buf, &buf_len );
    else {
        scapiQUITFUN( ErrorCode_GENERR, goto_generateKeyedHashQuit );
    }
    if ( buf_len != properlength ) {
        scapiQUITFUN( rval, goto_generateKeyedHashQuit );
    }
    if ( *macLength > buf_len )
        *macLength = buf_len;
    memcpy( mac, buf, *macLength );

goto_generateKeyedHashQuit:
    memset( buf, 0, UTILITIES_MAX_BUFFER_SMALL );
    return rval;
}

int Scapi_hash( const oid* hashType, size_t hashTypeLength, const u_char* message,
    size_t messageLength, u_char* mac, size_t* macLength )
{
    int rval = ErrorCode_SUCCESS;
    unsigned int tmp_len;
    int ret;
    const EVP_MD* hashfn;
    EVP_MD_CTX* cptr;

    DEBUG_TRACE;

    if ( hashType == NULL || message == NULL || messageLength <= 0 || mac == NULL || macLength == NULL )
        return ( ErrorCode_GENERR );
    ret = Scapi_getProperLength( hashType, hashTypeLength );
    if ( ( ret < 0 ) || ( *macLength < ( size_t )ret ) )
        return ( ErrorCode_GENERR );

    /*
     * Determine transform type.
     */
    if ( UTILITIES_ISTRANSFORM( hashType, hMACMD5Auth ) ) {
        hashfn = ( const EVP_MD* )EVP_md5();
    } else if ( UTILITIES_ISTRANSFORM( hashType, hMACSHA1Auth ) ) {
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
    EVP_DigestUpdate( cptr, message, messageLength );

    /** do the final pass */
    EVP_DigestFinal( cptr, mac, &tmp_len );
    *macLength = tmp_len;
    EVP_MD_CTX_destroy( cptr );

    return ( rval );
}

int Scapi_checkKeyedHash( const oid* authType, size_t authTypelength,
    const u_char* key, u_int keyLength,
    const u_char* message, u_int messageLength,
    const u_char* mac, u_int macLength )
{
    int rval = ErrorCode_SUCCESS;
    size_t buf_len = UTILITIES_MAX_BUFFER_SMALL;

    u_char buf[ UTILITIES_MAX_BUFFER_SMALL ];

    DEBUG_TRACE;

    /*
     * Sanity check.
     */
    if ( !authType || !key || !message || !mac
        || ( keyLength <= 0 ) || ( messageLength <= 0 ) || ( macLength <= 0 )
        || ( authTypelength != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_checkKeyedHashQuit );
    }

    if ( macLength != USM_MD5_AND_SHA_AUTH_LEN ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_checkKeyedHashQuit );
    }

    /*
     * Generate a full hash of the message, then compare
     * the result with the given MAC which may shorter than
     * the full hash length.
     */
    rval = Scapi_generateKeyedHash( authType, authTypelength,
        key, keyLength, message, messageLength, buf, &buf_len );

    scapiQUITFUN( rval, goto_checkKeyedHashQuit );

    if ( macLength > messageLength ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_checkKeyedHashQuit );

    } else if ( memcmp( buf, mac, macLength ) != 0 ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_checkKeyedHashQuit );
    }

goto_checkKeyedHashQuit:
    memset( buf, 0, UTILITIES_MAX_BUFFER_SMALL );

    return rval;
}

int Scapi_encrypt( const oid* privType, size_t privTypeLength,
    u_char* key, u_int keyLength,
    u_char* iv, u_int ivLength,
    const u_char* plainText, u_int plainTextLength,
    u_char* cipherText, size_t* cipherTextLength )
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

    if ( !privType || !key || !iv || !plainText || !cipherText || !cipherTextLength
        || ( keyLength <= 0 ) || ( ivLength <= 0 ) || ( plainTextLength <= 0 ) || ( *cipherTextLength <= 0 )
        || ( privTypeLength != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_scEncryptQuit );
    } else if ( plainTextLength > *cipherTextLength ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_scEncryptQuit );
    }

    /*
     * Determine privacy transform.
     */
    have_trans = 0;
    if ( UTILITIES_ISTRANSFORM( privType, dESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( scapiTRANS_PRIVLEN_1DES );
        properlength_iv = UTILITIES_BYTE_SIZE( scapiTRANS_PRIVLEN_1DES_IV );
        pad_size = properlength;
        have_trans = 1;
    }
    if ( UTILITIES_ISTRANSFORM( privType, aESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( scapiTRANS_PRIVLEN_AES );
        properlength_iv = UTILITIES_BYTE_SIZE( scapiTRANS_PRIVLEN_AES_IV );
        have_trans = 1;
    }
    if ( !have_trans ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_scEncryptQuit );
    }

    if ( ( keyLength < properlength ) || ( ivLength < properlength_iv ) ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_scEncryptQuit );
    }

    memset( my_iv, 0, sizeof( my_iv ) );

    if ( UTILITIES_ISTRANSFORM( privType, dESPriv ) ) {

        /*
         * now calculate the padding needed
         */
        pad = pad_size - ( plainTextLength % pad_size );
        plast = ( int )plainTextLength - ( pad_size - pad );
        if ( pad == pad_size )
            pad = 0;
        if ( plainTextLength + pad > *cipherTextLength ) {
            scapiQUITFUN( ErrorCode_GENERR, goto_scEncryptQuit ); /* not enough space */
        }
        if ( pad > 0 ) { /* copy data into pad block if needed */
            memcpy( pad_block, plainText + plast, pad_size - pad );
            memset( &pad_block[ pad_size - pad ], pad, pad ); /* filling in padblock */
        }

        memcpy( key_struct, key, sizeof( key_struct ) );
        ( void )DES_key_sched( &key_struct, key_sch );

        memcpy( my_iv, iv, ivLength );
        /*
         * encrypt the data
         */
        DES_ncbc_encrypt( plainText, cipherText, plast, key_sch,
            ( DES_cblock* )my_iv, DES_ENCRYPT );
        if ( pad > 0 ) {
            /*
             * then encrypt the pad block
             */
            DES_ncbc_encrypt( pad_block, cipherText + plast, pad_size,
                key_sch, ( DES_cblock* )my_iv, DES_ENCRYPT );
            *cipherTextLength = plast + pad_size;
        } else {
            *cipherTextLength = plast;
        }
    }
    if ( UTILITIES_ISTRANSFORM( privType, aESPriv ) ) {
        ( void )AES_set_encrypt_key( key, properlength * 8, &aes_key );

        memcpy( my_iv, iv, ivLength );
        /*
         * encrypt the data
         */
        AES_cfb128_encrypt( plainText, cipherText, plainTextLength,
            &aes_key, my_iv, &new_ivlen, AES_ENCRYPT );
        *cipherTextLength = plainTextLength;
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
}

int Scapi_decrypt( const oid* privType, size_t privTypeLength,
    u_char* key, u_int keyLength,
    u_char* iv, u_int ivLength,
    u_char* cipherText, u_int cipherTextLength,
    u_char* plainText, size_t* plainTextLength )
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

    if ( !privType || !key || !iv || !plainText || !cipherText || !plainTextLength
        || ( cipherTextLength <= 0 ) || ( *plainTextLength <= 0 ) || ( *plainTextLength < cipherTextLength )
        || ( privTypeLength != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_scDecryptQuit );
    }

    /*
     * Determine privacy transform.
     */
    have_transform = 0;
    if ( UTILITIES_ISTRANSFORM( privType, dESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( scapiTRANS_PRIVLEN_1DES );
        properlength_iv = UTILITIES_BYTE_SIZE( scapiTRANS_PRIVLEN_1DES_IV );
        have_transform = 1;
    }
    if ( UTILITIES_ISTRANSFORM( privType, aESPriv ) ) {
        properlength = UTILITIES_BYTE_SIZE( scapiTRANS_PRIVLEN_AES );
        properlength_iv = UTILITIES_BYTE_SIZE( scapiTRANS_PRIVLEN_AES_IV );
        have_transform = 1;
    }
    if ( !have_transform ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_scDecryptQuit );
    }

    if ( ( keyLength < properlength ) || ( ivLength < properlength_iv ) ) {
        scapiQUITFUN( ErrorCode_GENERR, goto_scDecryptQuit );
    }

    memset( my_iv, 0, sizeof( my_iv ) );
    if ( UTILITIES_ISTRANSFORM( privType, dESPriv ) ) {
        memcpy( key_struct, key, sizeof( key_struct ) );
        ( void )DES_key_sched( &key_struct, key_sch );

        memcpy( my_iv, iv, ivLength );
        DES_cbc_encrypt( cipherText, plainText, cipherTextLength, key_sch,
            ( DES_cblock* )my_iv, DES_DECRYPT );
        *plainTextLength = cipherTextLength;
    }
    if ( UTILITIES_ISTRANSFORM( privType, aESPriv ) ) {
        ( void )AES_set_encrypt_key( key, properlength * 8, &aes_key );

        memcpy( my_iv, iv, ivLength );
        /*
         * encrypt the data
         */
        AES_cfb128_encrypt( cipherText, plainText, cipherTextLength,
            &aes_key, my_iv, &new_ivlen, AES_DECRYPT );
        *plainTextLength = cipherTextLength;
    }

/**
 * exit cond
 */
goto_scDecryptQuit:

    memset( &key_sched_store, 0, sizeof( key_sched_store ) );
    memset( key_struct, 0, sizeof( key_struct ) );
    memset( my_iv, 0, sizeof( my_iv ) );
    return rval;
}
