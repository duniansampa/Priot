#include "Transport.h"
#include "System/Util/Utilities.h"
#include "Asn01.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "ReadConfig.h"
#include "Service.h"
#include "System/String.h"
#include "Api.h"
#include "Priot.h"
#include "System/Util/DefaultStore.h"

#include <ctype.h>

/*
 * Our list of supported transport domains.
 */

static Transport_Tdomain * _transport_domainList = NULL;

extern void UDPDomain_ctor();
extern void TPCDomain_ctor();
extern void AliasDomain_ctor();
extern void UnixDomain_ctor();


/*
 * The standard SNMP domains.
 */

oid             transport_uDPDomain[]   = { 1, 3, 6, 1, 6, 1, 1 };
size_t           transport_uDPDomainLen = ASN01_OID_LENGTH(transport_uDPDomain);
oid             transport_cLNSDomain[]  = { 1, 3, 6, 1, 6, 1, 2 };
size_t           transport_cLNSDomainLen = ASN01_OID_LENGTH(transport_cLNSDomain);
oid             transport_cONSDomain[] = { 1, 3, 6, 1, 6, 1, 3 };
size_t           transport_cONSDomainLen = ASN01_OID_LENGTH(transport_cONSDomain);
oid             transport_dDPDomain[] = { 1, 3, 6, 1, 6, 1, 4 };
size_t           transport_dDPDomainLen = ASN01_OID_LENGTH(transport_dDPDomain);
oid             transport_iPXDomain[] = { 1, 3, 6, 1, 6, 1, 5 };
size_t           transport_iPXDomainLen = ASN01_OID_LENGTH(transport_iPXDomain);



static void     _Transport_tdomainDump(void);



void Transport_initTransport(void)
{
    DefaultStore_registerConfig(ASN01_BOOLEAN,
                               "priot", "dontLoadHostConfig",
                               DsStore_LIBRARY_ID,
                               DsBool_DONT_LOAD_HOST_FILES);
}

/*
 * Make a deep copy of an Transport_Transport.
 */
Transport_Transport *
Transport_copy(Transport_Transport *t)
{
    Transport_Transport *n = NULL;

    if (t == NULL) {
        return NULL;
    }

    n = MEMORY_MALLOC_TYPEDEF(Transport_Transport);
    if (n == NULL) {
        return NULL;
    }

    if (t->domain != NULL) {
        n->domain = t->domain;
        n->domain_length = t->domain_length;
    } else {
        n->domain = NULL;
        n->domain_length = 0;
    }

    if (t->local != NULL) {
        n->local = (u_char *) malloc(t->local_length);
        if (n->local == NULL) {
            Transport_free(n);
            return NULL;
        }
        n->local_length = t->local_length;
        memcpy(n->local, t->local, t->local_length);
    } else {
        n->local = NULL;
        n->local_length = 0;
    }

    if (t->remote != NULL) {
        n->remote = (u_char *) malloc(t->remote_length);
        if (n->remote == NULL) {
            Transport_free(n);
            return NULL;
        }
        n->remote_length = t->remote_length;
        memcpy(n->remote, t->remote, t->remote_length);
    } else {
        n->remote = NULL;
        n->remote_length = 0;
    }

    if (t->data != NULL && t->data_length > 0) {
        n->data = malloc(t->data_length);
        if (n->data == NULL) {
            Transport_free(n);
            return NULL;
        }
        n->data_length = t->data_length;
        memcpy(n->data, t->data, t->data_length);
    } else {
        n->data = NULL;
        n->data_length = 0;
    }

    n->msgMaxSize = t->msgMaxSize;
    n->f_accept = t->f_accept;
    n->f_recv = t->f_recv;
    n->f_send = t->f_send;
    n->f_close = t->f_close;
    n->f_copy = t->f_copy;
    n->f_config = t->f_config;
    n->f_fmtaddr = t->f_fmtaddr;
    n->sock = t->sock;
    n->flags = t->flags;
    n->base_transport = Transport_copy(t->base_transport);

    /* give the transport a chance to do "special things" */
    if (t->f_copy)
        t->f_copy(t, n);

    return n;
}



void Transport_free(Transport_Transport *t)
{
    if (NULL == t)
        return;

    MEMORY_FREE(t->local);
    MEMORY_FREE(t->remote);
    MEMORY_FREE(t->data);
    Transport_free(t->base_transport);

    MEMORY_FREE(t);
}

/*
 * Transport_peerString
 *
 * returns string representation of peer address.
 *
 * caller is responsible for freeing the allocated string.
 */
char * Transport_peerString(Transport_Transport *t, void *data, int len)
{
    char           *str;

    if (NULL == t)
        return NULL;

    if (t->f_fmtaddr != NULL)
        str = t->f_fmtaddr(t, data, len);
    else
        str = strdup("<UNKNOWN>");

    return str;
}

int
Transport_sockaddrSize(struct sockaddr *sa)
{
    if (NULL == sa)
        return 0;

    switch (sa->sa_family) {
        case AF_INET:
            return sizeof(struct sockaddr_in);
        break;
    }

    return 0;
}

int
Transport_send(Transport_Transport *t, void *packet, int length,
                       void **opaque, int *olength)
{
    int dumpPacket, debugLength;

    if ((NULL == t) || (NULL == t->f_send)) {
        DEBUG_MSGTL(("transport:pkt:send", "NULL transport or send function\n"));
        return ErrorCode_GENERR;
    }

    dumpPacket = DefaultStore_getBoolean(DsStore_LIBRARY_ID,
                                        DsBool_DUMP_PACKET);
    debugLength = (ErrorCode_SUCCESS ==
                   Debug_isTokenRegistered("transport:send"));

    if (dumpPacket | debugLength) {
        char *str = Transport_peerString(t,
                                                  opaque ? *opaque : NULL,
                                                  olength ? *olength : 0);
        if (debugLength)
            DEBUG_MSGT_NC(("transport:send","%lu bytes to %s\n",
                          (unsigned long)length, str));
        if (dumpPacket)
            Logger_log(LOGGER_PRIORITY_DEBUG, "\nSending %lu bytes to %s\n",
                     (unsigned long)length, str);
        MEMORY_FREE(str);
    }
    if (dumpPacket)
        Priot_xdump(packet, length, "");

    return t->f_send(t, packet, length, opaque, olength);
}

int
Transport_recv(Transport_Transport *t, void *packet, int length,
                       void **opaque, int *olength)
{
    int debugLength;

    if ((NULL == t) || (NULL == t->f_recv)) {
        DEBUG_MSGTL(("transport:recv", "NULL transport or recv function\n"));
        return ErrorCode_GENERR;
    }

    length = t->f_recv(t, packet, length, opaque, olength);

    if (length <=0)
        return length; /* don't log timeouts/socket closed */

    debugLength = (ErrorCode_SUCCESS ==
                   Debug_isTokenRegistered("transport:recv"));

    if (debugLength) {
        char *str = Transport_peerString(t,
                                                  opaque ? *opaque : NULL,
                                                  olength ? *olength : 0);
        if (debugLength)
            DEBUG_MSGT_NC(("transport:recv","%d bytes from %s\n",
                          length, str));
        MEMORY_FREE(str);
    }

    return length;
}



int
Transport_tdomainSupport(const oid * in_oid,
                        size_t in_len,
                        const oid ** out_oid, size_t * out_len)
{
    Transport_Tdomain *d = NULL;

    for (d = _transport_domainList; d != NULL; d = d->next) {
        if (Api_oidEquals(in_oid, in_len, d->name, d->name_length) == 0) {
            if (out_oid != NULL && out_len != NULL) {
                *out_oid = d->name;
                *out_len = d->name_length;
            }
            return 1;
        }
    }
    return 0;
}


void
Transport_tdomainInit(void)
{
    DEBUG_MSGTL(("tdomain", "Transport_tdomainInit() called\n"));

/* include the configure generated list of constructor calls */
    /* This file is automatically generated by configure.  Do not modify by hand. */
    UDPDomain_ctor();
    TPCDomain_ctor();
    AliasDomain_ctor();
    UnixDomain_ctor();

    _Transport_tdomainDump();
}

void
Transport_clearTdomainList(void)
{
    Transport_Tdomain *list = _transport_domainList, *next = NULL;
    DEBUG_MSGTL(("tdomain", "clear_tdomain_list() called\n"));

    while (list != NULL) {
    next = list->next;
    MEMORY_FREE(list->prefix);
        /* attention!! list itself is not in the heap, so we must not free it! */
    list = next;
    }
    _transport_domainList = NULL;
}


static void
_Transport_tdomainDump(void)
{
    Transport_Tdomain *d;
    int i = 0;

    DEBUG_MSGTL(("tdomain", "transport_domainList -> "));
    for (d = _transport_domainList; d != NULL; d = d->next) {
        DEBUG_MSG(("tdomain", "{ "));
        DEBUG_MSGOID(("tdomain", d->name, d->name_length));
        DEBUG_MSG(("tdomain", ", \""));
        for (i = 0; d->prefix[i] != NULL; i++) {
            DEBUG_MSG(("tdomain", "%s%s", d->prefix[i],
              (d->prefix[i + 1]) ? "/" : ""));
        }
        DEBUG_MSG(("tdomain", "\" } -> "));
    }
    DEBUG_MSG(("tdomain", "[NIL]\n"));
}



int
Transport_tdomainRegister(Transport_Tdomain *n)
{
    Transport_Tdomain **prevNext = &_transport_domainList, *d;

    if (n != NULL) {
        for (d = _transport_domainList; d != NULL; d = d->next) {
            if (Api_oidEquals(n->name, n->name_length,
                                d->name, d->name_length) == 0) {
                /*
                 * Already registered.
                 */
                return 0;
            }
            prevNext = &(d->next);
        }
        n->next = NULL;
        *prevNext = n;
        return 1;
    } else {
        return 0;
    }
}



int
Transport_tdomainUnregister(Transport_Tdomain *n)
{
    Transport_Tdomain **prevNext = &_transport_domainList, *d;

    if (n != NULL) {
        for (d = _transport_domainList; d != NULL; d = d->next) {
            if (Api_oidEquals(n->name, n->name_length,
                                d->name, d->name_length) == 0) {
                *prevNext = n->next;
        MEMORY_FREE(n->prefix);
                return 1;
            }
            prevNext = &(d->next);
        }
        return 0;
    } else {
        return 0;
    }
}


static Transport_Tdomain *
_Transport_findTdomain(const char* spec)
{
    Transport_Tdomain *d;
    for (d = _transport_domainList; d != NULL; d = d->next) {
        int i;
        for (i = 0; d->prefix[i] != NULL; i++)
            if (strcasecmp(d->prefix[i], spec) == 0) {
                DEBUG_MSGTL(("tdomain",
                            "Found domain \"%s\" from specifier \"%s\"\n",
                            d->prefix[0], spec));
                return d;
            }
    }
    DEBUG_MSGTL(("tdomain", "Found no domain from specifier \"%s\"\n", spec));
    return NULL;
}

static int
_Transport_isFqdn(const char *thename)
{
    if (!thename)
        return 0;
    while(*thename) {
        if (*thename != '.' && !isupper((unsigned char)*thename) &&
            !islower((unsigned char)*thename) &&
            !isdigit((unsigned char)*thename) && *thename != '-') {
            return 0;
        }
        thename++;
    }
    return 1;
}

/*
 * Locate the appropriate transport domain and call the create function for
 * it.
 */
Transport_Transport *
Transport_tdomainTransportFull(const char *application,
                               const char *str, int local,
                               const char *default_domain,
                               const char *default_target)
{
    Transport_Tdomain    *match = NULL;
    const char         *addr = NULL;
    const char * const *spec = NULL;
    int                 any_found = 0;
    char buf[UTILITIES_MAX_PATH];
    char **lspec = 0;

    DEBUG_MSGTL(("tdomain",
                "Transport_tdomainTransportFull(\"%s\", \"%s\", %d, \"%s\", \"%s\")\n",
                application, str ? str : "[NIL]", local,
                default_domain ? default_domain : "[NIL]",
                default_target ? default_target : "[NIL]"));

    /* see if we can load a host-name specific set of conf files */
    if (!DefaultStore_getBoolean(DsStore_LIBRARY_ID,
                                DsBool_DONT_LOAD_HOST_FILES) &&
        _Transport_isFqdn(str)) {
        static int have_added_handler = 0;
        char *newhost;
        struct ReadConfig_ConfigLine_s *config_handlers;
        struct ReadConfig_ConfigFiles_s file_names;
        char *prev_hostname;

        /* register a "transport" specifier */
        if (!have_added_handler) {
            have_added_handler = 1;
            DefaultStore_registerConfig(ASN01_OCTET_STR,
                                       "priot", "transport",
                                       DsStore_LIBRARY_ID,
                                       DsStr_HOSTNAME);
        }

        /* we save on specific setting that we don't allow to change
           from one transport creation to the next; ie, we don't want
           the "transport" specifier to be a default.  It should be a
           single invocation use only */
        prev_hostname = DefaultStore_getString(DsStore_LIBRARY_ID,
                                              DsStr_HOSTNAME);
        if (prev_hostname)
            prev_hostname = strdup(prev_hostname);

        /* read in the hosts/STRING.conf files */
        config_handlers = ReadConfig_getHandlers("priot");
        snprintf(buf, sizeof(buf)-1, "hosts/%s", str);
        file_names.fileHeader = buf;
        file_names.start = config_handlers;
        file_names.next = NULL;
        DEBUG_MSGTL(("tdomain", "checking for host specific config %s\n",
                    buf));
        ReadConfig_filesOfType(EITHER_CONFIG, &file_names);

        if (NULL !=
            (newhost = DefaultStore_getString(DsStore_LIBRARY_ID,
                                             DsStr_HOSTNAME))) {
            String_copyTruncate(buf, newhost, sizeof(buf));
            str = buf;
        }

        DefaultStore_setString(DsStore_LIBRARY_ID,
                              DsStr_HOSTNAME,
                              prev_hostname);
        MEMORY_FREE(prev_hostname);
    }

    /* First try - assume that there is a domain in str (domain:target) */

    if (str != NULL) {
        const char *cp;
        if ((cp = strchr(str, ':')) != NULL) {
            char* mystring = (char*)malloc(cp + 1 - str);
            memcpy(mystring, str, cp - str);
            mystring[cp - str] = '\0';
            addr = cp + 1;

            match = _Transport_findTdomain(mystring);
            free(mystring);
        }
    }

    /*
     * Second try, if there is no domain in str (target), then try the
     * default domain
     */

    if (match == NULL) {
        addr = str;
        if (addr && *addr == '/') {
            DEBUG_MSGTL(("tdomain",
                        "Address starts with '/', so assume \"unix\" "
                        "domain\n"));
            match = _Transport_findTdomain("unix");
        } else if (default_domain) {
            DEBUG_MSGTL(("tdomain",
                        "Use user specified default domain \"%s\"\n",
                        default_domain));
            if (!strchr(default_domain, ','))
                match = _Transport_findTdomain(default_domain);
            else {
                int commas = 0;
                const char *cp = default_domain;
                char *dup = strdup(default_domain);

                while (*++cp) if (*cp == ',') commas++;
                lspec = (char **) calloc(commas+2, sizeof(char *));
                commas = 1;
                lspec[0] = strtok(dup, ",");
                while ((lspec[commas++] = strtok(NULL, ",")))
                    ;
                spec = (const char * const *)lspec;
            }
        } else {
            spec = Service_lookupDefaultDomains(application);
            if (spec == NULL) {
                DEBUG_MSGTL(("tdomain",
                            "No default domain found, assume \"udp\"\n"));
                match = _Transport_findTdomain("udp");
            } else {
                const char * const * r = spec;
                DEBUG_MSGTL(("tdomain",
                            "Use application default domains"));
                while(*r) {
                    DEBUG_MSG(("tdomain", " \"%s\"", *r));
                    ++r;
                }
                DEBUG_MSG(("tdomain", "\n"));
            }
        }
    }

    for(;;) {
        if (match) {
            Transport_Transport *t = NULL;
            const char* addr2;

            any_found = 1;
            /*
             * Ok, we know what domain to try, lets see what default data
             * should be used with it
             */
            if (default_target != NULL)
                addr2 = default_target;
            else
                addr2 = Service_lookupDefaultTarget(application,
                                                      match->prefix[0]);
            DEBUG_MSGTL(("tdomain",
                        "trying domain \"%s\" address \"%s\" "
                        "default address \"%s\"\n",
                        match->prefix[0], addr ? addr : "[NIL]",
                        addr2 ? addr2 : "[NIL]"));
            if (match->f_create_from_tstring) {
                LOGGER_LOGONCE((LOGGER_PRIORITY_WARNING,
                                 "transport domain %s uses deprecated f_create_from_tstring\n",
                                 match->prefix[0]));
                t = match->f_create_from_tstring(addr, local);
            }
            else
                t = match->f_create_from_tstring_new(addr, local, addr2);
            if (t) {
                if (lspec) {
                    free(lspec[0]);
                    free(lspec);
                }
                return t;
            }
        }
        addr = str;
        if (spec && *spec)
            match = _Transport_findTdomain(*spec++);
        else
            break;
    }
    if (!any_found)
        Logger_log(LOGGER_PRIORITY_ERR, "No support for any checked transport domain\n");
    if (lspec) {
        free(lspec[0]);
        free(lspec);
    }
    return NULL;
}


Transport_Transport *
Transport_tdomainTransport(const char *str, int local,
              const char *default_domain)
{
    return Transport_tdomainTransportFull("priot", str, local, default_domain,
                      NULL);
}


Transport_Transport *
Transport_tdomainTransportOid(const oid * dom,
                              size_t dom_len,
                              const u_char * o, size_t o_len, int local)
{
    Transport_Tdomain *d;
    int             i;

    DEBUG_MSGTL(("tdomain", "domain \""));
    DEBUG_MSGOID(("tdomain", dom, dom_len));
    DEBUG_MSG(("tdomain", "\"\n"));

    for (d = _transport_domainList; d != NULL; d = d->next) {
        for (i = 0; d->prefix[i] != NULL; i++) {
            if (Api_oidEquals(dom, dom_len, d->name, d->name_length) ==
                0) {
                return d->f_create_from_ostring(o, o_len, local);
            }
        }
    }

    Logger_log(LOGGER_PRIORITY_ERR, "No support for requested transport domain\n");
    return NULL;
}

Transport_Transport*
Transport_open(const char* application, const char* str, int local)
{
    return Transport_tdomainTransportFull(application, str, local, NULL, NULL);
}

Transport_Transport*
Transport_openServer(const char* application, const char* str)
{
    return Transport_tdomainTransportFull(application, str, 1, NULL, NULL);
}

Transport_Transport*
Transport_openClient(const char* application, const char* str)
{
    return Transport_tdomainTransportFull(application, str, 0, NULL, NULL);
}

/** adds a transport to a linked list of transports.
    Returns 1 on failure, 0 on success */
int Transport_addToList(Transport_TransportList **transport_list,
                              Transport_Transport *transport)
{
    Transport_TransportList *newptr =
        MEMORY_MALLOC_TYPEDEF(Transport_TransportList);

    if (!newptr)
        return 1;

    newptr->next = *transport_list;
    newptr->transport = transport;

    *transport_list = newptr;

    return 0;
}


/**  removes a transport from a linked list of transports.
     Returns 1 on failure, 0 on success */
int
Transport_removeFromList(Transport_TransportList **transport_list,
                                   Transport_Transport *transport)
{
    Transport_TransportList *ptr = *transport_list, *lastptr = NULL;

    while (ptr && ptr->transport != transport) {
        lastptr = ptr;
        ptr = ptr->next;
    }

    if (!ptr)
        return 1;

    if (lastptr)
        lastptr->next = ptr->next;
    else
        *transport_list = ptr->next;

    MEMORY_FREE(ptr);

    return 0;
}

int Transport_configCompare(Transport_Config *left,
                                 Transport_Config *right) {
    return strcmp(left->key, right->key);
}

Transport_Config *
Transport_createConfig(char *key, char *value) {
    Transport_Config *entry =
        MEMORY_MALLOC_TYPEDEF(Transport_Config);
    entry->key = strdup(key);
    entry->value = strdup(value);
    return entry;
}
