#include "UDPBaseDomain.h"

#include "UDPDomain.h"
#include "../System.h"
#include "SocketBaseDomain.h"
#include <arpa/inet.h>
#include "../Debug.h"
#include "Assert.h"
#include "System.h"



void UDPBaseDomain_sockOptSet(int fd, int local)
{

    /*
     * Patch for Linux.  Without this, UDP packets that fail get an ICMP
     * response.  Linux turns the failed ICMP response into an error message
     * and return value, unlike all other OS's.
     */
    if (0 == System_osPrematch("Linux","2.4"))
    {
        int             one = 1;
        DEBUG_MSGTL(("socket:option", "setting socket option SO_BSDCOMPAT\n"));
        setsockopt(fd, SOL_SOCKET, SO_BSDCOMPAT, (void *) &one,
                   sizeof(one));
    }


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



int UDPBaseDomain_recvfrom(int s, void *buf, int len, struct sockaddr *from,
                         socklen_t *fromlen, struct sockaddr *dstip,
                         socklen_t *dstlen, int *if_index)
{
    int r;
    struct iovec iov;
    char cmsg[CMSG_SPACE(cmsg_data_size)];
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
        int r2 = getsockname(s, dstip, dstlen);
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


int UDPBaseDomain_sendto(int fd, struct in_addr *srcip, int if_index,
                           struct sockaddr *remote, void *data, int len)
{
    struct iovec iov;
    struct msghdr m = { 0 };
    char          cmsg[CMSG_SPACE(cmsg_data_size)];
    int           rc;

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


int UDPBaseDomain_recv(Transport_Transport *t, void *buf, int size, void **opaque, int *olength)
{
    int             rc = -1;
    socklen_t       fromlen = sizeof(Transport_SockaddrStorage);
    struct Transport_IndexedAddrPair_s *addr_pair = NULL;
    struct sockaddr *from;

    if (t != NULL && t->sock >= 0) {
        addr_pair = (struct Transport_IndexedAddrPair_s *) malloc(sizeof(struct Transport_IndexedAddrPair_s));
        if (addr_pair == NULL) {
            *opaque = NULL;
            *olength = 0;
            return -1;
        } else {
            memset(addr_pair, 0, sizeof(struct Transport_IndexedAddrPair_s));
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
            DEBUG_IF("priotUdp") {
                char *str = UDPDomain_fmtaddr(
                            NULL, addr_pair, sizeof(struct Transport_IndexedAddrPair_s));
                DEBUG_MSGTL(("priotUdp",
                            "recvfrom fd %d got %d bytes (from %s)\n",
                            t->sock, rc, str));
                free(str);
            }
        } else {
            DEBUG_MSGTL(("priotUdp", "recvfrom fd %d err %d (\"%s\")\n",
                        t->sock, errno, strerror(errno)));
        }
        *opaque = (void *)addr_pair;
        *olength = sizeof(struct Transport_IndexedAddrPair_s);
    }
    return rc;
}


int UDPBaseDomain_send(Transport_Transport *t, void *buf, int size,
                         void **opaque, int *olength)
{
    int rc = -1;
    struct Transport_IndexedAddrPair_s *addr_pair = NULL;
    struct sockaddr *to = NULL;

    if (opaque != NULL && *opaque != NULL &&
            ((*olength == sizeof(struct Transport_IndexedAddrPair_s) ||
              (*olength == sizeof(struct sockaddr_in))))) {
        addr_pair = (struct Transport_IndexedAddrPair_s *) (*opaque);
    } else if (t != NULL && t->data != NULL &&
               t->data_length == sizeof(struct Transport_IndexedAddrPair_s)) {
        addr_pair = (struct Transport_IndexedAddrPair_s *) (t->data);
    }

    to = &addr_pair->remote_addr.sa;

    if (to != NULL && t != NULL && t->sock >= 0) {
        DEBUG_IF("priotUdp") {
            char *str = UDPDomain_fmtaddr(NULL, (void *) addr_pair,
                                            sizeof(struct Transport_IndexedAddrPair_s));
            DEBUG_MSGTL(("priotUdp", "send %d bytes from %p to %s on fd %d\n",
                        size, buf, str, t->sock));
            free(str);
        }
        while (rc < 0) {

            rc = UDPDomain_sendto(t->sock,
                                    addr_pair ? &(addr_pair->local_addr.sin.sin_addr) : NULL,
                                    addr_pair ? addr_pair->if_index : 0, to, buf, size);

            if (rc < 0 && errno != EINTR) {
                DEBUG_MSGTL(("priotUdp", "sendto error, rc %d (errno %d)\n",
                            rc, errno));
                break;
            }
        }
    }
    return rc;
}


void UDPBaseDomain_ctor(void){

}
