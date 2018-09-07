#ifndef IOT_SCAPI_H
#define IOT_SCAPI_H

#include "Generals.h"

/**
 * @brief SCAPI stands for the “Secure Computation API"
 * It provides a reliable, efficient, and highly flexible cryptographic infrastructure
 */

/** ============================[ Macros ============================ */

/**
 * Authentication/privacy transform bitlengths.
 */
#define scapiTRANS_AUTHLEN_HMACMD5 128
#define scapiTRANS_AUTHLEN_HMACSHA1 160

#define scapiTRANS_PRIVLEN_1DES 64
#define scapiTRANS_PRIVLEN_1DES_IV 64

#define scapiTRANS_PRIVLEN_AES 128
#define scapiTRANS_PRIVLEN_AES_IV 128

/** =============================[ Functions Prototypes ]================== */

/**
 * @brief Scapi_init
 *        init the Scapi
 *
 * @return ErrorCode_SUCCESS : success.
 */
int Scapi_init( void );

/**
 * @brief  Scapi_getProperLength
 *         Given a hashing type ("hashType" and its length hashTypeLength),
 *         return the length of the hash result.
 *
 * @param hashType - a hashing type
 * @param hashTypeLength - the length of the hashType
 *
 * @returns the length : on success
 *          ErrorCode_GENERR : for an unknown hashing type.
 */
int Scapi_getProperLength( const oid* hashType, u_int hashTypeLength );

/**
 * @brief Scapi_getProperPrivLength
 *        Given a hashing type ("privType" and its length privTypeLength),
 *        return the length of the hash result.
 *
 * @param privType - a hashing type
 * @param privTypeLength - the length of the privType
 *
 * @returns the length : on success
 *          0 : for an unknown hashing type.
 */
int Scapi_getProperPrivLength( const oid* privType, u_int privTypeLength );

/**
 * @brief Scapi_random
 *        puts num cryptographically strong pseudo-random bytes into buf
 *
 *
 * @param buffer - pre-allocated buffer.
 * @param bufferSize - Size of buffer.
 *
 * @return ErrorCode_SUCCESS
 */
int Scapi_random( u_char* buffer, size_t* bufferLength );

/**
 * @brief Scapi_generateKeyedHash
 *        A hash of the first @p messageLength bytes of @p message using a keyed hash defined
 *        by authType is created and stored in @p mac.  @p mac is ASSUMED to be a buffer
 *        of at least macLength bytes.  If the length of the hash is greater than
 *        macLength, it is truncated to fit the buffer.  If the length of the hash is
 *        less than macLength, macLength set to the number of hash bytes generated.
 *
 *        ASSUMED that the number of hash bits is a multiple of 8.
 *
 *	@param[in] authType	- type of authentication transform.
 *	@param[in] authTypeLength - the length of authType
 *	@param[in] key - pointer to key (Kul) to use in keyed hash.
 *	@param[in] keyLength - length of key in bytes.
 *	@param[in] message - pointer to the message to hash.
 *	@param[in] messageLength - length of the message.
 *	@param[out] mac - will be returned with allocated bytes containg hash.
 *	@param[out] macLength - length of the hash buffer in bytes; also indicates
 *                          whether the MAC should be truncated.
 *
 * @returns ErrorCode_SUCCESS : success.
 *          ErrorCode_GENERR : all errors
 */
int Scapi_generateKeyedHash( const oid* authType, size_t authTypeLength, const u_char* key, u_int keyLength,
    const u_char* message, u_int messageLength, u_char* mac, size_t* macLength );

/**
 * @brief Scapi_checkKeyedHash
 *        check the hash given in @p mac against the hash of message.  If the length
 *        of @p mac is less than the length of the transform hash output, only macLength
 *        bytes are compared.  The length of @p mac cannot be greater than the
 *        length of the hash transform output.
 *
 *	@param[in] authType - transform type of authentication hash.
 *  @param[in] authTypeLength - the length of authType
 *	@param[in] key - key bits in a string of bytes.
 *	@param[in] keyLength - length of key in bytes.
 *	@param[in] message - message for which to check the hash.
 *	@param[in] messageLength - length of message.
 *	@param[in] mac - given hash.
 *	@param[in] macLength - Length of given hash; indicates truncation if it is
 *                         shorter than the normal size of output for
 *                         given hash transform.
 * @returns
 *	ErrorCode_SUCCESS : Success.
 *	SNMP_SC_GENERAL_FAILURE : any error
 *
 */
int Scapi_checkKeyedHash( const oid* authType, size_t authTypeLength, const u_char* key,
    u_int keyLength, const u_char* message, u_int messageLength, const u_char* mac, u_int macLength );

/**
 * @brief Scapi_encrypt
 *        Encrypt @p plainText into @p cipherText using @p key and @p iv.
 *        chiperTextLength contains actual number of crypted bytes in cipherText upon
 *        successful return.
 *
 * @param[in] privType - type of privacy cryptographic transform.
 * @param[in] privTypeLength - the length of privType
 * @param[in] key - key bits for crypting.
 * @param[in] keylen - length of key (buffer) in bytes.
 * @param[in] iv - IV bits for crypting.
 * @param[in] ivLength - Length of iv (buffer) in bytes.
 * @param[in] plainText - plaintext to crypt.
 * @param[in] plainTextLength - length of plaintext.
 * @param[out] cipherText - the ciphertext (crypted text)
 * @param[out] chiperTextLength - length of ciphertext.
 *
 * @returns
 *	ErrorCode_SUCCESS : success.
 *	ErrorCode_SC_NOT_CONFIGURED : encryption is not supported.
 *	ErrorCode_SC_GENERAL_FAILURE : any other error
 */
int Scapi_encrypt( const oid* privType, size_t privTypeLength,
    u_char* key, u_int keyLength, u_char* iv, u_int ivLength,
    const u_char* plainText, u_int plainTextLength, u_char* cipherText, size_t* chiperTextLength );

/**
 * @brief Scapi_decrypt
 *        Decrypt @p cipherText into @p plainText using @p key and @p iv.
 *        @p plainTextLength contains actual number of plaintext bytes in
 *        @p plainText upon successful return.
 *
 * @param[in] privType - oid pointer to a hash type
 * @param[in] privTypeLength - the length of privType
 * @param[in] key - key bits for decrypting.
 * @param[in] keyLength - length of key (buffer) in bytes.
 * @param[in] iv - IV bits for crypting.
 * @param[in] ivLength - Length of iv (buffer) in bytes.
 * @param[in] cipherText - ciphertext to decrypt.
 * @param[in] chipherTextLength - length of cipherText.
 * @param[out] plainText - the plaintext (decrypted text)
 * @param[out] plainTextLength - length of plaintext.
 *
 * @returns
 *	ErrorCode_SUCCESS : Success.
 *	ErrorCode_SC_NOT_CONFIGURED	: Encryption is not supported.
 *      ErrorCode_SC_GENERAL_FAILURE : any other error
 */
int Scapi_decrypt( const oid* privType, size_t privTypeLength,
    u_char* key, u_int keyLength, u_char* iv, u_int ivLength,
    u_char* cipherText, u_int chipherTextLength, u_char* plainText, size_t* plainTextLength );

/**
 * @brief Scapi_hash
 *        generates the hash of @p buffer and stores the result in @p mac.
 *
 * @param[in] hashType - oid pointer to a hash type
 * @param[in] hashTypeLength - length of oid pointer
 * @param[in] message - u_char message to be hashed
 * @param[in] messageLength - integer length of message data
 * @param[out] mac - pre-malloced space to store hash output.
 * @param[in, out] macLength - length of the passed @p mac buffer size or
 *                             length of @p mac output to the @p mac buffer.
 *
 * @returns
 *      ErrorCode_SUCCESS : success.
 *      ErrorCode_GENERR : any error.
 *      ErrorCode_SC_NOT_CONFIGURED :  hash type not supported.
 */
int Scapi_hash( const oid* hashType, size_t hashTypeLength,
    const u_char* message, size_t messageLength, u_char* mac, size_t* macLength );

#endif // IOT_SCAPI_H
