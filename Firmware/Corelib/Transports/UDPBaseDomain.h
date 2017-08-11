#ifndef UDPBASEDOMAIN_H
#define UDPBASEDOMAIN_H

#include "../Transport.h"


void     UDPBaseDomain_sockOptSet(_i32 fd, _i32 local);


_i32     UDPBaseDomain_recvfrom(_i32 s, void *buf, _i32 len, struct sockaddr *from, socklen_t *fromlen, struct sockaddr *dstip, socklen_t *dstlen, _i32 *if_index);

_i32     UDPBaseDomain_sendto(_i32 fd, struct in_addr *srcip, _i32 if_index, struct sockaddr *remote, void *data, _i32 len);

_i32     UDPBaseDomain_recv(Transport_Transport *t, void *buf, _i32 size, void **opaque, _i32 *olength);

_i32     UDPBaseDomain_send(Transport_Transport *t, void *buf, _i32 size, void **opaque, _i32 *olength);

#endif // UDPBASEDOMAIN_H
