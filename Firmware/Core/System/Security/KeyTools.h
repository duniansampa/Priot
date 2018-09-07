#ifndef IOT_KEYTOOLS_H
#define IOT_KEYTOOLS_H

#include "Types.h"

/* 1Meg. */
#define keyUSM_LENGTH_EXPANDED_PASSPHRASE ( 1024 * 1024 )

/* In bytes. */
#define keyUSM_LENGTH_KU_HASHBLOCK 64

/* In characters. */
#define keyUSM_LENGTH_P_MIN 8

/**
 * @brief KeyTools_generateKu
 *        Convert a passphrase into a master user key, Ku, according to the
 *        algorithm given in RFC 2274 concerning the SNMPv3 User Security Model (USM)
 *        as follows:
 *
 *          Expand the passphrase to fill the passphrase buffer space, if necessary,
 *          concatenation as many duplicates as possible of @p password to itself.
 *          If @p password is larger than the buffer space, truncate it to fit.
 *
 *          Then hash the result with the given hashtype transform.  Return
 *          the result as Ku.
 *
 *          If successful, kuLength contains the size of the hash written to Ku.
 *
 *          @note Passphrases less than KEYTOOLS_USM_LENGTH_P_MIN characters in length
 *          cause an error to be returned. Punt this check to the cmdline apps?  XXX)
 *
 *	@param[in] hashType - OID for the transform type for hashing.
 *	@param[in] hashTypeLength - Length of OID value.
 *	@param[in] password - Pre-allocated bytes of passpharase.
 *	@param[in] passwordLength - Length of passphrase.
 *	@param[out] ku - Buffer to contain Ku.
 *	@param[out] kuLength - Length of Ku buffer.
 *
 * @returns
 *      ErrorCode_SUCCESS : Success.
 *      ErrorCode_GENERR  : All errors.
 */
int KeyTools_generateKu( const oid* hashType, u_int hashTypeLength,
    const u_char* password, size_t passwordLength, u_char* ku, size_t* kuLength );

/**
 * @brief KeyTools_generateKul
 *        Ku MUST be the proper length (currently fixed) for the given hashType.
 *
 *        Upon successful return, Kul contains the localized form of Ku at
 *        engineId, and the length of the key is stored in kulLength.
 *
 *        The localized key method is defined in RFC2274, Sections 2.6 and A.2, and
 *        originally documented in:
 *        U. Blumenthal, N. C. Hien, B. Wijnen,
 *        "Key Derivation for Network Management Applications",
 *        IEEE Network Magazine, April/May issue, 1997.
 *
 *        ASSUMES  UTILITIES_MAX_BUFFER >= sizeof(Ku + engineId + Ku).
 *
 *        @note  Localized keys for privacy transforms are generated via
 *        the authentication transform held by the same usmUser.
 *
 *        An engineId of any length is accepted, even if larger than
 *        what is specified for the textual convention.
 *
 * @param[in] hashType - OID for the transform type for hashing.
 * @param[in] hashTypeLength - Length of OID value.
 * @param[in] engineId - the engine identifier
 * @param[in] engineIdLength - the length of the engineId
 * @param[in] ku - Master key for a given user.
 * @param[in] kuLength - Length of Ku in bytes.
 * @param[out] kul - Localized key for a given user at engineID.
 * @param[out] kulLength - Length of Kul buffer (IN); Length of Kul key (OUT).
 *
 * @returns :
 *      ErrorCode_SUCCESS : Success.
 *      ErrorCode_GENERR  :	All errors.
 */
int KeyTools_generateKul( const oid* hashType, u_int hashTypeLength, const u_char* engineId,
    size_t engineIdLength, const u_char* ku, size_t kuLength, u_char* kul, size_t* kulLength );

/**
 * @brief KeyTools_encodeKeyChange
 *        Uses oldKey and acquired random bytes to encode newKey into kcString
 *        according to the rules of the KeyChange TC described in RFC 2274, Section 5.
 *
 *        Upon successful return, *kcStringLength contains the length of the
 *        encoded string.
 *
 *        ASSUMES	Old and new key are always equal to each other, although
 *		  this may be less than the transform type hash output
 *        output length (eg, using KeyChange for a DESPriv key when
 *		  the user also uses SHA1Auth).  This also implies that the
 *		  hash placed in the second 1/2 of the key change string
 *		  will be truncated before the XOR'ing when the hash output is
 *		  larger than that 1/2 of the key change string.
 *
 *		  *kcStringLength will be returned as exactly twice that same
 *		  length though the input buffer may be larger.
 *
 *        FIX: Does not handle varibable length keys.
 *        FIX: Does not handle keys larger than the hash algorithm used.
 *
 *  @param[in] hashType - OID for the hash transform type.
 *	@param[in] hashTypeLength - Length of the OID hash transform type.
 *	@param[in] oldKey - Old key that is used to encodes the new key.
 *  @param[in] oldKeyLength - Length of oldKey in bytes.
 *	@param[in] newKey - New key that is encoded using the old key.
 *	@param[in] newKeyLength - Length of new key in bytes.
 *	@param[out] kcString - Buffer to contain the KeyChange TC string.
 *	@param kcStringLength - Length of kcString buffer.
 *
 * @returns
 *	ErrorCode_SUCCESS : Success.
 *	ErrorCode_GENERR  : All errors.
 */
int KeyTools_encodeKeyChange( const oid* hashType, u_int hashTypeLength, u_char* oldKey, size_t oldKeyLength,
    u_char* newKey, size_t newKeyLength, u_char* kcString, size_t* kcStringLength );

/**
 * @brief KeyTools_decodeKeyChange
 *        Decodes a string of bits encoded according to the KeyChange TC described
 *        in RFC 2274, Section 5.  The new key is extracted from *kcString with
 *        the aid of the old key.
 *
 *        Upon successful return, *newKeyLength contains the length of the new key.
 *
 *        ASSUMES	Old key is exactly 1/2 the length of the KeyChange buffer,
 *        although this length may be less than the hash transform
 *        output.  Thus the new key length will be equal to the old
 *        key length.
 *
 *	@param[in] hashType - OID of the hash transform to use.
 *	@param[in] hashTypeLength -	Length of the hash transform MIB OID.
 *	@param[in] oldKey - Old key that is used to encode the new key.
 *	@param[in] oldKeyLength - Length of oldkey in bytes.
 *	@param[in] kcString - Encoded KeyString buffer containing the new key.
 *	@param[in] kcStringLength - Length of kcstring in bytes.
 *	@param[out] newKey	- Buffer to hold the extracted new key.
 *	@param[out] newKeyLength - Length of newkey in bytes.
 *
 * @returns
 *	ErrorCode_SUCCESS : Success.
 *	ErrorCode_GENERR  : All errors.
 */
int KeyTools_decodeKeyChange( const oid* hashtype,
    u_int hashtype_len, u_char* oldkey,
    size_t oldkey_len, u_char* kcstring,
    size_t kcstring_len, u_char* newkey,
    size_t* newkey_len );

#endif // IOT_KEYTOOLS_H
