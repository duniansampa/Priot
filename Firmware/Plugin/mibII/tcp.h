/*
 *  TCP MIB group interface - tcp.h
 *
 */
#ifndef _MIBGROUP_TCP_H
#define _MIBGROUP_TCP_H

#include "AgentHandler.h"
#include "CacheHandler.h"


extern void     init_tcp(void);
extern NodeHandlerFT tcp_handler;
extern CacheLoadFT tcp_load;
extern CacheFreeFT tcp_free;


#define TCPRTOALGORITHM      1
#define TCPRTOMIN	     2
#define TCPRTOMAX	     3
#define TCPMAXCONN	     4
#define TCPACTIVEOPENS	     5
#define TCPPASSIVEOPENS      6
#define TCPATTEMPTFAILS      7
#define TCPESTABRESETS	     8
#define TCPCURRESTAB	     9
#define TCPINSEGS	    10
#define TCPOUTSEGS	    11
#define TCPRETRANSSEGS	    12
#define TCPCONNTABLE        13	/* Placeholder */
#define TCPINERRS           14
#define TCPOUTRSTS          15


#endif                          /* _MIBGROUP_TCP_H */
