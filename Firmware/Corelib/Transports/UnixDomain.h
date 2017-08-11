#ifndef UNIXDOMAIN_H
#define UNIXDOMAIN_H

#include "../Transport.h"


#define TRANSPORT_DOMAIN_LOCAL	1,3,6,1,2,1,100,1,13

Transport_Transport *   UnixDomain_transport(struct sockaddr_un *addr, int local);

void                UnixDomain_agentConfigTokensRegister(void);

void                UnixDomain_parseSecurity(const char *token, char *param);

void                UnixDomain_ctor(void);

void                UnixDomain_createPathWithMode(int mode);


void                UnixDomain_dontCreatePath(void);


Transport_Transport *   UnixDomain_createTstring(const char *string, int local, const char *default_target);


Transport_Transport *   UnixDomain_createOstring(const u_char * o, size_t o_len, int local);


int                 UnixDomain_getSecName(void *opaque, int olength,
                                          const char *community,
                                          size_t community_len,
                                          const char **secName, const char **contextName);


void                UnixDomain_com2SecListFree(void);



#endif // UNIXDOMAIN_H
