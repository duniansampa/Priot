/*
 *  Errormibess watching mib group
 */
#ifndef _MIBGROUP_ERRORMIB_H
#define _MIBGROUP_ERRORMIB_H

#include "Vars.h"
#include "mibdefs.h"

void            init_errormib(void);


void            setPerrorstatus(const char *);
void            seterrorstatus(const char *, int);
extern FindVarMethodFT var_extensible_errors;


#endif                          /* _MIBGROUP_ERRORMIB_H */
