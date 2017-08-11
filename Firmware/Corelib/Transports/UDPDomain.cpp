#include "UDPDomain.h"

#include  "IPv4BaseDomain.h"
#include "UDPIPv4BaseDomain.h"
#include "UDPBaseDomain.h"
#include "SocketBaseDomain.h"
#include "Vacm.h"
#include "System.h"
#include <stddef.h>
#include "../Debug.h"
#include "ReadConfig.h"
#include "Impl.h"

static Transport_Tdomain uDPDomain_uDPDomain;

/*
 * needs to be in sync with the definitions in snmplib/snmpTCPDomain.c
 * and perl/agent/agent.xs
 */
typedef Transport_IndexedAddrPair UDPDomain_UdpAddrPair;


/*
 * Return a string representing the address in data, or else the "far end"
 * address if data is NULL.
 */

char * UDPDomain_fmtaddr(Transport_Transport *t, void *data, int len)
{
    return IPv4BaseDomain_fmtaddr("UDP", t, data, len);
}


int UDPDomain_recvfrom(int s, void *buf, int len, struct sockaddr *from, socklen_t *fromlen, struct sockaddr *dstip, socklen_t *dstlen, int *if_index)
{
    /** udpiv4 just calls udpbase. should we skip directly to there? */
    return UDPIPv4BaseDomain_recvfrom(s, buf, len, from, fromlen, dstip, dstlen, if_index);
}

int UDPDomain_sendto(int fd, struct in_addr *srcip, int if_index, struct sockaddr *remote,
                void *data, int len)
{
    /** udpiv4 just calls udpbase. should we skip directly to there? */
    return UDPIPv4BaseDomain_sendto(fd, srcip, if_index, remote, data, len);
}

/*
 * Open a UDP-based transport for SNMP.  Local is TRUE if addr is the local
 * address to bind to (i.e. this is a server-type session); otherwise addr is
 * the remote address to send things to.
 */

Transport_Transport * UDPDomain_transport(struct sockaddr_in *addr, int local)
{
    Transport_Transport *t = NULL;

    t = UDPIPv4BaseDomain_transport(addr, local);
    if (NULL == t) {
        return NULL;
    }

    /*
     * Set Domain
     */

    t->domain = transport_uDPDomain;
    t->domain_length = transport_uDPDomainLen;

    /*
     * 16-bit length field, 8 byte UDP header, 20 byte IPv4 header
     */

    t->msgMaxSize = 0xffff - 8 - 20;
    t->f_recv     = UDPBaseDomain_recv;
    t->f_send     = UDPBaseDomain_send;
    t->f_close    = SocketBaseDomain_close;
    t->f_accept   = NULL;
    t->f_fmtaddr  = UDPDomain_fmtaddr;

    return t;
}

/*
 * The following functions provide the "com2sec" configuration token
 * functionality for compatibility.
 */

#define EXAMPLE_NETWORK		"NETWORK"
#define EXAMPLE_COMMUNITY	"COMMUNITY"

typedef struct UDPDomain_Com2SecEntry_s {
    const char *secName;
    const char *contextName;
    struct UDPDomain_Com2SecEntry_s *next;
    in_addr_t   network;
    in_addr_t   mask;
    const char  community[1];
} UDPDomain_Com2SecEntry;

static UDPDomain_Com2SecEntry   *uDPDomain_com2SecList = NULL, *uDPDomain_com2SecListLast = NULL;

void UDPDomain_parseSecurity(const char *token, char *param)
{
    char            secName[VACM_VACMSTRINGLEN + 1];
    size_t          secNameLen;
    char            contextName[VACM_VACMSTRINGLEN + 1];
    size_t          contextNameLen;
    char            community[IMPL_COMMUNITY_MAX_LEN + 1];
    size_t          communityLen;
    char            source[270]; /* dns-name(253)+/(1)+mask(15)+\0(1) */
    struct in_addr  network, mask;

    /*
     * Get security, source address/netmask and community strings.
     */

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
        ReadConfig_configPerror("missing SOURCE parameter");
        return;
    }
    param = ReadConfig_copyNword( param, source, sizeof(source));
    if (source[0] == '\0') {
        ReadConfig_configPerror("empty SOURCE parameter");
        return;
    }
    if (strncmp(source, EXAMPLE_NETWORK, strlen(EXAMPLE_NETWORK)) == 0) {
        ReadConfig_configPerror("example config NETWORK not properly configured");
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

    /* Deal with the "default" case first. */
    if (strcmp(source, "default") == 0) {
        network.s_addr = 0;
        mask.s_addr = 0;
    } else {
        /* Split the source/netmask parts */
        char *strmask = strchr(source, '/');
        if (strmask != NULL)
            /* Mask given. */
            *strmask++ = '\0';

        /* Try interpreting as a dotted quad. */
        if (inet_pton(AF_INET, source, &network) == 0) {
            /* Nope, wasn't a dotted quad.  Must be a hostname. */
            int ret = System_gethostbynameV4(source, &network.s_addr);
            if (ret < 0) {
                ReadConfig_configPerror("cannot resolve source hostname");
                return;
            }
        }

        /* Now work out the mask. */
        if (strmask == NULL || *strmask == '\0') {
            /* No mask was given. Assume /32 */
            mask.s_addr = (in_addr_t)(~0UL);
        } else {
            /* Try to interpret mask as a "number of 1 bits". */
            char* cp;
            long maskLen = strtol(strmask, &cp, 10);
            if (*cp == '\0') {
                if (0 < maskLen && maskLen <= 32)
                    mask.s_addr = htonl((in_addr_t)(~0UL << (32 - maskLen)));
                else if (maskLen == 0)
                    mask.s_addr = 0;
                else {
                    ReadConfig_configPerror("bad mask length");
                    return;
                }
            }
            /* Try to interpret mask as a dotted quad. */
            else if (inet_pton(AF_INET, strmask, &mask) == 0) {
                ReadConfig_configPerror("bad mask");
                return;
            }

            /* Check that the network and mask are consistent. */
            if (network.s_addr & ~mask.s_addr) {
                ReadConfig_configPerror("source/mask mismatch");
                return;
            }
        }
    }

    {
        void* v = malloc(offsetof(UDPDomain_Com2SecEntry, community) + communityLen +
                         secNameLen + contextNameLen);

        UDPDomain_Com2SecEntry* e = (UDPDomain_Com2SecEntry*)v;
        char* last = ((char*)v) + offsetof(UDPDomain_Com2SecEntry, community);

        if (v == NULL) {
            ReadConfig_configPerror("memory error");
            return;
        }

        /*
         * Everything is okay.  Copy the parameters to the structure allocated
         * above and add it to END of the list.
         */

        {
          char buf1[INET_ADDRSTRLEN];
          char buf2[INET_ADDRSTRLEN];
          DEBUG_MSGTL(("netsnmp_udp_parse_security",
                      "<\"%s\", %s/%s> => \"%s\"\n", community,
                      inet_ntop(AF_INET, &network, buf1, sizeof(buf1)),
                      inet_ntop(AF_INET, &mask, buf2, sizeof(buf2)),
                      secName));
        }

        memcpy(last, community, communityLen);
        last += communityLen;
        memcpy(last, secName, secNameLen);
        e->secName = last;
        last += secNameLen;
        if (contextNameLen) {
            memcpy(last, contextName, contextNameLen);
            e->contextName = last;
        } else
            e->contextName = last - 1;
        e->network = network.s_addr;
        e->mask = mask.s_addr;
        e->next = NULL;

        if (uDPDomain_com2SecListLast != NULL) {
            uDPDomain_com2SecListLast->next = e;
            uDPDomain_com2SecListLast = e;
        } else {
            uDPDomain_com2SecListLast = uDPDomain_com2SecList = e;
        }
    }
}

void UDPDomain_com2SecListFree(void)
{
    UDPDomain_Com2SecEntry   *e = uDPDomain_com2SecList;
    while (e != NULL) {
        UDPDomain_Com2SecEntry   *tmp = e;
        e = e->next;
        free(tmp);
    }
    uDPDomain_com2SecList = uDPDomain_com2SecListLast = NULL;
}

void UDPDomain_agentConfigTokensRegister(void)
{
    ReadConfig_registerAppConfigHandler("com2sec", UDPDomain_parseSecurity,
                                UDPDomain_com2SecListFree,
                                "[-Cn CONTEXT] secName IPv4-network-address[/netmask] community");
}



/*
 * Return 0 if there are no com2sec entries, or return 1 if there ARE com2sec
 * entries.  On return, if a com2sec entry matched the passed parameters,
 * then *secName points at the appropriate security name, or is NULL if the
 * parameters did not match any com2sec entry.
 */

int UDPDomain_getSecName(void *opaque, int olength,
                       const char *community,
                       size_t community_len, const char **secName,
                       const char **contextName)
{
    const UDPDomain_Com2SecEntry *c;
    UDPDomain_UdpAddrPair *addr_pair = (UDPDomain_UdpAddrPair *) opaque;
    struct sockaddr_in *from = (struct sockaddr_in *) &(addr_pair->remote_addr);
    char           *ztcommunity = NULL;

    if (secName != NULL) {
        *secName = NULL;  /* Haven't found anything yet */
    }

    /*
     * Special case if there are NO entries (as opposed to no MATCHING
     * entries).
     */

    if (uDPDomain_com2SecList == NULL) {
        DEBUG_MSGTL(("netsnmp_udp_getSecName", "no com2sec entries\n"));
        return 0;
    }

    /*
     * If there is no IPv4 source address, then there can be no valid security
     * name.
     */

   DEBUG_MSGTL(("netsnmp_udp_getSecName", "opaque = %p (len = %d), sizeof = %d, family = %d (%d)\n",
   opaque, olength, (int)sizeof(UDPDomain_UdpAddrPair), from->sin_family, AF_INET));
    if (opaque == NULL || olength != sizeof(UDPDomain_UdpAddrPair) ||
        from->sin_family != AF_INET) {
        DEBUG_MSGTL(("netsnmp_udp_getSecName",
            "no IPv4 source address in PDU?\n"));
        return 1;
    }

    DEBUG_IF("netsnmp_udp_getSecName") {
    ztcommunity = (char *)malloc(community_len + 1);
    if (ztcommunity != NULL) {
        memcpy(ztcommunity, community, community_len);
        ztcommunity[community_len] = '\0';
    }

    DEBUG_MSGTL(("netsnmp_udp_getSecName", "resolve <\"%s\", 0x%08lx>\n",
            ztcommunity ? ztcommunity : "<malloc error>",
            (unsigned long)(from->sin_addr.s_addr)));
    }

    for (c = uDPDomain_com2SecList; c != NULL; c = c->next) {
        {
            char buf1[INET_ADDRSTRLEN];
            char buf2[INET_ADDRSTRLEN];
            DEBUG_MSGTL(("netsnmp_udp_getSecName","compare <\"%s\", %s/%s>",
                        c->community,
                        inet_ntop(AF_INET, &c->network, buf1, sizeof(buf1)),
                        inet_ntop(AF_INET, &c->mask, buf2, sizeof(buf2))));
        }
        if ((community_len == strlen(c->community)) &&
        (memcmp(community, c->community, community_len) == 0) &&
            ((from->sin_addr.s_addr & c->mask) == c->network)) {
            DEBUG_MSG(("netsnmp_udp_getSecName", "... SUCCESS\n"));
            if (secName != NULL) {
                *secName = c->secName;
                *contextName = c->contextName;
            }
            break;
        }
        DEBUG_MSG(("netsnmp_udp_getSecName", "... nope\n"));
    }
    if (ztcommunity != NULL) {
        free(ztcommunity);
    }
    return 1;
}

Transport_Transport *
UDPDomain_createTstring(const char *str, int local,
               const char *default_target)
{
    struct sockaddr_in addr;

    if (IPv4BaseDomain_sockaddrIn2(&addr, str, default_target)) {
        return UDPDomain_transport(&addr, local);
    } else {
        return NULL;
    }
}


Transport_Transport * UDPDomain_createOstring(const u_char * o, size_t o_len, int local)
{
    struct sockaddr_in addr;

    if (o_len == 6) {
        unsigned short porttmp = (o[4] << 8) + o[5];
        addr.sin_family = AF_INET;
        memcpy((u_char *) & (addr.sin_addr.s_addr), o, 4);
        addr.sin_port = htons(porttmp);
        return UDPDomain_transport(&addr, local);
    }
    return NULL;
}


void UDPDomain_ctor(void)
{
    uDPDomain_uDPDomain.name = transport_uDPDomain;
    uDPDomain_uDPDomain.name_length = transport_uDPDomainLen;
    uDPDomain_uDPDomain.prefix = (const char**)calloc(2, sizeof(char *));
    uDPDomain_uDPDomain.prefix[0] = "udp";

    uDPDomain_uDPDomain.f_create_from_tstring     = NULL;
    uDPDomain_uDPDomain.f_create_from_tstring_new = UDPDomain_createTstring;
    uDPDomain_uDPDomain.f_create_from_ostring     = UDPDomain_createOstring;

    Transport_tdomainRegister(&uDPDomain_uDPDomain);
}
