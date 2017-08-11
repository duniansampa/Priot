#ifndef SYSTEM_H
#define SYSTEM_H

#include "Generals.h"

#include <netinet/in.h>
#include <netdb.h>


void                System_daemonPrep(_i32 stderr_log);
_i32                System_daemonize(_i32 quit_immediately, _i32 stderr_log);
struct hostent *    System_gethostbyaddr(const void *addr, socklen_t len, _i32 type);
_s32                System_getUptime(void);
_i32                System_gethostbynameV4(const _s8 * name, in_addr_t *addr_out);
_i32                System_getaddrinfo(const _s8 *name, const _s8 *service, const struct addrinfo *h_i32s, struct addrinfo **res);
struct hostent *    System_gethostbyname(const _s8 *name);
_i32                System_calculateTimeDiff(const struct timeval *now, const struct timeval *then);
_u32                System_calculateSectimeDiff(const struct timeval *now, const struct timeval *then);
_i32                System_mkdirhier(const _s8 *pathname, mode_t mode, _i32 skiplast);
_i32                System_strToUid(const _s8 *useroruid);
_i32                System_strToGid(const _s8 *grouporgid);
in_addr_t           System_getMyAddr(void);


#endif // SYSTEM_H
