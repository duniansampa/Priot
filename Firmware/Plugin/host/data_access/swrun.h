/*
 * swrun data access header
 *
 * $Id: swrun.h 15346 2006-09-26 23:34:50Z rstory $
 */
/*
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#ifndef NETSNMP_ACCESS_SWRUN_CONFIG_H
#define NETSNMP_ACCESS_SWRUN_CONFIG_H

#include "CacheHandler.h"

void init_swrun(void);
void shutdown_swrun(void);

Cache     *netsnmp_swrun_cache(void);
Container_Container *netsnmp_swrun_container(void);

#endif /* NETSNMP_ACCESS_SWRUN_CONFIG_H */
