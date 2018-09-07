#include "KeyTools.h"
#include "Api.h"
#include "Scapi.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"
#include "Types.h"
#include "Usm.h"
#include <netinet/in.h>
#include <openssl/hmac.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int KeyTools_generateKu( const oid* hashType, u_int hashTypeLength, const u_char* password,
    size_t passwordLength, u_char* ku, size_t* kuLength )
{
    int rval = ErrorCode_SUCCESS,
        nbytes = keyUSM_LENGTH_EXPANDED_PASSPHRASE;

    u_int i, pindex = 0;

    u_char buf[ keyUSM_LENGTH_KU_HASHBLOCK ], *bufp;

    EVP_MD_CTX* ctx = NULL;

    /*
     * Sanity check.
     */
    if ( !hashType || !password || !ku || !kuLength || ( *kuLength <= 0 )
        || ( hashTypeLength != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_generateKuQuit );
    }

    if ( passwordLength < keyUSM_LENGTH_P_MIN ) {
        Logger_log( LOGGER_PRIORITY_ERR, "Error: passphrase chosen is below the length "
                                         "requirements of the USM (min=%d).\n",
            keyUSM_LENGTH_P_MIN );
        Api_setDetail( "The supplied password length is too short." );
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_generateKuQuit );
    }

    /*
     * Setup for the transform type.
     */
    ctx = EVP_MD_CTX_create();

    if ( UTILITIES_ISTRANSFORM( hashType, hMACMD5Auth ) ) {
        if ( !EVP_DigestInit( ctx, EVP_md5() ) )
            return ErrorCode_GENERR;
    } else if ( UTILITIES_ISTRANSFORM( hashType, hMACSHA1Auth ) ) {
        if ( !EVP_DigestInit( ctx, EVP_sha1() ) )
            return ErrorCode_GENERR;
    } else
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_generateKuQuit );

    while ( nbytes > 0 ) {
        bufp = buf;
        for ( i = 0; i < keyUSM_LENGTH_KU_HASHBLOCK; i++ ) {
            *bufp++ = password[ pindex++ % passwordLength ];
        }
        EVP_DigestUpdate( ctx, buf, keyUSM_LENGTH_KU_HASHBLOCK );
        nbytes -= keyUSM_LENGTH_KU_HASHBLOCK;
    }

    {
        unsigned int tmp_len;

        tmp_len = *kuLength;
        EVP_DigestFinal( ctx, ( unsigned char* )ku, &tmp_len );
        *kuLength = tmp_len;
        /*
     * what about free()
     */
    }

goto_generateKuQuit:
    memset( buf, 0, sizeof( buf ) );

    if ( ctx ) {
        EVP_MD_CTX_destroy( ctx );
    }

    return rval;
}

int KeyTools_generateKul( const oid* hashType, u_int hashTypeLength, const u_char* engineId,
    size_t engineIdLength, const u_char* ku, size_t kuLength, u_char* kul, size_t* kulLength )
{

    int rval = ErrorCode_SUCCESS;
    u_int nbytes = 0;
    size_t properlength;
    int iproperlength;

    u_char buf[ UTILITIES_MAX_BUFFER ];

    /*
     * Sanity check.
     */
    if ( !hashType || !engineId || !ku || !kul || !kulLength
        || ( engineIdLength <= 0 ) || ( kuLength <= 0 ) || ( *kulLength <= 0 )
        || ( hashTypeLength != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_generateKulQuit );
    }

    iproperlength = Scapi_getProperLength( hashType, hashTypeLength );
    if ( iproperlength == ErrorCode_GENERR )
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_generateKulQuit );

    properlength = ( size_t )iproperlength;

    if ( ( *kulLength < properlength ) || ( kuLength < properlength ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_generateKulQuit );
    }

    /*
     * Concatenate Ku and engineID properly, then hash the result.
     * Store it in Kul.
     */
    nbytes = 0;
    memcpy( buf, ku, properlength );
    nbytes += properlength;
    memcpy( buf + nbytes, engineId, engineIdLength );
    nbytes += engineIdLength;
    memcpy( buf + nbytes, ku, properlength );
    nbytes += properlength;

    rval = Scapi_hash( hashType, hashTypeLength, buf, nbytes, kul, kulLength );

    UTILITIES_QUIT_FUN( rval, goto_generateKulQuit );

goto_generateKulQuit:
    return rval;
}

int KeyTools_encodeKeyChange( const oid* hashType, u_int hashTypeLength, u_char* oldKey,
    size_t oldKeyLength, u_char* newKey, size_t newKeyLength, u_char* kcString,
    size_t* kcStringLength )
{
    int rval = ErrorCode_SUCCESS;
    int iproperlength;
    size_t properlength;
    size_t nbytes = 0;

    u_char* tmpbuf = NULL;

    /*
         * Sanity check.
         */
    if ( !kcString || !kcStringLength )
        return ErrorCode_GENERR;

    if ( !hashType || !oldKey || !newKey || !kcString || !kcStringLength
        || ( oldKeyLength <= 0 ) || ( newKeyLength <= 0 ) || ( *kcStringLength <= 0 )
        || ( hashTypeLength != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_encodeKeychangeQuit );
    }

    /*
         * Setup for the transform type.
         */
    iproperlength = Scapi_getProperLength( hashType, hashTypeLength );
    if ( iproperlength == ErrorCode_GENERR )
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_encodeKeychangeQuit );

    if ( ( oldKeyLength != newKeyLength ) || ( *kcStringLength < ( 2 * oldKeyLength ) ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_encodeKeychangeQuit );
    }

    properlength = UTILITIES_MIN_VALUE( oldKeyLength, ( size_t )iproperlength );

    /**
     * Use the old key and some random bytes to encode the new key
     * in the KeyChange TC format:
     *      . Get random bytes (store in first half of kcstring),
     *      . Hash (oldkey | random_bytes) (into second half of kcstring),
     *      . XOR hash and newkey (into second half of kcstring).
     *
     * Getting the wrong number of random bytes is considered an error.
     */
    nbytes = properlength;

    rval = Scapi_random( kcString, &nbytes );
    UTILITIES_QUIT_FUN( rval, goto_encodeKeychangeQuit );
    if ( nbytes != properlength ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_encodeKeychangeQuit );
    }

    tmpbuf = ( u_char* )Memory_malloc( properlength * 2 );
    if ( tmpbuf ) {
        memcpy( tmpbuf, oldKey, properlength );
        memcpy( tmpbuf + properlength, kcString, properlength );

        *kcStringLength -= properlength;
        rval = Scapi_hash( hashType, hashTypeLength, tmpbuf, properlength * 2,
            kcString + properlength, kcStringLength );

        UTILITIES_QUIT_FUN( rval, goto_encodeKeychangeQuit );

        *kcStringLength = ( properlength * 2 );

        kcString += properlength;
        nbytes = 0;
        while ( ( nbytes++ ) < properlength ) {
            *kcString++ ^= *newKey++;
        }
    }

goto_encodeKeychangeQuit:
    if ( rval != ErrorCode_SUCCESS )
        memset( kcString, 0, *kcStringLength );
    MEMORY_FREE( tmpbuf );

    return rval;
}

int KeyTools_decodeKeyChange( const oid* hashType, u_int hashTypeLength, u_char* oldKey, size_t oldKeyLength,
    u_char* kcString, size_t kcStringLength, u_char* newKey, size_t* newKeyLength )
{

    int rval = ErrorCode_SUCCESS;
    size_t properlength = 0;
    int iproperlength = 0;
    u_int nbytes = 0;

    u_char *bufp, tmp_buf[ UTILITIES_MAX_BUFFER ];
    size_t tmp_buf_len = UTILITIES_MAX_BUFFER;
    u_char* tmpbuf = NULL;

    /*
     * Sanity check.
     */
    if ( !hashType || !oldKey || !kcString || !newKey || !newKeyLength
        || ( oldKeyLength <= 0 ) || ( kcStringLength <= 0 ) || ( *newKeyLength <= 0 )
        || ( hashTypeLength != UTILITIES_USM_LENGTH_OID_TRANSFORM ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_decodeKeychangeQuit );
    }

    /*
     * Setup for the transform type.
     */
    iproperlength = Scapi_getProperLength( hashType, hashTypeLength );
    if ( iproperlength == ErrorCode_GENERR )
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_decodeKeychangeQuit );

    properlength = ( size_t )iproperlength;

    if ( ( ( oldKeyLength * 2 ) != kcStringLength ) || ( *newKeyLength < oldKeyLength ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_decodeKeychangeQuit );
    }

    properlength = oldKeyLength;
    *newKeyLength = properlength;

    /*
     * Use the old key and the given KeyChange TC string to recover
     * the new key:
     *      . Hash (oldkey | random_bytes) (into newkey),
     *      . XOR hash and encoded (second) half of kcstring (into newkey).
     */
    tmpbuf = ( u_char* )malloc( properlength * 2 );
    if ( tmpbuf ) {
        memcpy( tmpbuf, oldKey, properlength );
        memcpy( tmpbuf + properlength, kcString, properlength );

        rval = Scapi_hash( hashType, hashTypeLength, tmpbuf, properlength * 2,
            tmp_buf, &tmp_buf_len );
        UTILITIES_QUIT_FUN( rval, goto_decodeKeychangeQuit );

        memcpy( newKey, tmp_buf, properlength );
        bufp = kcString + properlength;
        nbytes = 0;
        while ( ( nbytes++ ) < properlength ) {
            *newKey++ ^= *bufp++;
        }
    }

goto_decodeKeychangeQuit:
    if ( rval != ErrorCode_SUCCESS ) {
        if ( newKey )
            memset( newKey, 0, properlength );
    }
    memset( tmp_buf, 0, UTILITIES_MAX_BUFFER );
    MEMORY_FREE( tmpbuf );

    return rval;
}
