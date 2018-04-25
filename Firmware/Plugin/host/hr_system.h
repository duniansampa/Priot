/*
 *  Host Resources MIB - system group interface - hr_system.h
 *
 */
#ifndef _MIBGROUP_HRSYSTEM_H
#define _MIBGROUP_HRSYSTEM_H

#include "Vars.h"

extern void     init_hr_system(void);
extern FindVarMethodFT var_hrsys;

int ns_set_time(int action, u_char * var_val, u_char var_val_type, size_t var_val_len, u_char * statP, oid * name, size_t name_len);

#endif                          /* _MIBGROUP_HRSYSTEM_H */
