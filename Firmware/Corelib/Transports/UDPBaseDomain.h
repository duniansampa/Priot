#ifndef UDPBASEDOMAIN_H
#define UDPBASEDOMAIN_H

#include "../Transport.h"


void     UDPBaseDomain_sockOptSet(int fd, int local);


int     UDPBaseDomain_recvfrom(int s, void *buf, int len, struct sockaddr *from, socklen_t *fromlen, struct sockaddr *dstip, socklen_t *dstlen, int *if_index);

int     UDPBaseDomain_sendto(int fd, struct in_addr *srcip, int if_index, struct sockaddr *remote, void *data, int len);

int     UDPBaseDomain_recv(Transport_Transport *t, void *buf, int size, void **opaque, int *olength);

int     UDPBaseDomain_send(Transport_Transport *t, void *buf, int size, void **opaque, int *olength);

#endif // UDPBASEDOMAIN_H
