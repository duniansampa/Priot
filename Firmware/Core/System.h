#ifndef SYSTEM_H
#define SYSTEM_H

#include "Generals.h"

#include <netinet/in.h>
#include <netdb.h>


int                System_daemonize(int quit_immediately, int stderr_log);
struct hostent *   System_gethostbyaddr(const void *addr, socklen_t len, int type);
long               System_getUptime(void);
int                System_gethostbynameV4(const char * name, in_addr_t *addr_out);
int                System_getaddrinfo(const char *name, const char *service, const struct addrinfo *h_i32s, struct addrinfo **res);
struct hostent *   System_gethostbyname(const char *name);
int                System_calculateTimeDiff(const struct timeval *now, const struct timeval *then);
uint               System_calculateSectimeDiff(const struct timeval *now, const struct timeval *then);
int                System_mkdirhier(const char *pathname, mode_t mode, int skiplast);
const char *       System_mktemp(void);
int                System_osPrematch(const char *ospmname, const char *ospmrelprefix);
int                System_strToUid(const char *useroruid);
int                System_strToGid(const char *grouporgid);
in_addr_t          System_getMyAddr(void);


#endif // SYSTEM_H
