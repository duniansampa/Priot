/*
 *  Template MIB group interface - udp.h
 *
 */
#ifndef _MIBGROUP_UDP_H
#define _MIBGROUP_UDP_H

#include "AgentHandler.h"
#include "CacheHandler.h"

extern void     init_udp(void);
extern NodeHandlerFT udp_handler;
extern CacheLoadFT udp_load;
extern CacheFreeFT udp_free;


#define UDPINDATAGRAMS	    1
#define UDPNOPORTS	    2
#define UDPINERRORS	    3
#define UDPOUTDATAGRAMS     4

#endif                          /* _MIBGROUP_UDP_H */
