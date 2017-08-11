#ifndef V3_H
#define V3_H

#include "Generals.h"
#include "Types.h"


#define MAX_ENGINEID_LENGTH 32 /* per SNMP-FRAMEWORK-MIB SnmpEngineID TC */

#define ENGINEID_TYPE_IPV4    1
#define ENGINEID_TYPE_IPV6    2
#define ENGINEID_TYPE_MACADDR 3
#define ENGINEID_TYPE_TEXT    4
#define ENGINEID_TYPE_EXACT   5
#define ENGINEID_TYPE_NETSNMP_RND 128

#define	DEFAULT_NIC "eth0"


int             V3_setupEngineID(u_char ** eidp, const char *text);
void            V3_engineIDConf(const char *word, char *cptr);
void            V3_engineBootsConf(const char *, char *);
void            V3_engineIDTypeConf(const char *, char *);
void            V3_engineIDNicConf(const char *, char *);

void            V3_initV3(const char *);
int             V3_initPostConfig(int majorid, int minorid,
                                        void *serverarg,
                                        void *clientarg);
int             V3_initPostPremibConfig(int majorid,
                                               int minorid,
                                               void *serverarg,
                                               void *clientarg);
//void            V3_shutdown(const char *type);
int             V3_store(int majorID, int minorID, void *serverarg,
                             void *clientarg);

u_long          V3_localEngineBoots(void);
int             V3_cloneEngineID(u_char **, size_t *, u_char *,
                                      size_t);

size_t          V3_getEngineID(u_char * buf, size_t buflen);

u_char  *       V3_generateEngineID(size_t *);

u_long          V3_localEngineTime(void);
//int             V3_getDefaultSecLevel(void);
//void            V3_setEngineBootsAndTime(int boots, int ttime);
int             V3_freeEngineID(int majorid, int minorid, void *serverarg,
                                 void *clientarg);

int             V3_parseSecLevelConf(const char* word, char *cptr);

int            V3_options(char *optarg, Types_Session * session, char **Apsz,
                          char **Xpsz, int argc, char *const *argv);

#endif // V3_H
