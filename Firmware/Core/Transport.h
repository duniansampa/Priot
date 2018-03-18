#ifndef TRANSPORT_H
#define TRANSPORT_H

#include "Generals.h"
#include "Types.h"
#include <netinet/in.h>



/*  Some transport-type constants.  */

#define	TRANSPORT_STREAM_QUEUE_LEN	5

/*  Some transport-type flags.  */

#define		TRANSPORT_FLAG_STREAM	 0x01
#define		TRANSPORT_FLAG_LISTEN	 0x02
#define		TRANSPORT_FLAG_TUNNELED	 0x04
#define     TRANSPORT_FLAG_TMSTATE   0x08  /* indicates opaque is a
                                                          TSM tmStateReference */
#define		TRANSPORT_FLAG_EMPTY_PKT 0x10
#define		TRANSPORT_FLAG_OPENED	 0x20  /* f_open called */
#define		TRANSPORT_FLAG_HOSTNAME	 0x80  /* for fmtaddr hook */

/*
 * The standard SNMP domains.
 */

extern oid             transport_uDPDomain[];   //  { 1, 3, 6, 1, 6, 1, 1 };
extern size_t           transport_uDPDomainLen;  //  ASN01_OID_LENGTH(transport_uDPDomain);
extern oid             transport_cLNSDomain[];  //  { 1, 3, 6, 1, 6, 1, 2 };
extern size_t           transport_cLNSDomainLen; //  ASN01_OID_LENGTH(transport_cLNSDomain);
extern oid             transport_cONSDomain[];  //  { 1, 3, 6, 1, 6, 1, 3 };
extern size_t           transport_cONSDomainLen; //  ASN01_OID_LENGTH(transport_cONSDomain);
extern oid             transport_dDPDomain[];   //  { 1, 3, 6, 1, 6, 1, 4 };
extern size_t           transport_dDPDomainLen;  //  ASN01_OID_LENGTH(transport_dDPDomain);
extern oid             transport_iPXDomain[];   //  { 1, 3, 6, 1, 6, 1, 5 };
extern size_t           transport_iPXDomainLen;  //  ASN01_OID_LENGTH(transport_iPXDomain);


/* Structure which stores transport security model specific parameters */
/* isms-secshell-11 section 4.1 */

/* contents documented in draft-ietf-isms-transport-security-model
   Section 3.2 */
/* note: VACM only allows <= 32 so this is overkill till another ACM comes */
#define TRANSPORT_TM_MAX_SECNAME 256

typedef union Transport_SockaddrStorage_s {
    struct sockaddr     sa;
    struct sockaddr_in  sin;
} Transport_SockaddrStorage;

typedef struct Transport_AddrPair_s {
   Transport_SockaddrStorage remote_addr;
   Transport_SockaddrStorage local_addr;
} priot_addr_pair;

typedef struct Transport_IndexedAddrPair_s {
   Transport_SockaddrStorage remote_addr;
   Transport_SockaddrStorage local_addr;
   int if_index;
} Transport_IndexedAddrPair;

typedef struct Transport_TmStateReference_s {
   oid    transportDomain[TYPES_MAX_OID_LEN];
   size_t transportDomainLen;
   char   securityName[TRANSPORT_TM_MAX_SECNAME];
   size_t securityNameLen;
   int    requestedSecurityLevel;
   int    transportSecurityLevel;
   char   sameSecurity;
   char   sessionID[8];

   char   have_addresses;
   Transport_IndexedAddrPair addresses;
   void *otherTransportOpaque; /* XXX: May have mem leak issues */
} Transport_TmStateReference;

/*  Structure which defines the transport-independent API.  */


typedef struct Transport_Transport_s {
    /*  The transport domain object identifier.  */

    const oid      *domain;
    int             domain_length;  /*  In sub-IDs, not octets.  */

    /*  Local transport address (in relevant SNMP-style encoding).  */

    unsigned char  *local;
    int             local_length;   /*  In octets.  */

    /*  Remote transport address (in relevant SNMP-style encoding).  */

    unsigned char  *remote;
    int             remote_length;  /*  In octets.  */

    /*  The actual socket.  */

    int             sock;

    /*  Flags (see #definitions above).  */

    unsigned int    flags;

    /*  Protocol-specific opaque data pointer.  */

    void           *data;
    int             data_length;

    /*  Maximum size of PDU that can be sent/received by this transport.  */

    size_t          msgMaxSize;

    /* tunneled transports */
    struct Transport_Transport_s * base_transport;

    /*  Callbacks.  Arguments are:
     *
     *              "this" pointer, fd, buf, size, *opaque, *opaque_length
     */

    int             (*f_recv)   (struct Transport_Transport_s *, void *,
                 int, void **, int *);
    int             (*f_send)   (struct Transport_Transport_s *, void *,
                 int, void **, int *);
    int             (*f_close)  (struct Transport_Transport_s *);

    /* Optional: opening can occur during creation if more appropriate */
   struct Transport_Transport_s * (*f_open)   (struct Transport_Transport_s *);

    /*  This callback is only necessary for stream-oriented transports.  */

    int    (*f_accept) (struct Transport_Transport_s *);

    /*  Optional callback to format a transport address.  */

    char   *(*f_fmtaddr)(struct Transport_Transport_s *, void *, int);

    /*  Optional callback to support extra configuration token/value pairs */
    /*  return non-zero on error */
    int    (*f_config)(struct Transport_Transport_s *, const char *,
                               const char *);

    /*  Optional callback that is called after the first transport is
        cloned to the second */
    int   (*f_copy)(struct Transport_Transport_s *,
                             struct Transport_Transport_s *);

    /*  Setup initial session config if special things are needed */
   int     (*f_setup_session)(struct Transport_Transport_s *,
                                      struct Types_Session_s *);

    /* allocated host name identifier; used by configuration system
       to load localhost.conf for host-specific configuration */
    u_char   *identifier; /* udp:localhost:161 -> "localhost" */
} Transport_Transport;

typedef struct Transport_TransportList_s {
    Transport_Transport *transport;
    struct Transport_TransportList_s *next;
} Transport_TransportList;

typedef struct Transport_Tdomain_s {
    const oid      *name;
    size_t          name_length;
    const char    **prefix;

    /*
     * The f_create_from_tstring field is deprecated, please do not use it
     * for new code and try to migrate old code away from using it.
     */
    Transport_Transport *(*f_create_from_tstring) (const char *, int);

    Transport_Transport *(*f_create_from_ostring) (const u_char *, size_t, int);

    struct Transport_Tdomain_s *next;

     Transport_Transport *(*f_create_from_tstring_new) (const char *, int,
                             const char*);

} Transport_Tdomain;

typedef struct Transport_Config_s {
   char *key;
   char *value;
} Transport_Config;




void Transport_initTransport(void);

/*  Some utility functions.  */

char * Transport_peerString(Transport_Transport *t, void *data, int len);

int Transport_send(Transport_Transport *t, void *data, int len,
                           void **opaque, int *olength);
int Transport_recv(Transport_Transport *t, void *data, int len,
                           void **opaque, int *olength);

int Transport_addToList(Transport_TransportList **transport_list,
                  Transport_Transport *transport);

int Transport_removeFromList(Transport_TransportList **transport_list,
                       Transport_Transport *transport);

int Transport_sockaddrSize(struct sockaddr *sa);


/*
 * Return an exact (deep) copy of t, or NULL if there is a memory allocation
 * problem (for instance).
 */

Transport_Transport * Transport_copy(Transport_Transport *t);


/*  Free an Transport_Transport.  */


void Transport_free(Transport_Transport *t);


/*
 * If the passed oid (in_oid, in_len) corresponds to a supported transport
 * domain, return 1; if not return 0.  If out_oid is not NULL and out_len is
 * not NULL, then the "internal" oid which should be used to identify this
 * domain (e.g. in pdu->tDomain etc.) is written to *out_oid and its length to
 * *out_len.
 */


int Transport_tdomainSupport(const oid *in_oid, size_t in_len,
                    const oid **out_oid, size_t *out_len);

int Transport_tdomainRegister(Transport_Tdomain *domain);

int Transport_tdomainUnregister(Transport_Tdomain *domain);


void Transport_clearTdomainList(void);

void Transport_tdomainInit(void);


Transport_Transport * Transport_tdomainTransport(const char *str,
                         int local,
                         const char *default_domain);


Transport_Transport * Transport_tdomainTransportFull(const char *application,
                          const char *str,
                          int local,
                          const char *default_domain,
                          const char *default_target);


Transport_Transport *
Transport_tdomainTransportOid( const oid *   dom,
                               size_t         dom_len,
                               const u_char * o,
                               size_t         o_len,
                               int            local );


Transport_Transport*
Transport_openClient(const char* application, const char* str);


Transport_Transport*
Transport_openServer(const char* application, const char* str);

Transport_Transport*
Transport_open(const char* application, const char* str, int local);


int Transport_configCompare(Transport_Config *left,
                                     Transport_Config *right);

Transport_Config * Transport_createConfig(char *key, char *value);

#endif // TRANSPORT_H
