#include "TCPDomain.h"

#include  "IPv4BaseDomain.h"
#include  "SocketBaseDomain.h"
#include  "TCPBaseDomain.h"
#include  "System/Util/Trace.h"
#include  "System/Util/Utilities.h"
#include  "Impl.h"
/*
 * needs to be in sync with the definitions in snmplib/snmpUDPDomain.c
 * and perl/agent/agent.xs
 */
typedef struct Transport_IndexedAddrPair_s TPCDomain_UdpAddrPair;

oid tPCDomain_priotTCPDomain[] = { TRANSPORT_DOMAIN_TCP_IP };

static Transport_Tdomain _tCPDomain_tcpDomain;


/*
 * Return a string representing the address in data, or else the "far end"
 * address if data is NULL.
 */

static char * _TPCDomain_fmtaddr(Transport_Transport *t, void *data, int len)
{
    return IPv4BaseDomain_fmtaddr("TCP", t, data, len);
}

static int _TPCDomain_accept(Transport_Transport *t)
{
    struct sockaddr *farend = NULL;
    TPCDomain_UdpAddrPair *addr_pair = NULL;
    int             newsock = -1;
    socklen_t       farendlen = sizeof(TPCDomain_UdpAddrPair);

    addr_pair = (TPCDomain_UdpAddrPair *)malloc(farendlen);
    if (addr_pair == NULL) {
        /*
         * Indicate that the acceptance of this socket failed.
         */
         DEBUG_MSGTL(("priotTcp", "accept: malloc failed\n"));
        return -1;
    }
    memset(addr_pair, 0, sizeof *addr_pair);
    farend = &addr_pair->remote_addr.sa;

    if (t != NULL && t->sock >= 0) {
        newsock = accept(t->sock, farend, &farendlen);

        if (newsock < 0) {
             DEBUG_MSGTL(("priotTcp", "accept failed rc %d errno %d \"%s\"\n",
            newsock, errno, strerror(errno)));
            free(addr_pair);
            return newsock;
        }

        if (t->data != NULL) {
            free(t->data);
        }

        t->data = addr_pair;
        t->data_length = sizeof(TPCDomain_UdpAddrPair);
        DEBUG_IF("priotTcp") {
            char *str = _TPCDomain_fmtaddr(NULL, farend, farendlen);
            DEBUG_MSGTL(("priotTcp", "accept succeeded (from %s)\n", str));
            free(str);
        }

        /*
         * Try to make the new socket blocking.
         */

        if (SocketBaseDomain_setNonBlockingMode(newsock, FALSE) < 0)
            DEBUG_MSGTL(("priotTcp", "couldn't f_getfl of fd %d\n",
                        newsock));

        /*
         * Allow user to override the send and receive buffers. Default is
         * to use os default.  Don't worry too much about errors --
         * just plough on regardless.
         */
        SocketBaseDomain_bufferSet(newsock, SO_SNDBUF, 1, 0);
        SocketBaseDomain_bufferSet(newsock, SO_RCVBUF, 1, 0);

        return newsock;
    } else {
        free(addr_pair);
        return -1;
    }
}



/*
 * Open a TCP-based transport for SNMP.  Local is TRUE if addr is the local
 * address to bind to (i.e. this is a server-type session); otherwise addr is
 * the remote address to send things to.
 */

Transport_Transport *
TPCDomain_transport(struct sockaddr_in *addr, int local)
{
    Transport_Transport *t = NULL;
    TPCDomain_UdpAddrPair *addr_pair = NULL;
    int rc = 0;

    if (addr == NULL || addr->sin_family != AF_INET) {
        return NULL;
    }

    t = MEMORY_MALLOC_TYPEDEF(Transport_Transport);
    if (t == NULL) {
        return NULL;
    }

    addr_pair = (TPCDomain_UdpAddrPair *)malloc(sizeof(TPCDomain_UdpAddrPair));
    if (addr_pair == NULL) {
        Transport_free(t);
        return NULL;
    }
    memset(addr_pair, 0, sizeof *addr_pair);
    t->data = addr_pair;
    t->data_length = sizeof(TPCDomain_UdpAddrPair);
    memcpy(&(addr_pair->remote_addr), addr, sizeof(struct sockaddr_in));

    t->domain = tPCDomain_priotTCPDomain;
    t->domain_length =
        sizeof(tPCDomain_priotTCPDomain) / sizeof(tPCDomain_priotTCPDomain[0]);

    t->sock = socket(PF_INET, SOCK_STREAM, 0);
    if (t->sock < 0) {
        Transport_free(t);
        return NULL;
    }

    t->flags = TRANSPORT_FLAG_STREAM;

    if (local) {
        int opt = 1;

        /*
         * This session is inteneded as a server, so we must bind to the given
         * IP address (which may include an interface address, or could be
         * INADDR_ANY, but will always include a port number.
         */

        t->flags |= TRANSPORT_FLAG_LISTEN;
        t->local = (u_char *)malloc(6);
        if (t->local == NULL) {
            SocketBaseDomain_close(t);
            Transport_free(t);
            return NULL;
        }
        memcpy(t->local, (u_char *) & (addr->sin_addr.s_addr), 4);
        t->local[4] = (ntohs(addr->sin_port) & 0xff00) >> 8;
        t->local[5] = (ntohs(addr->sin_port) & 0x00ff) >> 0;
        t->local_length = 6;

        /*
         * We should set SO_REUSEADDR too.
         */

        setsockopt(t->sock, SOL_SOCKET, SO_REUSEADDR, (void *)&opt,
           sizeof(opt));

        rc = bind(t->sock, (struct sockaddr *)addr, sizeof(struct sockaddr));
        if (rc != 0) {
            SocketBaseDomain_close(t);
            Transport_free(t);
            return NULL;
        }

        /*
         * Since we are going to be letting select() tell us when connections
         * are ready to be accept()ed, we need to make the socket non-blocking
         * to avoid the race condition described in W. R. Stevens, ``Unix
         * Network Programming Volume I Second Edition'', pp. 422--4, which
         * could otherwise wedge the agent.
         */

        SocketBaseDomain_setNonBlockingMode(t->sock, TRUE);

        /*
         * Now sit here and wait for connections to arrive.
         */

        rc = listen(t->sock, TRANSPORT_STREAM_QUEUE_LEN);
        if (rc != 0) {
            SocketBaseDomain_close(t);
            Transport_free(t);
            return NULL;
        }

    } else {
      t->remote = (u_char *)malloc(6);
        if (t->remote == NULL) {
            SocketBaseDomain_close(t);
            Transport_free(t);
            return NULL;
        }
        memcpy(t->remote, (u_char *) & (addr->sin_addr.s_addr), 4);
        t->remote[4] = (ntohs(addr->sin_port) & 0xff00) >> 8;
        t->remote[5] = (ntohs(addr->sin_port) & 0x00ff) >> 0;
        t->remote_length = 6;

        /*
         * This is a client-type session, so attempt to connect to the far
         * end.  We don't go non-blocking here because it's not obvious what
         * you'd then do if you tried to do snmp_sends before the connection
         * had completed.  So this can block.
         */

        rc = connect(t->sock, (struct sockaddr *)addr,
             sizeof(struct sockaddr));

        if (rc < 0) {
            SocketBaseDomain_close(t);
            Transport_free(t);
            return NULL;
        }

        /*
         * Allow user to override the send and receive buffers. Default is
         * to use os default.  Don't worry too much about errors --
         * just plough on regardless.
         */
        SocketBaseDomain_bufferSet(t->sock, SO_SNDBUF, local, 0);
        SocketBaseDomain_bufferSet(t->sock, SO_RCVBUF, local, 0);
    }

    /*
     * Message size is not limited by this transport (hence msgMaxSize
     * is equal to the maximum legal size of an SNMP message).
     */

    t->msgMaxSize = 0x7fffffff;
    t->f_recv     = TCPBaseDomain_recv;
    t->f_send     = TCPBaseDomain_send;
    t->f_close    = SocketBaseDomain_close;
    t->f_accept   = _TPCDomain_accept;
    t->f_fmtaddr  = _TPCDomain_fmtaddr;

    return t;
}



Transport_Transport *
TPCDomain_createTstring(const char *str, int local,
               const char *default_target)
{
    struct sockaddr_in addr;

    if (IPv4BaseDomain_sockaddrIn2(&addr, str, default_target)) {
        return TPCDomain_transport(&addr, local);
    } else {
        return NULL;
    }
}



Transport_Transport *
TPCDomain_createOstring(const u_char * o, size_t o_len, int local)
{
    struct sockaddr_in addr;

    if (o_len == 6) {
        unsigned short porttmp = (o[4] << 8) + o[5];
        addr.sin_family = AF_INET;
        memcpy((u_char *) & (addr.sin_addr.s_addr), o, 4);
        addr.sin_port = htons(porttmp);
        return TPCDomain_transport(&addr, local);
    }
    return NULL;
}



void TPCDomain_ctor(void)
{
    _tCPDomain_tcpDomain.name = tPCDomain_priotTCPDomain;
    _tCPDomain_tcpDomain.name_length = sizeof(tPCDomain_priotTCPDomain) / sizeof(oid);
    _tCPDomain_tcpDomain.prefix = (const char **)calloc(2, sizeof(char *));
    _tCPDomain_tcpDomain.prefix[0] = "tcp";

    _tCPDomain_tcpDomain.f_create_from_tstring     = NULL;
    _tCPDomain_tcpDomain.f_create_from_tstring_new = TPCDomain_createTstring;
    _tCPDomain_tcpDomain.f_create_from_ostring     = TPCDomain_createOstring;

    Transport_tdomainRegister(&_tCPDomain_tcpDomain);
}
