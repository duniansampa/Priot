#include "UDPIPv4BaseDomain.h"

#include "UDPBaseDomain.h"
#include "UDPDomain.h"
#include "SocketBaseDomain.h"
#include "DefaultStore.h"
#include "IPv4BaseDomain.h"
#include "../Debug.h"
#include "Tools.h"
#include "Assert.h"
#include "Assert.h"


_i32 UDPIPv4BaseDomain_recvfrom(_i32 s, void *buf, _i32 len, struct sockaddr *from,
                             socklen_t *fromlen, struct sockaddr *dstip,
                             socklen_t *dstlen, _i32 *if_index)
{
    return UDPBaseDomain_recvfrom(s, buf, len, from, fromlen, dstip, dstlen, if_index);
}

_i32 UDPIPv4BaseDomain_sendto(_i32 fd, struct in_addr *srcip, _i32 if_index,
                           struct sockaddr *remote, void *data, _i32 len)
{
    return UDPBaseDomain_sendto(fd, srcip, if_index, remote, data, len);
}


Transport_Transport * UDPIPv4BaseDomain_transport(struct sockaddr_in *addr, _i32 local){

    Transport_Transport *t = NULL;
    _i32             rc = 0, rc2;
    _s8           *client_socket = NULL;
    Transport_IndexedAddrPair_s addr_pair;
    socklen_t       local_addr_len;


    if (addr == NULL || addr->sin_family != AF_INET) {
        return NULL;
    }

    memset(&addr_pair, 0, sizeof(Transport_IndexedAddrPair_s));
    memcpy(&(addr_pair.remote_addr), addr, sizeof(struct sockaddr_in));

    t = TOOLS_MALLOC_TYPEDEF(Transport_Transport);
    Assert_assertOrReturn(t != NULL, NULL);

    DEBUG_IF("netsnmp_udpbase") {
        _s8 *str = UDPDomain_fmtaddr(NULL, (void *)&addr_pair,
                                        sizeof(Transport_IndexedAddrPair_s));
        DEBUG_MSGTL(("netsnmp_udpbase", "open %s %s\n",
                    local ? "local" : "remote", str));
        free(str);
    }

    t->sock = socket(PF_INET, SOCK_DGRAM, 0);
    DEBUG_MSGTL(("UDPBase", "openned socket %d as local=%d\n", t->sock, local));
    if (t->sock < 0) {
        Transport_free(t);
        return NULL;
    }

    UDPBaseDomain_sockOptSet(t->sock, local);

    if (local) {
        /*
         * This session is inteneded as a server, so we must bind on to the
         * given IP address, which may include an interface address, or could
         * be INADDR_ANY, but certainly includes a port number.
         */

        t->local = (_u8 *) malloc(6);
        if (t->local == NULL) {
            Transport_free(t);
            return NULL;
        }
        memcpy(t->local, (_u8 *) & (addr->sin_addr.s_addr), 4);
        t->local[4] = (ntohs(addr->sin_port) & 0xff00) >> 8;
        t->local[5] = (ntohs(addr->sin_port) & 0x00ff) >> 0;
        t->local_length = 6;
        {
            _i32 sockopt = 1;
            if (setsockopt(t->sock, SOL_IP, IP_PKTINFO, &sockopt, sizeof sockopt) == -1) {
                DEBUG_MSGTL(("netsnmp_udpbase", "couldn't set IP_PKTINFO: %s\n",
                    strerror(errno)));
                Transport_free(t);
                return NULL;
            }
            DEBUG_MSGTL(("netsnmp_udpbase", "set IP_PKTINFO\n"));
        }

        rc = bind(t->sock, (struct sockaddr *) addr,
                  sizeof(struct sockaddr));
        if (rc != 0) {
            SocketBaseDomain_close(t);
            Transport_free(t);
            return NULL;
        }
        t->data = NULL;
        t->data_length = 0;

    } else {
        /*
         * This is a client session.  If we've been given a
         * client address to send from, then bind to that.
         * Otherwise the send will use "something sensible".
         */
        client_socket = (_s8 *) DefaultStore_getString(DEFAULTSTORE_STORAGE::LIBRARY_ID, DEFAULTSTORE_LIB_STRING::CLIENT_ADDR);

        if (client_socket) {
            struct sockaddr_in client_addr;
            IPv4BaseDomain_sockaddrIn2(&client_addr, client_socket, NULL);
            client_addr.sin_port = 0;
            DEBUG_MSGTL(("netsnmp_udpbase", "binding socket: %d\n", t->sock));
            rc = bind(t->sock, (struct sockaddr *)&client_addr,
                  sizeof(struct sockaddr));
            if ( rc != 0 ) {
                DEBUG_MSGTL(("netsnmp_udpbase", "failed to bind for clientaddr: %d %s\n",
                            errno, strerror(errno)));
                SocketBaseDomain_close(t);
                Transport_free(t);
                return NULL;
            }
            local_addr_len = sizeof(addr_pair.local_addr);
            rc2 = getsockname(t->sock, (struct sockaddr*)&addr_pair.local_addr,
                              &local_addr_len);
            Assert_assert(rc2 == 0);
        }

        DEBUG_IF("netsnmp_udpbase") {
            _s8 *str = UDPDomain_fmtaddr(NULL, (void *)&addr_pair,
                                            sizeof(Transport_IndexedAddrPair_s));
            DEBUG_MSGTL(("netsnmp_udpbase", "client open %s\n", str));
            free(str);
        }

        /*
         * Save the (remote) address in the
         * transport-specific data pointer for later use by netsnmp_udp_send.
         */

        t->data = malloc(sizeof(Transport_IndexedAddrPair_s));
        t->remote = (_u8 *)malloc(6);
        if (t->data == NULL || t->remote == NULL) {
            Transport_free(t);
            return NULL;
        }
        memcpy(t->remote, (_u8 *) & (addr->sin_addr.s_addr), 4);
        t->remote[4] = (ntohs(addr->sin_port) & 0xff00) >> 8;
        t->remote[5] = (ntohs(addr->sin_port) & 0x00ff) >> 0;
        t->remote_length = 6;
        memcpy(t->data, &addr_pair, sizeof(Transport_IndexedAddrPair_s));
        t->data_length = sizeof(Transport_IndexedAddrPair_s);
    }

    return t;
}
