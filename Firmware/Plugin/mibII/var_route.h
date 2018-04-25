/*
 *  Template MIB group interface - var_route.h
 *
 */
#ifndef _MIBGROUP_VAR_ROUTE_H
#define _MIBGROUP_VAR_ROUTE_H

#include "Vars.h"

void            init_var_route(void);

extern FindVarMethodFT var_ipRouteEntry;

struct rtentry **netsnmp_get_routes(size_t *out_numroutes);

#endif                          /* _MIBGROUP_VAR_ROUTE_H */
