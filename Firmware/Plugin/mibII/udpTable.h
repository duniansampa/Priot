/*
 *  Template MIB group interface - udp.h
 *
 */
#ifndef _MIBGROUP_UDPTABLE_H
#define _MIBGROUP_UDPTABLE_H

#include "AgentHandler.h"
#include "CacheHandler.h"
#include "TableIterator.h"

extern void     init_udpTable(void);
extern NodeHandlerFT udpTable_handler;
extern CacheLoadFT udpTable_load;
extern CacheFreeFT udpTable_free;
extern FirstDataPointFT udpTable_first_entry;
extern NextDataPointFT  udpTable_next_entry;

#define UDPLOCALADDRESS     1
#define UDPLOCALPORT	    2

#endif                          /* _MIBGROUP_UDPTABLE_H */
