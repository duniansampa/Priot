#ifndef SOCKETBASEDOMAIN_H
#define SOCKETBASEDOMAIN_H

#include "../Transport.h"



int             SocketBaseDomain_close(Transport_Transport *t);
int             SocketBaseDomain_bufferSet(int s, int optname, int local, int size);
int             SocketBaseDomain_setNonBlockingMode(int sock, int non_blocking_mode);
int             SocketBaseDomain_bufferSet(int s, int optname, int local, int size);

#endif // SOCKETBASEDOMAIN_H
