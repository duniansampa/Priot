/*
 * util_funcs/header_simple_table.h:  utilitiy functions for extensible
 * groups.
 */
#ifndef _MIBGROUP_UTIL_FUNCS_HEADER_SIMPLE_TABLE_H
#define _MIBGROUP_UTIL_FUNCS_HEADER_SIMPLE_TABLE_H

#include "Vars.h"

int header_simple_table(struct Variable_s *, oid *, size_t *, int, size_t *,
                        WriteMethodFT **, int);


#endif /* _MIBGROUP_UTIL_FUNCS_HEADER_SIMPLE_TABLE_H */
