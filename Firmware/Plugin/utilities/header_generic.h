/*
 *  util_funcs/header_generic.h:  utilitiy functions for extensible groups.
 */
#ifndef _MIBGROUP_UTIL_FUNCS_HEADER_GENERIC_H
#define _MIBGROUP_UTIL_FUNCS_HEADER_GENERIC_H

#include "Vars.h"

int header_generic(struct Variable_s *, oid *, size_t *, int, size_t *,
                   WriteMethodFT **);



#endif /* _MIBGROUP_UTIL_FUNCS_HEADER_GENERIC_H */
