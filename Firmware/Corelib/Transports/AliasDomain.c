#include "AliasDomain.h"
#include "Impl.h"
#include "DataList.h"
#include "ReadConfig.h"
#include "Logger.h"

oid aliasDomain_priotALIASDomain[] = { 1,3,6,1,4,1,8072,3,3,7 };
static Transport_Tdomain _aliasDomain_aliasDomain;

/* simple storage mechanism */
static struct DataList_DataList_s *_aliasDomain_aliasMemory = NULL;

#define free_wrapper free

/* An alias parser */
void AliasDomain_parseAliasConfig(const char *token, char *line) {
    char aliasname[IMPL_SPRINT_MAX_LEN];
    char transportdef[IMPL_SPRINT_MAX_LEN];
    /* copy the first word (the alias) out and then assume the rest is
       transport */
    line = ReadConfig_copyNword(line, aliasname, sizeof(aliasname));
    line = ReadConfig_copyNword(line, transportdef, sizeof(transportdef));
    if (line)
        ReadConfig_configPerror("more data than expected");
    DataList_addNode(&_aliasDomain_aliasMemory,
                               DataList_create(aliasname,
                                                        strdup(transportdef),
                                                        &free_wrapper));
}

void AliasDomain_freeAliasConfig(void) {
    DataList_freeAll(_aliasDomain_aliasMemory);
    _aliasDomain_aliasMemory = NULL;
}

/*
 * Open a ALIAS-based transport for SNMP.  Local is TRUE if addr is the local
 * address to bind to (i.e. this is a server-type session); otherwise addr is
 * the remote address to send things to.
 */

Transport_Transport *
AliasDomain_createTstring(const char *str, int local,
               const char *default_target)
{
    const char *aliasdata;

    aliasdata = (const char*)DataList_get(_aliasDomain_aliasMemory, str);
    if (!aliasdata) {
        Logger_log(LOGGER_PRIORITY_ERR, "No alias found for %s\n", str);
        return NULL;
    }

    return Transport_tdomainTransport(aliasdata,local,default_target);
}



Transport_Transport *
AliasDomain_createOstring(const u_char * o, size_t o_len, int local)
{
    fprintf(stderr, "make ostring\n");
    return NULL;
}

void
AliasDomain_ctor(void)
{
    _aliasDomain_aliasDomain.name = aliasDomain_priotALIASDomain;
    _aliasDomain_aliasDomain.name_length = sizeof(aliasDomain_priotALIASDomain) / sizeof(oid);
    _aliasDomain_aliasDomain.prefix = (const char **)calloc(2, sizeof(char *));
    _aliasDomain_aliasDomain.prefix[0] = "alias";

    _aliasDomain_aliasDomain.f_create_from_tstring     = NULL;
    _aliasDomain_aliasDomain.f_create_from_tstring_new = AliasDomain_createTstring;
    _aliasDomain_aliasDomain.f_create_from_ostring     = AliasDomain_createOstring;

    Transport_tdomainRegister(&_aliasDomain_aliasDomain);

    ReadConfig_registerConfigHandler("snmp", "alias", AliasDomain_parseAliasConfig,
                            AliasDomain_freeAliasConfig, "NAME TRANSPORT_DEFINITION");
}
