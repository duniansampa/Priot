#include "Usm.h"
#include "Api.h"
#include "Client.h"
#include "KeyTools.h"
#include "Engine.h"
#include "Priot.h"
#include "ReadConfig.h"
#include "Scapi.h"
#include "SecMod.h"
#include "System/Util/Trace.h"
#include "System/Util/DefaultStore.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"
#include "TextualConvention.h"
#include "V3.h"

/*
 * Usm.c
 *
 * Routines to manipulate a information about a "user" as
 * defined by the SNMP-USER-BASED-SM-MIB MIB.
 *
 * All functions usm_set_usmStateReference_*() return 0 on success, -1
 * otherwise.
 *
 * !! Tab stops set to 4 in some parts of this file. !!
 *    (Designated on a per function.)
 */

oid usm_noAuthProtocol[ 10 ] = { 1, 3, 6, 1, 6, 3, 10, 1, 1, 1 };
oid usm_hMACMD5AuthProtocol[ 10 ] = { 1, 3, 6, 1, 6, 3, 10, 1, 1, 2 };
oid usm_hMACSHA1AuthProtocol[ 10 ] = { 1, 3, 6, 1, 6, 3, 10, 1, 1, 3 };
oid usm_noPrivProtocol[ 10 ] = { 1, 3, 6, 1, 6, 3, 10, 1, 2, 1 };
oid usm_dESPrivProtocol[ 10 ] = { 1, 3, 6, 1, 6, 3, 10, 1, 2, 2 };
oid usm_aESPrivProtocol[ 10 ] = { 1, 3, 6, 1, 6, 3, 10, 1, 2, 4 };
/* backwards compat */
oid* usm_aES128PrivProtocol = usm_aESPrivProtocol;

static u_int _usm_dummyEtime, _usm_dummyEboot; /* For LCDTIME_ISENGINEKNOWN(). */

/*
 * Set up default snmpv3 parameter value storage.
 */
static const oid* _usm_defaultAuthType = NULL;
static size_t _usm_defaultAuthTypeLen = 0;
static const oid* _usm_defaultPrivType = NULL;
static size_t _usm_defaultPrivTypeLen = 0;

/*
 * Globals.
 */
static u_int _usm_saltInteger;
static u_int _usm_saltInteger64One, _usm_saltInteger64Two;
/*
         * 1/2 of seed for the salt.   Cf. RFC2274, Sect 8.1.1.1.
         */

static struct Usm_User_s* _usm_noNameUser = NULL;
/*
 * Local storage (LCD) of the default user list.
 */
static struct Usm_User_s* _usm_userList = NULL;

/*
 * Prototypes
 */
int Usm_checkSecLevelVsProtocols( int level,
    const oid* authProtocol,
    u_int authProtocolLen,
    const oid* privProtocol,
    u_int privProtocolLen );
int Usm_calcOffsets( size_t globalDataLen,
    int secLevel, size_t secEngineIDLen,
    size_t secNameLen, size_t scopedPduLen,
    u_long engineboots, long engine_time,
    size_t* theTotalLength,
    size_t* authParamsOffset,
    size_t* privParamsOffset,
    size_t* dataOffset, size_t* datalen,
    size_t* msgAuthParmLen,
    size_t* msgPrivParmLen, size_t* otstlen,
    size_t* seq_len, size_t* msgSecParmLen );
/*
 * Set a given field of the secStateRef.
 *
 * Allocate <len> bytes for type <type> pointed to by ref-><field>.
 * Then copy in <item> and record its length in ref-><field_len>.
 *
 * Return 0 on success, -1 otherwise.
 */
#define USM_MAKE_ENTRY( type, item, len, field, field_len )                       \
    {                                                                             \
        if ( ref == NULL )                                                        \
            return -1;                                                            \
        if ( ref->field != NULL ) {                                               \
            MEMORY_FILL_ZERO( ref->field, ref->field_len );                       \
            MEMORY_FREE( ref->field );                                            \
        }                                                                         \
        ref->field_len = 0;                                                       \
        if ( len == 0 || item == NULL ) {                                         \
            return 0;                                                             \
        }                                                                         \
        if ( ( ref->field = ( type* )malloc( len * sizeof( type ) ) ) == NULL ) { \
            return -1;                                                            \
        }                                                                         \
                                                                                  \
        memcpy( ref->field, item, len * sizeof( type ) );                         \
        ref->field_len = len;                                                     \
                                                                                  \
        return 0;                                                                 \
    }

int Usm_freeEnginetimeOnShutdown( int majorid, int minorid, void* serverarg,
    void* clientarg )
{
    u_char engineID[ API_MAX_ENG_SIZE ];
    size_t engineID_len = sizeof( engineID );

    DEBUG_MSGTL( ( "snmpv3", "free enginetime callback called\n" ) );

    engineID_len = V3_getEngineID( engineID, engineID_len );
    if ( engineID_len > 0 )
        Engine_free( engineID, engineID_len );
    return 0;
}

struct Usm_StateReference_s*
Usm_mallocUsmStateReference( void )
{
    struct Usm_StateReference_s* retval = ( struct Usm_StateReference_s* )
        calloc( 1, sizeof( struct Usm_StateReference_s ) );

    return retval;
} /* end Usm_mallocUsmStateReference() */

void Usm_freeUsmStateReference( void* old )
{
    struct Usm_StateReference_s* old_ref = ( struct Usm_StateReference_s* )old;

    if ( old_ref ) {

        MEMORY_FREE( old_ref->usr_name );
        MEMORY_FREE( old_ref->usr_engine_id );
        MEMORY_FREE( old_ref->usr_auth_protocol );
        MEMORY_FREE( old_ref->usr_priv_protocol );

        if ( old_ref->usr_auth_key ) {
            MEMORY_FILL_ZERO( old_ref->usr_auth_key, old_ref->usr_auth_key_length );
            MEMORY_FREE( old_ref->usr_auth_key );
        }
        if ( old_ref->usr_priv_key ) {
            MEMORY_FILL_ZERO( old_ref->usr_priv_key, old_ref->usr_priv_key_length );
            MEMORY_FREE( old_ref->usr_priv_key );
        }

        MEMORY_FILL_ZERO( old_ref, sizeof( *old_ref ) );
        MEMORY_FREE( old_ref );
    }

} /* end Usm_freeUsmStateReference() */

struct Usm_User_s* Usm_getUserList( void )
{
    return _usm_userList;
}

int Usm_setUsmStateReferenceName( struct Usm_StateReference_s* ref,
    char* name, size_t name_len )
{
    USM_MAKE_ENTRY( char, name, name_len, usr_name, usr_name_length );
}

int Usm_setUsmStateReferenceEngineId( struct Usm_StateReference_s* ref,
    u_char* engine_id,
    size_t engine_id_len )
{
    USM_MAKE_ENTRY( u_char, engine_id, engine_id_len,
        usr_engine_id, usr_engine_id_length );
}

int Usm_setUsmStateReferenceAuthProtocol( struct Usm_StateReference_s* ref,
    oid* auth_protocol,
    size_t auth_protocol_len )
{
    USM_MAKE_ENTRY( oid, auth_protocol, auth_protocol_len,
        usr_auth_protocol, usr_auth_protocol_length );
}

int Usm_setUsmStateReferenceAuthKey( struct Usm_StateReference_s* ref,
    u_char* auth_key, size_t auth_key_len )
{
    USM_MAKE_ENTRY( u_char, auth_key, auth_key_len,
        usr_auth_key, usr_auth_key_length );
}

int Usm_setUsmStateReferencePrivProtocol( struct Usm_StateReference_s* ref,
    oid* priv_protocol,
    size_t priv_protocol_len )
{
    USM_MAKE_ENTRY( oid, priv_protocol, priv_protocol_len,
        usr_priv_protocol, usr_priv_protocol_length );
}

int Usm_setUsmStateReferencePrivKey( struct Usm_StateReference_s* ref,
    u_char* priv_key, size_t priv_key_len )
{
    USM_MAKE_ENTRY( u_char, priv_key, priv_key_len,
        usr_priv_key, usr_priv_key_length );
}

int Usm_setUsmStateReferenceSecLevel( struct Usm_StateReference_s* ref,
    int sec_level )
{
    if ( ref == NULL )
        return -1;
    ref->usr_sec_level = sec_level;
    return 0;
}

int Usm_cloneUsmStateReference( struct Usm_StateReference_s* from, struct Usm_StateReference_s** to )
{
    struct Usm_StateReference_s* cloned_usmStateRef;

    if ( from == NULL || to == NULL )
        return -1;

    *to = Usm_mallocUsmStateReference();
    cloned_usmStateRef = *to;

    if ( Usm_setUsmStateReferenceName( cloned_usmStateRef, from->usr_name, from->usr_name_length ) || Usm_setUsmStateReferenceEngineId( cloned_usmStateRef, from->usr_engine_id, from->usr_engine_id_length ) || Usm_setUsmStateReferenceAuthProtocol( cloned_usmStateRef, from->usr_auth_protocol, from->usr_auth_protocol_length ) || Usm_setUsmStateReferenceAuthKey( cloned_usmStateRef, from->usr_auth_key, from->usr_auth_key_length ) || Usm_setUsmStateReferencePrivProtocol( cloned_usmStateRef, from->usr_priv_protocol, from->usr_priv_protocol_length ) || Usm_setUsmStateReferencePrivKey( cloned_usmStateRef, from->usr_priv_key, from->usr_priv_key_length ) || Usm_setUsmStateReferenceSecLevel( cloned_usmStateRef, from->usr_sec_level ) ) {
        Usm_freeUsmStateReference( *to );
        *to = NULL;
        return -1;
    }

    return 0;
}

/*******************************************************************-o-******
 * Usm_asnPredictIntLength
 *
 * Parameters:
 *	type	(UNUSED)
 *	number
 *	len
 *
 * Returns:
 *	Number of bytes necessary to store the ASN.1 encoded value of 'number'.
 *
 *
 *	This gives the number of bytes that the ASN.1 encoder (in asn1.c) will
 *	use to encode a particular integer value.
 *
 *	Returns the length of the integer -- NOT THE HEADER!
 *
 *	Do this the same way as Asn01_buildInt()...
 */
int Usm_asnPredictIntLength( int type, long number, size_t len )
{
    register u_long mask;

    if ( len != sizeof( long ) )
        return -1;

    mask = ( ( u_long )0x1FF ) << ( ( 8 * ( sizeof( long ) - 1 ) ) - 1 );
    /*
     * mask is 0xFF800000 on a big-endian machine
     */

    while ( ( ( ( number & mask ) == 0 ) || ( ( number & mask ) == mask ) )
        && len > 1 ) {
        len--;
        number <<= 8;
    }

    return len;

} /* end Usm_asnPredictLength() */

/*******************************************************************-o-******
 * Usm_asnPredictLength
 *
 * Parameters:
 *	 type
 *	*ptr
 *	 u_char_len
 *
 * Returns:
 *	Length in bytes:	1 + <n> + <u_char_len>, where
 *
 *		1		For the ASN.1 type.
 *		<n>		# of bytes to store length of data.
 *		<u_char_len>	Length of data associated with ASN.1 type.
 *
 *	This gives the number of bytes that the ASN.1 encoder (in asn1.c) will
 *	use to encode a particular integer value.  This is as broken as the
 *	currently used encoder.
 *
 * XXX	How is <n> chosen, exactly??
 */
int Usm_asnPredictLength( int type, u_char* ptr, size_t u_char_len )
{

    if ( type & asnSEQUENCE )
        return 1 + 3 + u_char_len;

    if ( type & asnINTEGER ) {
        u_long value;
        memcpy( &value, ptr, u_char_len );
        u_char_len = Usm_asnPredictIntLength( type, value, u_char_len );
    }

    if ( u_char_len < 0x80 )
        return 1 + 1 + u_char_len;
    else if ( u_char_len < 0xFF )
        return 1 + 2 + u_char_len;
    else
        return 1 + 3 + u_char_len;

} /* end Usm_asnPredictLength() */

/*******************************************************************-o-******
 * Usm_calcOffsets
 *
 * Parameters:
 *	(See list below...)
 *
 * Returns:
 *	0	On success,
 *	-1	Otherwise.
 *
 *
 *	This routine calculates the offsets into an outgoing message buffer
 *	for the necessary values.  The outgoing buffer will generically
 *	look like this:
 *
 *	SNMPv3 Message
 *	SEQ len[11]
 *		INT len version
 *	Header
 *		SEQ len
 *			INT len MsgID
 *			INT len msgMaxSize
 *			OST len msgFlags (OST = OCTET STRING)
 *			INT len msgSecurityModel
 *	MsgSecurityParameters
 *		[1] OST len[2]
 *			SEQ len[3]
 *				OST len msgAuthoritativeEngineID
 *				INT len msgAuthoritativeEngineBoots
 *				INT len msgAuthoritativeEngineTime
 *				OST len msgUserName
 *				OST len[4] [5] msgAuthenticationParameters
 *				OST len[6] [7] msgPrivacyParameters
 *	MsgData
 *		[8] OST len[9] [10] encryptedPDU
 *		or
 *		[8,10] SEQUENCE len[9] scopedPDU
 *	[12]
 *
 *	The bracketed points will be needed to be identified ([x] is an index
 *	value, len[x] means a length value).  Here is a semantic guide to them:
 *
 *	[1] = globalDataLen (input)
 *	[2] = otstlen
 *	[3] = seq_len
 *	[4] = msgAuthParmLen (may be 0 or 12)
 *	[5] = authParamsOffset
 *	[6] = msgPrivParmLen (may be 0 or 8)
 *	[7] = privParamsOffset
 *	[8] = globalDataLen + msgSecParmLen
 *	[9] = datalen
 *	[10] = dataOffset
 *	[11] = theTotalLength - the length of the header itself
 *	[12] = theTotalLength
 */
int Usm_calcOffsets( size_t globalDataLen, /* SNMPv3Message + HeaderData */
    int secLevel, size_t secEngineIDLen, size_t secNameLen, size_t scopedPduLen, /* An BER encoded sequence. */
    u_long engineboots, /* XXX (asn1.c works in long, not int.) */
    long engine_time, /* XXX (asn1.c works in long, not int.) */
    size_t* theTotalLength, /* globalDataLen + msgSecurityP. + msgData */
    size_t* authParamsOffset, /* Distance to auth bytes.                 */
    size_t* privParamsOffset, /* Distance to priv bytes.                 */
    size_t* dataOffset, /* Distance to scopedPdu SEQ  -or-  the
                                         *   crypted (data) portion of msgData.    */
    size_t* datalen, /* Size of msgData OCTET STRING encoding.  */
    size_t* msgAuthParmLen, /* Size of msgAuthenticationParameters.    */
    size_t* msgPrivParmLen, /* Size of msgPrivacyParameters.           */
    size_t* otstlen, /* Size of msgSecurityP. O.S. encoding.    */
    size_t* seq_len, /* Size of msgSecurityP. SEQ data.         */
    size_t* msgSecParmLen )
{ /* Size of msgSecurityP. SEQ.              */
    int engIDlen, /* Sizes of OCTET STRING and SEQ encodings */
        engBtlen, /*   for fields within                     */
        engTmlen, /*   msgSecurityParameters portion of      */
        namelen, /*   SNMPv3Message.                        */
        authlen, privlen, ret;

    /*
     * If doing authentication, msgAuthParmLen = 12 else msgAuthParmLen = 0.
     * If doing encryption,     msgPrivParmLen = 8  else msgPrivParmLen = 0.
     */
    *msgAuthParmLen = ( secLevel == PRIOT_SEC_LEVEL_AUTHNOPRIV
                          || secLevel == PRIOT_SEC_LEVEL_AUTHPRIV )
        ? 12
        : 0;

    *msgPrivParmLen = ( secLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) ? 8 : 0;

    /*
     * Calculate lengths.
     */
    if ( ( engIDlen = Usm_asnPredictLength( asnOCTET_STR,
               NULL, secEngineIDLen ) )
        == -1 ) {
        return -1;
    }

    if ( ( engBtlen = Usm_asnPredictLength( asnINTEGER,
               ( u_char* )&engineboots,
               sizeof( long ) ) )
        == -1 ) {
        return -1;
    }

    if ( ( engTmlen = Usm_asnPredictLength( asnINTEGER,
               ( u_char* )&engine_time,
               sizeof( long ) ) )
        == -1 ) {
        return -1;
    }

    if ( ( namelen = Usm_asnPredictLength( asnOCTET_STR,
               NULL, secNameLen ) )
        == -1 ) {
        return -1;
    }

    if ( ( authlen = Usm_asnPredictLength( asnOCTET_STR,
               NULL, *msgAuthParmLen ) )
        == -1 ) {
        return -1;
    }

    if ( ( privlen = Usm_asnPredictLength( asnOCTET_STR,
               NULL, *msgPrivParmLen ) )
        == -1 ) {
        return -1;
    }

    *seq_len = engIDlen + engBtlen + engTmlen + namelen + authlen + privlen;

    if ( ( ret = Usm_asnPredictLength( asnSEQUENCE,
               NULL, *seq_len ) )
        == -1 ) {
        return -1;
    }
    *otstlen = ( size_t )ret;

    if ( ( ret = Usm_asnPredictLength( asnOCTET_STR,
               NULL, *otstlen ) )
        == -1 ) {
        return -1;
    }
    *msgSecParmLen = ( size_t )ret;

    *authParamsOffset = globalDataLen + +( *msgSecParmLen - *seq_len )
        + engIDlen + engBtlen + engTmlen + namelen
        + ( authlen - *msgAuthParmLen );

    *privParamsOffset = *authParamsOffset + *msgAuthParmLen
        + ( privlen - *msgPrivParmLen );

    /*
     * Compute the size of the plaintext.  Round up to account for cipher
     * block size, if necessary.
     *
     * XXX  This is hardwired for 1DES... If scopedPduLen is already
     *      a multiple of 8, then *add* 8 more; otherwise, round up
     *      to the next multiple of 8.
     *
     * FIX  Calculation of encrypted portion of msgData and consequent
     *      setting and sanity checking of theTotalLength, et al. should
     *      occur *after* encryption has taken place.
     */
    if ( secLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        scopedPduLen = UTILITIES_ROUND_UP8( scopedPduLen );

        if ( ( ret = Usm_asnPredictLength( asnOCTET_STR, NULL, scopedPduLen ) ) == -1 ) {
            return -1;
        }
        *datalen = ( size_t )ret;
    } else {
        *datalen = scopedPduLen;
    }

    *dataOffset = globalDataLen + *msgSecParmLen + ( *datalen - scopedPduLen );
    *theTotalLength = globalDataLen + *msgSecParmLen + *datalen;

    return 0;

} /* end Usm_calcOffsets() */

/*******************************************************************-o-******
 * Usm_setSalt
 *
 * Parameters:
 *	*iv		  (O)   Buffer to contain IV.
 *	*iv_length	  (O)   Length of iv.
 *	*priv_salt	  (I)   Salt portion of private key.
 *	 priv_salt_length (I)   Length of priv_salt.
 *	*msgSalt	  (I/O) Pointer salt portion of outgoing msg buffer.
 *
 * Returns:
 *	0	On success,
 *	-1	Otherwise.
 *
 *	Determine the initialization vector for the DES-CBC encryption.
 *	(Cf. RFC 2274, 8.1.1.1.)
 *
 *	iv is defined as the concatenation of engineBoots and the
 *		salt integer.
 *	The salt integer is incremented.
 *	The resulting salt is copied into the msgSalt buffer.
 *	The result of the concatenation is then XORed with the salt
 *		portion of the private key (last 8 bytes).
 *	The IV result is returned individually for further use.
 */
int Usm_setSalt( u_char* iv,
    size_t* iv_length,
    u_char* priv_salt, size_t priv_salt_length, u_char* msgSalt )
{
    size_t propersize_salt = UTILITIES_BYTE_SIZE( USM_DES_SALT_LENGTH );
    int net_boots;
    int net_salt_int;
    /*
     * net_* should be encoded in network byte order.  XXX  Why?
     */
    int iindex;

    /*
     * Sanity check.
     */
    if ( !iv || !iv_length || !priv_salt || ( *iv_length != propersize_salt )
        || ( priv_salt_length < propersize_salt ) ) {
        return -1;
    }

    net_boots = htonl( V3_localEngineBoots() );
    net_salt_int = htonl( _usm_saltInteger );

    _usm_saltInteger += 1;

    memcpy( iv, &net_boots, propersize_salt / 2 );
    memcpy( iv + ( propersize_salt / 2 ), &net_salt_int, propersize_salt / 2 );

    if ( msgSalt )
        memcpy( msgSalt, iv, propersize_salt );

    /*
     * Turn the salt into an IV: XOR <boots, salt_int> with salt
     * portion of priv_key.
     */
    for ( iindex = 0; iindex < ( int )propersize_salt; iindex++ )
        iv[ iindex ] ^= priv_salt[ iindex ];

    return 0;

} /* end Usm_setSalt() */

/*******************************************************************-o-******
 * Usm_setAesIv
 *
 * Parameters:
 *	*iv		  (O)   Buffer to contain IV.
 *	*iv_length	  (O)   Length of iv.
 *      net_boots         (I)   the network byte order of the authEng boots val
 *      net_time         (I)   the network byte order of the authEng time val
 *      *salt             (O)   A buffer for the outgoing salt (= 8 bytes of iv)
 *
 * Returns:
 *	0	On success,
 *	-1	Otherwise.
 *
 *	Determine the initialization vector for AES encryption.
 *	(draft-blumenthal-aes-usm-03.txt, 3.1.2.2)
 *
 *	iv is defined as the concatenation of engineBoots, engineTime
    and a 64 bit salt-integer.
 *	The 64 bit salt integer is incremented.
 *	The resulting salt is copied into the salt buffer.
 *	The IV result is returned individually for further use.
 */
int Usm_setAesIv( u_char* iv,
    size_t* iv_length,
    u_int net_boots,
    u_int net_time,
    u_char* salt )
{
    /*
     * net_* should be encoded in network byte order.
     */
    int net_salt_int1, net_salt_int2;
#define PROPER_AES_IV_SIZE 64

    /*
     * Sanity check.
     */
    if ( !iv || !iv_length ) {
        return -1;
    }

    net_salt_int1 = htonl( _usm_saltInteger64One );
    net_salt_int2 = htonl( _usm_saltInteger64Two );

    if ( ( _usm_saltInteger64Two += 1 ) == 0 )
        _usm_saltInteger64Two += 1;

    /* XXX: warning: hard coded proper lengths */
    memcpy( iv, &net_boots, 4 );
    memcpy( iv + 4, &net_time, 4 );
    memcpy( iv + 8, &net_salt_int1, 4 );
    memcpy( iv + 12, &net_salt_int2, 4 );

    memcpy( salt, iv + 8, 8 ); /* only copy the needed portion */
    return 0;
} /* end Usm_setSalt() */

int Usm_secmodGenerateOutMsg( struct OutgoingParams_s* parms )
{
    if ( !parms )
        return ErrorCode_GENERR;

    return Usm_generateOutMsg( parms->msgProcModel,
        parms->globalData, parms->globalDataLength,
        parms->maxMsgSize, parms->secModel,
        parms->secEngineId, parms->secEngineIdLength,
        parms->secName, parms->secNameLength,
        parms->secLevel,
        parms->scopedPdu, parms->scopedPduLength,
        parms->secStateRef,
        parms->secParams, parms->secParamsLength,
        parms->wholeMsg, parms->wholeMsgLength );
}

/*******************************************************************-o-******
 * Usm_generateOutMsg
 *
 * Parameters:
 *	(See list below...)
 *
 * Returns:
 *	ErrorCode_SUCCESS			On success.
 *	ErrorCode_USM_AUTHENTICATIONFAILURE
 *	ErrorCode_USM_ENCRYPTIONERROR
 *	ErrorCode_USM_GENERICERROR
 *	ErrorCode_USM_UNKNOWNSECURITYNAME
 *	ErrorCode_USM_GENERICERROR
 *	ErrorCode_USM_UNSUPPORTEDSECURITYLEVEL
 *
 *
 * Generates an outgoing message.
 *
 * XXX	Beware of misnomers!
 */
int Usm_generateOutMsg( int msgProcModel, /* (UNUSED) */
    u_char* globalData, /* IN */
    /*
                      * Pointer to msg header data will point to the beginning
                      * * of the entire packet buffer to be transmitted on wire,
                      * * memory will be contiguous with secParams, typically
                      * * this pointer will be passed back as beginning of
                      * * wholeMsg below.  asn seq. length is updated w/ new length.
                      * *
                      * * While this points to a buffer that should be big enough
                      * * for the whole message, only the first two parts
                      * * of the message are completed, namely SNMPv3Message and
                      * * HeaderData.  globalDataLen (next parameter) represents
                      * * the length of these two completed parts.
                      */
    size_t globalDataLen, /* IN - Length of msg header data.      */
    int maxMsgSize, /* (UNUSED) */
    int secModel, /* (UNUSED) */
    u_char* secEngineID, /* IN - Pointer snmpEngineID.           */
    size_t secEngineIDLen, /* IN - SnmpEngineID length.            */
    char* secName, /* IN - Pointer to securityName.        */
    size_t secNameLen, /* IN - SecurityName length.            */
    int secLevel, /* IN - AuthNoPriv, authPriv etc.       */
    u_char* scopedPdu, /* IN */
    /*
                      * Pointer to scopedPdu will be encrypted by USM if needed
                      * * and written to packet buffer immediately following
                      * * securityParameters, entire msg will be authenticated by
                      * * USM if needed.
                      */
    size_t scopedPduLen, /* IN - scopedPdu length. */
    void* secStateRef, /* IN */
    /*
                      * secStateRef, pointer to cached info provided only for
                      * * Response, otherwise NULL.
                      */
    u_char* secParams, /* OUT */
    /*
                      * BER encoded securityParameters pointer to offset within
                      * * packet buffer where secParams should be written, the
                      * * entire BER encoded OCTET STRING (including header) is
                      * * written here by USM secParams = globalData +
                      * * globalDataLen.
                      */
    size_t* secParamsLen, /* IN/OUT - Len available, len returned. */
    u_char** wholeMsg, /* OUT */
    /*
                      * Complete authenticated/encrypted message - typically
                      * * the pointer to start of packet buffer provided in
                      * * globalData is returned here, could also be a separate
                      * * buffer.
                      */
    size_t* wholeMsgLen )
{ /* IN/OUT - Len available, len returned. */
    size_t otstlen;
    size_t seq_len;
    size_t msgAuthParmLen;
    size_t msgPrivParmLen;
    size_t msgSecParmLen;
    size_t authParamsOffset;
    size_t privParamsOffset;
    size_t datalen;
    size_t dataOffset;
    size_t theTotalLength;

    u_char* ptr;
    size_t ptr_len;
    size_t remaining;
    size_t offSet;
    u_int boots_uint;
    u_int time_uint;
    long boots_long;
    long time_long;

    /*
     * Indirection because secStateRef values override parameters.
     *
     * None of these are to be free'd - they are either pointing to
     * what's in the secStateRef or to something either in the
     * actual prarmeter list or the user list.
     */

    char* theName = NULL;
    u_int theNameLength = 0;
    u_char* theEngineID = NULL;
    u_int theEngineIDLength = 0;
    u_char* theAuthKey = NULL;
    u_int theAuthKeyLength = 0;
    const oid* theAuthProtocol = NULL;
    u_int theAuthProtocolLength = 0;
    u_char* thePrivKey = NULL;
    u_int thePrivKeyLength = 0;
    const oid* thePrivProtocol = NULL;
    u_int thePrivProtocolLength = 0;
    int theSecLevel = 0; /* No defined const for bad
                                         * value (other then err).
                                         */

    DEBUG_MSGTL( ( "usm", "USM processing has begun.\n" ) );

    if ( secStateRef != NULL ) {
        /*
         * To hush the compiler for now.  XXX
         */
        struct Usm_StateReference_s* ref
            = ( struct Usm_StateReference_s* )secStateRef;

        theName = ref->usr_name;
        theNameLength = ref->usr_name_length;
        theEngineID = ref->usr_engine_id;
        theEngineIDLength = ref->usr_engine_id_length;

        if ( !theEngineIDLength ) {
            theEngineID = secEngineID;
            theEngineIDLength = secEngineIDLen;
        }

        theAuthProtocol = ref->usr_auth_protocol;
        theAuthProtocolLength = ref->usr_auth_protocol_length;
        theAuthKey = ref->usr_auth_key;
        theAuthKeyLength = ref->usr_auth_key_length;
        thePrivProtocol = ref->usr_priv_protocol;
        thePrivProtocolLength = ref->usr_priv_protocol_length;
        thePrivKey = ref->usr_priv_key;
        thePrivKeyLength = ref->usr_priv_key_length;
        theSecLevel = ref->usr_sec_level;
    }

    /*
     * Identify the user record.
     */
    else {
        struct Usm_User_s* user;

        /*
         * we do allow an unknown user name for
         * unauthenticated requests.
         */
        if ( ( user = Usm_getUser( secEngineID, secEngineIDLen, secName ) )
                == NULL
            && secLevel != PRIOT_SEC_LEVEL_NOAUTH ) {
            DEBUG_MSGTL( ( "usm", "Unknown User(%s)\n", secName ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_USM_UNKNOWNSECURITYNAME;
        }

        theName = secName;
        theNameLength = secNameLen;
        theEngineID = secEngineID;
        theSecLevel = secLevel;
        theEngineIDLength = secEngineIDLen;
        if ( user ) {
            theAuthProtocol = user->authProtocol;
            theAuthProtocolLength = user->authProtocolLen;
            theAuthKey = user->authKey;
            theAuthKeyLength = user->authKeyLen;
            thePrivProtocol = user->privProtocol;
            thePrivProtocolLength = user->privProtocolLen;
            thePrivKey = user->privKey;
            thePrivKeyLength = user->privKeyLen;
        } else {
            /*
             * unknown users can not do authentication (obviously)
             */
            theAuthProtocol = usm_noAuthProtocol;
            theAuthProtocolLength = sizeof( usm_noAuthProtocol ) / sizeof( oid );
            theAuthKey = NULL;
            theAuthKeyLength = 0;
            thePrivProtocol = usm_noPrivProtocol;
            thePrivProtocolLength = sizeof( usm_noPrivProtocol ) / sizeof( oid );
            thePrivKey = NULL;
            thePrivKeyLength = 0;
        }
    } /* endif -- secStateRef==NULL */

    /*
     * From here to the end of the function, avoid reference to
     * secName, secEngineID, secLevel, and associated lengths.
     */

    /*
     * Check to see if the user can use the requested sec services.
     */
    if ( Usm_checkSecLevelVsProtocols( theSecLevel,
             theAuthProtocol,
             theAuthProtocolLength,
             thePrivProtocol,
             thePrivProtocolLength )
        == 1 ) {
        DEBUG_MSGTL( ( "usm", "Unsupported Security Level (%d)\n",
            theSecLevel ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_USM_UNSUPPORTEDSECURITYLEVEL;
    }

    /*
     * Retrieve the engine information.
     *
     * XXX  No error is declared in the EoP when sending messages to
     *      unknown engines, processing continues w/ boots/time == (0,0).
     */
    if ( Engine_get( theEngineID, theEngineIDLength,
             &boots_uint, &time_uint, FALSE )
        == -1 ) {
        DEBUG_MSGTL( ( "usm", "%s\n", "Failed to find engine data." ) );
    }

    boots_long = boots_uint;
    time_long = time_uint;

    /*
     * Set up the Offsets.
     */
    if ( Usm_calcOffsets( globalDataLen, theSecLevel, theEngineIDLength,
             theNameLength, scopedPduLen, boots_long,
             time_long, &theTotalLength, &authParamsOffset,
             &privParamsOffset, &dataOffset, &datalen,
             &msgAuthParmLen, &msgPrivParmLen, &otstlen,
             &seq_len, &msgSecParmLen )
        == -1 ) {
        DEBUG_MSGTL( ( "usm", "Failed calculating offsets.\n" ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_USM_GENERICERROR;
    }

    /*
     * So, we have the offsets for the three parts that need to be
     * determined, and an overall length.  Now we need to make
     * sure all of this would fit in the outgoing buffer, and
     * whether or not we need to make a new buffer, etc.
     */

    /*
     * Set wholeMsg as a pointer to globalData.  Sanity check for
     * the proper size.
     *
     * Mark workspace in the message with bytes of all 1's to make it
     * easier to find mistakes in raw message dumps.
     */
    ptr = *wholeMsg = globalData;
    if ( theTotalLength > *wholeMsgLen ) {
        DEBUG_MSGTL( ( "usm", "Message won't fit in buffer.\n" ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_USM_GENERICERROR;
    }

    ptr_len = *wholeMsgLen = theTotalLength;

    /*
     * Do the encryption.
     */
    if ( theSecLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        size_t encrypted_length = theTotalLength - dataOffset;
        size_t salt_length = UTILITIES_BYTE_SIZE( USM_MAX_SALT_LENGTH );
        u_char salt[ UTILITIES_BYTE_SIZE( USM_MAX_SALT_LENGTH ) ];

        /*
         * XXX  Hardwired to seek into a 1DES private key!
         */
        if ( UTILITIES_ISTRANSFORM( thePrivProtocol, aESPriv ) ) {
            if ( !thePrivKey || Usm_setAesIv( salt, &salt_length, htonl( boots_uint ), htonl( time_uint ), &ptr[ privParamsOffset ] ) == -1 ) {
                DEBUG_MSGTL( ( "usm", "Can't set AES iv.\n" ) );
                Usm_freeUsmStateReference( secStateRef );
                return ErrorCode_USM_GENERICERROR;
            }
        }
        if ( UTILITIES_ISTRANSFORM( thePrivProtocol, dESPriv ) ) {
            if ( !thePrivKey || ( Usm_setSalt( salt, &salt_length,
                                      thePrivKey + 8, thePrivKeyLength - 8,
                                      &ptr[ privParamsOffset ] )
                                    == -1 ) ) {
                DEBUG_MSGTL( ( "usm", "Can't set DES-CBC salt.\n" ) );
                Usm_freeUsmStateReference( secStateRef );
                return ErrorCode_USM_GENERICERROR;
            }
        }

        if ( Scapi_encrypt( thePrivProtocol, thePrivProtocolLength,
                 thePrivKey, thePrivKeyLength,
                 salt, salt_length,
                 scopedPdu, scopedPduLen,
                 &ptr[ dataOffset ], &encrypted_length )
            != PRIOT_ERR_NOERROR ) {
            DEBUG_MSGTL( ( "usm", "encryption error.\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_USM_ENCRYPTIONERROR;
        }

        ptr = *wholeMsg;
        ptr_len = *wholeMsgLen = theTotalLength;

        /*
         * XXX  Sanity check for salt length should be moved up
         *      under Usm_calcOffsets() or tossed.
         */
        if ( ( encrypted_length != ( theTotalLength - dataOffset ) )
            || ( salt_length != msgPrivParmLen ) ) {
            DEBUG_MSGTL( ( "usm", "encryption length error.\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_USM_ENCRYPTIONERROR;
        }

        DEBUG_MSGTL( ( "usm", "Encryption successful.\n" ) );
    }

    /*
     * No encryption for you!
     */
    else {
        memcpy( &ptr[ dataOffset ], scopedPdu, scopedPduLen );
    }

    /*
     * Start filling in the other fields (in prep for authentication).
     *
     * offSet is an octet string header, which is different from all
     * the other headers.
     */
    remaining = ptr_len - globalDataLen;

    offSet = ptr_len - remaining;
    Asn01_buildHeader( &ptr[ offSet ], &remaining,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ), otstlen );

    offSet = ptr_len - remaining;
    Asn01_buildSequence( &ptr[ offSet ], &remaining,
        ( u_char )( asnSEQUENCE | asnCONSTRUCTOR ), seq_len );

    offSet = ptr_len - remaining;
    DEBUG_DUMPHEADER( "send", "msgAuthoritativeEngineID" );
    Asn01_buildString( &ptr[ offSet ], &remaining,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ), theEngineID,
        theEngineIDLength );
    DEBUG_INDENTLESS();

    offSet = ptr_len - remaining;
    DEBUG_DUMPHEADER( "send", "msgAuthoritativeEngineBoots" );
    Asn_buildInt( &ptr[ offSet ], &remaining,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnINTEGER ),
        &boots_long, sizeof( long ) );
    DEBUG_INDENTLESS();

    offSet = ptr_len - remaining;
    DEBUG_DUMPHEADER( "send", "msgAuthoritativeEngineTime" );
    Asn_buildInt( &ptr[ offSet ], &remaining,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnINTEGER ),
        &time_long, sizeof( long ) );
    DEBUG_INDENTLESS();

    offSet = ptr_len - remaining;
    DEBUG_DUMPHEADER( "send", "msgUserName" );
    Asn01_buildString( &ptr[ offSet ], &remaining,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ), ( u_char* )theName,
        theNameLength );
    DEBUG_INDENTLESS();

    /*
     * Note: if there is no authentication being done,
     * msgAuthParmLen is 0, and there is no effect (other than
     * inserting a zero-length header) of the following
     * statements.
     */

    offSet = ptr_len - remaining;
    Asn01_buildHeader( &ptr[ offSet ],
        &remaining,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ), msgAuthParmLen );

    if ( theSecLevel == PRIOT_SEC_LEVEL_AUTHNOPRIV
        || theSecLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        offSet = ptr_len - remaining;
        memset( &ptr[ offSet ], 0, msgAuthParmLen );
    }

    remaining -= msgAuthParmLen;

    /*
     * Note: if there is no encryption being done, msgPrivParmLen
     * is 0, and there is no effect (other than inserting a
     * zero-length header) of the following statements.
     */

    offSet = ptr_len - remaining;
    Asn01_buildHeader( &ptr[ offSet ],
        &remaining,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ), msgPrivParmLen );

    remaining -= msgPrivParmLen; /* Skipping the IV already there. */

    /*
     * For privacy, need to add the octet string header for it.
     */
    if ( theSecLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        offSet = ptr_len - remaining;
        Asn01_buildHeader( &ptr[ offSet ],
            &remaining,
            ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ),
            theTotalLength - dataOffset );
    }

    /*
     * Adjust overall length and store it as the first SEQ length
     * of the SNMPv3Message.
     *
     * FIX  4 is a magic number!
     */
    remaining = theTotalLength;
    Asn01_buildSequence( ptr, &remaining,
        ( u_char )( asnSEQUENCE | asnCONSTRUCTOR ),
        theTotalLength - 4 );

    /*
     * Now, time to consider / do authentication.
     */
    if ( theSecLevel == PRIOT_SEC_LEVEL_AUTHNOPRIV
        || theSecLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        size_t temp_sig_len = msgAuthParmLen;
        u_char* temp_sig = ( u_char* )malloc( temp_sig_len );

        if ( temp_sig == NULL ) {
            DEBUG_MSGTL( ( "usm", "Out of memory.\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_USM_GENERICERROR;
        }

        if ( Scapi_generateKeyedHash( theAuthProtocol, theAuthProtocolLength,
                 theAuthKey, theAuthKeyLength,
                 ptr, ptr_len, temp_sig, &temp_sig_len )
            != PRIOT_ERR_NOERROR ) {
            /*
             * FIX temp_sig_len defined?!
             */
            MEMORY_FILL_ZERO( temp_sig, temp_sig_len );
            MEMORY_FREE( temp_sig );
            DEBUG_MSGTL( ( "usm", "Signing failed.\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_USM_AUTHENTICATIONFAILURE;
        }

        if ( temp_sig_len != msgAuthParmLen ) {
            MEMORY_FILL_ZERO( temp_sig, temp_sig_len );
            MEMORY_FREE( temp_sig );
            DEBUG_MSGTL( ( "usm", "Signing lengths failed.\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_USM_AUTHENTICATIONFAILURE;
        }

        memcpy( &ptr[ authParamsOffset ], temp_sig, msgAuthParmLen );

        MEMORY_FILL_ZERO( temp_sig, temp_sig_len );
        MEMORY_FREE( temp_sig );
    }

    /*
     * endif -- create keyed hash
     */
    Usm_freeUsmStateReference( secStateRef );

    DEBUG_MSGTL( ( "usm", "USM processing completed.\n" ) );

    return ErrorCode_SUCCESS;

} /* end Usm_generateOutMsg() */

int Usm_secmodRgenerateOutMsg( struct OutgoingParams_s* parms )
{
    if ( !parms )
        return ErrorCode_GENERR;

    return Usm_rgenerateOutMsg( parms->msgProcModel,
        parms->globalData, parms->globalDataLength,
        parms->maxMsgSize, parms->secModel,
        parms->secEngineId, parms->secEngineIdLength,
        parms->secName, parms->secNameLength,
        parms->secLevel,
        parms->scopedPdu, parms->scopedPduLength,
        parms->secStateRef,
        parms->wholeMsg, parms->wholeMsgLength,
        parms->wholeMsgOffset );
}

int Usm_rgenerateOutMsg( int msgProcModel, /* (UNUSED) */
    u_char* globalData, /* IN */
    /*
                       * points at the msgGlobalData, which is of length given by next
                       * parameter.
                       */
    size_t globalDataLen, /* IN - Length of msg header data.      */
    int maxMsgSize, /* (UNUSED) */
    int secModel, /* (UNUSED) */
    u_char* secEngineID, /* IN - Pointer snmpEngineID.           */
    size_t secEngineIDLen, /* IN - SnmpEngineID length.            */
    char* secName, /* IN - Pointer to securityName.        */
    size_t secNameLen, /* IN - SecurityName length.            */
    int secLevel, /* IN - AuthNoPriv, authPriv etc.       */
    u_char* scopedPdu, /* IN */
    /*
                       * Pointer to scopedPdu will be encrypted by USM if needed
                       * * and written to packet buffer immediately following
                       * * securityParameters, entire msg will be authenticated by
                       * * USM if needed.
                       */
    size_t scopedPduLen, /* IN - scopedPdu length. */
    void* secStateRef, /* IN */
    /*
                       * secStateRef, pointer to cached info provided only for
                       * * Response, otherwise NULL.
                       */
    u_char** wholeMsg, /*  IN/OUT  */
    /*
                       * Points at the pointer to the packet buffer, which might get extended
                       * if necessary via realloc().
                       */
    size_t* wholeMsgLen, /*  IN/OUT  */
    /*
                       * Length of the entire packet buffer, **not** the length of the
                       * packet.
                       */
    size_t* offset /*  IN/OUT  */
    /*
                       * Offset from the end of the packet buffer to the start of the packet,
                       * also known as the packet length.
                       */
    )
{
    size_t msgAuthParmLen = 0;

    u_int boots_uint;
    u_int time_uint;
    long boots_long;
    long time_long;

    /*
     * Indirection because secStateRef values override parameters.
     *
     * None of these are to be free'd - they are either pointing to
     * what's in the secStateRef or to something either in the
     * actual parameter list or the user list.
     */

    char* theName = NULL;
    u_int theNameLength = 0;
    u_char* theEngineID = NULL;
    u_int theEngineIDLength = 0;
    u_char* theAuthKey = NULL;
    u_int theAuthKeyLength = 0;
    const oid* theAuthProtocol = NULL;
    u_int theAuthProtocolLength = 0;
    u_char* thePrivKey = NULL;
    u_int thePrivKeyLength = 0;
    const oid* thePrivProtocol = NULL;
    u_int thePrivProtocolLength = 0;
    int theSecLevel = 0; /* No defined const for bad
                                         * value (other then err). */
    size_t salt_length = 0, save_salt_length = 0;
    u_char salt[ UTILITIES_BYTE_SIZE( USM_MAX_SALT_LENGTH ) ];
    u_char authParams[ USM_MAX_AUTHSIZE ];
    u_char iv[ UTILITIES_BYTE_SIZE( USM_MAX_SALT_LENGTH ) ];
    size_t sp_offset = 0, mac_offset = 0;
    int rc = 0;

    DEBUG_MSGTL( ( "usm", "USM processing has begun (offset %d)\n", ( int )*offset ) );

    if ( secStateRef != NULL ) {
        /*
         * To hush the compiler for now.  XXX
         */
        struct Usm_StateReference_s* ref
            = ( struct Usm_StateReference_s* )secStateRef;

        theName = ref->usr_name;
        theNameLength = ref->usr_name_length;
        theEngineID = ref->usr_engine_id;
        theEngineIDLength = ref->usr_engine_id_length;

        if ( !theEngineIDLength ) {
            theEngineID = secEngineID;
            theEngineIDLength = secEngineIDLen;
        }

        theAuthProtocol = ref->usr_auth_protocol;
        theAuthProtocolLength = ref->usr_auth_protocol_length;
        theAuthKey = ref->usr_auth_key;
        theAuthKeyLength = ref->usr_auth_key_length;
        thePrivProtocol = ref->usr_priv_protocol;
        thePrivProtocolLength = ref->usr_priv_protocol_length;
        thePrivKey = ref->usr_priv_key;
        thePrivKeyLength = ref->usr_priv_key_length;
        theSecLevel = ref->usr_sec_level;
    }

    /*
     * * Identify the user record.
     */
    else {
        struct Usm_User_s* user;

        /*
         * we do allow an unknown user name for
         * unauthenticated requests.
         */
        if ( ( user = Usm_getUser( secEngineID, secEngineIDLen, secName ) )
                == NULL
            && secLevel != PRIOT_SEC_LEVEL_NOAUTH ) {
            DEBUG_MSGTL( ( "usm", "Unknown User\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_USM_UNKNOWNSECURITYNAME;
        }

        theName = secName;
        theNameLength = secNameLen;
        theEngineID = secEngineID;
        theSecLevel = secLevel;
        theEngineIDLength = secEngineIDLen;
        if ( user ) {
            theAuthProtocol = user->authProtocol;
            theAuthProtocolLength = user->authProtocolLen;
            theAuthKey = user->authKey;
            theAuthKeyLength = user->authKeyLen;
            thePrivProtocol = user->privProtocol;
            thePrivProtocolLength = user->privProtocolLen;
            thePrivKey = user->privKey;
            thePrivKeyLength = user->privKeyLen;
        } else {
            /*
             * unknown users can not do authentication (obviously)
             */
            theAuthProtocol = usm_noAuthProtocol;
            theAuthProtocolLength = sizeof( usm_noAuthProtocol ) / sizeof( oid );
            theAuthKey = NULL;
            theAuthKeyLength = 0;
            thePrivProtocol = usm_noPrivProtocol;
            thePrivProtocolLength = sizeof( usm_noPrivProtocol ) / sizeof( oid );
            thePrivKey = NULL;
            thePrivKeyLength = 0;
        }
    } /* endif -- secStateRef==NULL */

    /*
     * From here to the end of the function, avoid reference to
     * secName, secEngineID, secLevel, and associated lengths.
     */

    /*
     * Check to see if the user can use the requested sec services.
     */
    if ( Usm_checkSecLevelVsProtocols( theSecLevel,
             theAuthProtocol,
             theAuthProtocolLength,
             thePrivProtocol,
             thePrivProtocolLength )
        == 1 ) {
        DEBUG_MSGTL( ( "usm", "Unsupported Security Level or type (%d)\n",
            theSecLevel ) );

        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_USM_UNSUPPORTEDSECURITYLEVEL;
    }

    /*
     * * Retrieve the engine information.
     * *
     * * XXX    No error is declared in the EoP when sending messages to
     * *        unknown engines, processing continues w/ boots/time == (0,0).
     */
    if ( Engine_get( theEngineID, theEngineIDLength,
             &boots_uint, &time_uint, FALSE )
        == -1 ) {
        DEBUG_MSGTL( ( "usm", "%s\n", "Failed to find engine data." ) );
    }

    boots_long = boots_uint;
    time_long = time_uint;

    if ( theSecLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        /*
         * Initially assume that the ciphertext will end up the same size as
         * the plaintext plus some padding.  Really Scapi_encrypt ought to be able
         * to grow this for us, a la Asn01_reallocRbuild_<type> functions, but
         * this will do for now.
         */
        u_char* ciphertext = NULL;
        size_t ciphertextlen = scopedPduLen + 64;

        if ( ( ciphertext = ( u_char* )malloc( ciphertextlen ) ) == NULL ) {
            DEBUG_MSGTL( ( "usm",
                "couldn't malloc %d bytes for encrypted PDU\n",
                ( int )ciphertextlen ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_MALLOC;
        }

        /*
         * XXX Hardwired to seek into a 1DES private key!
         */
        if ( UTILITIES_ISTRANSFORM( thePrivProtocol, aESPriv ) ) {
            salt_length = UTILITIES_BYTE_SIZE( USM_AES_SALT_LENGTH );
            save_salt_length = UTILITIES_BYTE_SIZE( USM_AES_SALT_LENGTH ) / 2;
            if ( !thePrivKey || Usm_setAesIv( salt, &salt_length, htonl( boots_uint ), htonl( time_uint ), iv ) == -1 ) {
                DEBUG_MSGTL( ( "usm", "Can't set AES iv.\n" ) );
                Usm_freeUsmStateReference( secStateRef );
                MEMORY_FREE( ciphertext );
                return ErrorCode_USM_GENERICERROR;
            }
        }
        if ( UTILITIES_ISTRANSFORM( thePrivProtocol, dESPriv ) ) {
            salt_length = UTILITIES_BYTE_SIZE( USM_DES_SALT_LENGTH );
            save_salt_length = UTILITIES_BYTE_SIZE( USM_DES_SALT_LENGTH );
            if ( !thePrivKey || ( Usm_setSalt( salt, &salt_length,
                                      thePrivKey + 8,
                                      thePrivKeyLength - 8,
                                      iv )
                                    == -1 ) ) {
                DEBUG_MSGTL( ( "usm", "Can't set DES-CBC salt.\n" ) );
                Usm_freeUsmStateReference( secStateRef );
                MEMORY_FREE( ciphertext );
                return ErrorCode_USM_GENERICERROR;
            }
        }

        if ( Scapi_encrypt( thePrivProtocol, thePrivProtocolLength,
                 thePrivKey, thePrivKeyLength,
                 salt, salt_length,
                 scopedPdu, scopedPduLen,
                 ciphertext, &ciphertextlen )
            != PRIOT_ERR_NOERROR ) {
            DEBUG_MSGTL( ( "usm", "encryption error.\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            MEMORY_FREE( ciphertext );
            return ErrorCode_USM_ENCRYPTIONERROR;
        }

        /*
         * Write the encrypted scopedPdu back into the packet buffer.
         */

        *offset = 0;
        rc = Asn01_reallocRbuildString( wholeMsg, wholeMsgLen, offset, 1,
            ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ),
            ciphertext, ciphertextlen );
        if ( rc == 0 ) {
            DEBUG_MSGTL( ( "usm", "Encryption failed.\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            MEMORY_FREE( ciphertext );
            return ErrorCode_USM_ENCRYPTIONERROR;
        }

        DEBUG_MSGTL( ( "usm", "Encryption successful.\n" ) );
        MEMORY_FREE( ciphertext );
    } else {
        /*
         * theSecLevel != PRIOT_SEC_LEVEL_AUTHPRIV
         */
    }

    /*
     * Start encoding the msgSecurityParameters.
     */

    sp_offset = *offset;

    DEBUG_DUMPHEADER( "send", "msgPrivacyParameters" );
    /*
     * msgPrivacyParameters (warning: assumes DES salt).
     */
    rc = Asn01_reallocRbuildString( wholeMsg, wholeMsgLen, offset, 1,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE
                                        | asnOCTET_STR ),
        iv,
        save_salt_length );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        DEBUG_MSGTL( ( "usm", "building privParams failed.\n" ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_TOO_LONG;
    }

    DEBUG_DUMPHEADER( "send", "msgAuthenticationParameters" );
    /*
     * msgAuthenticationParameters (warnings assumes 0x00 by 12).
     */
    if ( theSecLevel == PRIOT_SEC_LEVEL_AUTHNOPRIV
        || theSecLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        memset( authParams, 0, USM_MD5_AND_SHA_AUTH_LEN );
        msgAuthParmLen = USM_MD5_AND_SHA_AUTH_LEN;
    }

    rc = Asn01_reallocRbuildString( wholeMsg, wholeMsgLen, offset, 1,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE
                                        | asnOCTET_STR ),
        authParams,
        msgAuthParmLen );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        DEBUG_MSGTL( ( "usm", "building authParams failed.\n" ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_TOO_LONG;
    }

    /*
     * Remember where to put the actual HMAC we calculate later on.  An
     * encoded OCTET STRING of length USM_MD5_AND_SHA_AUTH_LEN has an ASN.1
     * header of length 2, hence the fudge factor.
     */

    mac_offset = *offset - 2;

    /*
     * msgUserName.
     */
    DEBUG_DUMPHEADER( "send", "msgUserName" );
    rc = Asn01_reallocRbuildString( wholeMsg, wholeMsgLen, offset, 1,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE
                                        | asnOCTET_STR ),
        ( u_char* )theName, theNameLength );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        DEBUG_MSGTL( ( "usm", "building authParams failed.\n" ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_TOO_LONG;
    }

    /*
     * msgAuthoritativeEngineTime.
     */
    DEBUG_DUMPHEADER( "send", "msgAuthoritativeEngineTime" );
    rc = Asn01_reallocRbuildInt( wholeMsg, wholeMsgLen, offset, 1,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnINTEGER ), &time_long,
        sizeof( long ) );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        DEBUG_MSGTL( ( "usm",
            "building msgAuthoritativeEngineTime failed.\n" ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_TOO_LONG;
    }

    /*
     * msgAuthoritativeEngineBoots.
     */
    DEBUG_DUMPHEADER( "send", "msgAuthoritativeEngineBoots" );
    rc = Asn01_reallocRbuildInt( wholeMsg, wholeMsgLen, offset, 1,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnINTEGER ), &boots_long,
        sizeof( long ) );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        DEBUG_MSGTL( ( "usm",
            "building msgAuthoritativeEngineBoots failed.\n" ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_TOO_LONG;
    }

    DEBUG_DUMPHEADER( "send", "msgAuthoritativeEngineID" );
    rc = Asn01_reallocRbuildString( wholeMsg, wholeMsgLen, offset, 1,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE
                                        | asnOCTET_STR ),
        theEngineID,
        theEngineIDLength );
    DEBUG_INDENTLESS();
    if ( rc == 0 ) {
        DEBUG_MSGTL( ( "usm", "building msgAuthoritativeEngineID failed.\n" ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_TOO_LONG;
    }

    /*
     * USM msgSecurityParameters sequence header
     */
    rc = Asn01_reallocRbuildSequence( wholeMsg, wholeMsgLen, offset, 1,
        ( u_char )( asnSEQUENCE | asnCONSTRUCTOR ),
        *offset - sp_offset );
    if ( rc == 0 ) {
        DEBUG_MSGTL( ( "usm", "building usm security parameters failed.\n" ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_TOO_LONG;
    }

    /*
     * msgSecurityParameters OCTET STRING wrapper.
     */
    rc = Asn01_reallocBuildHeader( wholeMsg, wholeMsgLen, offset, 1,
        ( u_char )( asnUNIVERSAL | asnPRIMITIVE
                                        | asnOCTET_STR ),
        *offset - sp_offset );

    if ( rc == 0 ) {
        DEBUG_MSGTL( ( "usm", "building msgSecurityParameters failed.\n" ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_TOO_LONG;
    }

    /*
     * Copy in the msgGlobalData and msgVersion.
     */
    while ( ( *wholeMsgLen - *offset ) < globalDataLen ) {
        if ( !Asn01_realloc( wholeMsg, wholeMsgLen ) ) {
            DEBUG_MSGTL( ( "usm", "building global data failed.\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_TOO_LONG;
        }
    }

    *offset += globalDataLen;
    memcpy( *wholeMsg + *wholeMsgLen - *offset, globalData, globalDataLen );

    /*
     * Total packet sequence.
     */
    rc = Asn01_reallocRbuildSequence( wholeMsg, wholeMsgLen, offset, 1,
        ( u_char )( asnSEQUENCE | asnCONSTRUCTOR ), *offset );
    if ( rc == 0 ) {
        DEBUG_MSGTL( ( "usm", "building master packet sequence failed.\n" ) );
        Usm_freeUsmStateReference( secStateRef );
        return ErrorCode_TOO_LONG;
    }

    /*
     * Now consider / do authentication.
     */

    if ( theSecLevel == PRIOT_SEC_LEVEL_AUTHNOPRIV || theSecLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        size_t temp_sig_len = msgAuthParmLen;
        u_char* temp_sig = ( u_char* )malloc( temp_sig_len );
        u_char* proto_msg = *wholeMsg + *wholeMsgLen - *offset;
        size_t proto_msg_len = *offset;

        if ( temp_sig == NULL ) {
            DEBUG_MSGTL( ( "usm", "Out of memory.\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_USM_GENERICERROR;
        }

        if ( Scapi_generateKeyedHash( theAuthProtocol, theAuthProtocolLength,
                 theAuthKey, theAuthKeyLength,
                 proto_msg, proto_msg_len,
                 temp_sig, &temp_sig_len )
            != PRIOT_ERR_NOERROR ) {
            MEMORY_FREE( temp_sig );
            DEBUG_MSGTL( ( "usm", "Signing failed.\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_USM_AUTHENTICATIONFAILURE;
        }

        if ( temp_sig_len != msgAuthParmLen ) {
            MEMORY_FREE( temp_sig );
            DEBUG_MSGTL( ( "usm", "Signing lengths failed.\n" ) );
            Usm_freeUsmStateReference( secStateRef );
            return ErrorCode_USM_AUTHENTICATIONFAILURE;
        }

        memcpy( *wholeMsg + *wholeMsgLen - mac_offset, temp_sig,
            msgAuthParmLen );
        MEMORY_FREE( temp_sig );
    }
    /*
     * endif -- create keyed hash
     */
    Usm_freeUsmStateReference( secStateRef );
    DEBUG_MSGTL( ( "usm", "USM processing completed.\n" ) );
    return ErrorCode_SUCCESS;
} /* end Usm_rgenerateOutMsg() */

/*******************************************************************-o-******
 * Usm_parseSecurityParameters
 *
 * Parameters:
 *	(See list below...)
 *
 * Returns:
 *	0	On success,
 *	-1	Otherwise.
 *
 *	tab stop 4
 *
 *	Extracts values from the security header and data portions of the
 *	incoming buffer.
 */
int Usm_parseSecurityParameters( u_char* secParams,
    size_t remaining,
    u_char* secEngineID,
    size_t* secEngineIDLen,
    u_int* boots_uint,
    u_int* time_uint,
    char* secName,
    size_t* secNameLen,
    u_char* signature,
    size_t* signature_length,
    u_char* salt,
    size_t* salt_length, u_char** data_ptr )
{
    u_char* parse_ptr = secParams;
    u_char* value_ptr;
    u_char* next_ptr;
    u_char type_value;

    size_t octet_string_length = remaining;
    size_t sequence_length;
    size_t remaining_bytes;

    long boots_long;
    long time_long;

    u_int origNameLen;

    /*
     * Eat the first octet header.
     */
    if ( ( value_ptr = Asn01_parseSequence( parse_ptr, &octet_string_length,
               &type_value,
               ( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ),
               "usm first octet" ) )
        == NULL ) {
        /*
         * RETURN parse error
         */ return -1;
    }

    /*
     * Eat the sequence header.
     */
    parse_ptr = value_ptr;
    sequence_length = octet_string_length;

    if ( ( value_ptr = Asn01_parseSequence( parse_ptr, &sequence_length,
               &type_value,
               ( asnSEQUENCE | asnCONSTRUCTOR ),
               "usm sequence" ) )
        == NULL ) {
        /*
         * RETURN parse error
         */ return -1;
    }

    /*
     * Retrieve the engineID.
     */
    parse_ptr = value_ptr;
    remaining_bytes = sequence_length;

    DEBUG_DUMPHEADER( "recv", "msgAuthoritativeEngineID" );
    if ( ( next_ptr
             = Asn01_parseString( parse_ptr, &remaining_bytes, &type_value,
                 secEngineID, secEngineIDLen ) )
        == NULL ) {
        DEBUG_INDENTLESS();
        /*
         * RETURN parse error
         */ return -1;
    }
    DEBUG_INDENTLESS();

    if ( type_value != ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ) ) {
        /*
         * RETURN parse error
         */ return -1;
    }

    /*
     * Retrieve the engine boots, notice switch in the way next_ptr and
     * remaining_bytes are used (to accomodate the asn code).
     */
    DEBUG_DUMPHEADER( "recv", "msgAuthoritativeEngineBoots" );
    if ( ( next_ptr = Asn_parseInt( next_ptr, &remaining_bytes, &type_value,
               &boots_long, sizeof( long ) ) )
        == NULL ) {
        DEBUG_INDENTLESS();
        /*
         * RETURN parse error
         */ return -1;
    }
    DEBUG_INDENTLESS();

    if ( type_value != ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnINTEGER ) ) {
        DEBUG_INDENTLESS();
        /*
         * RETURN parse error
         */ return -1;
    }

    *boots_uint = ( u_int )boots_long;

    /*
     * Retrieve the time value.
     */
    DEBUG_DUMPHEADER( "recv", "msgAuthoritativeEngineTime" );
    if ( ( next_ptr = Asn_parseInt( next_ptr, &remaining_bytes, &type_value,
               &time_long, sizeof( long ) ) )
        == NULL ) {
        /*
         * RETURN parse error
         */ return -1;
    }
    DEBUG_INDENTLESS();

    if ( type_value != ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnINTEGER ) ) {
        /*
         * RETURN parse error
         */ return -1;
    }

    *time_uint = ( u_int )time_long;

    if ( *boots_uint > UTILITIES_ENGINEBOOT_MAX || *time_uint > UTILITIES_ENGINETIME_MAX ) {
        return -1;
    }

    /*
     * Retrieve the secName.
     */
    origNameLen = *secNameLen;

    DEBUG_DUMPHEADER( "recv", "msgUserName" );
    if ( ( next_ptr
             = Asn01_parseString( next_ptr, &remaining_bytes, &type_value,
                 ( u_char* )secName, secNameLen ) )
        == NULL ) {
        DEBUG_INDENTLESS();
        /*
         * RETURN parse error
         */ return -1;
    }
    DEBUG_INDENTLESS();

    /*
     * FIX -- doesn't this also indicate a buffer overrun?
     */
    if ( origNameLen < *secNameLen + 1 ) {
        /*
         * RETURN parse error, but it's really a parameter error
         */
        return -1;
    }

    if ( *secNameLen > 32 ) {
        /*
         * This is a USM-specific limitation over and above the above
         * limitation (which will probably default to the length of an
         * SnmpAdminString, i.e. 255).  See RFC 2574, sec. 2.4.
         */
        return -1;
    }

    secName[ *secNameLen ] = '\0';

    if ( type_value != ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ) ) {
        /*
         * RETURN parse error
         */ return -1;
    }

    /*
     * Retrieve the signature and blank it if there.
     */
    DEBUG_DUMPHEADER( "recv", "msgAuthenticationParameters" );
    if ( ( next_ptr
             = Asn01_parseString( next_ptr, &remaining_bytes, &type_value,
                 signature, signature_length ) )
        == NULL ) {
        DEBUG_INDENTLESS();
        /*
         * RETURN parse error
         */ return -1;
    }
    DEBUG_INDENTLESS();

    if ( type_value != ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ) ) {
        /*
         * RETURN parse error
         */ return -1;
    }

    if ( *signature_length != 0 ) { /* Blanking for authentication step later */
        memset( next_ptr - ( u_long )*signature_length,
            0, *signature_length );
    }

    /*
     * Retrieve the salt.
     *
     * Note that the next ptr is where the data section starts.
     */
    DEBUG_DUMPHEADER( "recv", "msgPrivacyParameters" );
    if ( ( *data_ptr
             = Asn01_parseString( next_ptr, &remaining_bytes, &type_value,
                 salt, salt_length ) )
        == NULL ) {
        DEBUG_INDENTLESS();
        /*
         * RETURN parse error
         */ return -2;
    }
    DEBUG_INDENTLESS();

    if ( type_value != ( u_char )( asnUNIVERSAL | asnPRIMITIVE | asnOCTET_STR ) ) {
        /*
         * RETURN parse error
         */ return -2;
    }

    return 0;

} /* end Usm_parseSecurityParameters() */

/*******************************************************************-o-******
 * Usm_checkAndUpdateTimeliness
 *
 * Parameters:
 *	*secEngineID
 *	 secEngineIDen
 *	 boots_uint
 *	 time_uint
 *	*error
 *
 * Returns:
 *	0	On success,
 *	-1	Otherwise.
 *
 *
 * Performs the incoming timeliness checking and setting.
 */
int Usm_checkAndUpdateTimeliness( u_char* secEngineID,
    size_t secEngineIDLen,
    u_int boots_uint,
    u_int time_uint, int* error )
{
    u_char myID[ USM_MAX_ID_LENGTH ];
    u_long myIDLength = V3_getEngineID( myID, USM_MAX_ID_LENGTH );
    u_int myBoots;
    u_int myTime;

    if ( ( myIDLength > USM_MAX_ID_LENGTH ) || ( myIDLength == 0 ) ) {
        /*
         * We're probably already screwed...buffer overwrite.  XXX?
         */
        DEBUG_MSGTL( ( "usm", "Buffer overflow.\n" ) );
        *error = ErrorCode_USM_GENERICERROR;
        return -1;
    }

    myBoots = V3_localEngineBoots();
    myTime = V3_localEngineTime();

    /*
     * IF the time involved is local
     *     Make sure  message is inside the time window
     * ELSE
     *      IF boots is higher or boots is the same and time is higher
     *              remember this new data
     *      ELSE
     *              IF !(boots same and time within USM_TIME_WINDOW secs)
     *                      Message is too old
     *              ELSE
     *                      Message is ok, but don't take time
     *              ENDIF
     *      ENDIF
     * ENDIF
     */

    /*
     * This is a local reference.
     */
    if ( secEngineIDLen == myIDLength
        && memcmp( secEngineID, myID, myIDLength ) == 0 ) {
        u_int time_difference = myTime > time_uint ? myTime - time_uint : time_uint - myTime;

        if ( boots_uint == UTILITIES_ENGINEBOOT_MAX
            || boots_uint != myBoots
            || time_difference > USM_TIME_WINDOW ) {
            Api_incrementStatistic( API_STAT_USMSTATSNOTINTIMEWINDOWS );

            DEBUG_MSGTL( ( "usm",
                "boot_uint %u myBoots %u time_diff %u => not in time window\n",
                boots_uint, myBoots, time_difference ) );
            *error = ErrorCode_USM_NOTINTIMEWINDOW;
            return -1;
        }

        *error = ErrorCode_SUCCESS;
        return 0;
    }

    /*
     * This is a remote reference.
     */
    else {
        u_int theirBoots, theirTime, theirLastTime;
        u_int time_difference;

        if ( Engine_getEx( secEngineID, secEngineIDLen,
                 &theirBoots, &theirTime,
                 &theirLastTime, TRUE )
            != ErrorCode_SUCCESS ) {
            DEBUG_MSGTL( ( "usm", "%s\n",
                "Failed to get remote engine's times." ) );

            *error = ErrorCode_USM_GENERICERROR;
            return -1;
        }

        time_difference = theirTime > time_uint ? theirTime - time_uint : time_uint - theirTime;

        /*
         * XXX  Contrary to the pseudocode:
         *      See if boots is invalid first.
         */
        if ( theirBoots == UTILITIES_ENGINEBOOT_MAX || theirBoots > boots_uint ) {
            DEBUG_MSGTL( ( "usm", "%s\n", "Remote boot count invalid." ) );

            *error = ErrorCode_USM_NOTINTIMEWINDOW;
            return -1;
        }

        /*
         * Boots is ok, see if the boots is the same but the time
         * is old.
         */
        if ( theirBoots == boots_uint && time_uint < theirLastTime ) {
            if ( time_difference > USM_TIME_WINDOW ) {
                DEBUG_MSGTL( ( "usm", "%s\n", "Message too old." ) );
                *error = ErrorCode_USM_NOTINTIMEWINDOW;
                return -1;
            }

            else { /* Old, but acceptable */

                *error = ErrorCode_SUCCESS;
                return 0;
            }
        }

        /*
         * Message is ok, either boots has been advanced, or
         * time is greater than before with the same boots.
         */

        if ( Engine_set( secEngineID, secEngineIDLen,
                 boots_uint, time_uint, TRUE )
            != ErrorCode_SUCCESS ) {
            DEBUG_MSGTL( ( "usm", "%s\n",
                "Failed updating remote boot/time." ) );
            *error = ErrorCode_USM_GENERICERROR;
            return -1;
        }

        *error = ErrorCode_SUCCESS;
        return 0; /* Fresh message and time updated */

    } /* endif -- local or remote time reference. */

} /* end Usm_checkAndUpdateTimeliness() */

int Usm_secmodProcessInMsg( struct IncomingParams_s* parms )
{
    if ( !parms )
        return ErrorCode_GENERR;

    return Usm_processInMsg( parms->msgProcModel,
        parms->maxMsgSize,
        parms->secParams,
        parms->secModel,
        parms->secLevel,
        parms->wholeMsg,
        parms->wholeMsgLength,
        parms->secEngineId,
        parms->secEngineIdLength,
        parms->secName,
        parms->secNameLen,
        parms->scopedPdu,
        parms->scopedPduLength,
        parms->maxSizeResponse,
        parms->secStateRef,
        parms->sess, parms->msgFlags );
}

/*******************************************************************-o-******
 * Usm_processInMsg
 *
 * Parameters:
 *	(See list below...)
 *
 * Returns:
 *	ErrorCode_SUCCESS			On success.
 *	ErrorCode_USM_AUTHENTICATIONFAILURE
 *	ErrorCode_USM_DECRYPTIONERROR
 *	ErrorCode_USM_GENERICERROR
 *	ErrorCode_USM_PARSEERROR
 *	ErrorCode_USM_UNKNOWNENGINEID
 *	ErrorCode_USM_PARSEERROR
 *	ErrorCode_USM_UNKNOWNSECURITYNAME
 *	ErrorCode_USM_UNSUPPORTEDSECURITYLEVEL
 *
 *
 * ASSUMES size of decrypt_buf will always be >= size of encrypted sPDU.
 *
 * FIX  Memory leaks if secStateRef is allocated and a return occurs
 *	without cleaning up.  May contain secrets...
 */
int Usm_processInMsg( int msgProcModel, /* (UNUSED) */
    size_t maxMsgSize, /* IN     - Used to calc maxSizeResponse.  */
    u_char* secParams, /* IN     - BER encoded securityParameters. */
    int secModel, /* (UNUSED) */
    int secLevel, /* IN     - AuthNoPriv, authPriv etc.      */
    u_char* wholeMsg, /* IN     - Original v3 message.           */
    size_t wholeMsgLen, /* IN     - Msg length.                    */
    u_char* secEngineID, /* OUT    - Pointer snmpEngineID.          */
    size_t* secEngineIDLen, /* IN/OUT - Len available, len returned.   */
    /*
                    * NOTE: Memory provided by caller.
                    */
    char* secName, /* OUT    - Pointer to securityName.       */
    size_t* secNameLen, /* IN/OUT - Len available, len returned.   */
    u_char** scopedPdu, /* OUT    - Pointer to plaintext scopedPdu. */
    size_t* scopedPduLen, /* IN/OUT - Len available, len returned.   */
    size_t* maxSizeResponse, /* OUT    - Max size of Response PDU.      */
    void** secStateRf, /* OUT    - Ref to security state.         */
    Types_Session* sess, /* IN     - session which got the message  */
    u_char msg_flags )
{ /* IN     - v3 Message flags.              */
    size_t remaining = wholeMsgLen - ( u_int )( ( u_long )*secParams - ( u_long )*wholeMsg );
    u_int boots_uint;
    u_int time_uint;
    u_int net_boots, net_time;
    u_char signature[ UTILITIES_BYTE_SIZE( USM_MAX_KEYEDHASH_LENGTH ) ];
    size_t signature_length = UTILITIES_BYTE_SIZE( USM_MAX_KEYEDHASH_LENGTH );
    u_char salt[ UTILITIES_BYTE_SIZE( USM_MAX_SALT_LENGTH ) ];
    size_t salt_length = UTILITIES_BYTE_SIZE( USM_MAX_SALT_LENGTH );
    u_char iv[ UTILITIES_BYTE_SIZE( USM_MAX_SALT_LENGTH ) ];
    u_int iv_length = UTILITIES_BYTE_SIZE( USM_MAX_SALT_LENGTH );
    u_char* data_ptr;
    u_char* value_ptr;
    u_char type_value;
    u_char* end_of_overhead = NULL;
    int error;
    int i, rc = 0;
    struct Usm_StateReference_s** secStateRef = ( struct Usm_StateReference_s** )secStateRf;

    struct Usm_User_s* user;

    DEBUG_MSGTL( ( "usm", "USM processing begun...\n" ) );

    if ( secStateRef ) {
        Usm_freeUsmStateReference( *secStateRef );
        *secStateRef = Usm_mallocUsmStateReference();
        if ( *secStateRef == NULL ) {
            DEBUG_MSGTL( ( "usm", "Out of memory.\n" ) );
            return ErrorCode_USM_GENERICERROR;
        }
    }

    /*
     * Make sure the *secParms is an OCTET STRING.
     * Extract the user name, engine ID, and security level.
     */
    if ( ( rc = Usm_parseSecurityParameters( secParams, remaining,
               secEngineID, secEngineIDLen,
               &boots_uint, &time_uint,
               secName, secNameLen,
               signature, &signature_length,
               salt, &salt_length,
               &data_ptr ) )
        < 0 ) {
        DEBUG_MSGTL( ( "usm", "Parsing failed (rc %d).\n", rc ) );
        if ( rc == -2 ) {
            /*
             * This indicates a decryptionError.
             */
            Api_incrementStatistic( API_STAT_USMSTATSDECRYPTIONERRORS );
            return ErrorCode_USM_DECRYPTIONERROR;
        }
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        return ErrorCode_USM_PARSEERROR;
    }

    /*
     * RFC 2574 section 8.3.2
     * 1)  If the privParameters field is not an 8-octet OCTET STRING,
     * then an error indication (decryptionError) is returned to the
     * calling module.
     */
    if ( ( secLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) && ( salt_length != 8 ) ) {
        Api_incrementStatistic( API_STAT_USMSTATSDECRYPTIONERRORS );
        return ErrorCode_USM_DECRYPTIONERROR;
    }

    if ( secLevel != PRIOT_SEC_LEVEL_AUTHPRIV ) {
        /*
         * pull these out now so reports can use them
         */
        *scopedPdu = data_ptr;
        *scopedPduLen = wholeMsgLen - ( data_ptr - wholeMsg );
        end_of_overhead = data_ptr;
    }

    if ( secStateRef ) {
        /*
         * Cache the name, engine ID, and security level,
         * * per step 2 (section 3.2)
         */
        if ( Usm_setUsmStateReferenceName( *secStateRef, secName, *secNameLen ) == -1 ) {
            DEBUG_MSGTL( ( "usm", "%s\n", "Couldn't cache name." ) );
            return ErrorCode_USM_GENERICERROR;
        }

        if ( Usm_setUsmStateReferenceEngineId( *secStateRef, secEngineID, *secEngineIDLen ) == -1 ) {
            DEBUG_MSGTL( ( "usm", "%s\n", "Couldn't cache engine id." ) );
            return ErrorCode_USM_GENERICERROR;
        }

        if ( Usm_setUsmStateReferenceSecLevel( *secStateRef, secLevel ) == -1 ) {
            DEBUG_MSGTL( ( "usm", "%s\n", "Couldn't cache security level." ) );
            return ErrorCode_USM_GENERICERROR;
        }
    }

    /*
     * Locate the engine ID record.
     * If it is unknown, then either create one or note this as an error.
     */
    if ( ( sess && ( sess->isAuthoritative == API_SESS_AUTHORITATIVE || ( sess->isAuthoritative == API_SESS_UNKNOWNAUTH && ( msg_flags & PRIOT_MSG_FLAG_RPRT_BIT ) ) ) ) || ( !sess && ( msg_flags & PRIOT_MSG_FLAG_RPRT_BIT ) ) ) {
        if ( engineIS_ENGINE_KNOWN( secEngineID, *secEngineIDLen ) == FALSE ) {
            DEBUG_MSGTL( ( "usm", "Unknown Engine ID.\n" ) );
            Api_incrementStatistic( API_STAT_USMSTATSUNKNOWNENGINEIDS );
            return ErrorCode_USM_UNKNOWNENGINEID;
        }
    } else {
        if ( engineENSURE_ENGINE_RECORD( secEngineID, *secEngineIDLen )
            != ErrorCode_SUCCESS ) {
            DEBUG_MSGTL( ( "usm", "%s\n", "Couldn't ensure engine record." ) );
            return ErrorCode_USM_GENERICERROR;
        }
    }

    /*
     * Locate the User record.
     * If the user/engine ID is unknown, report this as an error.
     */
    if ( ( user = Usm_getUserFromList( secEngineID, *secEngineIDLen,
               secName, _usm_userList,
               ( ( ( sess && sess->isAuthoritative == API_SESS_AUTHORITATIVE ) || ( !sess ) ) ? 0 : 1 ) ) )
        == NULL ) {
        DEBUG_MSGTL( ( "usm", "Unknown User(%s)\n", secName ) );
        Api_incrementStatistic( API_STAT_USMSTATSUNKNOWNUSERNAMES );
        return ErrorCode_USM_UNKNOWNSECURITYNAME;
    }

    /* ensure the user is active */
    if ( user->userStatus != tcROW_STATUS_ACTIVE ) {
        DEBUG_MSGTL( ( "usm", "Attempt to use an inactive user.\n" ) );
        return ErrorCode_USM_UNKNOWNSECURITYNAME;
    }

    /*
     * Make sure the security level is appropriate.
     */

    rc = Usm_checkSecLevel( secLevel, user );
    if ( 1 == rc ) {
        DEBUG_MSGTL( ( "usm", "Unsupported Security Level (%d).\n",
            secLevel ) );
        Api_incrementStatistic( API_STAT_USMSTATSUNSUPPORTEDSECLEVELS );
        return ErrorCode_USM_UNSUPPORTEDSECURITYLEVEL;
    } else if ( rc != 0 ) {
        DEBUG_MSGTL( ( "usm", "Unknown issue.\n" ) );
        return ErrorCode_USM_GENERICERROR;
    }

    /*
     * Check the authentication credentials of the message.
     */
    if ( secLevel == PRIOT_SEC_LEVEL_AUTHNOPRIV
        || secLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        if ( Scapi_checkKeyedHash( user->authProtocol, user->authProtocolLen,
                 user->authKey, user->authKeyLen,
                 wholeMsg, wholeMsgLen,
                 signature, signature_length )
            != PRIOT_ERR_NOERROR ) {
            DEBUG_MSGTL( ( "usm", "Verification failed.\n" ) );
            Api_incrementStatistic( API_STAT_USMSTATSWRONGDIGESTS );
            Logger_log( LOGGER_PRIORITY_WARNING, "Authentication failed for %s\n",
                user->name );
            return ErrorCode_USM_AUTHENTICATIONFAILURE;
        }

        DEBUG_MSGTL( ( "usm", "Verification succeeded.\n" ) );
    }

    /*
     * Steps 10-11  user is already set - relocated before timeliness
     * check in case it fails - still save user data for response.
     *
     * Cache the keys and protocol oids, per step 11 (s3.2).
     */
    if ( secStateRef ) {
        if ( Usm_setUsmStateReferenceAuthProtocol( *secStateRef,
                 user->authProtocol,
                 user->authProtocolLen )
            == -1 ) {
            DEBUG_MSGTL( ( "usm", "%s\n",
                "Couldn't cache authentication protocol." ) );
            return ErrorCode_USM_GENERICERROR;
        }

        if ( Usm_setUsmStateReferenceAuthKey( *secStateRef,
                 user->authKey,
                 user->authKeyLen )
            == -1 ) {
            DEBUG_MSGTL( ( "usm", "%s\n",
                "Couldn't cache authentication key." ) );
            return ErrorCode_USM_GENERICERROR;
        }

        if ( Usm_setUsmStateReferencePrivProtocol( *secStateRef,
                 user->privProtocol,
                 user->privProtocolLen )
            == -1 ) {
            DEBUG_MSGTL( ( "usm", "%s\n",
                "Couldn't cache privacy protocol." ) );
            return ErrorCode_USM_GENERICERROR;
        }

        if ( Usm_setUsmStateReferencePrivKey( *secStateRef,
                 user->privKey,
                 user->privKeyLen )
            == -1 ) {
            DEBUG_MSGTL( ( "usm", "%s\n", "Couldn't cache privacy key." ) );
            return ErrorCode_USM_GENERICERROR;
        }
    }

    /*
     * Perform the timeliness/time manager functions.
     */
    if ( secLevel == PRIOT_SEC_LEVEL_AUTHNOPRIV
        || secLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        if ( Usm_checkAndUpdateTimeliness( secEngineID, *secEngineIDLen,
                 boots_uint, time_uint,
                 &error )
            == -1 ) {
            return error;
        }
    }
    /*
     * Cache the unauthenticated time to use in case we don't have
     * anything better - this guess will be no worse than (0,0)
     * that we normally use.
     */
    else {
        Engine_set( secEngineID, *secEngineIDLen,
            boots_uint, time_uint, FALSE );
    }

    /*
     * If needed, decrypt the scoped PDU.
     */
    if ( secLevel == PRIOT_SEC_LEVEL_AUTHPRIV ) {
        remaining = wholeMsgLen - ( data_ptr - wholeMsg );

        if ( ( value_ptr = Asn01_parseSequence( data_ptr, &remaining,
                   &type_value,
                   ( asnUNIVERSAL | asnPRIMITIVE
                                                    | asnOCTET_STR ),
                   "encrypted sPDU" ) )
            == NULL ) {
            DEBUG_MSGTL( ( "usm", "%s\n",
                "Failed while parsing encrypted sPDU." ) );
            Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
            Usm_freeUsmStateReference( *secStateRef );
            *secStateRef = NULL;
            return ErrorCode_USM_PARSEERROR;
        }

        if ( UTILITIES_ISTRANSFORM( user->privProtocol, dESPriv ) ) {
            /*
             * From RFC2574:
             *
             * "Before decryption, the encrypted data length is verified.
             * If the length of the OCTET STRING to be decrypted is not
             * an integral multiple of 8 octets, the decryption process
             * is halted and an appropriate exception noted."
             */

            if ( remaining % 8 != 0 ) {
                DEBUG_MSGTL( ( "usm",
                    "Ciphertext is %lu bytes, not an integer multiple of 8 (rem %lu)\n",
                    ( unsigned long )remaining, ( unsigned long )remaining % 8 ) );
                Api_incrementStatistic( API_STAT_USMSTATSDECRYPTIONERRORS );
                Usm_freeUsmStateReference( *secStateRef );
                *secStateRef = NULL;
                return ErrorCode_USM_DECRYPTIONERROR;
            }

            end_of_overhead = value_ptr;

            if ( !user->privKey ) {
                DEBUG_MSGTL( ( "usm", "No privacy pass phrase for %s\n", user->secName ) );
                Api_incrementStatistic( API_STAT_USMSTATSDECRYPTIONERRORS );
                Usm_freeUsmStateReference( *secStateRef );
                *secStateRef = NULL;
                return ErrorCode_USM_DECRYPTIONERROR;
            }

            /*
             * XOR the salt with the last (iv_length) bytes
             * of the priv_key to obtain the IV.
             */
            iv_length = UTILITIES_BYTE_SIZE( USM_DES_SALT_LENGTH );
            for ( i = 0; i < ( int )iv_length; i++ )
                iv[ i ] = salt[ i ] ^ user->privKey[ iv_length + i ];
        }
        if ( UTILITIES_ISTRANSFORM( user->privProtocol, aESPriv ) ) {
            iv_length = UTILITIES_BYTE_SIZE( USM_AES_SALT_LENGTH );
            net_boots = ntohl( boots_uint );
            net_time = ntohl( time_uint );
            memcpy( iv, &net_boots, 4 );
            memcpy( iv + 4, &net_time, 4 );
            memcpy( iv + 8, salt, salt_length );
        }

        if ( Scapi_decrypt( user->privProtocol, user->privProtocolLen,
                 user->privKey, user->privKeyLen,
                 iv, iv_length,
                 value_ptr, remaining, *scopedPdu, scopedPduLen )
            != PRIOT_ERR_NOERROR ) {
            DEBUG_MSGTL( ( "usm", "%s\n", "Failed decryption." ) );
            Api_incrementStatistic( API_STAT_USMSTATSDECRYPTIONERRORS );
            return ErrorCode_USM_DECRYPTIONERROR;
        }

    }
    /*
     * sPDU is plaintext.
     */
    else {
        *scopedPdu = data_ptr;
        *scopedPduLen = wholeMsgLen - ( data_ptr - wholeMsg );
        end_of_overhead = data_ptr;

    } /* endif -- PDU decryption */

    /*
     * Calculate the biggest sPDU for the response (i.e., whole - ovrhd).
     *
     * FIX  Correct?
     */
    *maxSizeResponse = maxMsgSize - ( end_of_overhead - wholeMsg );

    DEBUG_MSGTL( ( "usm", "USM processing completed.\n" ) );

    return ErrorCode_SUCCESS;

} /* end Usm_processInMsg() */

void Usm_handleReport( void* sessp,
    Transport_Transport* transport, Types_Session* session,
    int result, Types_Pdu* pdu )
{
    /*
     * handle reportable errors
     */

    /* this will get in our way */
    Usm_freeUsmStateReference( pdu->securityStateRef );
    pdu->securityStateRef = NULL;

    if ( result == ErrorCode_USM_AUTHENTICATIONFAILURE ) {
        int res = session->s_snmp_errno;
        session->s_snmp_errno = result;
        if ( session->callback ) {
            session->callback( API_CALLBACK_OP_RECEIVED_MESSAGE,
                session, pdu->reqid, pdu,
                session->callback_magic );
        }
        session->s_snmp_errno = res;
    }
    /* fallthrough */
    if ( result == ErrorCode_USM_AUTHENTICATIONFAILURE || result == ErrorCode_USM_UNKNOWNENGINEID || result == ErrorCode_USM_UNKNOWNSECURITYNAME || result == ErrorCode_USM_UNSUPPORTEDSECURITYLEVEL || result == ErrorCode_USM_NOTINTIMEWINDOW || result == ErrorCode_USM_DECRYPTIONERROR ) {

        if ( PRIOT_CMD_CONFIRMED( pdu->command ) || ( pdu->command == 0
                                                        && ( pdu->flags & PRIOT_MSG_FLAG_RPRT_BIT ) ) ) {
            Types_Pdu* pdu2;
            int flags = pdu->flags;

            pdu->flags |= PRIOT_UCD_MSG_FLAG_FORCE_PDU_COPY;
            pdu2 = Client_clonePdu( pdu );
            pdu->flags = pdu2->flags = flags;
            Api_v3MakeReport( pdu2, result );
            if ( 0 == Api_sessSend( sessp, pdu2 ) ) {
                Api_freePdu( pdu2 );
                /*
                 * TODO: indicate error
                 */
            }
        }
    }
}

/* sets up initial default session parameters */
int Usm_sessionInit( Types_Session* in_session, Types_Session* session )
{
    char* cp;
    size_t i;

    if ( in_session->securityAuthProtoLen > 0 ) {
        session->securityAuthProto = Api_duplicateObjid( in_session->securityAuthProto,
            in_session->securityAuthProtoLen );
        if ( session->securityAuthProto == NULL ) {
            in_session->s_snmp_errno = ErrorCode_MALLOC;
            return ErrorCode_MALLOC;
        }
    } else if ( Usm_getDefaultAuthtype( &i ) != NULL ) {
        session->securityAuthProto = Api_duplicateObjid( Usm_getDefaultAuthtype( NULL ), i );
        session->securityAuthProtoLen = i;
    }

    if ( in_session->securityPrivProtoLen > 0 ) {
        session->securityPrivProto = Api_duplicateObjid( in_session->securityPrivProto,
            in_session->securityPrivProtoLen );
        if ( session->securityPrivProto == NULL ) {
            in_session->s_snmp_errno = ErrorCode_MALLOC;
            return ErrorCode_MALLOC;
        }
    } else if ( Usm_getDefaultPrivtype( &i ) != NULL ) {
        session->securityPrivProto = Api_duplicateObjid( Usm_getDefaultPrivtype( NULL ), i );
        session->securityPrivProtoLen = i;
    }

    if ( ( in_session->securityAuthKeyLen <= 0 ) && ( ( cp = DefaultStore_getString( DsStore_LIBRARY_ID,
                                                            DsStr_AUTHMASTERKEY ) ) ) ) {
        size_t buflen = sizeof( session->securityAuthKey );
        u_char* tmpp = session->securityAuthKey;
        session->securityAuthKeyLen = 0;
        /* it will be a hex string */
        if ( !Convert_hexStringToBinaryStringWrapper( &tmpp, &buflen,
                 &session->securityAuthKeyLen, 0, cp ) ) {
            Api_setDetail( "error parsing authentication master key" );
            return PRIOT_ERR_GENERR;
        }
    } else if ( ( in_session->securityAuthKeyLen <= 0 ) && ( ( cp = DefaultStore_getString( DsStore_LIBRARY_ID,
                                                                   DsStr_AUTHPASSPHRASE ) )
                                                               || ( cp = DefaultStore_getString( DsStore_LIBRARY_ID,
                                                                        DsStr_PASSPHRASE ) ) ) ) {
        session->securityAuthKeyLen = USM_AUTH_KU_LEN;
        if ( KeyTools_generateKu( session->securityAuthProto,
                 session->securityAuthProtoLen,
                 ( u_char* )cp, strlen( cp ),
                 session->securityAuthKey,
                 &session->securityAuthKeyLen )
            != ErrorCode_SUCCESS ) {
            Api_setDetail( "Error generating a key (Ku) from the supplied authentication pass phrase." );
            return PRIOT_ERR_GENERR;
        }
    }

    if ( ( in_session->securityPrivKeyLen <= 0 ) && ( ( cp = DefaultStore_getString( DsStore_LIBRARY_ID,
                                                            DsStr_PRIVMASTERKEY ) ) ) ) {
        size_t buflen = sizeof( session->securityPrivKey );
        u_char* tmpp = session->securityPrivKey;
        session->securityPrivKeyLen = 0;
        /* it will be a hex string */
        if ( !Convert_hexStringToBinaryStringWrapper( &tmpp, &buflen,
                 &session->securityPrivKeyLen, 0, cp ) ) {
            Api_setDetail( "error parsing encryption master key" );
            return PRIOT_ERR_GENERR;
        }
    } else if ( ( in_session->securityPrivKeyLen <= 0 ) && ( ( cp = DefaultStore_getString( DsStore_LIBRARY_ID,
                                                                   DsStr_PRIVPASSPHRASE ) )
                                                               || ( cp = DefaultStore_getString( DsStore_LIBRARY_ID,
                                                                        DsStr_PASSPHRASE ) ) ) ) {
        session->securityPrivKeyLen = USM_PRIV_KU_LEN;
        if ( KeyTools_generateKu( session->securityAuthProto,
                 session->securityAuthProtoLen,
                 ( u_char* )cp, strlen( cp ),
                 session->securityPrivKey,
                 &session->securityPrivKeyLen )
            != ErrorCode_SUCCESS ) {
            Api_setDetail( "Error generating a key (Ku) from the supplied privacy pass phrase." );
            return PRIOT_ERR_GENERR;
        }
    }

    return ErrorCode_SUCCESS;
}

/*
 * Usm_createUserFromession(Types_Session *session):
 *
 * creates a user in the usm table from the information in a session.
 * If the user already exists, it is updated with the current
 * information from the session
 *
 * Parameters:
 * session -- IN: pointer to the session to use when creating the user.
 *
 * Returns:
 * ErrorCode_SUCCESS
 * ErrorCode_GENERR
 */
int Usm_createUserFromSession( Types_Session* session )
{
    struct Usm_User_s* user;
    int user_just_created = 0;
    char* cp;

    /*
     * - don't create-another/copy-into user for this session by default
     * - bail now (no error) if we don't have an engineID
     */
    if ( API_FLAGS_USER_CREATED == ( session->flags & API_FLAGS_USER_CREATED ) || session->securityModel != PRIOT_SEC_MODEL_USM || session->version != PRIOT_VERSION_3 || session->securityNameLen == 0 || session->securityEngineIDLen == 0 )
        return ErrorCode_SUCCESS;

    DEBUG_MSGTL( ( "usm", "no flag defined...  continuing\n" ) );
    session->flags |= API_FLAGS_USER_CREATED;

    /*
     * now that we have the engineID, create an entry in the USM list
     * for this user using the information in the session
     */
    user = Usm_getUserFromList( session->securityEngineID,
        session->securityEngineIDLen,
        session->securityName,
        Usm_getUserList(), 0 );
    DEBUG_MSGTL( ( "usm", "user exists? x=%p\n", user ) );
    if ( user == NULL ) {
        DEBUG_MSGTL( ( "usm", "Building user %s...\n",
            session->securityName ) );
        /*
         * user doesn't exist so we create and add it
         */
        user = ( struct Usm_User_s* )calloc( 1, sizeof( struct Usm_User_s ) );
        if ( user == NULL )
            return ErrorCode_GENERR;

        /*
         * copy in the securityName
         */
        if ( session->securityName ) {
            user->name = strdup( session->securityName );
            user->secName = strdup( session->securityName );
            if ( user->name == NULL || user->secName == NULL ) {
                Usm_freeUser( user );
                return ErrorCode_GENERR;
            }
        }

        /*
         * copy in the engineID
         */
        user->engineID = ( u_char* )Memory_memdup( session->securityEngineID,
            session->securityEngineIDLen );
        if ( session->securityEngineID && !user->engineID ) {
            Usm_freeUser( user );
            return ErrorCode_GENERR;
        }
        user->engineIDLen = session->securityEngineIDLen;

        user_just_created = 1;
    }

    /*
     * copy the auth protocol
     */
    if ( user->authProtocol == NULL && session->securityAuthProto != NULL ) {
        MEMORY_FREE( user->authProtocol );
        user->authProtocol = Api_duplicateObjid( session->securityAuthProto,
            session->securityAuthProtoLen );
        if ( user->authProtocol == NULL ) {
            Usm_freeUser( user );
            return ErrorCode_GENERR;
        }
        user->authProtocolLen = session->securityAuthProtoLen;
    }

    /*
     * copy the priv protocol
     */
    if ( user->privProtocol == NULL && session->securityPrivProto != NULL ) {
        MEMORY_FREE( user->privProtocol );
        user->privProtocol = Api_duplicateObjid( session->securityPrivProto,
            session->securityPrivProtoLen );
        if ( user->privProtocol == NULL ) {
            Usm_freeUser( user );
            return ErrorCode_GENERR;
        }
        user->privProtocolLen = session->securityPrivProtoLen;
    }

    /*
     * copy in the authentication Key.  If not localized, localize it
     */
    if ( user->authKey == NULL ) {
        if ( session->securityAuthLocalKey != NULL
            && session->securityAuthLocalKeyLen != 0 ) {
            /* already localized key passed in.  use it */
            MEMORY_FREE( user->authKey );
            user->authKey = ( u_char* )Memory_memdup( session->securityAuthLocalKey,
                session->securityAuthLocalKeyLen );
            if ( !user->authKey ) {
                Usm_freeUser( user );
                return ErrorCode_GENERR;
            }
            user->authKeyLen = session->securityAuthLocalKeyLen;
        } else if ( session->securityAuthKey != NULL
            && session->securityAuthKeyLen != 0 ) {
            MEMORY_FREE( user->authKey );
            user->authKey = ( u_char* )calloc( 1, keyUSM_LENGTH_KU_HASHBLOCK );
            if ( user->authKey == NULL ) {
                Usm_freeUser( user );
                return ErrorCode_GENERR;
            }
            user->authKeyLen = keyUSM_LENGTH_KU_HASHBLOCK;
            if ( KeyTools_generateKul( user->authProtocol, user->authProtocolLen,
                     session->securityEngineID,
                     session->securityEngineIDLen,
                     session->securityAuthKey,
                     session->securityAuthKeyLen, user->authKey,
                     &user->authKeyLen )
                != ErrorCode_SUCCESS ) {
                Usm_freeUser( user );
                return ErrorCode_GENERR;
            }
        } else if ( ( cp = DefaultStore_getString( DsStore_LIBRARY_ID,
                          DsStr_AUTHLOCALIZEDKEY ) ) ) {
            size_t buflen = USM_AUTH_KU_LEN;
            MEMORY_FREE( user->authKey );
            user->authKey = ( u_char* )malloc( buflen ); /* max length needed */
            user->authKeyLen = 0;
            /* it will be a hex string */
            if ( !Convert_hexStringToBinaryStringWrapper( &user->authKey, &buflen, &user->authKeyLen,
                     0, cp ) ) {
                Usm_freeUser( user );
                return ErrorCode_GENERR;
            }
        }
    }

    /*
     * copy in the privacy Key.  If not localized, localize it
     */
    if ( user->privKey == NULL ) {
        if ( session->securityPrivLocalKey != NULL
            && session->securityPrivLocalKeyLen != 0 ) {
            /* already localized key passed in.  use it */
            MEMORY_FREE( user->privKey );
            user->privKey = ( u_char* )Memory_memdup( session->securityPrivLocalKey,
                session->securityPrivLocalKeyLen );
            if ( !user->privKey ) {
                Usm_freeUser( user );
                return ErrorCode_GENERR;
            }
            user->privKeyLen = session->securityPrivLocalKeyLen;
        } else if ( session->securityPrivKey != NULL
            && session->securityPrivKeyLen != 0 ) {
            MEMORY_FREE( user->privKey );
            user->privKey = ( u_char* )calloc( 1, keyUSM_LENGTH_KU_HASHBLOCK );
            if ( user->privKey == NULL ) {
                Usm_freeUser( user );
                return ErrorCode_GENERR;
            }
            user->privKeyLen = keyUSM_LENGTH_KU_HASHBLOCK;
            if ( KeyTools_generateKul( user->authProtocol, user->authProtocolLen,
                     session->securityEngineID,
                     session->securityEngineIDLen,
                     session->securityPrivKey,
                     session->securityPrivKeyLen, user->privKey,
                     &user->privKeyLen )
                != ErrorCode_SUCCESS ) {
                Usm_freeUser( user );
                return ErrorCode_GENERR;
            }
        } else if ( ( cp = DefaultStore_getString( DsStore_LIBRARY_ID,
                          DsStr_PRIVLOCALIZEDKEY ) ) ) {
            size_t buflen = USM_PRIV_KU_LEN;
            MEMORY_FREE( user->privKey );
            user->privKey = ( u_char* )malloc( buflen ); /* max length needed */
            user->privKeyLen = 0;
            /* it will be a hex string */
            if ( !Convert_hexStringToBinaryStringWrapper( &user->privKey, &buflen, &user->privKeyLen,
                     0, cp ) ) {
                Usm_freeUser( user );
                return ErrorCode_GENERR;
            }
        }
    }

    if ( user_just_created ) {
        /*
         * add the user into the database
         */
        user->userStatus = tcROW_STATUS_ACTIVE;
        user->userStorageType = tcSTORAGE_TYPE_READONLY;
        Usm_addUser( user );
    }

    return ErrorCode_SUCCESS;
}

/* A wrapper around the hook */
int Usm_createUserFromSessionHook( void* slp, Types_Session* session )
{
    DEBUG_MSGTL( ( "usm", "potentially bootstrapping the USM table from session data\n" ) );
    return Usm_createUserFromSession( session );
}

static int
_Usm_buildProbePdu( Types_Pdu** pdu )
{
    struct Usm_User_s* user;

    /*
     * create the pdu
     */
    if ( !pdu )
        return -1;
    *pdu = Client_pduCreate( PRIOT_MSG_GET );
    if ( !( *pdu ) )
        return -1;
    ( *pdu )->version = PRIOT_VERSION_3;
    ( *pdu )->securityName = strdup( "" );
    ( *pdu )->securityNameLen = strlen( ( *pdu )->securityName );
    ( *pdu )->securityLevel = PRIOT_SEC_LEVEL_NOAUTH;
    ( *pdu )->securityModel = PRIOT_SEC_MODEL_USM;

    /*
     * create the empty user
     */
    user = Usm_getUser( NULL, 0, ( *pdu )->securityName );
    if ( user == NULL ) {
        user = ( struct Usm_User_s* )calloc( 1, sizeof( struct Usm_User_s ) );
        if ( user == NULL ) {
            Api_freePdu( *pdu );
            *pdu = ( Types_Pdu* )NULL;
            return -1;
        }
        user->name = strdup( ( *pdu )->securityName );
        user->secName = strdup( ( *pdu )->securityName );
        user->authProtocolLen = sizeof( usm_noAuthProtocol ) / sizeof( oid );
        user->authProtocol = Api_duplicateObjid( usm_noAuthProtocol, user->authProtocolLen );
        user->privProtocolLen = sizeof( usm_noPrivProtocol ) / sizeof( oid );
        user->privProtocol = Api_duplicateObjid( usm_noPrivProtocol, user->privProtocolLen );
        Usm_addUser( user );
    }
    return 0;
}

int Usm_discoverEngineid( void* slpv, Types_Session* session )
{
    Types_Pdu *pdu = NULL, *response = NULL;
    int status, i;
    struct Api_SessionList_s* slp = ( struct Api_SessionList_s* )slpv;

    if ( _Usm_buildProbePdu( &pdu ) != 0 ) {
        DEBUG_MSGTL( ( "snmp_api", "unable to create probe PDU\n" ) );
        return PRIOT_ERR_GENERR;
    }
    DEBUG_MSGTL( ( "snmp_api", "probing for engineID...\n" ) );
    session->flags |= API_FLAGS_DONT_PROBE; /* prevent recursion */
    status = Client_sessSynchResponse( slp, pdu, &response );

    if ( ( response == NULL ) && ( status == CLIENT_STAT_SUCCESS ) ) {
        status = CLIENT_STAT_ERROR;
    }

    switch ( status ) {
    case CLIENT_STAT_SUCCESS:
        session->s_snmp_errno = ErrorCode_INVALID_MSG; /* XX?? */
        DEBUG_MSGTL( ( "snmp_sess_open",
            "error: expected Report as response to probe: %s (%ld)\n",
            Api_errstring( response->errstat ),
            response->errstat ) );
        break;
    case CLIENT_STAT_ERROR: /* this is what we expected -> Report == STAT_ERROR */
        session->s_snmp_errno = ErrorCode_UNKNOWN_ENG_ID;
        break;
    case CLIENT_STAT_TIMEOUT:
        session->s_snmp_errno = ErrorCode_TIMEOUT;
        break;
    default:
        DEBUG_MSGTL( ( "snmp_sess_open",
            "unable to connect with remote engine: %s (%d)\n",
            Api_errstring( session->s_snmp_errno ),
            session->s_snmp_errno ) );
        break;
    }

    if ( slp->session->securityEngineIDLen == 0 ) {
        DEBUG_MSGTL( ( "snmp_api",
            "unable to determine remote engine ID\n" ) );
        /* clear the flag so that probe occurs on next inform */
        session->flags &= ~API_FLAGS_DONT_PROBE;
        return PRIOT_ERR_GENERR;
    }

    session->s_snmp_errno = ErrorCode_SUCCESS;
    if ( Debug_getDoDebugging() ) {
        DEBUG_MSGTL( ( "snmp_sess_open",
            "  probe found engineID:  " ) );
        for ( i = 0; i < slp->session->securityEngineIDLen; i++ )
            DEBUG_MSG( ( "snmp_sess_open", "%02x",
                slp->session->securityEngineID[ i ] ) );
        DEBUG_MSG( ( "snmp_sess_open", "\n" ) );
    }

    /*
     * if boot/time supplied set it for this engineID
     */
    if ( session->engineBoots || session->engineTime ) {
        Engine_set( session->securityEngineID,
            session->securityEngineIDLen,
            session->engineBoots, session->engineTime,
            TRUE );
    }
    return ErrorCode_SUCCESS;
}

void Usm_initUsm( void )
{
    struct SecModDefinition_s* def;
    char* type;

    DEBUG_MSGTL( ( "init_usm", "unit_usm: %"
                               "l"
                               "u %"
                               "l"
                               "u\n",
        usm_noPrivProtocol[ 0 ], usm_noPrivProtocol[ 1 ] ) );

    Scapi_init(); /* initalize scapi code */

    /*
     * register ourselves as a security service
     */
    def = MEMORY_MALLOC_STRUCT( SecModDefinition_s );
    if ( def == NULL )
        return;
    /*
     * XXX: def->init_sess_secmod move stuff from snmp_api.c
     */
    def->encodeReverseFunction = Usm_secmodRgenerateOutMsg;
    def->encodeForwardFunction = Usm_secmodGenerateOutMsg;
    def->decodeFunction = Usm_secmodProcessInMsg;
    def->pduFreeStateRefFunction = Usm_freeUsmStateReference;
    def->sessionSetupFunction = Usm_sessionInit;
    def->handleReportFunction = Usm_handleReport;
    def->probeEngineIdFunction = Usm_discoverEngineid;
    def->postProbeEngineIdFunction = Usm_createUserFromSessionHook;
    SecMod_register( USM_SEC_MODEL_NUMBER, "usm", def );

    Callback_register( CallbackMajor_LIBRARY,
        CallbackMinor_POST_PREMIB_READ_CONFIG,
        Usm_initUsmPostConfig, NULL );

    Callback_register( CallbackMajor_LIBRARY,
        CallbackMinor_SHUTDOWN,
        Usm_deinitUsmPostConfig, NULL );

    Callback_register( CallbackMajor_LIBRARY,
        CallbackMinor_SHUTDOWN,
        V3_freeEngineID, NULL );

    ReadConfig_registerConfigHandler( "priot", "defAuthType", Usm_v3AuthtypeConf,
        NULL, "MD5|SHA" );
    ReadConfig_registerConfigHandler( "priot", "defPrivType", Usm_v3PrivtypeConf,
        NULL,
        "DES|AES" );

    /*
     * Free stuff at shutdown time
     */
    Callback_register( CallbackMajor_LIBRARY,
        CallbackMinor_SHUTDOWN,
        Usm_freeEnginetimeOnShutdown, NULL );

    type = DefaultStore_getString( DsStore_LIBRARY_ID, DsStr_APPTYPE );

    ReadConfig_registerConfigHandler( type, "userSetAuthPass", Usm_setPassword,
        NULL, NULL );
    ReadConfig_registerConfigHandler( type, "userSetPrivPass", Usm_setPassword,
        NULL, NULL );
    ReadConfig_registerConfigHandler( type, "userSetAuthKey", Usm_setPassword, NULL,
        NULL );
    ReadConfig_registerConfigHandler( type, "userSetPrivKey", Usm_setPassword, NULL,
        NULL );
    ReadConfig_registerConfigHandler( type, "userSetAuthLocalKey", Usm_setPassword,
        NULL, NULL );
    ReadConfig_registerConfigHandler( type, "userSetPrivLocalKey", Usm_setPassword,
        NULL, NULL );
}

void Usm_initUsmConf( const char* app )
{
    ReadConfig_registerConfigHandler( app, "usmUser",
        Usm_parseConfigUsmUser, NULL, NULL );
    ReadConfig_registerConfigHandler( app, "createUser",
        Usm_parseCreateUsmUser, NULL,
        "username [-e ENGINEID] (MD5|SHA) authpassphrase [DES [privpassphrase]]" );

    /*
     * we need to be called back later
     */
    Callback_register( CallbackMajor_LIBRARY, CallbackMinor_STORE_DATA,
        Usm_storeUsers, NULL );
}

/*
 * initializations for the USM.
 *
 * Should be called after the (engineid) configuration files have been read.
 *
 * Set "arbitrary" portion of salt to a random number.
 */
int Usm_initUsmPostConfig( int majorid, int minorid, void* serverarg,
    void* clientarg )
{
    size_t salt_integer_len = sizeof( _usm_saltInteger );

    if ( Scapi_random( ( u_char* )&_usm_saltInteger, &salt_integer_len ) != ErrorCode_SUCCESS ) {
        DEBUG_MSGTL( ( "usm", "Scapi_random() failed: using time() as salt.\n" ) );
        _usm_saltInteger = ( u_int )time( NULL );
    }

    salt_integer_len = sizeof( _usm_saltInteger64One );
    if ( Scapi_random( ( u_char* )&_usm_saltInteger64One, &salt_integer_len ) != ErrorCode_SUCCESS ) {
        DEBUG_MSGTL( ( "usm", "Scapi_random() failed: using time() as aes1 salt.\n" ) );
        _usm_saltInteger64One = ( u_int )time( NULL );
    }
    salt_integer_len = sizeof( _usm_saltInteger64One );
    if ( Scapi_random( ( u_char* )&_usm_saltInteger64Two, &salt_integer_len ) != ErrorCode_SUCCESS ) {
        DEBUG_MSGTL( ( "usm", "Scapi_random() failed: using time() as aes2 salt.\n" ) );
        _usm_saltInteger64Two = ( u_int )time( NULL );
    }

    _usm_noNameUser = Usm_createInitialUser( "", usm_hMACMD5AuthProtocol,
        UTILITIES_USM_LENGTH_OID_TRANSFORM,
        usm_dESPrivProtocol,
        UTILITIES_USM_LENGTH_OID_TRANSFORM );

    if ( _usm_noNameUser ) {
        MEMORY_FREE( _usm_noNameUser->engineID );
        _usm_noNameUser->engineIDLen = 0;
    }

    return ErrorCode_SUCCESS;
} /* end Usm_initUsmPostConfig() */

int Usm_deinitUsmPostConfig( int majorid, int minorid, void* serverarg,
    void* clientarg )
{
    if ( Usm_freeUser( _usm_noNameUser ) != NULL ) {
        DEBUG_MSGTL( ( "Usm_deinitUsmPostConfig", "could not free initial user\n" ) );
        return ErrorCode_GENERR;
    }
    _usm_noNameUser = NULL;

    DEBUG_MSGTL( ( "Usm_deinitUsmPostConfig", "initial user removed\n" ) );
    return ErrorCode_SUCCESS;
} /* end Usm_deinitUsmPostConfig() */

void Usm_clearUserList( void )
{
    struct Usm_User_s *tmp = _usm_userList, *next = NULL;

    while ( tmp != NULL ) {
        next = tmp->next;
        Usm_freeUser( tmp );
        tmp = next;
    }
    _usm_userList = NULL;
}

void Usm_shutdownUsm( void )
{
    Engine_clear();
    Usm_clearUserList();
}

/*******************************************************************-o-******
 * Usm_checkSecLevel
 *
 * Parameters:
 *	 level
 *	*user
 *
 * Returns:
 *	0	On success,
 *	-1	Otherwise.
 *
 * Checks that a given security level is valid for a given user.
 */
int Usm_checkSecLevel( int level, struct Usm_User_s* user )
{

    if ( user->userStatus != tcROW_STATUS_ACTIVE )
        return -1;

    DEBUG_MSGTL( ( "comparex", "Comparing: %"
                               "l"
                               "u %"
                               "l"
                               "u ",
        usm_noPrivProtocol[ 0 ], usm_noPrivProtocol[ 1 ] ) );
    DEBUG_MSGOID( ( "comparex", usm_noPrivProtocol,
        sizeof( usm_noPrivProtocol ) / sizeof( oid ) ) );
    DEBUG_MSG( ( "comparex", "\n" ) );
    if ( level == PRIOT_SEC_LEVEL_AUTHPRIV
        && ( Api_oidEquals( user->privProtocol, user->privProtocolLen,
                 usm_noPrivProtocol,
                 sizeof( usm_noPrivProtocol ) / sizeof( oid ) )
               == 0 ) ) {
        DEBUG_MSGTL( ( "usm", "Level: %d\n", level ) );
        DEBUG_MSGTL( ( "usm", "User (%s) Auth Protocol: ", user->name ) );
        DEBUG_MSGOID( ( "usm", user->authProtocol, user->authProtocolLen ) );
        DEBUG_MSG( ( "usm", ", User Priv Protocol: " ) );
        DEBUG_MSGOID( ( "usm", user->privProtocol, user->privProtocolLen ) );
        DEBUG_MSG( ( "usm", "\n" ) );
        return 1;
    }
    if ( ( level == PRIOT_SEC_LEVEL_AUTHPRIV
             || level == PRIOT_SEC_LEVEL_AUTHNOPRIV )
        && ( Api_oidEquals( user->authProtocol, user->authProtocolLen, usm_noAuthProtocol,
                 sizeof( usm_noAuthProtocol ) / sizeof( oid ) )
               == 0 ) ) {
        DEBUG_MSGTL( ( "usm", "Level: %d\n", level ) );
        DEBUG_MSGTL( ( "usm", "User (%s) Auth Protocol: ", user->name ) );
        DEBUG_MSGOID( ( "usm", user->authProtocol, user->authProtocolLen ) );
        DEBUG_MSG( ( "usm", ", User Priv Protocol: " ) );
        DEBUG_MSGOID( ( "usm", user->privProtocol, user->privProtocolLen ) );
        DEBUG_MSG( ( "usm", "\n" ) );
        return 1;
    }

    return 0;

} /* end Usm_checkSecLevel() */

/*******************************************************************-o-******
 * Usm_checkSecLevelVsProtocols
 *
 * Parameters:
 *	 level
 *	*authProtocol
 *	 authProtocolLen
 *	*privProtocol
 *	 privProtocolLen
 *
 * Returns:
 *	0	On success,
 *	1	Otherwise.
 *
 * Same as above but with explicitly named transform types instead of taking
 * from the usmUser structure.
 */
int Usm_checkSecLevelVsProtocols( int level,
    const oid* authProtocol,
    u_int authProtocolLen,
    const oid* privProtocol,
    u_int privProtocolLen )
{

    if ( level == PRIOT_SEC_LEVEL_AUTHPRIV
        && ( Api_oidEquals( privProtocol, privProtocolLen, usm_noPrivProtocol,
                 sizeof( usm_noPrivProtocol ) / sizeof( oid ) )
               == 0 ) ) {
        DEBUG_MSGTL( ( "usm", "Level: %d\n", level ) );
        DEBUG_MSGTL( ( "usm", "Auth Protocol: " ) );
        DEBUG_MSGOID( ( "usm", authProtocol, authProtocolLen ) );
        DEBUG_MSG( ( "usm", ", Priv Protocol: " ) );
        DEBUG_MSGOID( ( "usm", privProtocol, privProtocolLen ) );
        DEBUG_MSG( ( "usm", "\n" ) );
        return 1;
    }
    if ( ( level == PRIOT_SEC_LEVEL_AUTHPRIV
             || level == PRIOT_SEC_LEVEL_AUTHNOPRIV )
        && ( Api_oidEquals( authProtocol, authProtocolLen, usm_noAuthProtocol,
                 sizeof( usm_noAuthProtocol ) / sizeof( oid ) )
               == 0 ) ) {
        DEBUG_MSGTL( ( "usm", "Level: %d\n", level ) );
        DEBUG_MSGTL( ( "usm", "Auth Protocol: " ) );
        DEBUG_MSGOID( ( "usm", authProtocol, authProtocolLen ) );
        DEBUG_MSG( ( "usm", ", Priv Protocol: " ) );
        DEBUG_MSGOID( ( "usm", privProtocol, privProtocolLen ) );
        DEBUG_MSG( ( "usm", "\n" ) );
        return 1;
    }

    return 0;

} /* end Usm_checkSecLevelVsProtocols() */

/*
 * Usm_getUser(): Returns a user from usm_userList based on the engineID,
 * engineIDLen and name of the requested user.
 */

struct Usm_User_s*
Usm_getUser( u_char* engineID, size_t engineIDLen, char* name )
{
    DEBUG_MSGTL( ( "usm", "getting user %s\n", name ) );
    return Usm_getUserFromList( engineID, engineIDLen, name, _usm_userList,
        1 );
}

struct Usm_User_s*
Usm_getUserFromList( u_char* engineID, size_t engineIDLen,
    char* name, struct Usm_User_s* puserList,
    int use_default )
{
    struct Usm_User_s* ptr;
    char noName[] = "";
    if ( name == NULL )
        name = noName;
    for ( ptr = puserList; ptr != NULL; ptr = ptr->next ) {
        if ( ptr->name && !strcmp( ptr->name, name ) ) {
            DEBUG_MSGTL( ( "usm", "match on user %s\n", ptr->name ) );
            if ( ptr->engineIDLen == engineIDLen && ( ( ptr->engineID == NULL && engineID == NULL ) || ( ptr->engineID != NULL && engineID != NULL && memcmp( ptr->engineID, engineID, engineIDLen ) == 0 ) ) )
                return ptr;
            DEBUG_MSGTL( ( "usm", "no match on engineID (" ) );
            if ( engineID ) {
                DEBUG_MSGHEX( ( "usm", engineID, engineIDLen ) );
            } else {
                DEBUG_MSGTL( ( "usm", "Empty EngineID" ) );
            }
            DEBUG_MSG( ( "usm", ")\n" ) );
        }
    }

    /*
     * return "" user used to facilitate engineID discovery
     */
    if ( use_default && !strcmp( name, "" ) )
        return _usm_noNameUser;
    return NULL;
}

/*
 * Usm_addUser(): Add's a user to the usm_userList, sorted by the
 * engineIDLength then the engineID then the name length then the name
 * to facilitate getNext calls on a usmUser table which is indexed by
 * these values.
 *
 * returns the head of the list (which could change due to this add).
 */

struct Usm_User_s*
Usm_addUser( struct Usm_User_s* user )
{
    struct Usm_User_s* uptr;
    uptr = Usm_addUserToList( user, _usm_userList );
    if ( uptr != NULL )
        _usm_userList = uptr;
    return uptr;
}

struct Usm_User_s*
Usm_addUserToList( struct Usm_User_s* user, struct Usm_User_s* puserList )
{
    struct Usm_User_s *nptr, *pptr, *optr;

    /*
     * loop through puserList till we find the proper, sorted place to
     * insert the new user
     */
    /* XXX - how to handle a NULL user->name ?? */
    /* XXX - similarly for a NULL nptr->name ?? */
    for ( nptr = puserList, pptr = NULL; nptr != NULL;
          pptr = nptr, nptr = nptr->next ) {
        if ( nptr->engineIDLen > user->engineIDLen )
            break;

        if ( user->engineID == NULL && nptr->engineID != NULL )
            break;

        if ( nptr->engineIDLen == user->engineIDLen && ( nptr->engineID != NULL && user->engineID != NULL && memcmp( nptr->engineID, user->engineID, user->engineIDLen ) > 0 ) )
            break;

        if ( !( nptr->engineID == NULL && user->engineID != NULL ) ) {
            if ( nptr->engineIDLen == user->engineIDLen && ( ( nptr->engineID == NULL && user->engineID == NULL ) || memcmp( nptr->engineID, user->engineID, user->engineIDLen ) == 0 )
                && strlen( nptr->name ) > strlen( user->name ) )
                break;

            if ( nptr->engineIDLen == user->engineIDLen && ( ( nptr->engineID == NULL && user->engineID == NULL ) || memcmp( nptr->engineID, user->engineID, user->engineIDLen ) == 0 )
                && strlen( nptr->name ) == strlen( user->name )
                && strcmp( nptr->name, user->name ) > 0 )
                break;

            if ( nptr->engineIDLen == user->engineIDLen && ( ( nptr->engineID == NULL && user->engineID == NULL ) || memcmp( nptr->engineID, user->engineID, user->engineIDLen ) == 0 )
                && strlen( nptr->name ) == strlen( user->name )
                && strcmp( nptr->name, user->name ) == 0 ) {
                /*
                 * the user is an exact match of a previous entry.
                 * Credentials may be different, though, so remove
                 * the old entry (and add the new one)!
                 */
                if ( pptr ) { /* change prev's next pointer */
                    pptr->next = nptr->next;
                }
                if ( nptr->next ) { /* change next's prev pointer */
                    nptr->next->prev = pptr;
                }
                optr = nptr;
                nptr = optr->next; /* add new user at this position */
                /* free the old user */
                optr->next = NULL;
                optr->prev = NULL;
                Usm_freeUser( optr );
                break; /* new user will be added below */
            }
        }
    }

    /*
     * nptr should now point to the user that we need to add ourselves
     * in front of, and pptr should be our new 'prev'.
     */

    /*
     * change our pointers
     */
    user->prev = pptr;
    user->next = nptr;

    /*
     * change the next's prev pointer
     */
    if ( user->next )
        user->next->prev = user;

    /*
     * change the prev's next pointer
     */
    if ( user->prev )
        user->prev->next = user;

    /*
     * rewind to the head of the list and return it (since the new head
     * could be us, we need to notify the above routine who the head now is.
     */
    for ( pptr = user; pptr->prev != NULL; pptr = pptr->prev )
        ;
    return pptr;
}

/*
 * Usm_removeUser(): finds and removes a user from a list
 */
struct Usm_User_s*
Usm_removeUser( struct Usm_User_s* user )
{
    return Usm_removeUserFromList( user, &_usm_userList );
}

struct Usm_User_s*
Usm_removeUserFromList( struct Usm_User_s* user,
    struct Usm_User_s** ppuserList )
{
    struct Usm_User_s *nptr, *pptr;

    /*
     * NULL pointers aren't allowed
     */
    if ( ppuserList == NULL )
        return NULL;

    if ( *ppuserList == NULL )
        return NULL;

    /*
     * find the user in the list
     */
    for ( nptr = *ppuserList, pptr = NULL; nptr != NULL;
          pptr = nptr, nptr = nptr->next ) {
        if ( nptr == user )
            break;
    }

    if ( nptr ) {
        /*
         * remove the user from the linked list
         */
        if ( pptr ) {
            pptr->next = nptr->next;
        }
        if ( nptr->next ) {
            nptr->next->prev = pptr;
        }
    } else {
        /*
         * user didn't exist
         */
        return NULL;
    }
    if ( nptr == *ppuserList ) /* we're the head of the list, need to change
                                 * * the head to the next user */
        *ppuserList = nptr->next;
    return *ppuserList;
} /* end Usm_removeUserFromList() */

/*
 * Usm_freeUser():  calls free() on all needed parts of struct Usm_User_s and
 * the user himself.
 *
 * Note: This should *not* be called on an object in a list (IE,
 * remove it from the list first, and set next and prev to NULL), but
 * will try to reconnect the list pieces again if it is called this
 * way.  If called on the head of the list, the entire list will be
 * lost.
 */
struct Usm_User_s*
Usm_freeUser( struct Usm_User_s* user )
{
    if ( user == NULL )
        return NULL;

    MEMORY_FREE( user->engineID );
    MEMORY_FREE( user->name );
    MEMORY_FREE( user->secName );
    MEMORY_FREE( user->cloneFrom );
    MEMORY_FREE( user->userPublicString );
    MEMORY_FREE( user->authProtocol );
    MEMORY_FREE( user->privProtocol );

    if ( user->authKey != NULL ) {
        MEMORY_FILL_ZERO( user->authKey, user->authKeyLen );
        MEMORY_FREE( user->authKey );
    }

    if ( user->privKey != NULL ) {
        MEMORY_FILL_ZERO( user->privKey, user->privKeyLen );
        MEMORY_FREE( user->privKey );
    }

    /*
     * FIX  Why not put this check *first?*
     */
    if ( user->prev != NULL ) { /* ack, this shouldn't happen */
        user->prev->next = user->next;
    }
    if ( user->next != NULL ) {
        user->next->prev = user->prev;
        if ( user->prev != NULL ) /* ack this is really bad, because it means
                                 * * we'll loose the head of some structure tree */
            DEBUG_MSGTL( ( "usm",
                "Severe: Asked to free the head of a usmUser tree somewhere." ) );
    }

    MEMORY_FILL_ZERO( user, sizeof( *user ) );
    MEMORY_FREE( user );

    return NULL; /* for convenience to returns from calling functions */

} /* end Usm_freeUser() */

/*
 * take a given user and clone the security info into another
 */
struct Usm_User_s*
Usm_cloneFromUser( struct Usm_User_s* from, struct Usm_User_s* to )
{
    /*
     * copy the authProtocol oid row pointer
     */
    MEMORY_FREE( to->authProtocol );

    if ( ( to->authProtocol = Api_duplicateObjid( from->authProtocol,
               from->authProtocolLen ) )
        != NULL )
        to->authProtocolLen = from->authProtocolLen;
    else
        to->authProtocolLen = 0;

    /*
     * copy the authKey
     */
    MEMORY_FREE( to->authKey );

    if ( from->authKeyLen > 0 && ( to->authKey = ( u_char* )malloc( from->authKeyLen ) ) != NULL ) {
        to->authKeyLen = from->authKeyLen;
        memcpy( to->authKey, from->authKey, to->authKeyLen );
    } else {
        to->authKey = NULL;
        to->authKeyLen = 0;
    }

    /*
     * copy the privProtocol oid row pointer
     */
    MEMORY_FREE( to->privProtocol );

    if ( ( to->privProtocol = Api_duplicateObjid( from->privProtocol,
               from->privProtocolLen ) )
        != NULL )
        to->privProtocolLen = from->privProtocolLen;
    else
        to->privProtocolLen = 0;

    /*
     * copy the privKey
     */
    MEMORY_FREE( to->privKey );

    if ( from->privKeyLen > 0 && ( to->privKey = ( u_char* )malloc( from->privKeyLen ) ) != NULL ) {
        to->privKeyLen = from->privKeyLen;
        memcpy( to->privKey, from->privKey, to->privKeyLen );
    } else {
        to->privKey = NULL;
        to->privKeyLen = 0;
    }
    return to;
}

/*
 * usm_create_user(void):
 * create a default empty user, instantiating only the auth/priv
 * protocols to noAuth and noPriv OID pointers
 */
struct Usm_User_s*
Usm_createUser( void )
{
    struct Usm_User_s* newUser;

    /*
     * create the new user
     */
    newUser = ( struct Usm_User_s* )calloc( 1, sizeof( struct Usm_User_s ) );
    if ( newUser == NULL )
        return NULL;

    /*
     * fill the auth/priv protocols
     */
    if ( ( newUser->authProtocol = Api_duplicateObjid( usm_noAuthProtocol,
               sizeof( usm_noAuthProtocol ) / sizeof( oid ) ) )
        == NULL )
        return Usm_freeUser( newUser );
    newUser->authProtocolLen = sizeof( usm_noAuthProtocol ) / sizeof( oid );

    if ( ( newUser->privProtocol = Api_duplicateObjid( usm_noPrivProtocol,
               sizeof( usm_noPrivProtocol ) / sizeof( oid ) ) )
        == NULL )
        return Usm_freeUser( newUser );
    newUser->privProtocolLen = sizeof( usm_noPrivProtocol ) / sizeof( oid );

    /*
     * set the storage type to nonvolatile, and the status to ACTIVE
     */
    newUser->userStorageType = tcSTORAGE_TYPE_NONVOLATILE;
    newUser->userStatus = tcROW_STATUS_ACTIVE;
    return newUser;

} /* end usm_clone_user() */

/*
 * usm_create_initial_user(void):
 * creates an initial user, filled with the defaults defined in the
 * USM document.
 */
struct Usm_User_s*
Usm_createInitialUser( const char* name,
    const oid* authProtocol, size_t authProtocolLen,
    const oid* privProtocol, size_t privProtocolLen )
{
    struct Usm_User_s* newUser = Usm_createUser();
    if ( newUser == NULL )
        return NULL;

    if ( ( newUser->name = strdup( name ) ) == NULL )
        return Usm_freeUser( newUser );

    if ( ( newUser->secName = strdup( name ) ) == NULL )
        return Usm_freeUser( newUser );

    if ( ( newUser->engineID = V3_generateEngineID( &newUser->engineIDLen ) ) == NULL )
        return Usm_freeUser( newUser );

    if ( ( newUser->cloneFrom = ( oid* )malloc( sizeof( oid ) * 2 ) ) == NULL )
        return Usm_freeUser( newUser );
    newUser->cloneFrom[ 0 ] = 0;
    newUser->cloneFrom[ 1 ] = 0;
    newUser->cloneFromLen = 2;

    MEMORY_FREE( newUser->privProtocol );
    if ( ( newUser->privProtocol = Api_duplicateObjid( privProtocol,
               privProtocolLen ) )
        == NULL ) {
        return Usm_freeUser( newUser );
    }
    newUser->privProtocolLen = privProtocolLen;

    MEMORY_FREE( newUser->authProtocol );
    if ( ( newUser->authProtocol = Api_duplicateObjid( authProtocol,
               authProtocolLen ) )
        == NULL ) {
        return Usm_freeUser( newUser );
    }
    newUser->authProtocolLen = authProtocolLen;

    newUser->userStatus = tcROW_STATUS_ACTIVE;
    newUser->userStorageType = tcSTORAGE_TYPE_READONLY;

    return newUser;
}

/*
 * this is a callback that can store all known users based on a
 * previously registered application ID
 */
int Usm_storeUsers( int majorID, int minorID, void* serverarg, void* clientarg )
{
    /*
     * figure out our application name
     */
    char* appname = ( char* )clientarg;
    if ( appname == NULL ) {
        appname = DefaultStore_getString( DsStore_LIBRARY_ID,
            DsStr_APPTYPE );
    }

    /*
     * save the user base
     */
    Usm_saveUsers( "usmUser", appname );

    /*
     * never fails
     */
    return ErrorCode_SUCCESS;
}

/*
 * usm_save_users(): saves a list of users to the persistent cache
 */
void Usm_saveUsers( const char* token, const char* type )
{
    Usm_saveUsersFromList( _usm_userList, token, type );
}

void Usm_saveUsersFromList( struct Usm_User_s* puserList, const char* token,
    const char* type )
{
    struct Usm_User_s* uptr;
    for ( uptr = puserList; uptr != NULL; uptr = uptr->next ) {
        if ( uptr->userStorageType == tcSTORAGE_TYPE_NONVOLATILE )
            Usm_saveUser( uptr, token, type );
    }
}

/*
 * usm_save_user(): saves a user to the persistent cache
 */
void Usm_saveUser( struct Usm_User_s* user, const char* token, const char* type )
{
    char line[ 4096 ];
    char* cptr;

    memset( line, 0, sizeof( line ) );

    sprintf( line, "%s %d %d ", token, user->userStatus,
        user->userStorageType );
    cptr = &line[ strlen( line ) ]; /* the NULL */
    cptr = ReadConfig_saveOctetString( cptr, user->engineID,
        user->engineIDLen );
    *cptr++ = ' ';
    cptr = ReadConfig_saveOctetString( cptr, ( u_char* )user->name,
        ( user->name == NULL ) ? 0 : strlen( user->name ) );
    *cptr++ = ' ';
    cptr = ReadConfig_saveOctetString( cptr, ( u_char* )user->secName,
        ( user->secName == NULL ) ? 0 : strlen( user->secName ) );
    *cptr++ = ' ';
    cptr = ReadConfig_saveObjid( cptr, user->cloneFrom, user->cloneFromLen );
    *cptr++ = ' ';
    cptr = ReadConfig_saveObjid( cptr, user->authProtocol,
        user->authProtocolLen );
    *cptr++ = ' ';
    cptr = ReadConfig_saveOctetString( cptr, user->authKey,
        user->authKeyLen );
    *cptr++ = ' ';
    cptr = ReadConfig_saveObjid( cptr, user->privProtocol,
        user->privProtocolLen );
    *cptr++ = ' ';
    cptr = ReadConfig_saveOctetString( cptr, user->privKey,
        user->privKeyLen );
    *cptr++ = ' ';
    cptr = ReadConfig_saveOctetString( cptr, user->userPublicString,
        user->userPublicStringLen );

    ReadConfig_store( type, line );
}

/*
 * usm_parse_user(): reads in a line containing a saved user profile
 * and returns a pointer to a newly created struct Usm_User_s.
 */
struct Usm_User_s*
Usm_readUser( const char* line )
{
    struct Usm_User_s* user;
    size_t len;
    size_t expected_privKeyLen = 0;

    user = Usm_createUser();
    if ( user == NULL )
        return NULL;

    user->userStatus = atoi( line );
    line = ReadConfig_skipTokenConst( line );
    user->userStorageType = atoi( line );
    line = ReadConfig_skipTokenConst( line );
    line = ReadConfig_readOctetStringConst( line, &user->engineID,
        &user->engineIDLen );

    /*
     * set the lcd entry for this engineID to the minimum boots/time
     * values so that its a known engineid and won't return a report pdu.
     * This is mostly important when receiving v3 traps so that the usm
     * will at least continue processing them.
     */
    Engine_set( user->engineID, user->engineIDLen, 1, 0, 0 );

    line = ReadConfig_readOctetString( line, ( u_char** )&user->name,
        &len );
    line = ReadConfig_readOctetString( line, ( u_char** )&user->secName,
        &len );
    MEMORY_FREE( user->cloneFrom );
    user->cloneFromLen = 0;

    line = ReadConfig_readObjidConst( line, &user->cloneFrom,
        &user->cloneFromLen );

    MEMORY_FREE( user->authProtocol );
    user->authProtocolLen = 0;

    line = ReadConfig_readObjidConst( line, &user->authProtocol,
        &user->authProtocolLen );
    line = ReadConfig_readOctetStringConst( line, &user->authKey,
        &user->authKeyLen );
    MEMORY_FREE( user->privProtocol );
    user->privProtocolLen = 0;

    line = ReadConfig_readObjidConst( line, &user->privProtocol,
        &user->privProtocolLen );
    line = ReadConfig_readOctetString( line, &user->privKey,
        &user->privKeyLen );
    if ( UTILITIES_ISTRANSFORM( user->privProtocol, dESPriv ) ) {
        /* DES uses a 128 bit key, 64 bits of which is a salt */
        expected_privKeyLen = 16;
    }
    if ( UTILITIES_ISTRANSFORM( user->privProtocol, aESPriv ) ) {
        expected_privKeyLen = 16;
    }
    /* For backwards compatibility */
    if ( user->privKeyLen > expected_privKeyLen ) {
        user->privKeyLen = expected_privKeyLen;
    }

    line = ReadConfig_readOctetString( line, &user->userPublicString,
        &user->userPublicStringLen );
    return user;
}

/*
 * priotd.conf parsing routines
 */
void Usm_parseConfigUsmUser( const char* token, char* line )
{
    struct Usm_User_s* uptr;

    uptr = Usm_readUser( line );
    if ( uptr )
        Usm_addUser( uptr );
}

/*******************************************************************-o-******
 * Usm_setPassword
 *
 * Parameters:
 *	*token
 *	*line
 *
 *
 * format: userSetAuthPass     secname engineIDLen engineID pass
 *     or: userSetPrivPass     secname engineIDLen engineID pass
 *     or: userSetAuthKey      secname engineIDLen engineID KuLen Ku
 *     or: userSetPrivKey      secname engineIDLen engineID KuLen Ku
 *     or: userSetAuthLocalKey secname engineIDLen engineID KulLen Kul
 *     or: userSetPrivLocalKey secname engineIDLen engineID KulLen Kul
 *
 * type is:	1=passphrase; 2=Ku; 3=Kul.
 *
 *
 * ASSUMES  Passwords are null-terminated printable strings.
 */
void Usm_setPassword( const char* token, char* line )
{
    char* cp;
    char nameBuf[ UTILITIES_MAX_BUFFER ];
    u_char* engineID = NULL;
    size_t engineIDLen = 0;
    struct Usm_User_s* user;

    cp = ReadConfig_copyNword( line, nameBuf, sizeof( nameBuf ) );
    if ( cp == NULL ) {
        ReadConfig_configPerror( "invalid name specifier" );
        return;
    }

    DEBUG_MSGTL( ( "usm", "comparing: %s and %s\n", cp, USM_WILDCARDSTRING ) );
    if ( strncmp( cp, USM_WILDCARDSTRING, strlen( USM_WILDCARDSTRING ) ) == 0 ) {
        /*
         * match against all engineIDs we know about
         */
        cp = ReadConfig_skipToken( cp );
        for ( user = _usm_userList; user != NULL; user = user->next ) {
            if ( user->secName && strcmp( user->secName, nameBuf ) == 0 ) {
                Usm_setUserPassword( user, token, cp );
            }
        }
    } else {
        cp = ReadConfig_readOctetString( cp, &engineID, &engineIDLen );
        if ( cp == NULL ) {
            ReadConfig_configPerror( "invalid engineID specifier" );
            MEMORY_FREE( engineID );
            return;
        }

        user = Usm_getUser( engineID, engineIDLen, nameBuf );
        if ( user == NULL ) {
            ReadConfig_configPerror( "not a valid user/engineID pair" );
            MEMORY_FREE( engineID );
            return;
        }
        Usm_setUserPassword( user, token, cp );
        MEMORY_FREE( engineID );
    }
}

/*
 * uses the rest of LINE to configure USER's password of type TOKEN
 */
void Usm_setUserPassword( struct Usm_User_s* user, const char* token, char* line )
{
    char* cp = line;
    u_char* engineID = user->engineID;
    size_t engineIDLen = user->engineIDLen;

    u_char** key;
    size_t* keyLen;
    u_char userKey[ UTILITIES_MAX_BUFFER_SMALL ];
    size_t userKeyLen = UTILITIES_MAX_BUFFER_SMALL;
    u_char* userKeyP = userKey;
    int type, ret;

    /*
     * Retrieve the "old" key and set the key type.
     */
    if ( !token ) {
        return;
    } else if ( strcmp( token, "userSetAuthPass" ) == 0 ) {
        key = &user->authKey;
        keyLen = &user->authKeyLen;
        type = 0;
    } else if ( strcmp( token, "userSetPrivPass" ) == 0 ) {
        key = &user->privKey;
        keyLen = &user->privKeyLen;
        type = 0;
    } else if ( strcmp( token, "userSetAuthKey" ) == 0 ) {
        key = &user->authKey;
        keyLen = &user->authKeyLen;
        type = 1;
    } else if ( strcmp( token, "userSetPrivKey" ) == 0 ) {
        key = &user->privKey;
        keyLen = &user->privKeyLen;
        type = 1;
    } else if ( strcmp( token, "userSetAuthLocalKey" ) == 0 ) {
        key = &user->authKey;
        keyLen = &user->authKeyLen;
        type = 2;
    } else if ( strcmp( token, "userSetPrivLocalKey" ) == 0 ) {
        key = &user->privKey;
        keyLen = &user->privKeyLen;
        type = 2;
    } else {
        /*
         * no old key, or token was not recognized
         */
        return;
    }

    if ( *key ) {
        /*
         * (destroy and) free the old key
         */
        memset( *key, 0, *keyLen );
        MEMORY_FREE( *key );
    }

    if ( type == 0 ) {
        /*
         * convert the password into a key
         */
        if ( cp == NULL ) {
            ReadConfig_configPerror( "missing user password" );
            return;
        }
        ret = KeyTools_generateKu( user->authProtocol, user->authProtocolLen,
            ( u_char* )cp, strlen( cp ), userKey, &userKeyLen );

        if ( ret != ErrorCode_SUCCESS ) {
            ReadConfig_configPerror( "setting key failed (in sc_genKu())" );
            return;
        }
    } else if ( type == 1 ) {
        cp = ReadConfig_readOctetString( cp, &userKeyP, &userKeyLen );

        if ( cp == NULL ) {
            ReadConfig_configPerror( "invalid user key" );
            return;
        }
    }

    if ( type < 2 ) {
        *key = ( u_char* )malloc( UTILITIES_MAX_BUFFER_SMALL );
        *keyLen = UTILITIES_MAX_BUFFER_SMALL;
        ret = KeyTools_generateKul( user->authProtocol, user->authProtocolLen,
            engineID, engineIDLen,
            userKey, userKeyLen, *key, keyLen );
        if ( ret != ErrorCode_SUCCESS ) {
            ReadConfig_configPerror( "setting key failed (in Keytools_generateKul())" );
            return;
        }

        /*
         * (destroy and) free the old key
         */
        memset( userKey, 0, sizeof( userKey ) );

    } else {
        /*
         * the key is given, copy it in
         */
        cp = ReadConfig_readOctetString( cp, key, keyLen );

        if ( cp == NULL ) {
            ReadConfig_configPerror( "invalid localized user key" );
            return;
        }
    }
} /* end Usm_setPassword() */

void Usm_parseCreateUsmUser( const char* token, char* line )
{
    char* cp;
    char buf[ UTILITIES_MAX_BUFFER_MEDIUM ];
    struct Usm_User_s* newuser;
    u_char userKey[ UTILITIES_MAX_BUFFER_SMALL ], *tmpp;
    size_t userKeyLen = UTILITIES_MAX_BUFFER_SMALL;
    size_t privKeyLen = 0;
    size_t ret;
    int ret2;
    int testcase;

    newuser = Usm_createUser();

    /*
     * READ: Security Name
     */
    cp = ReadConfig_copyNword( line, buf, sizeof( buf ) );

    /*
     * might be a -e ENGINEID argument
     */
    if ( strcmp( buf, "-e" ) == 0 ) {
        size_t ebuf_len = 32, eout_len = 0;
        u_char* ebuf = ( u_char* )malloc( ebuf_len );

        if ( ebuf == NULL ) {
            ReadConfig_configPerror( "malloc failure processing -e flag" );
            Usm_freeUser( newuser );
            return;
        }

        /*
         * Get the specified engineid from the line.
         */
        cp = ReadConfig_copyNword( cp, buf, sizeof( buf ) );
        if ( !Convert_hexStringToBinaryStringWrapper( &ebuf, &ebuf_len, &eout_len, 1, buf ) ) {
            ReadConfig_configPerror( "invalid EngineID argument to -e" );
            Usm_freeUser( newuser );
            MEMORY_FREE( ebuf );
            return;
        }

        newuser->engineID = ebuf;
        newuser->engineIDLen = eout_len;
        cp = ReadConfig_copyNword( cp, buf, sizeof( buf ) );
    } else {
        newuser->engineID = V3_generateEngineID( &ret );
        if ( ret == 0 ) {
            Usm_freeUser( newuser );
            return;
        }
        newuser->engineIDLen = ret;
    }

    newuser->secName = strdup( buf );
    newuser->name = strdup( buf );

    if ( !cp )
        goto goto_add; /* no authentication or privacy type */

    /*
     * READ: Authentication Type
     */
    if ( strncmp( cp, "MD5", 3 ) == 0 ) {
        memcpy( newuser->authProtocol, usm_hMACMD5AuthProtocol,
            sizeof( usm_hMACMD5AuthProtocol ) );
    } else if ( strncmp( cp, "SHA", 3 ) == 0 ) {
        memcpy( newuser->authProtocol, usm_hMACSHA1AuthProtocol,
            sizeof( usm_hMACSHA1AuthProtocol ) );
    } else {
        ReadConfig_configPerror( "Unknown authentication protocol" );
        Usm_freeUser( newuser );
        return;
    }

    cp = ReadConfig_skipToken( cp );

    /*
     * READ: Authentication Pass Phrase or key
     */
    if ( !cp ) {
        ReadConfig_configPerror( "no authentication pass phrase" );
        Usm_freeUser( newuser );
        return;
    }
    cp = ReadConfig_copyNword( cp, buf, sizeof( buf ) );
    if ( strcmp( buf, "-m" ) == 0 ) {
        /* a master key is specified */
        cp = ReadConfig_copyNword( cp, buf, sizeof( buf ) );
        ret = sizeof( userKey );
        tmpp = userKey;
        userKeyLen = 0;
        if ( !Convert_hexStringToBinaryStringWrapper( &tmpp, &ret, &userKeyLen, 0, buf ) ) {
            ReadConfig_configPerror( "invalid key value argument to -m" );
            Usm_freeUser( newuser );
            return;
        }
    } else if ( strcmp( buf, "-l" ) != 0 ) {
        /* a password is specified */
        userKeyLen = sizeof( userKey );
        ret2 = KeyTools_generateKu( newuser->authProtocol, newuser->authProtocolLen,
            ( u_char* )buf, strlen( buf ), userKey, &userKeyLen );
        if ( ret2 != ErrorCode_SUCCESS ) {
            ReadConfig_configPerror( "could not generate the authentication key from the "
                                     "supplied pass phrase." );
            Usm_freeUser( newuser );
            return;
        }
    }

    /*
     * And turn it into a localized key
     */
    ret2 = Scapi_getProperLength( newuser->authProtocol,
        newuser->authProtocolLen );
    if ( ret2 <= 0 ) {
        ReadConfig_configPerror( "Could not get proper authentication protocol key length" );
        Usm_freeUser( newuser );
        return;
    }
    newuser->authKey = ( u_char* )malloc( ret2 );

    if ( strcmp( buf, "-l" ) == 0 ) {
        /* a local key is directly specified */
        cp = ReadConfig_copyNword( cp, buf, sizeof( buf ) );
        newuser->authKeyLen = 0;
        ret = ret2;
        if ( !Convert_hexStringToBinaryStringWrapper( &newuser->authKey, &ret,
                 &newuser->authKeyLen, 0, buf ) ) {
            ReadConfig_configPerror( "invalid key value argument to -l" );
            Usm_freeUser( newuser );
            return;
        }
        if ( ret != newuser->authKeyLen ) {
            ReadConfig_configPerror( "improper key length to -l" );
            Usm_freeUser( newuser );
            return;
        }
    } else {
        newuser->authKeyLen = ret2;
        ret2 = KeyTools_generateKul( newuser->authProtocol, newuser->authProtocolLen,
            newuser->engineID, newuser->engineIDLen,
            userKey, userKeyLen,
            newuser->authKey, &newuser->authKeyLen );
        if ( ret2 != ErrorCode_SUCCESS ) {
            ReadConfig_configPerror( "could not generate localized authentication key "
                                     "(Kul) from the master key (Ku)." );
            Usm_freeUser( newuser );
            return;
        }
    }

    if ( !cp )
        goto goto_add; /* no privacy type (which is legal) */

    /*
     * READ: Privacy Type
     */
    testcase = 0;
    if ( strncmp( cp, "DES", 3 ) == 0 ) {
        memcpy( newuser->privProtocol, usm_dESPrivProtocol,
            sizeof( usm_dESPrivProtocol ) );
        testcase = 1;
        /* DES uses a 128 bit key, 64 bits of which is a salt */
        privKeyLen = 16;
    }

    if ( strncmp( cp, "AES128", 6 ) == 0 || strncmp( cp, "AES", 3 ) == 0 ) {
        memcpy( newuser->privProtocol, usm_aESPrivProtocol,
            sizeof( usm_aESPrivProtocol ) );
        testcase = 1;
        privKeyLen = 16;
    }
#
    if ( testcase == 0 ) {
        ReadConfig_configPerror( "Unknown privacy protocol" );
        Usm_freeUser( newuser );
        return;
    }

    cp = ReadConfig_skipToken( cp );
    /*
     * READ: Encryption Pass Phrase or key
     */
    if ( !cp ) {
        /*
         * assume the same as the authentication key
         */
        newuser->privKey = ( u_char* )Memory_memdup( newuser->authKey,
            newuser->authKeyLen );
        newuser->privKeyLen = newuser->authKeyLen;
    } else {
        cp = ReadConfig_copyNword( cp, buf, sizeof( buf ) );

        if ( strcmp( buf, "-m" ) == 0 ) {
            /* a master key is specified */
            cp = ReadConfig_copyNword( cp, buf, sizeof( buf ) );
            ret = sizeof( userKey );
            tmpp = userKey;
            userKeyLen = 0;
            if ( !Convert_hexStringToBinaryStringWrapper( &tmpp, &ret, &userKeyLen, 0, buf ) ) {
                ReadConfig_configPerror( "invalid key value argument to -m" );
                Usm_freeUser( newuser );
                return;
            }
        } else if ( strcmp( buf, "-l" ) != 0 ) {
            /* a password is specified */
            userKeyLen = sizeof( userKey );
            ret2 = KeyTools_generateKu( newuser->authProtocol, newuser->authProtocolLen,
                ( u_char* )buf, strlen( buf ), userKey, &userKeyLen );
            if ( ret2 != ErrorCode_SUCCESS ) {
                ReadConfig_configPerror( "could not generate the privacy key from the "
                                         "supplied pass phrase." );
                Usm_freeUser( newuser );
                return;
            }
        }

        /*
         * And turn it into a localized key
         */
        ret2 = Scapi_getProperLength( newuser->authProtocol,
            newuser->authProtocolLen );
        if ( ret2 < 0 ) {
            ReadConfig_configPerror( "could not get proper key length to use for the "
                                     "privacy algorithm." );
            Usm_freeUser( newuser );
            return;
        }
        newuser->privKey = ( u_char* )malloc( ret2 );

        if ( strcmp( buf, "-l" ) == 0 ) {
            /* a local key is directly specified */
            cp = ReadConfig_copyNword( cp, buf, sizeof( buf ) );
            ret = ret2;
            newuser->privKeyLen = 0;
            if ( !Convert_hexStringToBinaryStringWrapper( &newuser->privKey, &ret,
                     &newuser->privKeyLen, 0, buf ) ) {
                ReadConfig_configPerror( "invalid key value argument to -l" );
                Usm_freeUser( newuser );
                return;
            }
        } else {
            newuser->privKeyLen = ret2;
            ret2 = KeyTools_generateKul( newuser->authProtocol, newuser->authProtocolLen,
                newuser->engineID, newuser->engineIDLen,
                userKey, userKeyLen,
                newuser->privKey, &newuser->privKeyLen );
            if ( ret2 != ErrorCode_SUCCESS ) {
                ReadConfig_configPerror( "could not generate localized privacy key "
                                         "(Kul) from the master key (Ku)." );
                Usm_freeUser( newuser );
                return;
            }
        }
    }

    if ( ( newuser->privKeyLen >= privKeyLen ) || ( privKeyLen == 0 ) ) {
        newuser->privKeyLen = privKeyLen;
    } else {
        /* The privKey length is smaller than required by privProtocol */
        Usm_freeUser( newuser );
        return;
    }

goto_add:
    Usm_addUser( newuser );
    DEBUG_MSGTL( ( "usmUser", "created a new user %s at ", newuser->secName ) );
    DEBUG_MSGHEX( ( "usmUser", newuser->engineID, newuser->engineIDLen ) );
    DEBUG_MSG( ( "usmUser", "\n" ) );
}

void Usm_v3AuthtypeConf( const char* word, char* cptr )
{
    if ( strcasecmp( cptr, "MD5" ) == 0 )
        _usm_defaultAuthType = usm_hMACMD5AuthProtocol;
    else if ( strcasecmp( cptr, "SHA" ) == 0 )
        _usm_defaultAuthType = usm_hMACSHA1AuthProtocol;
    else
        ReadConfig_configPerror( "Unknown authentication type" );
    _usm_defaultAuthTypeLen = UTILITIES_USM_LENGTH_OID_TRANSFORM;
    DEBUG_MSGTL( ( "snmpv3", "set default authentication type: %s\n", cptr ) );
}

const oid*
Usm_getDefaultAuthtype( size_t* len )
{
    if ( _usm_defaultAuthType == NULL ) {
        _usm_defaultAuthType = API_DEFAULT_AUTH_PROTO;
        _usm_defaultAuthTypeLen = API_DEFAULT_AUTH_PROTOLEN;
    }
    if ( len )
        *len = _usm_defaultAuthTypeLen;
    return _usm_defaultAuthType;
}

void Usm_v3PrivtypeConf( const char* word, char* cptr )
{
    int testcase = 0;

    if ( strcasecmp( cptr, "DES" ) == 0 ) {
        testcase = 1;
        _usm_defaultPrivType = usm_dESPrivProtocol;
    }

    /* XXX AES: assumes oid length == des oid length */
    if ( strcasecmp( cptr, "AES128" ) == 0 || strcasecmp( cptr, "AES" ) == 0 ) {
        testcase = 1;
        _usm_defaultPrivType = usm_aES128PrivProtocol;
    }
    if ( testcase == 0 )
        ReadConfig_configPerror( "Unknown privacy type" );
    _usm_defaultPrivTypeLen = API_DEFAULT_PRIV_PROTOLEN;
    DEBUG_MSGTL( ( "snmpv3", "set default privacy type: %s\n", cptr ) );
}

const oid*
Usm_getDefaultPrivtype( size_t* len )
{
    if ( _usm_defaultPrivType == NULL ) {
        _usm_defaultPrivType = usm_dESPrivProtocol;

        _usm_defaultPrivTypeLen = UTILITIES_USM_LENGTH_OID_TRANSFORM;
    }
    if ( len )
        *len = _usm_defaultPrivTypeLen;
    return _usm_defaultPrivType;
}
