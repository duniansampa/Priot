/*
 *  Template MIB group interface - versioninfo.h
 *
 */
#ifndef _MIBGROUP_VERSIONINFO_H
#define _MIBGROUP_VERSIONINFO_H

#include "Vars.h"
#include "mibdefs.h"

void            init_versioninfo(void);

extern FindVarMethodFT var_extensible_version;
extern WriteMethodFT update_hook;
extern WriteMethodFT debugging_hook;
extern WriteMethodFT save_persistent;


/*
 * Version info mib 
 */
#define VERTAG 2
#define VERDATE 3
#define VERCDATE 4
#define VERIDENT 5
#define VERCONFIG 6
#define VERCLEARCACHE 10
#define VERUPDATECONFIG 11
#define VERRESTARTAGENT 12
#define VERSAVEPERSISTENT 13
#define VERDEBUGGING 20


#endif                          /* _MIBGROUP_VERSIONINFO_H */
