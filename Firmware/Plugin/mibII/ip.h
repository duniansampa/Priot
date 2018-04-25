/*
 *  Template MIB group interface - ip.h
 *
 */

#ifndef _MIBGROUP_IP_H
#define _MIBGROUP_IP_H

#include "AgentHandler.h"
#include "CacheHandler.h"

extern void     init_ip(void);
extern NodeHandlerFT ip_handler;
extern CacheLoadFT ip_load;
extern CacheFreeFT ip_free;


#define IPFORWARDING	  1
#define IPDEFAULTTTL	  2
#define IPINRECEIVES	  3
#define IPINHDRERRORS	  4
#define IPINADDRERRORS	  5
#define IPFORWDATAGRAMS   6
#define IPINUNKNOWNPROTOS 7
#define IPINDISCARDS	  8
#define IPINDELIVERS	  9
#define IPOUTREQUESTS	 10
#define IPOUTDISCARDS	 11
#define IPOUTNOROUTES	 12
#define IPREASMTIMEOUT	 13
#define IPREASMREQDS	 14
#define IPREASMOKS	 15
#define IPREASMFAILS	 16
#define IPFRAGOKS	 17
#define IPFRAGFAILS	 18
#define IPFRAGCREATES	 19
#define IPADDRTABLE	 20	/* Placeholder */
#define IPROUTETABLE	 21	/* Placeholder */
#define IPMEDIATABLE	 22	/* Placeholder */
#define IPROUTEDISCARDS	 23

#define IPADADDR	  1
#define IPADIFINDEX	  2
#define IPADNETMASK	  3
#define IPADBCASTADDR	  4
#define IPADREASMMAX	  5

#define IPROUTEDEST	  1
#define IPROUTEIFINDEX	  2
#define IPROUTEMETRIC1	  3
#define IPROUTEMETRIC2	  4
#define IPROUTEMETRIC3	  5
#define IPROUTEMETRIC4	  6
#define IPROUTENEXTHOP	  7
#define IPROUTETYPE	  8
#define IPROUTEPROTO	  9
#define IPROUTEAGE	 10
#define IPROUTEMASK	 11
#define IPROUTEMETRIC5	 12
#define IPROUTEINFO	 13

/* #define IPMEDIAIFINDEX		1 */
/* #define IPMEDIAPHYSADDRESS	2 */
/* #define IPMEDIANETADDRESS	3 */
/* #define IPMEDIATYPE		4 */

#endif                          /* _MIBGROUP_IP_H */
