/*
 *  pass: pass through extensiblity
 */
#ifndef _MIBGROUP_PASS_PERSIST_H
#define _MIBGROUP_PASS_PERSIST_H

#include "Vars.h"
#include "mibdefs.h"

void            init_pass_persist(void);
void            shutdown_pass_persist(void);
extern FindVarMethodFT var_extensible_pass_persist;
extern WriteMethodFT setPassPersist;

/*
 * config file parsing routines 
 */
void            pass_persist_free_config(void);
void            pass_persist_parse_config(const char *, char *);
int             pass_persist_compare(const void *, const void *);


#endif                          /* _MIBGROUP_PASS_PERSIST_H */
