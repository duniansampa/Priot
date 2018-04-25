#ifndef ROUTE_HEADER_H
#define ROUTE_HEADER_H

#define GATEWAY                 /* MultiNet is always configured this way! */
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <net/if.h>
#include <net/route.h>
#include <string.h>
#include <stdlib.h>
#include <sys/sysctl.h>

#undef	KERNEL
#define rt_hash rt_pad1
#define rt_refcnt rt_pad2
#define rt_use rt_pad3
#define rt_unit rt_refcnt       /* Reuse this field for device # */


#endif  //ROUTE_HEADER_H
