/*
 *  Loadaveess watching mib group
 */
#ifndef _MIBGROUP_LOADAVE_H
#define _MIBGROUP_LOADAVE_H

#include "Vars.h"
#include "mibdefs.h"

void            init_loadave(void);
extern FindVarMethodFT var_extensible_loadave;

/*
 * config file parsing routines
 */
void            loadave_parse_config(const char *, char *);
void            loadave_free_config(void);
int             try_getloadavg(double *r_ave, size_t s_ave);


#define LOADAVE 3
#define LOADMAXVAL 4
#define LOADAVEINT 5
#define LOADAVEFLOAT 6

#endif                          /* _MIBGROUP_LOADAVE_H */
