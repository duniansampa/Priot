#ifndef VACMCONF_H
#define VACMCONF_H

#include "Types.h"
#include "Callback.h"


#define VACM_CREATE_SIMPLE_V3       1
#define VACM_CREATE_SIMPLE_COM      2
#define VACM_CREATE_SIMPLE_COMIPV4  3
#define VACM_CREATE_SIMPLE_COMIPV6  4
#define VACM_CREATE_SIMPLE_COMUNIX  5

void            VacmConf_initVacmConf(void);
void            VacmConf_initVacmConfigTokens(void);
void            VacmConf_freeGroup(void);
void            VacmConf_freeAccess(void);
void            VacmConf_freeView(void);
void            VacmConf_parseGroup(const char *, char *);
void            VacmConf_parseAccess(const char *, char *);
void            VacmConf_parseSetaccess(const char *, char *);
void            VacmConf_parseView(const char *, char *);
void            VacmConf_parseRocommunity(const char *, char *);
void            VacmConf_parseRwcommunity(const char *, char *);
void            VacmConf_parseRocommunity6(const char *, char *);
void            VacmConf_parseRwcommunity6(const char *, char *);
void            VacmConf_parseRouser(const char *, char *);
void            VacmConf_parseRwuser(const char *, char *);
void            VacmConf_createSimple(const char *, char *, int, int);
void            VacmConf_parseAuthcommunity(const char *, char *);
void            VacmConf_parseAuthuser(const char *, char *);
void            VacmConf_parseAuthaccess(const char *, char *);

Callback_CallbackFT    VacmConf_inViewCallback;
Callback_CallbackFT    VacmConf_warnIfNotConfigured;
Callback_CallbackFT    VacmConf_standardViews;

int             VacmConf_inView(Types_Pdu *, oid *, size_t, int);
int             VacmConf_checkView(Types_Pdu *, oid *, size_t, int, int);
int             VacmConf_checkViewContents(Types_Pdu *, oid *, size_t, int, int, int);

#define VACM_CHECK_VIEW_CONTENTS_NO_FLAGS        0
#define VACM_CHECK_VIEW_CONTENTS_DNE_CONTEXT_OK  1


#endif // VACMCONF_H
