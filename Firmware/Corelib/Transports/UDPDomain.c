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

static Transport_Tdomain _uDPDomain_uDPDomain;

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

                        
void UDPDomain_agentConfigTokensRegister(void)
{

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
    _uDPDomain_uDPDomain.name = transport_uDPDomain;
    _uDPDomain_uDPDomain.name_length = transport_uDPDomainLen;
    _uDPDomain_uDPDomain.prefix = (const char**)calloc(2, sizeof(char *));
    _uDPDomain_uDPDomain.prefix[0] = "udp";

    _uDPDomain_uDPDomain.f_create_from_tstring     = NULL;
    _uDPDomain_uDPDomain.f_create_from_tstring_new = UDPDomain_createTstring;
    _uDPDomain_uDPDomain.f_create_from_ostring     = UDPDomain_createOstring;

    Transport_tdomainRegister(&_uDPDomain_uDPDomain);
}
