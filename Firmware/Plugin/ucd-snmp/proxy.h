#ifndef UCD_SNMP_PROXY_H
#define UCD_SNMP_PROXY_H

#include "AgentHandler.h"

struct simple_proxy {
    struct variable2 *variables;
    oid             name[asnMAX_OID_LEN];
    size_t          name_len;
    oid             base[asnMAX_OID_LEN];
    size_t          base_len;
    char           *context;
    Types_Session *sess;
    struct simple_proxy *next;
};

int             proxy_got_response(int, Types_Session *, int,
                                   Types_Pdu *, void *);
void            proxy_parse_config(const char *, char *);
void            init_proxy(void);
void            shutdown_proxy(void);
NodeHandlerFT proxy_handler;

#endif                          /* UCD_SNMP_PROXY_H */
