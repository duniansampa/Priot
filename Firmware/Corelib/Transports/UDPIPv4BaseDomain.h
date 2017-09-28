#ifndef UDPIPV4BASEDOMAIN_H
#define UDPIPV4BASEDOMAIN_H


#include "../Transport.h"

Transport_Transport *   UDPIPv4BaseDomain_transport(struct sockaddr_in *addr, int local);

int                UDPIPv4BaseDomain_recvfrom(int s, void *buf, int len, struct sockaddr *from,
                                                socklen_t *fromlen, struct sockaddr *dstip,
                                                socklen_t *dstlen, int *if_index);

int                UDPIPv4BaseDomain_sendto(int fd, struct in_addr *srcip, int if_index,
                                             struct sockaddr *remote, void *data, int len);

#endif // UDPIPV4BASEDOMAIN_H
