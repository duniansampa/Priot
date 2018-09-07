#ifndef USM_H
#define USM_H

#include "Generals.h"
#include "Types.h"
#include "SecMod.h"
#include "System/Util/Callback.h"
#include "Priot.h"


#define USM_WILDCARDSTRING "*"

/*
 * General.
 */
#define USM_MAX_ID_LENGTH		    1024    /* In bytes. */
#define USM_MAX_SALT_LENGTH		    128     /* In BITS. */
#define USM_DES_SALT_LENGTH		    64      /* In BITS. */
#define USM_AES_SALT_LENGTH		    128     /* In BITS. */
#define USM_MAX_KEYEDHASH_LENGTH	128     /* In BITS. */

#define USM_TIME_WINDOW			    150
#define USM_MD5_AND_SHA_AUTH_LEN    12      /* bytes */
#define USM_MAX_AUTHSIZE            USM_MD5_AND_SHA_AUTH_LEN

#define USM_SEC_MODEL_NUMBER        PRIOT_SEC_MODEL_USM


extern oid             usm_noAuthProtocol[10];
extern oid             usm_hMACMD5AuthProtocol[10];
extern oid             usm_hMACSHA1AuthProtocol[10];
extern oid             usm_noPrivProtocol[10];
extern oid             usm_dESPrivProtocol[10];
extern oid             usm_aESPrivProtocol[10];
extern oid *           usm_aES128PrivProtocol;



#define USM_AUTH_PROTO_NOAUTH_LEN  10
#define USM_AUTH_PROTO_MD5_LEN     10
#define USM_AUTH_PROTO_SHA_LEN     10
#define USM_PRIV_PROTO_NOPRIV_LEN  10
#define USM_PRIV_PROTO_DES_LEN     10

#define USM_PRIV_PROTO_AES_LEN     10
#define USM_PRIV_PROTO_AES128_LEN  10 /* backwards compat */
/*
 * Structures.
 */
struct Usm_StateReference_s {
    char           *usr_name;
    size_t          usr_name_length;
    u_char         *usr_engine_id;
    size_t          usr_engine_id_length;
    oid           *usr_auth_protocol;
    size_t          usr_auth_protocol_length;
    u_char         *usr_auth_key;
    size_t          usr_auth_key_length;
    oid           *usr_priv_protocol;
    size_t          usr_priv_protocol_length;
    u_char         *usr_priv_key;
    size_t          usr_priv_key_length;
    u_int           usr_sec_level;
};


/*
 * struct Usm_User_s: a structure to represent a given user in a list
 */
/*
 * Note: Any changes made to this structure need to be reflected in
 * the following functions:
 */

struct Usm_User_s {
    u_char         *engineID;
    size_t          engineIDLen;
    char           *name;
    char           *secName;
    oid            *cloneFrom;
    size_t          cloneFromLen;
    oid            *authProtocol;
    size_t          authProtocolLen;
    u_char         *authKey;
    size_t          authKeyLen;
    oid            *privProtocol;
    size_t          privProtocolLen;
    u_char         *privKey;
    size_t          privKeyLen;
    u_char         *userPublicString;
    size_t          userPublicStringLen;
    int             userStatus;
    int             userStorageType;
   /* these are actually DH * pointers but only if openssl is avail. */
    void           *usmDHUserAuthKeyChange;
    void           *usmDHUserPrivKeyChange;
    struct Usm_User_s *next;
    struct Usm_User_s *prev;
};



/*
 * Prototypes.
 */
struct Usm_StateReference_s * Usm_mallocUsmStateReference(void);

void            Usm_freeUsmStateReference(void *old);

int             Usm_setUsmStateReferenceName(struct Usm_StateReference_s
                                               *ref, char *name,
                                               size_t name_len);

int             Usm_setUsmStateReferenceEngineId(struct
                                                    Usm_StateReference_s
                                                    *ref,
                                                    u_char * engine_id,
                                                    size_t
                                                    engine_id_len);

int             Usm_setUsmStateReferenceAuthProtocol(struct
                                                        Usm_StateReference_s
                                                        *ref,
                                                        oid *
                                                        auth_protocol,
                                                        size_t
                                                        auth_protocol_len);

int             Usm_setUsmStateReferenceAuthKey(struct
                                                   Usm_StateReference_s
                                                   *ref,
                                                   u_char * auth_key,
                                                   size_t
                                                   auth_key_len);

int             Usm_setUsmStateReferencePrivProtocol(struct
                                                        Usm_StateReference_s
                                                        *ref,
                                                        oid *
                                                        priv_protocol,
                                                        size_t
                                                        priv_protocol_len);

int             Usm_setUsmStateReferencePrivKey(struct
                                                   Usm_StateReference_s
                                                   *ref,
                                                   u_char * priv_key,
                                                   size_t
                                                   priv_key_len);

int             Usm_setUsmStateReferenceSecLevel(struct
                                                    Usm_StateReference_s
                                                    *ref,
                                                    int sec_level);
int             Usm_cloneUsmStateReference(struct Usm_StateReference_s *from,
                                                struct Usm_StateReference_s **to);


int             Usm_asnPredictIntLength(int type, long number,
                                       size_t len);

int             Usm_asnPredictLength(int type, u_char * ptr,
                                   size_t u_char_len);

int             Usm_setSalt(u_char * iv,
                             size_t * iv_length,
                             u_char * priv_salt,
                             size_t priv_salt_length,
                             u_char * msgSalt);

int             Usm_parseSecurityParameters(u_char * secParams,
                                              size_t remaining,
                                              u_char * secEngineID,
                                              size_t * secEngineIDLen,
                                              u_int * boots_uint,
                                              u_int * time_uint,
                                              char *secName,
                                              size_t * secNameLen,
                                              u_char * signature,
                                              size_t *
                                              signature_length,
                                              u_char * salt,
                                              size_t * salt_length,
                                              u_char ** data_ptr);

int             Usm_checkAndUpdateTimeliness(u_char * secEngineID,
                                                size_t secEngineIDLen,
                                                u_int boots_uint,
                                                u_int time_uint,
                                                int *error);

SecModSessionCallback_f usm_openSession;
SecModOutMsg_f    usm_secmodGenerateOutMsg;
SecModOutMsg_f    usm_secmodGenerateOutMsg;
SecModInMsg_f     usm_secmodProcessInMsg;

int             Usm_generateOutMsg(int, u_char *, size_t, int, int,
                                     u_char *, size_t, char *, size_t,
                                     int, u_char *, size_t, void *,
                                     u_char *, size_t *, u_char **,
                                     size_t *);
int             Usm_rgenerateOutMsg(int, u_char *, size_t, int, int,
                                      u_char *, size_t, char *, size_t,
                                      int, u_char *, size_t, void *,
                                      u_char **, size_t *, size_t *);

int             Usm_processInMsg(int, size_t, u_char *, int, int,
                                   u_char *, size_t, u_char *,
                                   size_t *, char *, size_t *,
                                   u_char **, size_t *, size_t *,
                                   void **, Types_Session *, u_char);

int             Usm_checkSecLevel(int level, struct Usm_User_s *user);

struct Usm_User_s * Usm_getUserList(void);

struct Usm_User_s * Usm_getUser(u_char * engineID, size_t engineIDLen,
                             char *name);
struct Usm_User_s * Usm_getUserFromList(u_char * engineID,
                                       size_t engineIDLen, char *name,
                                       struct Usm_User_s *userList,
                                       int use_default);

struct Usm_User_s * Usm_addUser(struct Usm_User_s *user);
struct Usm_User_s * Usm_addUserToList(struct Usm_User_s *user,
                                     struct Usm_User_s *userList);

struct Usm_User_s * Usm_freeUser(struct Usm_User_s *user);

struct Usm_User_s * Usm_createUser(void);

struct Usm_User_s * Usm_createInitialUser(const char *name,
                                        const oid * authProtocol,
                                        size_t authProtocolLen,
                                        const oid * privProtocol,
                                        size_t privProtocolLen);

struct Usm_User_s * Usm_cloneFromUser(struct Usm_User_s *from,
                                   struct Usm_User_s *to);

struct Usm_User_s * Usm_removeUser(struct Usm_User_s *user);
struct Usm_User_s * Usm_removeUserFromList(struct Usm_User_s *user,
                                          struct Usm_User_s **userList);

void            Usm_saveUsers(const char *token, const char *type);
void            Usm_saveUsersFromList(struct Usm_User_s *user,
                                         const char *token,
                                         const char *type);
void            Usm_saveUser(struct Usm_User_s *user, const char *token,
                              const char *type);

Callback_f    Usm_storeUsers;

struct Usm_User_s * Usm_readUser(const char *line);

void            Usm_parseConfigUsmUser(const char *token,
                                         char *line);

void            Usm_setPassword(const char *token, char *line);

void            Usm_setUserPassword(struct Usm_User_s *user,
                                      const char *token, char *line);
void            Usm_initUsm(void);

void            Usm_initUsmConf(const char *app);
int             Usm_initUsmPostConfig(int majorid, int minorid,
                                     void *serverarg, void *clientarg);
int             Usm_deinitUsmPostConfig(int majorid, int minorid, void *serverarg,
                   void *clientarg);

void            Usm_clearUserList(void);

void            Usm_shutdownUsm(void);


int             Usm_createUserFromSession(Types_Session * session);
SecModPostDiscovery_f Usm_createUserFromSessionHook;

void            Usm_parseCreateUsmUser(const char *token,
                                         char *line);

const oid *    Usm_getDefaultAuthtype(size_t *);

const oid *    Usm_getDefaultPrivtype(size_t *);

void            Usm_v3PrivtypeConf(const char *word, char *cptr);

void            Usm_v3AuthtypeConf(const char *word, char *cptr);

void
Usm_saveUser(struct Usm_User_s *user, const char *token, const char *type);
#endif // USM_H
