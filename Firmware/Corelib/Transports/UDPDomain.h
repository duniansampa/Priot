#ifndef UDPDOMAIN_H
#define UDPDOMAIN_H

#include "Transport.h"



//protected-ish functions used by other core-code
char *              UDPDomain_fmtaddr(Transport_Transport *t, void *data, int len);

int                 UDPDomain_recvfrom(int s, void *buf, int len, struct sockaddr *from,
                                       socklen_t *fromlen, struct sockaddr *dstip, socklen_t *dstlen,
                                       int *if_index);

int                 UDPDomain_sendto(int fd, struct in_addr *srcip, int if_index,
                                     struct sockaddr *remote, void *data, int len);

Transport_Transport *   UDPDomain_transport(struct sockaddr_in *addr, int local);

//Register any configuration tokens specific to the agent.
void                UDPDomain_agentConfigTokensRegister(void);


Transport_Transport *   UDPDomain_createTstring(const char *str, int local,
                                             const char *default_target);

Transport_Transport *   UDPDomain_createOstring(const u_char * o, size_t o_len, int local);


// "Constructor" for transport domain object.

void                UDPDomain_ctor(void);



#endif // UDPDOMAIN_H
