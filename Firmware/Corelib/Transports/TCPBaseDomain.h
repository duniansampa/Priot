#ifndef TCPBASEDOMAIN_H
#define TCPBASEDOMAIN_H

#include "../Transport.h"


int TCPBaseDomain_recv(Transport_Transport *t, void *buf, int size, void **opaque, int *olength);
int TCPBaseDomain_send(Transport_Transport *t, void *buf, int size, void **opaque, int *olength);

#endif // TCPBASEDOMAIN_H
