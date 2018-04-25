/*
 * swinst data access header
 *
 * $Id: swinst.h 15346 2006-09-26 23:34:50Z rstory $
 */
/*
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#ifndef NETSNMP_ACCESS_SWINST_CONFIG_H
#define NETSNMP_ACCESS_SWINST_CONFIG_H

/*
 * all platforms use this generic code
 */

/*
 * select the appropriate architecture-specific interface code
 */

void init_swinst( void );
void shutdown_swinst( void );

#endif /* NETSNMP_ACCESS_SWINST_CONFIG_H */
