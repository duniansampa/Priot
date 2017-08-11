#include "TCPBaseDomain.h"
#include "../Debug.h"

/*
 * You can write something into opaque that will subsequently get passed back
 * to your send function if you like.  For instance, you might want to
 * remember where a PDU came from, so that you can send a reply there...
 */

_i32 TCPBaseDomain_recv(Transport_Transport *t, void *buf, _i32 size, void **opaque, _i32 *olength)
{
    _i32 rc = -1;

    if (t != NULL && t->sock >= 0) {
    while (rc < 0) {
        rc = recvfrom(t->sock, buf, size, 0, NULL, NULL);
        if (rc < 0 && errno != EINTR) {
        DEBUG_MSGTL(("netsnmp_tcpbase", "recv fd %d err %d (\"%s\")\n",
                t->sock, errno, strerror(errno)));
        break;
        }
        DEBUG_MSGTL(("netsnmp_tcpbase", "recv fd %d got %d bytes\n",
            t->sock, rc));
    }
    } else {
        return -1;
    }

    if (opaque != NULL && olength != NULL) {
        if (t->data_length > 0) {
            if ((*opaque = malloc(t->data_length)) != NULL) {
                memcpy(*opaque, t->data, t->data_length);
                *olength = t->data_length;
            } else {
                *olength = 0;
            }
        } else {
            *opaque = NULL;
            *olength = 0;
        }
    }

    return rc;
}

_i32 TCPBaseDomain_send(Transport_Transport *t, void *buf, _i32 size, void **opaque, _i32 *olength) {
    _i32 rc = -1;

    if (t != NULL && t->sock >= 0) {
    while (rc < 0) {
        rc = sendto(t->sock, buf, size, 0, NULL, 0);
        if (rc < 0 && errno != EINTR) {
        break;
        }
    }
    }
    return rc;
}
