#ifndef UCDCOMPAT_H
#define UCDCOMPAT_H

#include "Generals.h"

#include "Types.h"
/*
 * from snmp_api.h
 */

void            UcdCompat_setDumpPacket(int);

int             UcdCompat_getDumpPacket(void);

void            UcdCompat_setQuickPrint(int);

int             UcdCompat_getQuickPrint(void);

void            UcdCompat_setSuffixOnly(int);

int             UcdCompat_getSuffixOnly(void);

void            UcdCompat_setFullObjid(int);
int             UcdCompat_getFullObjid(void);

void            UcdCompat_setRandomAccess(int);

int             UcdCompat_getRandomAccess(void);

/*
 * from parse.h
 */

void            UcdCompat_setMibWarnings(int);

void            UcdCompat_setMibErrors(int);

void            UcdCompat_setSaveDescriptions(int);
void            UcdCompat_setMibCommentTerm(int);
void            UcdCompat_setMibParseLabel(int);


#endif // UCDCOMPAT_H
