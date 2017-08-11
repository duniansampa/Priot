#ifndef TCPBASEDOMAIN_H
#define TCPBASEDOMAIN_H

#include "../Transport.h"


_i32 TCPBaseDomain_recv(Transport_Transport *t, void *buf, _i32 size, void **opaque, _i32 *olength);
_i32 TCPBaseDomain_send(Transport_Transport *t, void *buf, _i32 size, void **opaque, _i32 *olength);

#endif // TCPBASEDOMAIN_H
