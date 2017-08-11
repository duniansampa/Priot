#ifndef UDPIPV4BASEDOMAIN_H
#define UDPIPV4BASEDOMAIN_H


#include "../Transport.h"

Transport_Transport *   UDPIPv4BaseDomain_transport(struct sockaddr_in *addr, _i32 local);

_i32                UDPIPv4BaseDomain_recvfrom(_i32 s, void *buf, _i32 len, struct sockaddr *from,
                                                socklen_t *fromlen, struct sockaddr *dstip,
                                                socklen_t *dstlen, _i32 *if_index);

_i32                UDPIPv4BaseDomain_sendto(_i32 fd, struct in_addr *srcip, _i32 if_index,
                                             struct sockaddr *remote, void *data, _i32 len);

#endif // UDPIPV4BASEDOMAIN_H
