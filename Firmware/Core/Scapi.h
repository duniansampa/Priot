#ifndef SCAPI_H
#define SCAPI_H


#include "Generals.h"


/*
 * Authentication/privacy transform bitlengths.
 */
#define SCAPI_TRANS_AUTHLEN_HMACMD5	    128
#define SCAPI_TRANS_AUTHLEN_HMACSHA1	160

#define SCAPI_TRANS_AUTHLEN_HMAC96	    96

#define SCAPI_TRANS_PRIVLEN_1DES		64
#define SCAPI_TRANS_PRIVLEN_1DES_IV	    64

#define SCAPI_TRANS_PRIVLEN_AES		    128
#define SCAPI_TRANS_PRIVLEN_AES_IV	    128
#define SCAPI_TRANS_AES_PADSIZE	   	    128  /* backwards compat */
#define SCAPI_TRANS_PRIVLEN_AES128	    128  /* backwards compat */
#define SCAPI_TRANS_PRIVLEN_AES128_IV	128  /* backwards compat */
#define SCAPI_TRANS_AES_AES128_PADSIZE  128  /* backwards compat */


/*
 * Prototypes.
 */
int             Scapi_getProperLength(const oid * hashtype,
                                    u_int hashtype_len);

int             Scapi_getProperPrivLength(const oid * privtype,
                                          u_int privtype_len);

int             Scapi_init(void);

int             Scapi_shutdown(int majorID, int minorID,
                            void *serverarg,
                            void *clientarg);

int             Scapi_random(u_char * buf, size_t * buflen);

int             Scapi_generateKeyedHash(const oid * authtype,
                                       size_t authtypelen,
                                       const u_char * key, u_int keylen,
                                       const u_char * message, u_int msglen,
                                       u_char * MAC, size_t * maclen);

int             Scapi_checkKeyedHash(const oid * authtype,
                                    size_t authtypelen, const u_char * key,
                                    u_int keylen, const u_char * message,
                                    u_int msglen, const u_char * MAC,
                                    u_int maclen);

int             Scapi_encrypt(const oid * privtype, size_t privtypelen,
                           u_char * key, u_int keylen,
                           u_char * iv, u_int ivlen,
                           const u_char * plaintext, u_int ptlen,
                           u_char * ciphertext, size_t * ctlen);

int             Scapi_decrypt(const oid * privtype, size_t privtypelen,
                           u_char * key, u_int keylen,
                           u_char * iv, u_int ivlen,
                           u_char * ciphertext, u_int ctlen,
                           u_char * plaintext, size_t * ptlen);

int             Scapi_hash(const oid * hashtype, size_t hashtypelen,
                        const u_char * buf, size_t buf_len,
                        u_char * MAC, size_t * MAC_len);

int             Scapi_getTransformType(oid * hashtype,
                                      u_int hashtype_len,
                                      int (**hash_fn) (const int mode,
                                                       void **context,
                                                       const u_char *
                                                       data,
                                                       const int
                                                       data_len,
                                                       u_char **
                                                       digest,
                                                       size_t *
                                                       digest_len));




#endif // SCAPI_H
