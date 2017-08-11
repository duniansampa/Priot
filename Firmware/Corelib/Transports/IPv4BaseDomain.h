#ifndef IPV4BASEDOMAIN_H
#define IPV4BASEDOMAIN_H

#include "../Transport.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>



_s8 *    IPv4BaseDomain_fmtaddr(const _s8 *prefix, Transport_Transport *t, void *data, _i32 len);

//Convert a "traditional" peername into a sockaddr_in structure which is
//written to *addr_  Returns 1 if the conversion was successful, or 0 if it
//failed
_i32     IPv4BaseDomain_sockaddrIn(struct sockaddr_in *addr, const _s8 *peername, _i32 remote_port);
_i32     IPv4BaseDomain_sockaddrIn2(struct sockaddr_in *addr, const _s8 *inpeername, const _s8 *default_target);


#endif // IPV4BASEDOMAIN_H
