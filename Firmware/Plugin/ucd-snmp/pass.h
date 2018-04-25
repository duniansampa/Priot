/*
 *  pass: pass through extensiblity
 */
#ifndef _MIBGROUP_PASS_H
#define _MIBGROUP_PASS_H

#include "Vars.h"
#include "mibdefs.h"


void            init_pass(void);


extern FindVarMethodFT var_extensible_pass;
WriteMethodFT     setPass;
int             pass_compare(const void *, const void *);

/*
 * config file parsing routines 
 */
void            pass_free_config(void);
void            pass_parse_config(const char *, char *);


#endif                          /* _MIBGROUP_PASS_H */
