/*
 *  Template MIB group interface - tcp.h
 *
 */
#ifndef _MIBGROUP_TCPTABLE_H
#define _MIBGROUP_TCPTABLE_H

#include "AgentHandler.h"
#include "CacheHandler.h"
#include "TableIterator.h"

struct inpcb {
    struct inpcb   *inp_next;   /* pointers to other pcb's */
    struct in_addr  inp_faddr;  /* foreign host table entry */
    u_short         inp_fport;  /* foreign port */
    struct in_addr  inp_laddr;  /* local host table entry */
    u_short         inp_lport;  /* local port */
    int             inp_state;
    int             uid;        /* owner of the connection */
};

extern void                init_tcpTable(void);
extern NodeHandlerFT     tcpTable_handler;
extern CacheLoadFT         tcpTable_load;
extern CacheFreeFT         tcpTable_free;
extern FirstDataPointFT tcpTable_first_entry;
extern NextDataPointFT  tcpTable_next_entry;

#define TCPCONNSTATE	     1
#define TCPCONNLOCALADDRESS  2
#define TCPCONNLOCALPORT     3
#define TCPCONNREMOTEADDRESS 4
#define TCPCONNREMOTEPORT    5

#endif                          /* _MIBGROUP_TCPTABLE_H */
