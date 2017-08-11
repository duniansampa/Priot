#include "UnixDomain.h"
#include "SocketBaseDomain.h"
#include "System.h"
#include "Vacm.h"
#include <stddef.h>
#include <sys/un.h>
#include "../Debug.h"
#include "../Logger.h"
#include "Tools.h"
#include "ReadConfig.h"
#include "Strlcpy.h"
#include "Impl.h"



_oid unixDomain_priotUnixDomain[] = { TRANSPORT_DOMAIN_LOCAL };
static Transport_Tdomain unixDomain_unixDomain;


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

static char * UnixDomain_fmtaddr(Transport_Transport *t, void *data, int len)
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

static int UnixDomain_recv(Transport_Transport *t, void *buf, int size, void **opaque, int *olength)
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
                DEBUG_MSGTL(("netsnmp_unix", "recv fd %d err %d (\"%s\")\n",
                            t->sock, errno, strerror(errno)));
                return rc;
            }
            *opaque = (void*)to;
            *olength = sizeof(struct sockaddr_un);
        }
        DEBUG_MSGTL(("netsnmp_unix", "recv fd %d got %d bytes\n", t->sock, rc));
    }
    return rc;
}



static int UnixDomain_send(Transport_Transport *t, void *buf, int size, void **opaque, int *olength)
{
    int rc = -1;

    if (t != NULL && t->sock >= 0) {
        DEBUG_MSGTL(("netsnmp_unix", "send %d bytes to %p on fd %d\n",
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



static int UnixDomain_close(Transport_Transport *t)
{
    int rc = 0;
    UnixDomain_SockaddrUnPair *sup = (UnixDomain_SockaddrUnPair *) t->data;

    if (t->sock >= 0) {
        rc = close(t->sock);
        t->sock = -1;
        if (sup != NULL) {
            if (sup->local) {
                if (sup->server.sun_path[0] != 0) {
                  DEBUG_MSGTL(("netsnmp_unix", "close: server unlink(\"%s\")\n",
                              sup->server.sun_path));
                  unlink(sup->server.sun_path);
                }
            } else {
                if (sup->client.sun_path[0] != 0) {
                  DEBUG_MSGTL(("netsnmp_unix", "close: client unlink(\"%s\")\n",
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



static int UnixDomain_accept(Transport_Transport *t)
{
    struct sockaddr *farend = NULL;
    int             newsock = -1;
    socklen_t       farendlen = sizeof(struct sockaddr_un);

    farend = (struct sockaddr *) malloc(farendlen);

    if (farend == NULL) {
        /*
         * Indicate that the acceptance of this socket failed.
         */
        DEBUG_MSGTL(("netsnmp_unix", "accept: malloc failed\n"));
        return -1;
    }
    memset(farend, 0, farendlen);

    if (t != NULL && t->sock >= 0) {
        newsock = accept(t->sock, farend, &farendlen);

        if (newsock < 0) {
            DEBUG_MSGTL(("netsnmp_unix","accept failed rc %d errno %d \"%s\"\n",
                        newsock, errno, strerror(errno)));
            free(farend);
            return newsock;
        }

        if (t->data != NULL) {
            free(t->data);
        }

        DEBUG_MSGTL(("netsnmp_unix", "accept succeeded (farend %p len %d)\n",
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

static int unixDomain_createPath = 0;
static mode_t unixDomain_createMode;

/** If trying to create unix sockets in nonexisting directories then
 *  try to create the directory with mask mode.
 */
 void UnixDomain_unixCreatePathWithMode(int mode)
{
    unixDomain_createPath = 1;
    unixDomain_createMode = mode;
}

/** If trying to create unix sockets in nonexisting directories then
 *  fail.
 */
void UnixDomain_dontCreatePath(void)
{
    unixDomain_createPath = 0;
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

    t = TOOLS_MALLOC_TYPEDEF(Transport_Transport);
    if (t == NULL) {
        return NULL;
    }

    DEBUG_IF("netsnmp_unix") {
        char *str = UnixDomain_fmtaddr(NULL, (void *)addr,
                                         sizeof(struct sockaddr_un));
        DEBUG_MSGTL(("netsnmp_unix", "open %s %s\n", local ? "local" : "remote",
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

        if (rc != 0 && errno == ENOENT && unixDomain_createPath) {
            rc = System_mkdirhier(addr->sun_path, unixDomain_createMode, 1);
            if (rc != 0) {
                UnixDomain_close(t);
                Transport_free(t);
                return NULL;
            }
            rc = bind(t->sock, (struct sockaddr *) addr, SUN_LEN(addr));
        }
        if (rc != 0) {
            DEBUG_MSGTL(("netsnmp_unix_transport",
                        "couldn't bind \"%s\", errno %d (%s)\n",
                        addr->sun_path, errno, strerror(errno)));
            UnixDomain_close(t);
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
            DEBUG_MSGTL(("netsnmp_unix_transport",
                        "couldn't listen to \"%s\", errno %d (%s)\n",
                        addr->sun_path, errno, strerror(errno)));
            UnixDomain_close(t);
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
            DEBUG_MSGTL(("netsnmp_unix_transport",
                        "couldn't connect to \"%s\", errno %d (%s)\n",
                        addr->sun_path, errno, strerror(errno)));
            UnixDomain_close(t);
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
    t->f_recv     = UnixDomain_recv;
    t->f_send     = UnixDomain_send;
    t->f_close    = UnixDomain_close;
    t->f_accept   = UnixDomain_accept;
    t->f_fmtaddr  = UnixDomain_fmtaddr;

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
        Strlcpy_strlcpy(addr.sun_path, string, sizeof(addr.sun_path));
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
        Strlcpy_strlcpy(addr.sun_path, (const char *)o, sizeof(addr.sun_path));
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
    unixDomain_unixDomain.name = unixDomain_priotUnixDomain;
    unixDomain_unixDomain.name_length = sizeof(unixDomain_priotUnixDomain) / sizeof(_oid);
    unixDomain_unixDomain.prefix = (const char**)calloc(2, sizeof(char *));
    unixDomain_unixDomain.prefix[0] = "unix";

    unixDomain_unixDomain.f_create_from_tstring     = NULL;
    unixDomain_unixDomain.f_create_from_tstring_new = UnixDomain_createTstring;
    unixDomain_unixDomain.f_create_from_ostring     = UnixDomain_createOstring;

    Transport_tdomainRegister(&unixDomain_unixDomain);
}


#define EXAMPLE_COMMUNITY "COMMUNITY"

typedef struct UnixDomain_Com2SecUnixEntry_s {
    const char*     sockpath;
    const char*     secName;
    const char*     contextName;
    struct UnixDomain_Com2SecUnixEntry_s *next;
    unsigned short  pathlen;
    const char      community[1];
} UnixDomain_Com2SecUnixEntry;

static UnixDomain_Com2SecUnixEntry   *unixDomain_com2SecUnixList = NULL, *unixDomain_com2SecUnixListLast = NULL;


int UnixDomain_getSecName(void *opaque, int olength,
                        const char *community,
                        size_t community_len,
                        const char **secName, const char **contextName)
{
    const UnixDomain_Com2SecUnixEntry   *c;
    struct sockaddr_un *to = (struct sockaddr_un *) opaque;
    char           *ztcommunity = NULL;

    if (secName != NULL) {
        *secName = NULL;  /* Haven't found anything yet */
    }

    /*
     * Special case if there are NO entries (as opposed to no MATCHING
     * entries).
     */

    if (unixDomain_com2SecUnixList == NULL) {
        DEBUG_MSGTL(("netsnmp_unix_getSecName", "no com2sec entries\n"));
        return 0;
    }

    /*
     * If there is no unix socket path, then there can be no valid security
     * name.
     */

    if (opaque == NULL || olength != sizeof(struct sockaddr_un) ||
        to->sun_family != AF_UNIX) {
        DEBUG_MSGTL(("netsnmp_unix_getSecName",
                    "no unix destine address in PDU?\n"));
        return 1;
    }

    DEBUG_IF("netsnmp_unix_getSecName") {
        ztcommunity = (char *)malloc(community_len + 1);
        if (ztcommunity != NULL) {
            memcpy(ztcommunity, community, community_len);
            ztcommunity[community_len] = '\0';
        }

        DEBUG_MSGTL(("netsnmp_unix_getSecName", "resolve <\"%s\">\n",
                    ztcommunity ? ztcommunity : "<malloc error>"));
    }

    for (c = unixDomain_com2SecUnixList; c != NULL; c = c->next) {
        DEBUG_MSGTL(("netsnmp_unix_getSecName","compare <\"%s\",to socket %s>",
                    c->community, c->sockpath ));
        if ((community_len == strlen(c->community)) &&
            (memcmp(community, c->community, community_len) == 0) &&
            /* compare sockpath, if pathlen == 0, always match */
            (strlen(to->sun_path) == c->pathlen || c->pathlen == 0) &&
            (memcmp(to->sun_path, c->sockpath, c->pathlen) == 0)
            ) {
            DEBUG_MSG(("netsnmp_unix_getSecName", "... SUCCESS\n"));
            if (secName != NULL) {
                *secName = c->secName;
                *contextName = c->contextName;
            }
            break;
        }
        DEBUG_MSG(("netsnmp_unix_getSecName", "... nope\n"));
    }
    if (ztcommunity != NULL) {
        free(ztcommunity);
    }
    return 1;
}

void UnixDomain_parseSecurity(const char *token, char *param)
{
    char   secName[VACM_VACMSTRINGLEN + 1];
    size_t secNameLen;
    char   contextName[VACM_VACMSTRINGLEN + 1];
    size_t contextNameLen;
    char   community[IMPL_COMMUNITY_MAX_LEN + 1];
    size_t communityLen;
    char   sockpath[sizeof(((struct sockaddr_un*)0)->sun_path) + 1];
    size_t sockpathLen;

    param = ReadConfig_copyNword( param, secName, sizeof(secName));
    if (strcmp(secName, "-Cn") == 0) {
        if (!param) {
            ReadConfig_configPerror("missing CONTEXT_NAME parameter");
            return;
        }
        param = ReadConfig_copyNword( param, contextName, sizeof(contextName));
        contextNameLen = strlen(contextName) + 1;
        if (contextNameLen > VACM_VACMSTRINGLEN) {
            ReadConfig_configPerror("context name too long");
            return;
        }
        if (!param) {
            ReadConfig_configPerror("missing NAME parameter");
            return;
        }
        param = ReadConfig_copyNword( param, secName, sizeof(secName));
    } else {
        contextNameLen = 0;
    }

    secNameLen = strlen(secName) + 1;
    if (secNameLen == 1) {
        ReadConfig_configPerror("empty NAME parameter");
        return;
    } else if (secNameLen > VACM_VACMSTRINGLEN) {
        ReadConfig_configPerror("security name too long");
        return;
    }

    if (!param) {
        ReadConfig_configPerror("missing SOCKPATH parameter");
        return;
    }
    param = ReadConfig_copyNword( param, sockpath, sizeof(sockpath));
    if (sockpath[0] == '\0') {
        ReadConfig_configPerror("empty SOCKPATH parameter");
        return;
    }
    sockpathLen = strlen(sockpath) + 1;
    if (sockpathLen > sizeof(((struct sockaddr_un*)0)->sun_path)) {
        ReadConfig_configPerror("sockpath too long");
        return;
    }

    if (!param) {
        ReadConfig_configPerror("missing COMMUNITY parameter");
        return;
    }
    param = ReadConfig_copyNword( param, community, sizeof(community));
    if (community[0] == '\0') {
        ReadConfig_configPerror("empty COMMUNITY parameter");
        return;
    }
    communityLen = strlen(community) + 1;
    if (communityLen >= IMPL_COMMUNITY_MAX_LEN) {
        ReadConfig_configPerror("community name too long");
        return;
    }
    if (communityLen == sizeof(EXAMPLE_COMMUNITY) &&
        memcmp(community, EXAMPLE_COMMUNITY, sizeof(EXAMPLE_COMMUNITY)) == 0) {
        ReadConfig_configPerror("example config COMMUNITY not properly configured");
        return;
    }

    /* Deal with the "default" case */
    if(strcmp(sockpath, "default") == 0) {
        sockpathLen = 0;
    }

    {
        void* v = malloc(offsetof(UnixDomain_Com2SecUnixEntry, community) + communityLen +
                         sockpathLen + secNameLen + contextNameLen);
        UnixDomain_Com2SecUnixEntry* e = (UnixDomain_Com2SecUnixEntry*)v;
        char* last = ((char*)v) + offsetof(UnixDomain_Com2SecUnixEntry, community);
        if (e == NULL) {
            ReadConfig_configPerror("memory error");
            return;
        }

        DEBUG_MSGTL(("netsnmp_unix_parse_security",
                    "<\"%s\", \"%.*s\"> => \"%s\"\n",
                    community, (int)sockpathLen, sockpath, secName));

        memcpy(last, community, communityLen);
        last += communityLen;

        if (sockpathLen) {
            e->sockpath = last;
            memcpy(last, sockpath, sockpathLen);
            last += sockpathLen;
            e->pathlen = sockpathLen - 1;
        } else {
            e->sockpath = last - 1;
            e->pathlen = 0;
        }

        e->secName = last;
        memcpy(last, secName, secNameLen);
        last += secNameLen;

        if (contextNameLen) {
            e->contextName = last;
            memcpy(last, contextName, contextNameLen);
            last += contextNameLen;
        } else
            e->contextName = last - 1;

        e->next = NULL;

        if (unixDomain_com2SecUnixListLast != NULL) {
            unixDomain_com2SecUnixListLast->next = e;
            unixDomain_com2SecUnixListLast = e;
        } else {
            unixDomain_com2SecUnixListLast = unixDomain_com2SecUnixList = e;
        }
    }
}

void UnixDomain_com2SecListFree(void)
{
    UnixDomain_Com2SecUnixEntry   *e = unixDomain_com2SecUnixList;
    while (e != NULL) {
        UnixDomain_Com2SecUnixEntry   *tmp = e;
        e = e->next;
        free(tmp);
    }
    unixDomain_com2SecUnixList = unixDomain_com2SecUnixListLast = NULL;
}

void UnixDomain_agentConfigTokensRegister(void)
{
    ReadConfig_registerAppConfigHandler("com2secunix", UnixDomain_parseSecurity,
                                UnixDomain_com2SecListFree,
                                "[-Cn CONTEXT] secName sockpath community");
}
