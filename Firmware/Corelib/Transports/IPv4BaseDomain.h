#ifndef IPV4BASEDOMAIN_H
#define IPV4BASEDOMAIN_H

#include "Transport.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>



char *    IPv4BaseDomain_fmtaddr(const char *prefix, Transport_Transport *t, void *data, int len);

//Convert a "traditional" peername into a sockaddr_in structure which is
//written to *addr_  Returns 1 if the conversion was successful, or 0 if it
//failed
int     IPv4BaseDomain_sockaddrIn(struct sockaddr_in *addr, const char *peername, int remote_port);
int     IPv4BaseDomain_sockaddrIn2(struct sockaddr_in *addr, const char *inpeername, const char *default_target);


#endif // IPV4BASEDOMAIN_H
