#ifndef TCPDOMAIN_H
#define TCPDOMAIN_H

#include "Transport.h"

/*
 * The SNMP over TCP over IPv4 transport domain is identified by
 * transportDomainTcpIpv4 as defined in RFC 3419.
 */

#define TRANSPORT_DOMAIN_TCP_IP		1,3,6,1,2,1,100,1,5

extern oid tPCDomain_priotTCPDomain[];

Transport_Transport *   TPCDomain_transport(struct sockaddr_in *addr, int local);

Transport_Transport *   TPCDomain_createTstring(const char *str, int local, const char *default_target);

//* "Constructor" for transport domain object.
void                TPCDomain_ctor(void);

Transport_Transport *   TPCDomain_createOstring(const u_char * o, size_t o_len, int local);


#endif // TCPDOMAIN_H
