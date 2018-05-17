#include "UnixDomain.h"
#include "SocketBaseDomain.h"
#include "System.h"
#include "Vacm.h"
#include <stddef.h>
#include <sys/un.h>
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"
#include "ReadConfig.h"
#include "System/String.h"
#include "Impl.h"



oid unixDomain_priotUnixDomain[] = { TRANSPORT_DOMAIN_LOCAL };
static Transport_Tdomain _unixDomain_unixDomain;


/*
 * This is the structure we use to hold transport-specific data.
 */

typedef struct UnixDomain_SockaddrUnPair_s {
    int             local;
    struct sockaddr_un server;
    struct sockaddr_un client;
} UnixDomain_SockaddrUnPair;


/*
 * Return a string representing the address in data, or else the "far end"
 * address if data is NULL.
 */

static char * _UnixDomain_fmtaddr(Transport_Transport *t, void *data, int len)
{
    struct sockaddr_un *to = NULL;

    if (data != NULL) {
        to = (struct sockaddr_un *) data;
    } else if (t != NULL && t->data != NULL) {
        to = &(((UnixDomain_SockaddrUnPair *) t->data)->server);
        len = SUN_LEN(to);
    }
    if (to == NULL) {
        /*
         * "Local IPC" is the Posix.1g term for Unix domain protocols,
         * according to W. R. Stevens, ``Unix Network Programming Volume I
         * Second Edition'', p. 374.
         */
        return strdup("Local IPC: unknown");
    } else if (to->sun_path[0] == 0) {
        /*
         * This is an abstract name.  We could render it as hex or something
         * but let's not worry about that for now.
         */
        return strdup("Local IPC: abstract");
    } else {
        char           *tmp = (char *) malloc(16 + len);
        if (tmp != NULL) {
            sprintf(tmp, "Local IPC: %s", to->sun_path);
        }
        return tmp;
    }
}



/*
 * You can write something into opaque that will subsequently get passed back
 * to your send function if you like.  For instance, you might want to
 * remember where a PDU came from, so that you can send a reply there...
 */

static int _UnixDomain_recv(Transport_Transport *t, void *buf, int size, void **opaque, int *olength)
{
    int rc = -1;
    socklen_t       tolen = sizeof(struct sockaddr_un);
    struct sockaddr *to;


    if (t != NULL && t->sock >= 0) {
        to = (struct sockaddr *) malloc(sizeof(struct sockaddr_un));
        if (to == NULL) {
            *opaque = NULL;
            *olength = 0;
            return -1;
        } else {
            memset(to, 0, tolen);
        }
        if(getsockname(t->sock, to, &tolen) != 0){
            free(to);
            *opaque = NULL;
            *olength = 0;
            return -1;
        };
        while (rc < 0) {
            rc = recvfrom(t->sock, buf, size, MSG_DONTWAIT, NULL, NULL);

            if (rc < 0 && errno != EINTR) {
                DEBUG_MSGTL(("priotUnix", "recv fd %d err %d (\"%s\")\n",
                            t->sock, errno, strerror(errno)));
                return rc;
            }
            *opaque = (void*)to;
            *olength = sizeof(struct sockaddr_un);
        }
        DEBUG_MSGTL(("priotUnix", "recv fd %d got %d bytes\n", t->sock, rc));
    }
    return rc;
}



static int _UnixDomain_send(Transport_Transport *t, void *buf, int size, void **opaque, int *olength)
{
    int rc = -1;

    if (t != NULL && t->sock >= 0) {
        DEBUG_MSGTL(("priotUnix", "send %d bytes to %p on fd %d\n",
                    size, buf, t->sock));
        while (rc < 0) {
            rc = sendto(t->sock, buf, size, 0, NULL, 0);
            if (rc < 0 && errno != EINTR) {
                break;
            }
        }
    }
    return rc;
}



static int _UnixDomain_close(Transport_Transport *t)
{
    int rc = 0;
    UnixDomain_SockaddrUnPair *sup = (UnixDomain_SockaddrUnPair *) t->data;

    if (t->sock >= 0) {
        rc = close(t->sock);
        t->sock = -1;
        if (sup != NULL) {
            if (sup->local) {
                if (sup->server.sun_path[0] != 0) {
                  DEBUG_MSGTL(("priotUnix", "close: server unlink(\"%s\")\n",
                              sup->server.sun_path));
                  unlink(sup->server.sun_path);
                }
            } else {
                if (sup->client.sun_path[0] != 0) {
                  DEBUG_MSGTL(("priotUnix", "close: client unlink(\"%s\")\n",
                              sup->client.sun_path));
                  unlink(sup->client.sun_path);
                }
            }
        }
        return rc;
    } else {
        return -1;
    }
}



static int _UnixDomain_accept(Transport_Transport *t)
{
    struct sockaddr *farend = NULL;
    int             newsock = -1;
    socklen_t       farendlen = sizeof(struct sockaddr_un);

    farend = (struct sockaddr *) malloc(farendlen);

    if (farend == NULL) {
        /*
         * Indicate that the acceptance of this socket failed.
         */
        DEBUG_MSGTL(("priotUnix", "accept: malloc failed\n"));
        return -1;
    }
    memset(farend, 0, farendlen);

    if (t != NULL && t->sock >= 0) {
        newsock = accept(t->sock, farend, &farendlen);

        if (newsock < 0) {
            DEBUG_MSGTL(("priotUnix","accept failed rc %d errno %d \"%s\"\n",
                        newsock, errno, strerror(errno)));
            free(farend);
            return newsock;
        }

        if (t->data != NULL) {
            free(t->data);
        }

        DEBUG_MSGTL(("priotUnix", "accept succeeded (farend %p len %d)\n",
                    farend, (int) farendlen));
        t->data = farend;
        t->data_length = sizeof(struct sockaddr_un);
       SocketBaseDomain_bufferSet(newsock, SO_SNDBUF, 1, 0);
       SocketBaseDomain_bufferSet(newsock, SO_RCVBUF, 1, 0);
        return newsock;
    } else {
        free(farend);
        return -1;
    }
}

static int _unixDomain_createPath = 0;
static mode_t _unixDomain_createMode;

/** If trying to create unix sockets in nonexisting directories then
 *  try to create the directory with mask mode.
 */
 void UnixDomain_createPathWithMode(int mode)
{
    _unixDomain_createPath = 1;
    _unixDomain_createMode = mode;
}

/** If trying to create unix sockets in nonexisting directories then
 *  fail.
 */
void UnixDomain_dontCreatePath(void)
{
    _unixDomain_createPath = 0;
}

/*
 * Open a Unix-domain transport for SNMP.  Local is TRUE if addr is the local
 * address to bind to (i.e. this is a server-type session); otherwise addr is
 * the remote address to send things to (and we make up a temporary name for
 * the local end of the connection).
 */

Transport_Transport * UnixDomain_transport(struct sockaddr_un *addr, int local)
{
    Transport_Transport *t = NULL;
    UnixDomain_SockaddrUnPair *sup = NULL;
    int             rc = 0;


    if (addr == NULL || addr->sun_family != AF_UNIX) {
        return NULL;
    }

    t = MEMORY_MALLOC_TYPEDEF(Transport_Transport);
    if (t == NULL) {
        return NULL;
    }

    DEBUG_IF("priotUnix") {
        char *str = _UnixDomain_fmtaddr(NULL, (void *)addr,
                                         sizeof(struct sockaddr_un));
        DEBUG_MSGTL(("priotUnix", "open %s %s\n", local ? "local" : "remote",
                    str));
        free(str);
    }

    t->domain = unixDomain_priotUnixDomain;
    t->domain_length =
        sizeof(unixDomain_priotUnixDomain) / sizeof(unixDomain_priotUnixDomain[0]);

    t->data = malloc(sizeof(UnixDomain_SockaddrUnPair));
    if (t->data == NULL) {
        Transport_free(t);
        return NULL;
    }
    memset(t->data, 0, sizeof(UnixDomain_SockaddrUnPair));
    t->data_length = sizeof(UnixDomain_SockaddrUnPair);
    sup = (UnixDomain_SockaddrUnPair *) t->data;

    t->sock = socket(PF_UNIX, SOCK_STREAM, 0);
    if (t->sock < 0) {
        Transport_free(t);
        return NULL;
    }

    t->flags = TRANSPORT_FLAG_STREAM;

    if (local) {
        t->local = (u_char *)malloc(strlen(addr->sun_path));
        if (t->local == NULL) {
            Transport_free(t);
            return NULL;
        }
        memcpy(t->local, addr->sun_path, strlen(addr->sun_path));
        t->local_length = strlen(addr->sun_path);

        /*
         * This session is inteneded as a server, so we must bind to the given
         * path (unlinking it first, to avoid errors).
         */

        t->flags |= TRANSPORT_FLAG_LISTEN;

        unlink(addr->sun_path);
        rc = bind(t->sock, (struct sockaddr *) addr, SUN_LEN(addr));

        if (rc != 0 && errno == ENOENT && _unixDomain_createPath) {
            rc = System_mkdirhier(addr->sun_path, _unixDomain_createMode, 1);
            if (rc != 0) {
                _UnixDomain_close(t);
                Transport_free(t);
                return NULL;
            }
            rc = bind(t->sock, (struct sockaddr *) addr, SUN_LEN(addr));
        }
        if (rc != 0) {
            DEBUG_MSGTL(("priotUnixTransport",
                        "couldn't bind \"%s\", errno %d (%s)\n",
                        addr->sun_path, errno, strerror(errno)));
            _UnixDomain_close(t);
            Transport_free(t);
            return NULL;
        }

        /*
         * Save the address in the transport-specific data pointer for later
         * use by netsnmp_unix_close.
         */

        sup->server.sun_family = AF_UNIX;
        strcpy(sup->server.sun_path, addr->sun_path);
        sup->local = 1;

        /*
         * Now sit here and listen for connections to arrive.
         */

        rc = listen(t->sock, TRANSPORT_STREAM_QUEUE_LEN);
        if (rc != 0) {
            DEBUG_MSGTL(("priotUnixTransport",
                        "couldn't listen to \"%s\", errno %d (%s)\n",
                        addr->sun_path, errno, strerror(errno)));
            _UnixDomain_close(t);
            Transport_free(t);
            return NULL;
        }

    } else {
        t->remote = (u_char *)malloc(strlen(addr->sun_path));
        if (t->remote == NULL) {
            Transport_free(t);
            return NULL;
        }
        memcpy(t->remote, addr->sun_path, strlen(addr->sun_path));
        t->remote_length = strlen(addr->sun_path);

        rc = connect(t->sock, (struct sockaddr *) addr,
                     sizeof(struct sockaddr_un));
        if (rc != 0) {
            DEBUG_MSGTL(("priotUnixTransport",
                        "couldn't connect to \"%s\", errno %d (%s)\n",
                        addr->sun_path, errno, strerror(errno)));
            _UnixDomain_close(t);
            Transport_free(t);
            return NULL;
        }

        /*
         * Save the remote address in the transport-specific data pointer for
         * later use by netsnmp_unix_send.
         */

        sup->server.sun_family = AF_UNIX;
        strcpy(sup->server.sun_path, addr->sun_path);
        sup->local = 0;
        SocketBaseDomain_bufferSet(t->sock, SO_SNDBUF, local, 0);
        SocketBaseDomain_bufferSet(t->sock, SO_RCVBUF, local, 0);
    }

    /*
     * Message size is not limited by this transport (hence msgMaxSize
     * is equal to the maximum legal size of an SNMP message).
     */

    t->msgMaxSize = 0x7fffffff;
    t->f_recv     = _UnixDomain_recv;
    t->f_send     = _UnixDomain_send;
    t->f_close    = _UnixDomain_close;
    t->f_accept   = _UnixDomain_accept;
    t->f_fmtaddr  = _UnixDomain_fmtaddr;

    return t;
}

Transport_Transport *   UnixDomain_createTstring(const char *string, int local, const char *default_target)
{
    struct sockaddr_un addr;

    if (string && *string != '\0') {
    } else if (default_target && *default_target != '\0') {
      string = default_target;
    }

    if ((string != NULL && *string != '\0') &&
    (strlen(string) < sizeof(addr.sun_path))) {
        addr.sun_family = AF_UNIX;
        memset(addr.sun_path, 0, sizeof(addr.sun_path));
        String_copyTruncate(addr.sun_path, string, sizeof(addr.sun_path));
        return UnixDomain_transport(&addr, local);
    } else {
        if (string != NULL && *string != '\0') {
            Logger_log(LOGGER_PRIORITY_ERR, "Path too long for Unix domain transport\n");
        }
        return NULL;
    }
}



Transport_Transport *   UnixDomain_createOstring(const u_char * o, size_t o_len, int local)
{
    struct sockaddr_un addr;

    if (o_len > 0 && o_len < (sizeof(addr.sun_path) - 1)) {
        addr.sun_family = AF_UNIX;
        memset(addr.sun_path, 0, sizeof(addr.sun_path));
        String_copyTruncate(addr.sun_path, (const char *)o, sizeof(addr.sun_path));
        return UnixDomain_transport(&addr, local);
    } else {
        if (o_len > 0) {
            Logger_log(LOGGER_PRIORITY_ERR, "Path too long for Unix domain transport\n");
        }
    }
    return NULL;
}



void UnixDomain_ctor(void)
{
    _unixDomain_unixDomain.name = unixDomain_priotUnixDomain;
    _unixDomain_unixDomain.name_length = sizeof(unixDomain_priotUnixDomain) / sizeof(oid);
    _unixDomain_unixDomain.prefix = (const char**)calloc(2, sizeof(char *));
    _unixDomain_unixDomain.prefix[0] = "unix";

    _unixDomain_unixDomain.f_create_from_tstring     = NULL;
    _unixDomain_unixDomain.f_create_from_tstring_new = UnixDomain_createTstring;
    _unixDomain_unixDomain.f_create_from_ostring     = UnixDomain_createOstring;

    Transport_tdomainRegister(&_unixDomain_unixDomain);
}




void UnixDomain_agentConfigTokensRegister(void)
{
}
