#include "UDPBaseDomain.h"

#include "UDPDomain.h"
#include "../System.h"
#include "SocketBaseDomain.h"
#include <arpa/inet.h>
#include "../Debug.h"
#include "Assert.h"



void UDPBaseDomain_sockOptSet(_i32 fd, _i32 local)
{

    /*
     * SO_REUSEADDR will allow multiple apps to open the same port at
     * the same time. Only the last one to open the socket will get
     * data. Obviously, for an agent, this is a bad thing. There should
     * only be one listener.
     */

    /*
     * Try to set the send and receive buffers to a reasonably large value, so
     * that we can send and receive big PDUs (defaults to 8192 bytes (!) on
     * Solaris, for instance).  Don't worry too much about errors -- just
     * plough on regardless.
     */
     SocketBaseDomain_bufferSet(fd, SO_SNDBUF, local, 0);
     SocketBaseDomain_bufferSet(fd, SO_RCVBUF, local, 0);
}

enum {
    cmsg_data_size = sizeof(struct in_pktinfo)
};



_i32 UDPBaseDomain_recvfrom(_i32 s, void *buf, _i32 len, struct sockaddr *from,
                         socklen_t *fromlen, struct sockaddr *dstip,
                         socklen_t *dstlen, _i32 *if_index)
{
    _i32 r;
    struct iovec iov;
    _s8 cmsg[CMSG_SPACE(cmsg_data_size)];
    struct cmsghdr *cm;
    struct msghdr msg;

    iov.iov_base = buf;
    iov.iov_len = len;

    memset(&msg, 0, sizeof msg);
    msg.msg_name = from;
    msg.msg_namelen = *fromlen;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = &cmsg;
    msg.msg_controllen = sizeof(cmsg);

    r = recvmsg(s, &msg, MSG_DONTWAIT);


    if (r == -1) {
        return -1;
    }

    DEBUG_MSGTL(("udpbase:recv", "got source addr: %s\n",
                inet_ntoa(((struct sockaddr_in *)from)->sin_addr)));

    {
        /* Get the local port number for use in diagnostic messages */
        _i32 r2 = getsockname(s, dstip, dstlen);
        Assert_assert(r2 == 0);
    }
    for (cm = CMSG_FIRSTHDR(&msg); cm != NULL; cm = CMSG_NXTHDR(&msg, cm)) {

        if (cm->cmsg_level == SOL_IP && cm->cmsg_type == IP_PKTINFO) {
            struct in_pktinfo* src = (struct in_pktinfo *)CMSG_DATA(cm);
            Assert_assert(dstip->sa_family == AF_INET);
            ((struct sockaddr_in*)dstip)->sin_addr = src->ipi_addr;
            *if_index = src->ipi_ifindex;
            DEBUG_MSGTL(("udpbase:recv",
                        "got destination (local) addr %s, iface %d\n",
                        inet_ntoa(src->ipi_addr), *if_index));
        }
    }

    return r;
}


_i32 UDPBaseDomain_sendto(_i32 fd, struct in_addr *srcip, _i32 if_index,
                           struct sockaddr *remote, void *data, _i32 len)
{
    struct iovec iov;
    struct msghdr m = { 0 };
    _s8          cmsg[CMSG_SPACE(cmsg_data_size)];
    _i32           rc;

    iov.iov_base = data;
    iov.iov_len  = len;

    m.msg_name		= remote;
    m.msg_namelen	= sizeof(struct sockaddr_in);
    m.msg_iov		= &iov;
    m.msg_iovlen	= 1;
    m.msg_flags		= 0;

    if (srcip && srcip->s_addr != INADDR_ANY) {
        struct cmsghdr *cm;

        DEBUG_MSGTL(("udpbase:sendto", "sending from %s\n", inet_ntoa(*srcip)));

        memset(cmsg, 0, sizeof(cmsg));

        m.msg_control    = &cmsg;
        m.msg_controllen = sizeof(cmsg);

        cm = CMSG_FIRSTHDR(&m);
        cm->cmsg_len = CMSG_LEN(cmsg_data_size);

        cm->cmsg_level = SOL_IP;
        cm->cmsg_type = IP_PKTINFO;

        {
            struct in_pktinfo ipi;

            memset(&ipi, 0, sizeof(ipi));
            /*
             * Except in the case of responding
             * to a broadcast, setting the ifindex
             * when responding results in incorrect
             * behavior of changing the source address
             * that the manager sees the response
             * come from.
             */
            ipi.ipi_ifindex = 0;

            ipi.ipi_spec_dst.s_addr = srcip->s_addr;

            memcpy(CMSG_DATA(cm), &ipi, sizeof(ipi));
        }

        rc = sendmsg(fd, &m, MSG_NOSIGNAL|MSG_DONTWAIT);
        if (rc >= 0 || errno != EINVAL)
            return rc;

        /*
         * The error might be caused by broadcast srcip (i.e. we're responding
         * to a broadcast request) - sendmsg does not like it. Try to resend it
         * using the interface on which it was received
         */

        DEBUG_MSGTL(("udpbase:sendto", "re-sending on iface %d\n", if_index));

        {
            struct in_pktinfo ipi;

            memset(&ipi, 0, sizeof(ipi));
            ipi.ipi_ifindex = if_index;
            ipi.ipi_spec_dst.s_addr = INADDR_ANY;

            memcpy(CMSG_DATA(cm), &ipi, sizeof(ipi));
        }
        rc = sendmsg(fd, &m, MSG_NOSIGNAL|MSG_DONTWAIT);
        if (rc >= 0 || errno != EINVAL)
            return rc;

        DEBUG_MSGTL(("udpbase:sendto", "re-sending without source address\n"));
        m.msg_control = NULL;
        m.msg_controllen = 0;
    }

    return sendmsg(fd, &m, MSG_NOSIGNAL|MSG_DONTWAIT);
}

/*
 * You can write something into opaque that will subsequently get passed back
 * to your send function if you like.  For instance, you might want to
 * remember where a PDU came from, so that you can send a reply there...
 */


_i32 UDPBaseDomain_recv(Transport_Transport *t, void *buf, _i32 size, void **opaque, _i32 *olength)
{
    _i32             rc = -1;
    socklen_t       fromlen = sizeof(Transport_SockaddrStorage);
    Transport_IndexedAddrPair_s *addr_pair = NULL;
    struct sockaddr *from;

    if (t != NULL && t->sock >= 0) {
        addr_pair = (Transport_IndexedAddrPair_s *) malloc(sizeof(Transport_IndexedAddrPair_s));
        if (addr_pair == NULL) {
            *opaque = NULL;
            *olength = 0;
            return -1;
        } else {
            memset(addr_pair, 0, sizeof(Transport_IndexedAddrPair_s));
            from = &addr_pair->remote_addr.sa;
        }

        while (rc < 0) {
            socklen_t local_addr_len = sizeof(addr_pair->local_addr);
            rc = UDPDomain_recvfrom(t->sock, buf, size, from, &fromlen,
                                      (struct sockaddr*)&(addr_pair->local_addr),
                                      &local_addr_len, &(addr_pair->if_index));

            if (rc < 0 && errno != EINTR) {
                break;
            }
        }

        if (rc >= 0) {
            DEBUG_IF("netsnmp_udp") {
                _s8 *str = UDPDomain_fmtaddr(
                            NULL, addr_pair, sizeof(Transport_IndexedAddrPair_s));
                DEBUG_MSGTL(("netsnmp_udp",
                            "recvfrom fd %d got %d bytes (from %s)\n",
                            t->sock, rc, str));
                free(str);
            }
        } else {
            DEBUG_MSGTL(("netsnmp_udp", "recvfrom fd %d err %d (\"%s\")\n",
                        t->sock, errno, strerror(errno)));
        }
        *opaque = (void *)addr_pair;
        *olength = sizeof(Transport_IndexedAddrPair_s);
    }
    return rc;
}


_i32 UDPBaseDomain_send(Transport_Transport *t, void *buf, _i32 size,
                         void **opaque, _i32 *olength)
{
    _i32 rc = -1;
    Transport_IndexedAddrPair_s *addr_pair = NULL;
    struct sockaddr *to = NULL;

    if (opaque != NULL && *opaque != NULL &&
            ((*olength == sizeof(Transport_IndexedAddrPair_s) ||
              (*olength == sizeof(struct sockaddr_in))))) {
        addr_pair = (Transport_IndexedAddrPair_s *) (*opaque);
    } else if (t != NULL && t->data != NULL &&
               t->data_length == sizeof(Transport_IndexedAddrPair_s)) {
        addr_pair = (Transport_IndexedAddrPair_s *) (t->data);
    }

    to = &addr_pair->remote_addr.sa;

    if (to != NULL && t != NULL && t->sock >= 0) {
        DEBUG_IF("netsnmp_udp") {
            _s8 *str = UDPDomain_fmtaddr(NULL, (void *) addr_pair,
                                            sizeof(Transport_IndexedAddrPair_s));
            DEBUG_MSGTL(("netsnmp_udp", "send %d bytes from %p to %s on fd %d\n",
                        size, buf, str, t->sock));
            free(str);
        }
        while (rc < 0) {

            rc = UDPDomain_sendto(t->sock,
                                    addr_pair ? &(addr_pair->local_addr.sin.sin_addr) : NULL,
                                    addr_pair ? addr_pair->if_index : 0, to, buf, size);

            if (rc < 0 && errno != EINTR) {
                DEBUG_MSGTL(("netsnmp_udp", "sendto error, rc %d (errno %d)\n",
                            rc, errno));
                break;
            }
        }
    }
    return rc;
}
