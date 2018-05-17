#include "IPv4BaseDomain.h"


#include "DefaultStore.h"
#include "System.h"
#include "System/Util/Debug.h"
#include "Priot.h"



//initialize the sockaddr_in struct.
int IPv4BaseDomain_sockaddrIn(struct sockaddr_in *addr,
                                        const char *inpeername, int remote_port)
{
    char buf[sizeof(int) * 3 + 2];
    sprintf(buf, ":%u", remote_port);
    return IPv4BaseDomain_sockaddrIn2(addr, inpeername, remote_port ? buf : NULL);
}

//initialize the sockaddr_in struct.
int IPv4BaseDomain_sockaddrIn2(struct sockaddr_in *addr,
                                         const char *inpeername, const char *default_target)
{
    int ret;

    if (addr == NULL) {
        return 0;
    }

    DEBUG_MSGTL(("sockaddrIn",
                "addr %p, inpeername \"%s\", default_target \"%s\"\n",
                addr, inpeername ? inpeername : "[NIL]",
                default_target ? default_target : "[NIL]"));

    memset(addr, 0, sizeof(struct sockaddr_in));
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    addr->sin_family = AF_INET;
    addr->sin_port = htons((u_short)PRIOT_PORT);

    {
        int port = DefaultStore_getInt(DsStorage_LIBRARY_ID,
                                                      DsInt_DEFAULT_PORT);

        if (port != 0) {
            addr->sin_port = htons((u_short)port);
        } else if (default_target != NULL)
            IPv4BaseDomain_sockaddrIn2(addr, default_target, NULL);  //recursive call
    }

    if (inpeername != NULL && *inpeername != '\0') {
        const char     *host, *port;
        char           *peername = NULL;
        char           *cp;
        /*
         * Duplicate the peername because we might want to mank around with
         * it.
         */

        peername = strdup(inpeername);
        if (peername == NULL) {
            return 0;
        }

        /*
         * Try and extract an appended port number.
         */
        cp = strchr(peername, ':');
        if (cp != NULL) {
            *cp = '\0';
            port = cp + 1;
            host = peername;
        } else {
            host = NULL;
            port = peername;
        }

        /*
         * Try to convert the user port specifier
         */
        if (port && *port == '\0')
            port = NULL;

        if (port != NULL) {
            long int l;
            char * ep;

            DEBUG_MSGTL(("sockaddrIn", "check user service %s\n",
                        port));

            l = strtol(port, &ep, 10);
            if (ep != port && *ep == '\0' && 0 <= l && l <= 0x0ffff)
                addr->sin_port = htons((u_short)l);
            else {
                if (host == NULL) {
                    DEBUG_MSGTL(("sockaddrIn",
                                "servname not numeric, "
                                "check if it really is a destination)\n"));
                    host = port;
                    port = NULL;
                } else {
                    DEBUG_MSGTL(("sockaddrIn", "servname not numeric\n"));
                    free(peername);
                    return 0;
                }
            }
        }

        /*
         * Try to convert the user host specifier
         */
        if (host && *host == '\0')
            host = NULL;

        if (host != NULL) {
            DEBUG_MSGTL(("sockaddrIn", "check destination %s\n", host));


            if (strcmp(peername, "255.255.255.255") == 0 ) {
                /*
                 * The explicit broadcast address hack
                 */
                DEBUG_MSGTL(("sockaddrIn", "Explicit UDP broadcast\n"));
                addr->sin_addr.s_addr = INADDR_NONE;
            } else {
                ret = System_gethostbynameV4(peername, &addr->sin_addr.s_addr);
                if (ret < 0) {
                    DEBUG_MSGTL(("sockaddrIn", "couldn't resolve hostname\n"));
                    free(peername);
                    return 0;
                }
                DEBUG_MSGTL(("sockaddrIn", "hostname (resolved okay)\n"));
            }
        }
        free(peername);
    }

    /*
     * Finished
     */

    DEBUG_MSGTL(("sockaddrIn", "return { AF_INET, %s:%hu }\n",
                inet_ntoa(addr->sin_addr), ntohs(addr->sin_port)));
    return 1;
}

char * IPv4BaseDomain_fmtaddr(const char *prefix, Transport_Transport *t,
                                           void *data, int len)
{
    Transport_IndexedAddrPair *addr_pair = NULL;
    struct hostent *host;
    char tmp[64];

    if (data != NULL && len == sizeof(Transport_IndexedAddrPair)) {
        addr_pair = (Transport_IndexedAddrPair *) data;
    } else if (t != NULL && t->data != NULL) {
        addr_pair = (Transport_IndexedAddrPair *) t->data;
    }

    if (addr_pair == NULL) {
        snprintf(tmp, sizeof(tmp), "%s: unknown", prefix);
    } else {
        struct sockaddr_in *to = NULL;
        to = (struct sockaddr_in *) &(addr_pair->remote_addr);
        if (to == NULL) {
            snprintf(tmp, sizeof(tmp), "%s: unknown->[%s]:%hu", prefix,
                     inet_ntoa(addr_pair->local_addr.sin.sin_addr),
                     ntohs(addr_pair->local_addr.sin.sin_port));
        } else if ( t && t->flags &  TRANSPORT_FLAG_HOSTNAME  ) {
            /* XXX: hmm...  why isn't this prefixed */
            /* assuming intentional */
            host = System_gethostbyaddr((char *)&to->sin_addr, sizeof(struct in_addr), AF_INET);
            return (host ? strdup(host->h_name) : NULL);
        } else {
            snprintf(tmp, sizeof(tmp), "%s: [%s]:%hu->", prefix,
                     inet_ntoa(to->sin_addr), ntohs(to->sin_port));
            snprintf(tmp + strlen(tmp), sizeof(tmp)-strlen(tmp), "[%s]:%hu",
                     inet_ntoa(addr_pair->local_addr.sin.sin_addr),
                     ntohs(addr_pair->local_addr.sin.sin_port));
        }
    }
    tmp[sizeof(tmp)-1] = '\0';
    return strdup(tmp);
}
