#include "Keytools.h"
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include "Types.h"
#include "Tools.h"
#include "Logger.h"
#include "Scapi.h"
#include "Api.h"
#include "Usm.h"
#include <openssl/hmac.h>


/*******************************************************************-o-******
 * Keytools_generateKu
 *
 * Parameters:
 *	*hashtype	MIB OID for the transform type for hashing.
 *	 hashtype_len	Length of OID value.
 *	*P		Pre-allocated bytes of passpharase.
 *	 pplen		Length of passphrase.
 *	*Ku		Buffer to contain Ku.
 *	*kulen		Length of Ku buffer.
 *
 * Returns:
 *	ErrorCode_SUCCESS			Success.
 *	ErrorCode_GENERR			All errors.
 *
 *
 * Convert a passphrase into a master user key, Ku, according to the
 * algorithm given in RFC 2274 concerning the SNMPv3 User Security Model (USM)
 * as follows:
 *
 * Expand the passphrase to fill the passphrase buffer space, if necessary,
 * concatenation as many duplicates as possible of P to itself.  If P is
 * larger than the buffer space, truncate it to fit.
 *
 * Then hash the result with the given hashtype transform.  Return
 * the result as Ku.
 *
 * If successful, kulen contains the size of the hash written to Ku.
 *
 * NOTE  Passphrases less than KEYTOOLS_USM_LENGTH_P_MIN characters in length
 *	 cause an error to be returned.
 *	 (Punt this check to the cmdline apps?  XXX)
 */
int Keytools_generateKu(const oid * hashtype, u_int hashtype_len, const u_char * P, size_t pplen, u_char * Ku, size_t * kulen){
    int     rval = ErrorCode_SUCCESS,
    nbytes = KEYTOOLS_USM_LENGTH_EXPANDED_PASSPHRASE;


    u_int           i, pindex = 0;

    u_char          buf[KEYTOOLS_USM_LENGTH_KU_HASHBLOCK], *bufp;

    EVP_MD_CTX     *ctx = NULL;

    /*
     * Sanity check.
     */
    if (!hashtype || !P || !Ku || !kulen || (*kulen <= 0)
        || (hashtype_len != TOOLS_USM_LENGTH_OID_TRANSFORM)) {
        TOOLS_QUITFUN(ErrorCode_GENERR, goto_generateKuQuit);
    }

    if (pplen < KEYTOOLS_USM_LENGTH_P_MIN) {
        Logger_log(LOGGER_PRIORITY_ERR, "Error: passphrase chosen is below the length "
                 "requirements of the USM (min=%d).\n", KEYTOOLS_USM_LENGTH_P_MIN);
        Api_setDetail("The supplied password length is too short.");
        TOOLS_QUITFUN(ErrorCode_GENERR, goto_generateKuQuit);
    }


    /*
     * Setup for the transform type.
     */
    ctx = EVP_MD_CTX_create();

    if (TOOLS_ISTRANSFORM(hashtype, hMACMD5Auth)) {
        if (!EVP_DigestInit(ctx, EVP_md5()))
            return ErrorCode_GENERR;
    } else    if (TOOLS_ISTRANSFORM(hashtype, hMACSHA1Auth)) {
        if (!EVP_DigestInit(ctx, EVP_sha1()))
            return ErrorCode_GENERR;
    } else
        TOOLS_QUITFUN(ErrorCode_GENERR, goto_generateKuQuit);


    while (nbytes > 0) {
        bufp = buf;
        for (i = 0; i < KEYTOOLS_USM_LENGTH_KU_HASHBLOCK; i++) {
            *bufp++ = P[pindex++ % pplen];
        }
        EVP_DigestUpdate(ctx, buf, KEYTOOLS_USM_LENGTH_KU_HASHBLOCK);
        nbytes -= KEYTOOLS_USM_LENGTH_KU_HASHBLOCK;
    }

    {
    unsigned int    tmp_len;

    tmp_len = *kulen;
    EVP_DigestFinal(ctx, (unsigned char *) Ku, &tmp_len);
    *kulen = tmp_len;
    /*
     * what about free()
     */
    }


goto_generateKuQuit:
  memset(buf, 0, sizeof(buf));

  if (ctx) {
      EVP_MD_CTX_destroy(ctx);
  }

  return rval;
}

/*******************************************************************-o-******
 * Keytools_generateKul
 *
 * Parameters:
 *	*hashtype
 *	 hashtype_len
 *	*engineID
 *	 engineID_len
 *	*Ku		Master key for a given user.
 *	 ku_len		Length of Ku in bytes.
 *	*Kul		Localized key for a given user at engineID.
 *	*kul_len	Length of Kul buffer (IN); Length of Kul key (OUT).
 *
 * Returns:
 *	ErrorCode_SUCCESS			Success.
 *	ErrorCode_GENERR			All errors.
 *
 *
 * Ku MUST be the proper length (currently fixed) for the given hashtype.
 *
 * Upon successful return, Kul contains the localized form of Ku at
 * engineID, and the length of the key is stored in kul_len.
 *
 * The localized key method is defined in RFC2274, Sections 2.6 and A.2, and
 * originally documented in:
 *  	U. Blumenthal, N. C. Hien, B. Wijnen,
 *     	"Key Derivation for Network Management Applications",
 *	IEEE Network Magazine, April/May issue, 1997.
 *
 *
 * ASSUMES  TOOLS_MAXBUF >= sizeof(Ku + engineID + Ku).
 *
 * NOTE  Localized keys for privacy transforms are generated via
 *	 the authentication transform held by the same usmUser.
 *
 * XXX	An engineID of any length is accepted, even if larger than
 *	what is spec'ed for the textual convention.
 */
int Keytools_generateKul(const oid * hashtype, u_int hashtype_len, const u_char * engineID, size_t engineID_len, const u_char * Ku, size_t ku_len, u_char * Kul, size_t * kul_len){

    int             rval = ErrorCode_SUCCESS;
    u_int           nbytes = 0;
    size_t          properlength;
    int             iproperlength;

    u_char          buf[TOOLS_MAXBUF];

    /*
     * Sanity check.
     */
    if (!hashtype || !engineID || !Ku || !Kul || !kul_len
        || (engineID_len <= 0) || (ku_len <= 0) || (*kul_len <= 0)
        || (hashtype_len != TOOLS_USM_LENGTH_OID_TRANSFORM)) {
        TOOLS_QUITFUN(ErrorCode_GENERR, goto_generateKulQuit);
    }


    iproperlength = Scapi_getProperLength(hashtype, hashtype_len);
    if (iproperlength == ErrorCode_GENERR)
        TOOLS_QUITFUN(ErrorCode_GENERR, goto_generateKulQuit);

    properlength = (size_t) iproperlength;

    if ((*kul_len < properlength) || (ku_len < properlength)) {
        TOOLS_QUITFUN(ErrorCode_GENERR, goto_generateKulQuit);
    }

    /*
     * Concatenate Ku and engineID properly, then hash the result.
     * Store it in Kul.
     */
    nbytes = 0;
    memcpy(buf, Ku, properlength);
    nbytes += properlength;
    memcpy(buf + nbytes, engineID, engineID_len);
    nbytes += engineID_len;
    memcpy(buf + nbytes, Ku, properlength);
    nbytes += properlength;

    rval = Scapi_hash(hashtype, hashtype_len, buf, nbytes, Kul, kul_len);


    TOOLS_QUITFUN(rval, goto_generateKulQuit);


goto_generateKulQuit:
    return rval;

}                               /* end generate_kul() */

/*******************************************************************-o-******
 * Keytools_encodeKeychange
 *
 * Parameters:
 *	*hashtype	MIB OID for the hash transform type.
 *	 hashtype_len	Length of the MIB OID hash transform type.
 *	*oldkey		Old key that is used to encodes the new key.
 *	 oldkey_len	Length of oldkey in bytes.
 *	*newkey		New key that is encoded using the old key.
 *	 newkey_len	Length of new key in bytes.
 *	*kcstring	Buffer to contain the KeyChange TC string.
 *	*kcstring_len	Length of kcstring buffer.
 *
 * Returns:
 *	ErrorCode_SUCCESS			Success.
 *	ErrorCode_GENERR			All errors.
 *
 *
 * Uses oldkey and acquired random bytes to encode newkey into kcstring
 * according to the rules of the KeyChange TC described in RFC 2274, Section 5.
 *
 * Upon successful return, *kcstring_len contains the length of the
 * encoded string.
 *
 * ASSUMES	Old and new key are always equal to each other, although
 *		this may be less than the transform type hash output
 * 		output length (eg, using KeyChange for a DESPriv key when
 *		the user also uses SHA1Auth).  This also implies that the
 *		hash placed in the second 1/2 of the key change string
 *		will be truncated before the XOR'ing when the hash output is
 *		larger than that 1/2 of the key change string.
 *
 *		*kcstring_len will be returned as exactly twice that same
 *		length though the input buffer may be larger.
 *
 * XXX FIX:     Does not handle varibable length keys.
 * XXX FIX:     Does not handle keys larger than the hash algorithm used.
 */
int Keytools_encodeKeychange(const oid * hashtype, u_int hashtype_len, u_char * oldkey, size_t oldkey_len, u_char * newkey, size_t newkey_len, u_char * kcstring, size_t * kcstring_len)
{
        int             rval = ErrorCode_SUCCESS;
        int             iproperlength;
        size_t          properlength;
        size_t          nbytes = 0;

        u_char         *tmpbuf = NULL;

        /*
         * Sanity check.
         */
        if (!kcstring || !kcstring_len)
            return ErrorCode_GENERR;

        if (!hashtype || !oldkey || !newkey || !kcstring || !kcstring_len
            || (oldkey_len <= 0) || (newkey_len <= 0) || (*kcstring_len <= 0)
            || (hashtype_len != TOOLS_USM_LENGTH_OID_TRANSFORM)) {
            TOOLS_QUITFUN(ErrorCode_GENERR, goto_encodeKeychangeQuit);
        }

        /*
         * Setup for the transform type.
         */
        iproperlength = Scapi_getProperLength(hashtype, hashtype_len);
        if (iproperlength == ErrorCode_GENERR)
            TOOLS_QUITFUN(ErrorCode_GENERR, goto_encodeKeychangeQuit);

        if ((oldkey_len != newkey_len) || (*kcstring_len < (2 * oldkey_len))) {
            TOOLS_QUITFUN(ErrorCode_GENERR, goto_encodeKeychangeQuit);
        }

        properlength = TOOLS_MIN(oldkey_len, (size_t)iproperlength);

        /*
         * Use the old key and some random bytes to encode the new key
         * in the KeyChange TC format:
         *      . Get random bytes (store in first half of kcstring),
         *      . Hash (oldkey | random_bytes) (into second half of kcstring),
         *      . XOR hash and newkey (into second half of kcstring).
         *
         * Getting the wrong number of random bytes is considered an error.
         */
        nbytes = properlength;

        rval = Scapi_random(kcstring, &nbytes);
        TOOLS_QUITFUN(rval, goto_encodeKeychangeQuit);
        if (nbytes != properlength) {
            TOOLS_QUITFUN(ErrorCode_GENERR, goto_encodeKeychangeQuit);
        }


        tmpbuf = (u_char *) malloc(properlength * 2);
        if (tmpbuf) {
            memcpy(tmpbuf, oldkey, properlength);
            memcpy(tmpbuf + properlength, kcstring, properlength);

            *kcstring_len -= properlength;
            rval = Scapi_hash(hashtype, hashtype_len, tmpbuf, properlength * 2,
                           kcstring + properlength, kcstring_len);

            TOOLS_QUITFUN(rval, goto_encodeKeychangeQuit);

            *kcstring_len = (properlength * 2);

            kcstring += properlength;
            nbytes = 0;
            while ((nbytes++) < properlength) {
                *kcstring++ ^= *newkey++;
            }
        }

goto_encodeKeychangeQuit:
        if (rval != ErrorCode_SUCCESS)
            memset(kcstring, 0, *kcstring_len);
        TOOLS_FREE(tmpbuf);

        return rval;

}

    /*******************************************************************-o-******
     * Keytools_decodeKeychange
     *
     * Parameters:
     *	*hashtype	MIB OID of the hash transform to use.
     *	 hashtype_len	Length of the hash transform MIB OID.
     *	*oldkey		Old key that is used to encode the new key.
     *	 oldkey_len	Length of oldkey in bytes.
     *	*kcstring	Encoded KeyString buffer containing the new key.
     *	 kcstring_len	Length of kcstring in bytes.
     *	*newkey		Buffer to hold the extracted new key.
     *	*newkey_len	Length of newkey in bytes.
     *
     * Returns:
     *	ErrorCode_SUCCESS			Success.
     *	ErrorCode_GENERR			All errors.
     *
     *
     * Decodes a string of bits encoded according to the KeyChange TC described
     * in RFC 2274, Section 5.  The new key is extracted from *kcstring with
     * the aid of the old key.
     *
     * Upon successful return, *newkey_len contains the length of the new key.
     *
     *
     * ASSUMES	Old key is exactly 1/2 the length of the KeyChange buffer,
     *		although this length may be less than the hash transform
     *		output.  Thus the new key length will be equal to the old
     *		key length.
     */
    /*
     * XXX:  if the newkey is not long enough, it should be freed and remalloced
     *//*******************************************************************-o-******
 * Keytools_decodeKeychange
 *
 * Parameters:
 *	*hashtype	MIB OID of the hash transform to use.
 *	 hashtype_len	Length of the hash transform MIB OID.
 *	*oldkey		Old key that is used to encode the new key.
 *	 oldkey_len	Length of oldkey in bytes.
 *	*kcstring	Encoded KeyString buffer containing the new key.
 *	 kcstring_len	Length of kcstring in bytes.
 *	*newkey		Buffer to hold the extracted new key.
 *	*newkey_len	Length of newkey in bytes.
 *
 * Returns:
 *	ErrorCode_SUCCESS			Success.
 *	ErrorCode_GENERR			All errors.
 *
 *
 * Decodes a string of bits encoded according to the KeyChange TC described
 * in RFC 2274, Section 5.  The new key is extracted from *kcstring with
 * the aid of the old key.
 *
 * Upon successful return, *newkey_len contains the length of the new key.
 *
 *
 * ASSUMES	Old key is exactly 1/2 the length of the KeyChange buffer,
 *		although this length may be less than the hash transform
 *		output.  Thus the new key length will be equal to the old
 *		key length.
 */
/*
 * XXX:  if the newkey is not long enough, it should be freed and remalloced
 */
int Keytools_decodeKeychange(const oid * hashtype, u_int hashtype_len, u_char * oldkey, size_t oldkey_len,
                             u_char * kcstring, size_t kcstring_len, u_char * newkey, size_t * newkey_len){

    int             rval = ErrorCode_SUCCESS;
    size_t          properlength = 0;
    int             iproperlength = 0;
    u_int           nbytes = 0;

    u_char         *bufp, tmp_buf[TOOLS_MAXBUF];
    size_t          tmp_buf_len = TOOLS_MAXBUF;
    u_char         *tmpbuf = NULL;

    /*
     * Sanity check.
     */
    if (!hashtype || !oldkey || !kcstring || !newkey || !newkey_len
        || (oldkey_len <= 0) || (kcstring_len <= 0) || (*newkey_len <= 0)
        || (hashtype_len != TOOLS_USM_LENGTH_OID_TRANSFORM)) {
        TOOLS_QUITFUN(ErrorCode_GENERR, goto_decodeKeychangeQuit);
    }


    /*
     * Setup for the transform type.
     */
    iproperlength = Scapi_getProperLength(hashtype, hashtype_len);
    if (iproperlength == ErrorCode_GENERR)
        TOOLS_QUITFUN(ErrorCode_GENERR, goto_decodeKeychangeQuit);

    properlength = (size_t) iproperlength;

    if (((oldkey_len * 2) != kcstring_len) || (*newkey_len < oldkey_len)) {
        TOOLS_QUITFUN(ErrorCode_GENERR, goto_decodeKeychangeQuit);
    }

    properlength = oldkey_len;
    *newkey_len = properlength;

    /*
     * Use the old key and the given KeyChange TC string to recover
     * the new key:
     *      . Hash (oldkey | random_bytes) (into newkey),
     *      . XOR hash and encoded (second) half of kcstring (into newkey).
     */
    tmpbuf = (u_char *) malloc(properlength * 2);
    if (tmpbuf) {
        memcpy(tmpbuf, oldkey, properlength);
        memcpy(tmpbuf + properlength, kcstring, properlength);

        rval = Scapi_hash(hashtype, hashtype_len, tmpbuf, properlength * 2,
                       tmp_buf, &tmp_buf_len);
        TOOLS_QUITFUN(rval, goto_decodeKeychangeQuit);

        memcpy(newkey, tmp_buf, properlength);
        bufp = kcstring + properlength;
        nbytes = 0;
        while ((nbytes++) < properlength) {
            *newkey++ ^= *bufp++;
        }
    }

  goto_decodeKeychangeQuit:
    if (rval != ErrorCode_SUCCESS) {
        if (newkey)
            memset(newkey, 0, properlength);
    }
    memset(tmp_buf, 0, TOOLS_MAXBUF);
    TOOLS_FREE(tmpbuf);

    return rval;

}                               /* end decode_keychange() */
