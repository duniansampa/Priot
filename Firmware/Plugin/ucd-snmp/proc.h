/*
 *  Process watching mib group
 */
#ifndef _MIBGROUP_PROC_H
#define _MIBGROUP_PROC_H

#include "mibdefs.h"
#include "Vars.h"


void            init_proc(void);

extern FindVarMethodFT var_extensible_proc;
extern WriteMethodFT fixProcError;
int             sh_count_procs(char *);

/*
 * config file parsing routines 
 */
void            proc_free_config(void);
void            proc_parse_config(const char *, char *);
void            procfix_parse_config(const char *, char *);


#define PROCMIN 3
#define PROCMAX 4
#define PROCCOUNT 5

#endif                          /* _MIBGROUP_PROC_H */
