#include "VacmConf.h"
#include "PriotSettings.h"
#include "Debug.h"
#include "AgentReadConfig.h"
#include "Vacm.h"
#include "AgentCallbacks.h"
#include "ReadConfig.h"
#include "Priot.h"
#include "Enum.h"
#include "Strlcpy.h"
#include "Mib.h"
#include "Impl.h"
#include "Transports/UDPDomain.h"
#include "Transports/UnixDomain.h"
#include "Transports/TCPDomain.h"
#include "DefaultStore.h"
#include "DsAgent.h"
#include "AgentRegistry.h"
#include "Logger.h"
#include "Secmod.h"


/**
 * Registers the VACM token handlers for inserting rows into the vacm tables.
 * These tokens will be recognised by both 'snmpd' and 'snmptrapd'.
 */
void
VacmConf_initVacmConfigTokens(void) {
    AgentReadConfig_priotdRegisterConfigHandler("group", VacmConf_parseGroup,
                                  VacmConf_freeGroup,
                                  "name v1|v2c|usm|... security");
    AgentReadConfig_priotdRegisterConfigHandler("access", VacmConf_parseAccess,
                                  VacmConf_freeAccess,
                                  "name context model level prefix read write notify");
    AgentReadConfig_priotdRegisterConfigHandler("setaccess", VacmConf_parseSetaccess,
                                  VacmConf_freeAccess,
                                  "name context model level prefix viewname viewval");
    AgentReadConfig_priotdRegisterConfigHandler("view", VacmConf_parseView, VacmConf_freeView,
                                  "name type subtree [mask]");
    AgentReadConfig_priotdRegisterConstConfigHandler("vacmView",
                                        Vacm_parseConfigView, NULL, NULL);
    AgentReadConfig_priotdRegisterConstConfigHandler("vacmGroup",
                                        Vacm_parseConfigGroup,
                                        NULL, NULL);
    AgentReadConfig_priotdRegisterConstConfigHandler("vacmAccess",
                                        Vacm_parseConfigAccess,
                                        NULL, NULL);
    AgentReadConfig_priotdRegisterConstConfigHandler("vacmAuthAccess",
                                        Vacm_parseConfigAuthAccess,
                                        NULL, NULL);

    /* easy community auth handler */
    AgentReadConfig_priotdRegisterConfigHandler("authcommunity",
                                  VacmConf_parseAuthcommunity,
                                  NULL, "authtype1,authtype2 community [default|hostname|network/bits [oid|-V view [context]]]");

    /* easy user auth handler */
    AgentReadConfig_priotdRegisterConfigHandler("authuser",
                                  VacmConf_parseAuthuser,
                                  NULL, "authtype1,authtype2 [-s secmodel] user [noauth|auth|priv [oid|-V view [context]]]");
    /* easy group auth handler */
    AgentReadConfig_priotdRegisterConfigHandler("authgroup",
                                  VacmConf_parseAuthuser,
                                  NULL, "authtype1,authtype2 [-s secmodel] group [noauth|auth|priv [oid|-V view [context]]]");

    AgentReadConfig_priotdRegisterConfigHandler("authaccess", VacmConf_parseAuthaccess,
                                  VacmConf_freeAccess,
                                  "name authtype1,authtype2 [-s secmodel] group view [noauth|auth|priv [context|context*]]");

    /*
     * Define standard views "_all_" and "_none_"
     */
    Callback_registerCallback(CALLBACK_LIBRARY,
                           CALLBACK_PRE_READ_CONFIG,
                           VacmConf_standardViews, NULL);
    Callback_registerCallback(CALLBACK_LIBRARY,
                           CALLBACK_POST_READ_CONFIG,
                           VacmConf_warnIfNotConfigured, NULL);
}

/**
 * Registers the easier-to-use VACM token handlers for quick access rules.
 * These tokens will only be recognised by 'snmpd'.
 */
void
VacmConf_initVacmPriotdEasyTokens(void) {

    AgentReadConfig_priotdRegisterConfigHandler("rwuser", VacmConf_parseRwuser, NULL,
                                  "user [noauth|auth|priv [oid|-V view [context]]]");
    AgentReadConfig_priotdRegisterConfigHandler("rouser", VacmConf_parseRouser, NULL,
                                  "user [noauth|auth|priv [oid|-V view [context]]]");
}

void
VacmConf_initVacmConf(void)
{
    VacmConf_initVacmConfigTokens();
    VacmConf_initVacmPriotdEasyTokens();
    /*
     * register ourselves to handle access control  ('snmpd' only)
     */
    Callback_registerCallback(CALLBACK_APPLICATION,
                           PriotdCallback_ACM_CHECK, VacmConf_inViewCallback,
                           NULL);
    Callback_registerCallback(CALLBACK_APPLICATION,
                           PriotdCallback_ACM_CHECK_INITIAL,
                           VacmConf_inViewCallback, NULL);
    Callback_registerCallback(CALLBACK_APPLICATION,
                           PriotdCallback_ACM_CHECK_SUBTREE,
                           VacmConf_inViewCallback, NULL);
}



void
VacmConf_parseGroup(const char *token, char *param)
{
    char            group[VACM_VACMSTRINGLEN], model[VACM_VACMSTRINGLEN], security[VACM_VACMSTRINGLEN];
    int             imodel;
    struct Vacm_GroupEntry_s *gp = NULL;
    char           *st;

    st = ReadConfig_copyNword(param, group, sizeof(group)-1);
    st = ReadConfig_copyNword(st, model, sizeof(model)-1);
    st = ReadConfig_copyNword(st, security, sizeof(security)-1);

    if (group[0] == 0) {
        ReadConfig_configPerror("missing GROUP parameter");
        return;
    }
    if (model[0] == 0) {
        ReadConfig_configPerror("missing MODEL parameter");
        return;
    }
    if (security[0] == 0) {
        ReadConfig_configPerror("missing SECURITY parameter");
        return;
    }
    if (strcasecmp(model, "v1") == 0)
        imodel = PRIOT_SEC_MODEL_SNMPv1;
    else if (strcasecmp(model, "v2c") == 0)
        imodel = PRIOT_SEC_MODEL_SNMPv2c;
    else if (strcasecmp(model, "any") == 0) {
        ReadConfig_configPerror
            ("bad security model \"any\" should be: v1, v2c, usm or a registered security plugin name - installing anyway");
        imodel = PRIOT_SEC_MODEL_ANY;
    } else {
        if ((imodel = Enum_seFindValueInSlist("snmp_secmods", model)) ==
            ENUM_SE_DNE) {
            ReadConfig_configPerror
                ("bad security model, should be: v1, v2c or usm or a registered security plugin name");
            return;
        }
    }
    if (strlen(security) + 1 > sizeof(gp->groupName)) {
        ReadConfig_configPerror("security name too long");
        return;
    }
    gp = Vacm_createGroupEntry(imodel, security);
    if (!gp) {
        ReadConfig_configPerror("failed to create group entry");
        return;
    }
    Strlcpy_strlcpy(gp->groupName, group, sizeof(gp->groupName));
    gp->storageType = PRIOT_STORAGE_PERMANENT;
    gp->status = PRIOT_ROW_ACTIVE;
    free(gp->reserved);
    gp->reserved = NULL;
}

void
VacmConf_freeGroup(void)
{
    Vacm_destroyAllGroupEntries();
}

#define PARSE_CONT 0
#define PARSE_FAIL 1

int
_VacmConf_parseAccessCommon(const char *token, char *param, char **st,
                          char **name, char **context, int *imodel,
                          int *ilevel, int *iprefix)
{
    char *model, *level, *prefix;

    *name = strtok_r(param, " \t\n", st);
    if (!*name) {
        ReadConfig_configPerror("missing NAME parameter");
        return PARSE_FAIL;
    }
    *context = strtok_r(NULL, " \t\n", st);
    if (!*context) {
        ReadConfig_configPerror("missing CONTEXT parameter");
        return PARSE_FAIL;
    }

    model = strtok_r(NULL, " \t\n", st);
    if (!model) {
        ReadConfig_configPerror("missing MODEL parameter");
        return PARSE_FAIL;
    }
    level = strtok_r(NULL, " \t\n", st);
    if (!level) {
        ReadConfig_configPerror("missing LEVEL parameter");
        return PARSE_FAIL;
    }
    prefix = strtok_r(NULL, " \t\n", st);
    if (!prefix) {
        ReadConfig_configPerror("missing PREFIX parameter");
        return PARSE_FAIL;
    }

    if (strcmp(*context, "\"\"") == 0 || strcmp(*context, "\'\'") == 0)
        **context = 0;
    if (strcasecmp(model, "any") == 0)
        *imodel = PRIOT_SEC_MODEL_ANY;
    else if (strcasecmp(model, "v1") == 0)
        *imodel = PRIOT_SEC_MODEL_SNMPv1;
    else if (strcasecmp(model, "v2c") == 0)
        *imodel = PRIOT_SEC_MODEL_SNMPv2c;
    else {
        if ((*imodel = Enum_seFindValueInSlist("snmp_secmods", model))
            == ENUM_SE_DNE) {
            ReadConfig_configPerror
                ("bad security model, should be: v1, v2c or usm or a registered security plugin name");
            return PARSE_FAIL;
        }
    }

    if (strcasecmp(level, "noauth") == 0)
        *ilevel = PRIOT_SEC_LEVEL_NOAUTH;
    else if (strcasecmp(level, "noauthnopriv") == 0)
        *ilevel = PRIOT_SEC_LEVEL_NOAUTH;
    else if (strcasecmp(level, "auth") == 0)
        *ilevel = PRIOT_SEC_LEVEL_AUTHNOPRIV;
    else if (strcasecmp(level, "authnopriv") == 0)
        *ilevel = PRIOT_SEC_LEVEL_AUTHNOPRIV;
    else if (strcasecmp(level, "priv") == 0)
        *ilevel = PRIOT_SEC_LEVEL_AUTHPRIV;
    else if (strcasecmp(level, "authpriv") == 0)
        *ilevel = PRIOT_SEC_LEVEL_AUTHPRIV;
    else {
        ReadConfig_configPerror
            ("bad security level (noauthnopriv, authnopriv, authpriv)");
        return PARSE_FAIL;
    }

    if (strcmp(prefix, "exact") == 0)
        *iprefix = 1;
    else if (strcmp(prefix, "prefix") == 0)
        *iprefix = 2;
    else if (strcmp(prefix, "0") == 0) {
        ReadConfig_configPerror
            ("bad prefix match parameter \"0\", should be: exact or prefix - installing anyway");
        *iprefix = 1;
    } else {
        ReadConfig_configPerror
            ("bad prefix match parameter, should be: exact or prefix");
        return PARSE_FAIL;
    }

    return PARSE_CONT;
}

/* **************************************/
/* authorization parsing token handlers */
/* **************************************/

int
VacmConf_parseAuthtokens(const char *token, char **confline)
{
    char authspec[TOOLS_MAXBUF_MEDIUM];
    char *strtok_state;
    char *type;
    int viewtype, viewtypes = 0;

    *confline = ReadConfig_copyNword(*confline, authspec, sizeof(authspec));

    DEBUG_MSGTL(("VacmConf_parseAuthtokens","parsing %s",authspec));
    if (!*confline) {
        ReadConfig_configPerror("Illegal configuration line: missing fields");
        return -1;
    }

    type = strtok_r(authspec, ",|:", &strtok_state);
    while(type && *type != '\0') {
        viewtype = Enum_seFindValueInSlist(VACM_VIEW_ENUM_NAME, type);
        if (viewtype < 0 || viewtype >= VACM_MAX_VIEWS) {
            ReadConfig_configPerror("Illegal view name");
        } else {
            viewtypes |= (1 << viewtype);
        }
        type = strtok_r(NULL, ",|:", &strtok_state);
    }
    DEBUG_MSG(("VacmConf_parseAuthtokens","  .. result = 0x%x\n",viewtypes));
    return viewtypes;
}

void
VacmConf_parseAuthuser(const char *token, char *confline)
{
    int viewtypes = VacmConf_parseAuthtokens(token, &confline);
    if (viewtypes != -1)
        VacmConf_createSimple(token, confline, VACM_CREATE_SIMPLE_V3, viewtypes);
}

void
VacmConf_parseAuthcommunity(const char *token, char *confline)
{
    int viewtypes = VacmConf_parseAuthtokens(token, &confline);
    if (viewtypes != -1)
        VacmConf_createSimple(token, confline, VACM_CREATE_SIMPLE_COM, viewtypes);
}

void
VacmConf_parseAuthaccess(const char *token, char *confline)
{
    char *group, *view, *tmp;
    const char *context;
    int  model = PRIOT_SEC_MODEL_ANY;
    int  level, prefix;
    int  i;
    char   *st;
    struct Vacm_AccessEntry_s *ap;
    int  viewtypes = VacmConf_parseAuthtokens(token, &confline);

    if (viewtypes == -1)
        return;

    group = strtok_r(confline, " \t\n", &st);
    if (!group) {
        ReadConfig_configPerror("missing GROUP parameter");
        return;
    }
    view = strtok_r(NULL, " \t\n", &st);
    if (!view) {
        ReadConfig_configPerror("missing VIEW parameter");
        return;
    }

    /*
     * Check for security model option
     */
    if ( strcasecmp(view, "-s") == 0 ) {
        tmp = strtok_r(NULL, " \t\n", &st);
        if (tmp) {
            if (strcasecmp(tmp, "any") == 0)
                model = PRIOT_SEC_MODEL_ANY;
            else if (strcasecmp(tmp, "v1") == 0)
                model = PRIOT_SEC_MODEL_SNMPv1;
            else if (strcasecmp(tmp, "v2c") == 0)
                model = PRIOT_SEC_MODEL_SNMPv2c;
            else {
                model = Enum_seFindValueInSlist("snmp_secmods", tmp);
                if (model == ENUM_SE_DNE) {
                    ReadConfig_configPerror
                        ("bad security model, should be: v1, v2c or usm or a registered security plugin name");
                    return;
                }
            }
        } else {
            ReadConfig_configPerror("missing SECMODEL parameter");
            return;
        }
        view = strtok_r(NULL, " \t\n", &st);
        if (!view) {
            ReadConfig_configPerror("missing VIEW parameter");
            return;
        }
    }
    if (strlen(view) >= VACM_VACMSTRINGLEN ) {
        ReadConfig_configPerror("View value too long");
        return;
    }

    /*
     * Now parse optional fields, or provide default values
     */

    tmp = strtok_r(NULL, " \t\n", &st);
    if (tmp) {
        if (strcasecmp(tmp, "noauth") == 0)
            level = PRIOT_SEC_LEVEL_NOAUTH;
        else if (strcasecmp(tmp, "noauthnopriv") == 0)
            level = PRIOT_SEC_LEVEL_NOAUTH;
        else if (strcasecmp(tmp, "auth") == 0)
            level = PRIOT_SEC_LEVEL_AUTHNOPRIV;
        else if (strcasecmp(tmp, "authnopriv") == 0)
            level = PRIOT_SEC_LEVEL_AUTHNOPRIV;
        else if (strcasecmp(tmp, "priv") == 0)
            level = PRIOT_SEC_LEVEL_AUTHPRIV;
        else if (strcasecmp(tmp, "authpriv") == 0)
            level = PRIOT_SEC_LEVEL_AUTHPRIV;
        else {
            ReadConfig_configPerror
                ("bad security level (noauthnopriv, authnopriv, authpriv)");
                return;
        }
    } else {
        /*  What about  PRIOT_SEC_MODEL_ANY ?? */
        if ( model == PRIOT_SEC_MODEL_SNMPv1 ||
             model == PRIOT_SEC_MODEL_SNMPv2c )
            level = PRIOT_SEC_LEVEL_NOAUTH;
        else
            level = PRIOT_SEC_LEVEL_AUTHNOPRIV;
    }


    context = tmp = strtok_r(NULL, " \t\n", &st);
    if (tmp) {
        tmp = (tmp + strlen(tmp)-1);
        if (tmp && *tmp == '*') {
            *tmp = '\0';
            prefix = 2;
        } else {
            prefix = 1;
        }
    } else {
        context = "";
        prefix  = 1;   /* Or prefix(2) ?? */
    }

    /*
     * Now we can create the access entry
     */
    ap = Vacm_getAccessEntry(group, context, model, level);
    if (!ap) {
        ap = Vacm_createAccessEntry(group, context, model, level);
        DEBUG_MSGTL(("vacm:conf:authaccess",
                    "no existing access found; creating a new one\n"));
    } else {
        DEBUG_MSGTL(("vacm:conf:authaccess",
                    "existing access found, using it\n"));
    }
    if (!ap) {
        ReadConfig_configPerror("failed to create access entry");
        return;
    }

    for (i = 0; i <= VACM_MAX_VIEWS; i++) {
        if (viewtypes & (1 << i)) {
            strcpy(ap->views[i], view);
        }
    }
    ap->contextMatch = prefix;
    ap->storageType  = PRIOT_STORAGE_PERMANENT;
    ap->status       = PRIOT_ROW_ACTIVE;
    if (ap->reserved)
        free(ap->reserved);
    ap->reserved = NULL;
}

void
VacmConf_parseSetaccess(const char *token, char *param)
{
    char *name, *context, *viewname, *viewval;
    int  imodel, ilevel, iprefix;
    int  viewnum;
    char   *st;
    struct Vacm_AccessEntry_s *ap;

    if (_VacmConf_parseAccessCommon(token, param, &st, &name,
                                  &context, &imodel, &ilevel, &iprefix)
        == PARSE_FAIL) {
        return;
    }

    viewname = strtok_r(NULL, " \t\n", &st);
    if (!viewname) {
        ReadConfig_configPerror("missing viewname parameter");
        return;
    }
    viewval = strtok_r(NULL, " \t\n", &st);
    if (!viewval) {
        ReadConfig_configPerror("missing viewval parameter");
        return;
    }

    if (strlen(viewval) + 1 > sizeof(ap->views[VACM_VIEW_NOTIFY])) {
        ReadConfig_configPerror("View value too long");
        return;
    }

    viewnum = Enum_seFindValueInSlist(VACM_VIEW_ENUM_NAME, viewname);
    if (viewnum < 0 || viewnum >= VACM_MAX_VIEWS) {
        ReadConfig_configPerror("Illegal view name");
        return;
    }

    ap = Vacm_getAccessEntry(name, context, imodel, ilevel);
    if (!ap) {
        ap = Vacm_createAccessEntry(name, context, imodel, ilevel);
        DEBUG_MSGTL(("vacm:conf:setaccess",
                    "no existing access found; creating a new one\n"));
    } else {
        DEBUG_MSGTL(("vacm:conf:setaccess",
                    "existing access found, using it\n"));
    }
    if (!ap) {
        ReadConfig_configPerror("failed to create access entry");
        return;
    }

    strcpy(ap->views[viewnum], viewval);
    ap->contextMatch = iprefix;
    ap->storageType = PRIOT_STORAGE_PERMANENT;
    ap->status = PRIOT_ROW_ACTIVE;
    free(ap->reserved);
    ap->reserved = NULL;
}

void
VacmConf_parseAccess(const char *token, char *param)
{
    char           *name, *context, *readView, *writeView, *notify;
    int             imodel, ilevel, iprefix;
    struct Vacm_AccessEntry_s *ap;
    char   *st;


    if (_VacmConf_parseAccessCommon(token, param, &st, &name,
                                  &context, &imodel, &ilevel, &iprefix)
        == PARSE_FAIL) {
        return;
    }

    readView = strtok_r(NULL, " \t\n", &st);
    if (!readView) {
        ReadConfig_configPerror("missing readView parameter");
        return;
    }
    writeView = strtok_r(NULL, " \t\n", &st);
    if (!writeView) {
        ReadConfig_configPerror("missing writeView parameter");
        return;
    }
    notify = strtok_r(NULL, " \t\n", &st);
    if (!notify) {
        ReadConfig_configPerror("missing notifyView parameter");
        return;
    }

    if (strlen(readView) + 1 > sizeof(ap->views[VACM_VIEW_READ])) {
        ReadConfig_configPerror("readView too long");
        return;
    }
    if (strlen(writeView) + 1 > sizeof(ap->views[VACM_VIEW_WRITE])) {
        ReadConfig_configPerror("writeView too long");
        return;
    }
    if (strlen(notify) + 1 > sizeof(ap->views[VACM_VIEW_NOTIFY])) {
        ReadConfig_configPerror("notifyView too long");
        return;
    }
    ap = Vacm_createAccessEntry(name, context, imodel, ilevel);
    if (!ap) {
        ReadConfig_configPerror("failed to create access entry");
        return;
    }
    strcpy(ap->views[VACM_VIEW_READ], readView);
    strcpy(ap->views[VACM_VIEW_WRITE], writeView);
    strcpy(ap->views[VACM_VIEW_NOTIFY], notify);
    ap->contextMatch = iprefix;
    ap->storageType = PRIOT_STORAGE_PERMANENT;
    ap->status = PRIOT_ROW_ACTIVE;
    free(ap->reserved);
    ap->reserved = NULL;
}

void
VacmConf_freeAccess(void)
{
    Vacm_destroyAllAccessEntries();
}

void
VacmConf_parseView(const char *token, char *param)
{
    char           *name, *type, *subtree, *mask;
    int             inclexcl;
    struct Vacm_ViewEntry_s *vp;
    oid             suboid[ASN01_MAX_OID_LEN];
    size_t          suboid_len = 0;
    size_t          mask_len = 0;
    u_char          viewMask[VACM_VACMSTRINGLEN];
    size_t          i;
    char            *st;

    name = strtok_r(param, " \t\n", &st);
    if (!name) {
        ReadConfig_configPerror("missing NAME parameter");
        return;
    }
    type = strtok_r(NULL, " \n\t", &st);
    if (!type) {
        ReadConfig_configPerror("missing TYPE parameter");
        return;
    }
    subtree = strtok_r(NULL, " \t\n", &st);
    if (!subtree) {
        ReadConfig_configPerror("missing SUBTREE parameter");
        return;
    }
    mask = strtok_r(NULL, "\0", &st);

    if (strcmp(type, "included") == 0)
        inclexcl = PRIOT_VIEW_INCLUDED;
    else if (strcmp(type, "excluded") == 0)
        inclexcl = PRIOT_VIEW_EXCLUDED;
    else {
        ReadConfig_configPerror("TYPE must be included/excluded?");
        return;
    }
    suboid_len = strlen(subtree)-1;
    if (subtree[suboid_len] == '.')
        subtree[suboid_len] = '\0';   /* stamp on a trailing . */
    suboid_len = ASN01_MAX_OID_LEN;
    if (!Mib_parseOid(subtree, suboid, &suboid_len)) {
        ReadConfig_configPerror("bad SUBTREE object id");
        return;
    }
    if (mask) {
        unsigned int val;
        i = 0;
        for (mask = strtok_r(mask, " .:", &st); mask; mask = strtok_r(NULL, " .:", &st)) {
            if (i >= sizeof(viewMask)) {
                ReadConfig_configPerror("MASK too long");
                return;
            }
            if (sscanf(mask, "%x", &val) == 0) {
                ReadConfig_configPerror("invalid MASK");
                return;
            }
            viewMask[i] = val;
            i++;
        }
        mask_len = i;
    } else {
        for (i = 0; i < sizeof(viewMask); i++)
            viewMask[i] = 0xff;
    }
    vp = Vacm_createViewEntry(name, suboid, suboid_len);
    if (!vp) {
        ReadConfig_configPerror("failed to create view entry");
        return;
    }
    memcpy(vp->viewMask, viewMask, sizeof(viewMask));
    vp->viewMaskLen = mask_len;
    vp->viewType = inclexcl;
    vp->viewStorageType = PRIOT_STORAGE_PERMANENT;
    vp->viewStatus = PRIOT_ROW_ACTIVE;
    free(vp->reserved);
    vp->reserved = NULL;
}

void
VacmConf_freeView(void)
{
    Vacm_destroyAllViewEntries();
}

void
VacmConf_genCom2sec(int commcount, const char *community, const char *addressname,
                 const char *publishtoken,
                 void (*parser)(const char *, char *),
                 char *secname, size_t secname_len,
                 char *viewname, size_t viewname_len, int version,
                 const char *context)
{
    char            line[IMPL_SPRINT_MAX_LEN];

    /*
     * com2sec6|comsec [-Cn CONTEXT] anonymousSecNameNUM    ADDRESS  COMMUNITY
     */
    snprintf(secname, secname_len-1, "comm%d", commcount);
    secname[secname_len-1] = '\0';
    if (viewname) {
        snprintf(viewname, viewname_len-1, "viewComm%d", commcount);
        viewname[viewname_len-1] = '\0';
    }
    if ( context && *context )
       snprintf(line, sizeof(line), "-Cn %s %s %s '%s'",
             context, secname, addressname, community);
    else
       snprintf(line, sizeof(line), "%s %s '%s'",
             secname, addressname, community);
    line[ sizeof(line)-1 ] = 0;
    DEBUG_MSGTL((publishtoken, "passing: %s %s\n", publishtoken, line));
    (*parser)(publishtoken, line);

    /*
     * sec->group mapping
     */
    /*
     * group   anonymousGroupNameNUM  any      anonymousSecNameNUM
     */
    if ( version & PRIOT_SEC_MODEL_SNMPv1 ) {
        snprintf(line, sizeof(line),
             "grp%.28s v1 %s", secname, secname);
        line[ sizeof(line)-1 ] = 0;
        DEBUG_MSGTL((publishtoken, "passing: %s %s\n", "group", line));
        VacmConf_parseGroup("group", line);
    }

    if ( version & PRIOT_SEC_MODEL_SNMPv2c ) {
        snprintf(line, sizeof(line),
             "grp%.28s v2c %s", secname, secname);
        line[ sizeof(line)-1 ] = 0;
        DEBUG_MSGTL((publishtoken, "passing: %s %s\n", "group", line));
        VacmConf_parseGroup("group", line);
    }
}

void
VacmConf_parseRwuser(const char *token, char *confline)
{
    VacmConf_createSimple(token, confline, VACM_CREATE_SIMPLE_V3,
                       VACM_VIEW_READ_BIT | VACM_VIEW_WRITE_BIT);
}

void
VacmConf_parseRouser(const char *token, char *confline)
{
    VacmConf_createSimple(token, confline, VACM_CREATE_SIMPLE_V3,
                       VACM_VIEW_READ_BIT);
}

void
VacmConf_parseRocommunity(const char *token, char *confline)
{
    VacmConf_createSimple(token, confline, VACM_CREATE_SIMPLE_COMIPV4,
                       VACM_VIEW_READ_BIT);
}

void
VacmConf_parseRwcommunity(const char *token, char *confline)
{
    VacmConf_createSimple(token, confline, VACM_CREATE_SIMPLE_COMIPV4,
                       VACM_VIEW_READ_BIT | VACM_VIEW_WRITE_BIT);
}

void
VacmConf_parseRocommunity6(const char *token, char *confline)
{
    VacmConf_createSimple(token, confline, VACM_CREATE_SIMPLE_COMIPV6,
                       VACM_VIEW_READ_BIT);
}

void
VacmConf_parseRwcommunity6(const char *token, char *confline)
{
    VacmConf_createSimple(token, confline, VACM_CREATE_SIMPLE_COMIPV6,
                       VACM_VIEW_READ_BIT | VACM_VIEW_WRITE_BIT);
}


void
VacmConf_createSimple(const char *token, char *confline,
                   int parsetype, int viewtypes)
{
    char            line[IMPL_SPRINT_MAX_LEN];
    char            community[IMPL_COMMUNITY_MAX_LEN];
    char            theoid[IMPL_SPRINT_MAX_LEN];
    char            viewname[IMPL_SPRINT_MAX_LEN];
    char           *view_ptr = viewname;
    const char     *rw = "none";
    char            model[IMPL_SPRINT_MAX_LEN];
    char           *cp, *tmp;
    char            secname[IMPL_SPRINT_MAX_LEN];
    char            grpname[IMPL_SPRINT_MAX_LEN];
    char            authlevel[IMPL_SPRINT_MAX_LEN];
    char            context[IMPL_SPRINT_MAX_LEN];
    int             ctxprefix = 1;  /* Default to matching all contexts */
    static int      commcount = 0;
    /* Conveniently, the community-based security
       model values can also be used as bit flags */
    int             commversion = PRIOT_SEC_MODEL_SNMPv1 |
                                  PRIOT_SEC_MODEL_SNMPv2c;

    /*
     * init
     */
    strcpy(model, "any");
    memset(context, 0, sizeof(context));
    memset(secname, 0, sizeof(secname));
    memset(grpname, 0, sizeof(grpname));

    /*
     * community name or user name
     */
    cp = ReadConfig_copyNword(confline, community, sizeof(community));

    if (parsetype == VACM_CREATE_SIMPLE_V3) {
        /*
         * maybe security model type
         */
        if (strcmp(community, "-s") == 0) {
            /*
             * -s model ...
             */
            if (cp)
                cp = ReadConfig_copyNword(cp, model, sizeof(model));
            if (!cp) {
                ReadConfig_configPerror("illegal line");
                return;
            }
            if (cp)
                cp = ReadConfig_copyNword(cp, community, sizeof(community));
        } else {
            strcpy(model, "usm");
        }
        /*
         * authentication level
         */
        if (cp && *cp)
            cp = ReadConfig_copyNword(cp, authlevel, sizeof(authlevel));
        else
            strcpy(authlevel, "auth");
        DEBUG_MSGTL((token, "setting auth level: \"%s\"\n", authlevel));
    }

    /*
     * oid they can touch
     */
    if (cp && *cp) {
        if (strncmp(cp, "-V ", 3) == 0) {
             cp = ReadConfig_skipToken(cp);
             cp = ReadConfig_copyNword(cp, viewname, sizeof(viewname));
             view_ptr = NULL;
        } else {
             cp = ReadConfig_copyNword(cp, theoid, sizeof(theoid));
        }
    } else {
        strcpy(theoid, ".1");
        strcpy(viewname, "_all_");
        view_ptr = NULL;
    }
    /*
     * optional, non-default context
     */
    if (cp && *cp) {
        cp = ReadConfig_copyNword(cp, context, sizeof(context));
        tmp = (context + strlen(context)-1);
        if (tmp && *tmp == '*') {
            *tmp = '\0';
            ctxprefix = 1;
        } else {
            /*
             * If no context field is given, then we default to matching
             *   all contexts (for compatability with previous releases).
             * But if a field context is specified (not ending with '*')
             *   then this should be taken as an exact match.
             * Specifying a context field of "" will match the default
             *   context (and *only* the default context).
             */
            ctxprefix = 0;
        }
    }

    if (viewtypes & VACM_VIEW_WRITE_BIT)
        rw = viewname;

    commcount++;


    if (parsetype == VACM_CREATE_SIMPLE_V3) {
        /* support for SNMPv3 user names */
        if (view_ptr) {
            sprintf(viewname,"viewUSM%d",commcount);
        }
        if ( strcmp( token, "authgroup" ) == 0 ) {
            Strlcpy_strlcpy(grpname, community, sizeof(grpname));
        } else {
            Strlcpy_strlcpy(secname, community, sizeof(secname));

            /*
             * sec->group mapping
             */
            /*
             * group   anonymousGroupNameNUM  any      anonymousSecNameNUM
             */
            snprintf(grpname, sizeof(grpname), "grp%.28s", secname);
            for (tmp=grpname; *tmp; tmp++)
                if (!isalnum((unsigned char)(*tmp)))
                    *tmp = '_';
            snprintf(line, sizeof(line),
                     "%s %s \"%s\"", grpname, model, secname);
            line[ sizeof(line)-1 ] = 0;
            DEBUG_MSGTL((token, "passing: %s %s\n", "group", line));
            VacmConf_parseGroup("group", line);
        }
    } else {
        snprintf(grpname, sizeof(grpname), "grp%.28s", secname);
        for (tmp=grpname; *tmp; tmp++)
            if (!isalnum((unsigned char)(*tmp)))
                *tmp = '_';
    }

    /*
     * view definition
     */
    /*
     * view    anonymousViewNUM       included OID
     */
    if (view_ptr) {
        snprintf(line, sizeof(line), "%s included %s", viewname, theoid);
        line[ sizeof(line)-1 ] = 0;
        DEBUG_MSGTL((token, "passing: %s %s\n", "view", line));
        VacmConf_parseView("view", line);
    }

    /*
     * map everything together
     */
    if ((viewtypes == VACM_VIEW_READ_BIT) ||
        (viewtypes == (VACM_VIEW_READ_BIT | VACM_VIEW_WRITE_BIT))) {
        /* Use the simple line access command */
        /*
         * access  anonymousGroupNameNUM  "" MODEL AUTHTYPE prefix anonymousViewNUM [none/anonymousViewNUM] [none/anonymousViewNUM]
         */
        snprintf(line, sizeof(line),
                 "%s %s %s %s %s %s %s %s",
                 grpname, context[0] ? context : "\"\"",
                 model, authlevel,
                (ctxprefix ? "prefix" : "exact"),
                 viewname, rw, rw);
        line[ sizeof(line)-1 ] = 0;
        DEBUG_MSGTL((token, "passing: %s %s\n", "access", line));
        VacmConf_parseAccess("access", line);
    } else {
        /* Use one setaccess line per access type */
        /*
         * setaccess  anonymousGroupNameNUM  "" MODEL AUTHTYPE prefix viewname viewval
         */
        int i;
        DEBUG_MSGTL((token, " checking view levels for %x\n", viewtypes));
        for(i = 0; i <= VACM_MAX_VIEWS; i++) {
            if (viewtypes & (1 << i)) {
                snprintf(line, sizeof(line),
                         "%s %s %s %s %s %s %s",
                         grpname, context[0] ? context : "\"\"",
                         model, authlevel,
                        (ctxprefix ? "prefix" : "exact"),
                         Enum_seFindLabelInSlist(VACM_VIEW_ENUM_NAME, i),
                         viewname);
                line[ sizeof(line)-1 ] = 0;
                DEBUG_MSGTL((token, "passing: %s %s\n", "setaccess", line));
                VacmConf_parseSetaccess("setaccess", line);
            }
        }
    }
}

int
VacmConf_standardViews(int majorID, int minorID, void *serverarg,
                            void *clientarg)
{
    char            line[IMPL_SPRINT_MAX_LEN];

    memset(line, 0, sizeof(line));

    snprintf(line, sizeof(line), "_all_ included .0");
    VacmConf_parseView("view", line);
    snprintf(line, sizeof(line), "_all_ included .1");
    VacmConf_parseView("view", line);
    snprintf(line, sizeof(line), "_all_ included .2");
    VacmConf_parseView("view", line);

    snprintf(line, sizeof(line), "_none_ excluded .0");
    VacmConf_parseView("view", line);
    snprintf(line, sizeof(line), "_none_ excluded .1");
    VacmConf_parseView("view", line);
    snprintf(line, sizeof(line), "_none_ excluded .2");
    VacmConf_parseView("view", line);

    return PRIOT_ERR_NOERROR;
}

int
VacmConf_warnIfNotConfigured(int majorID, int minorID, void *serverarg,
                            void *clientarg)
{
    const char * name = DefaultStore_getString(DsStorage_LIBRARY_ID,
                                               DsStr_APPTYPE);
    const int agent_mode =  DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                                                   DsAgentBoolean_ROLE);
    if (NULL==name)
        name = "snmpd";

    if (!Vacm_isConfigured()) {
        /*
         *  An AgentX subagent relies on the master agent to apply suitable
         *    access control checks, so doesn't need local VACM configuration.
         *  The trap daemon has a separate check (see below).
         *
         *  Otherwise, an AgentX master or SNMP standalone agent requires some
         *    form of VACM configuration.  No config means that no incoming
         *    requests will be accepted, so warn the user accordingly.
         */
        if ((MASTER_AGENT == agent_mode) && (strcmp(name, "snmptrapd") != 0)) {
            Logger_log(LOGGER_PRIORITY_WARNING,
                 "Warning: no access control information configured.\n"
                 "  (Config search path: %s)\n"
                 "  It's unlikely this agent can serve any useful purpose in this state.\n"
                 "  Run \"snmpconf -g basic_setup\" to help you "
                 "configure the %s.conf file for this agent.\n",
                 ReadConfig_getConfigurationDirectory(), name);
        }

        /*
         *  The trap daemon implements VACM-style access control for incoming
         *    notifications, but offers a way of turning this off (for backwards
         *    compatability).  Check for this explicitly, and warn if necessary.
         *
         *  NB:  The NETSNMP_DS_APP_NO_AUTHORIZATION definition is a duplicate
         *       of an identical setting in "apps/snmptrapd_ds.h".
         *       These two need to be kept in synch.
         */

        if (!strcmp(name, "snmptrapd") &&
            !DefaultStore_getBoolean(DsStorage_APPLICATION_ID,
                                     DsAgentBoolean_APP_NO_AUTHORIZATION)) {
            Logger_log(LOGGER_PRIORITY_WARNING,
                 "Warning: no access control information configured.\n"
                 "  (Config search path: %s)\n"
                 "This receiver will *NOT* accept any incoming notifications.\n",
                 ReadConfig_getConfigurationDirectory());
        }
    }
    return PRIOT_ERR_NOERROR;
}

int
VacmConf_inViewCallback(int majorID, int minorID, void *serverarg,
                      void *clientarg)
{
    struct ViewParameters_s *view_parms =
        (struct ViewParameters_s *) serverarg;
    int             retval;

    if (view_parms == NULL)
        return 1;
    retval = VacmConf_inView(view_parms->pdu, view_parms->name,
                          view_parms->namelen, view_parms->check_subtree);
    if (retval != 0)
        view_parms->errorcode = retval;
    return retval;
}


/**
 * VacmConf_inView: decides if a given PDU can be acted upon
 *
 * Parameters:
 *	*pdu
 *	*name
 *	 namelen
 *       check_subtree
 *
 * Returns:
 * VACM_SUCCESS(0)	   On success.
 * VACM_NOSECNAME(1)	   Missing security name.
 * VACM_NOGROUP(2)	   Missing group
 * VACM_NOACCESS(3)	   Missing access
 * VACM_NOVIEW(4)	   Missing view
 * VACM_NOTINVIEW(5)	   Not in view
 * VACM_NOSUCHCONTEXT(6)   No Such Context
 * VACM_SUBTREE_UNKNOWN(7) When testing an entire subtree, UNKNOWN (ie, the entire
 *                         subtree has both allowed and disallowed portions)
 *
 * Debug output listed as follows:
 *	\<securityName\> \<groupName\> \<viewName\> \<viewType\>
 */
int
VacmConf_inView(Types_Pdu *pdu, oid * name, size_t namelen,
             int check_subtree)
{
    int viewtype;

    switch (pdu->command) {
    case PRIOT_MSG_GET:
    case PRIOT_MSG_GETNEXT:
    case PRIOT_MSG_GETBULK:
        viewtype = VACM_VIEW_READ;
        break;
    case PRIOT_MSG_SET:
        viewtype = VACM_VIEW_WRITE;
        break;
    case PRIOT_MSG_TRAP:
    case PRIOT_MSG_TRAP2:
    case PRIOT_MSG_INFORM:
        viewtype = VACM_VIEW_NOTIFY;
        break;
    default:
        Logger_log(LOGGER_PRIORITY_ERR, "bad msg type in VacmConf_inView: %d\n",
                 pdu->command);
        viewtype = VACM_VIEW_READ;
    }
    return VacmConf_checkView(pdu, name, namelen, check_subtree, viewtype);
}

/**
 * VacmConf_checkView: decides if a given PDU can be taken based on a view type
 *
 * Parameters:
 *	*pdu
 *	*name
 *	 namelen
 *       check_subtree
 *       viewtype
 *
 * Returns:
 * VACM_SUCCESS(0)	   On success.
 * VACM_NOSECNAME(1)	   Missing security name.
 * VACM_NOGROUP(2)	   Missing group
 * VACM_NOACCESS(3)	   Missing access
 * VACM_NOVIEW(4)	   Missing view
 * VACM_NOTINVIEW(5)	   Not in view
 * VACM_NOSUCHCONTEXT(6)   No Such Context
 * VACM_SUBTREE_UNKNOWN(7) When testing an entire subtree, UNKNOWN (ie, the entire
 *                         subtree has both allowed and disallowed portions)
 *
 * Debug output listed as follows:
 *	\<securityName\> \<groupName\> \<viewName\> \<viewType\>
 */
int
VacmConf_checkView(Types_Pdu *pdu, oid * name, size_t namelen,
                int check_subtree, int viewtype)
{
    return VacmConf_checkViewContents(pdu, name, namelen, check_subtree, viewtype,
                                    VACM_CHECK_VIEW_CONTENTS_NO_FLAGS);
}

int
VacmConf_checkViewContents(Types_Pdu *pdu, oid * name, size_t namelen,
                         int check_subtree, int viewtype, int flags)
{
    struct Vacm_AccessEntry_s *ap;
    struct Vacm_GroupEntry_s *gp;
    struct Vacm_ViewEntry_s *vp;
    char            vacm_default_context[1] = "";
    const char     *contextName = vacm_default_context;
    const char     *sn = NULL;
    char           *vn;
    const char     *pdu_community;

    /*
     * len defined by the vacmContextName object
     */
#define CONTEXTNAMEINDEXLEN 32
    char            contextNameIndex[CONTEXTNAMEINDEXLEN + 1];

      if (Secmod_find(pdu->securityModel)) {
        /*
         * any legal defined v3 security model
         */
        DEBUG_MSG(("mibII/vacm_vars",
                  "VacmConf_inView: ver=%ld, model=%d, secName=%s\n",
                  pdu->version, pdu->securityModel, pdu->securityName));
        sn = pdu->securityName;
        contextName = pdu->contextName;
    } else {
        sn = NULL;
    }

    if (sn == NULL) {
        DEBUG_MSGTL(("mibII/vacm_vars",
                    "VacmConf_inView: No security name found\n"));
        return VACM_NOSECNAME;
    }

    if (pdu->contextNameLen > CONTEXTNAMEINDEXLEN) {
        DEBUG_MSGTL(("mibII/vacm_vars",
                    "VacmConf_inView: bad ctxt length %d\n",
                    (int)pdu->contextNameLen));
        return VACM_NOSUCHCONTEXT;
    }
    /*
     * NULL termination of the pdu field is ugly here.  Do in PDU parsing?
     */
    if (pdu->contextName)
        memcpy(contextNameIndex, pdu->contextName, pdu->contextNameLen);
    else
        contextNameIndex[0] = '\0';

    contextNameIndex[pdu->contextNameLen] = '\0';
    if (!(flags & VACM_CHECK_VIEW_CONTENTS_DNE_CONTEXT_OK) &&
        !AgentRegistry_subtreeFindFirst(contextNameIndex)) {
        /*
         * rfc 3415 section 3.2, step 1
         * no such context here; return no such context error
         */
        DEBUG_MSGTL(("mibII/vacm_vars", "VacmConf_inView: no such ctxt \"%s\"\n",
                    contextNameIndex));
        return VACM_NOSUCHCONTEXT;
    }

    DEBUG_MSGTL(("mibII/vacm_vars", "VacmConf_inView: sn=%s", sn));

    gp = Vacm_getGroupEntry(pdu->securityModel, sn);
    if (gp == NULL) {
        DEBUG_MSG(("mibII/vacm_vars", "\n"));
        return VACM_NOGROUP;
    }
    DEBUG_MSG(("mibII/vacm_vars", ", gn=%s", gp->groupName));

    ap = Vacm_getAccessEntry(gp->groupName, contextNameIndex,
                             pdu->securityModel, pdu->securityLevel);
    if (ap == NULL) {
        DEBUG_MSG(("mibII/vacm_vars", "\n"));
        return VACM_NOACCESS;
    }

    if (name == NULL) { /* only check the setup of the vacm for the request */
        DEBUG_MSG(("mibII/vacm_vars", ", Done checking setup\n"));
        return VACM_SUCCESS;
    }

    if (viewtype < 0 || viewtype >= VACM_MAX_VIEWS) {
        DEBUG_MSG(("mibII/vacm_vars", " illegal view type\n"));
        return VACM_NOACCESS;
    }
    vn = ap->views[viewtype];
    DEBUG_MSG(("mibII/vacm_vars", ", vn=%s", vn));

    if (check_subtree) {
        DEBUG_MSG(("mibII/vacm_vars", "\n"));
        return Vacm_checkSubtree(vn, name, namelen);
    }

    vp = Vacm_getViewEntry(vn, name, namelen, VACM_MODE_FIND);

    if (vp == NULL) {
        DEBUG_MSG(("mibII/vacm_vars", "\n"));
        return VACM_NOVIEW;
    }
    DEBUG_MSG(("mibII/vacm_vars", ", vt=%d\n", vp->viewType));

    if (vp->viewType == PRIOT_VIEW_EXCLUDED) {

        return VACM_NOTINVIEW;
    }

    return VACM_SUCCESS;

}                               /* end VacmConf_inView() */

