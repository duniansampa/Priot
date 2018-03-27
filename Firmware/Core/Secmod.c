#include "Secmod.h"
#include "Callback.h"
#include "DefaultStore.h"
#include "Tools.h"
#include "Api.h"
#include "Enum.h"
#include "Debug.h"
#include "Logger.h"
#include "Usm.h"

extern void Usm_initUsm();
extern void Usm_shutdownUsm();

static struct Secmod_List_s * _secmod_registeredServices = NULL;

static Callback_CallbackFT _Secmod_setDefaultSecmod;

void Secmod_init(void)
{
    Callback_registerCallback(CALLBACK_LIBRARY,
                           CALLBACK_SESSION_INIT, _Secmod_setDefaultSecmod,
                           NULL);

    DefaultStore_registerConfig(ASN01_OCTET_STR, "priot", "defSecurityModel",
                   DsStorage_LIBRARY_ID, DsStr_SECMODEL);

    /* This file is automatically generated by configure.  Do not modify by hand. */
    Usm_initUsm();
}

void Secmod_shutdown(void)
{
    /* This file is automatically generated by configure.  Do not modify by hand. */
    Usm_shutdownUsm();

}

int Secmod_register(int secmod, const char *modname,
                 struct Secmod_Def_s *newdef)
{
    int             result;
    struct Secmod_List_s *sptr;
    char           *othername;

    for (sptr = _secmod_registeredServices; sptr; sptr = sptr->next) {
        if (sptr->securityModel == secmod) {
            return ErrorCode_GENERR;
        }
    }
    sptr = TOOLS_MALLOC_STRUCT(Secmod_List_s);
    if (sptr == NULL)
        return ErrorCode_MALLOC;
    sptr->secDef = newdef;
    sptr->securityModel = secmod;
    sptr->next = _secmod_registeredServices;
    _secmod_registeredServices = sptr;
    if ((result =
         Enum_seAddPairToSlist("priotSecmods", strdup(modname), secmod))
        != ENUM_SE_OK) {
        switch (result) {
        case ENUM_SE_NOMEM:
            Logger_log(LOGGER_PRIORITY_CRIT, "priot_secmod: no memory\n");
            break;

        case ENUM_SE_ALREADY_THERE:
            othername = Enum_seFindLabelInSlist("priotSecmods", secmod);
            if (strcmp(othername, modname) != 0) {
                Logger_log(LOGGER_PRIORITY_ERR,
                         "priot_secmod: two security modules %s and %s registered with the same security number\n",
                         modname, othername);
            }
            break;

        default:
            Logger_log(LOGGER_PRIORITY_ERR,
                     "priot_secmod: unknown error trying to register a new security module\n");
            break;
        }
        return ErrorCode_GENERR;
    }
    return ErrorCode_SUCCESS;
}

int Secmod_unregister(int secmod)
{
    struct Secmod_List_s *sptr, *lptr;

    for (sptr = _secmod_registeredServices, lptr = NULL; sptr;
         lptr = sptr, sptr = sptr->next) {
        if (sptr->securityModel == secmod) {
            if ( lptr )
                lptr->next = sptr->next;
            else
                _secmod_registeredServices = sptr->next;
        TOOLS_FREE(sptr->secDef);
            TOOLS_FREE(sptr);
            return ErrorCode_SUCCESS;
        }
    }
    /*
     * not registered
     */
    return ErrorCode_GENERR;
}

void Secmod_clear(void)
{
    struct Secmod_List_s *tmp = _secmod_registeredServices, *next = NULL;

    while (tmp != NULL) {
        next = tmp->next;
        TOOLS_FREE(tmp->secDef);
        TOOLS_FREE(tmp);
        tmp = next;
    }
    _secmod_registeredServices = NULL;
}


struct Secmod_Def_s * Secmod_find(int secmod)
{
    struct Secmod_List_s *sptr;

    for (sptr = _secmod_registeredServices; sptr; sptr = sptr->next) {
        if (sptr->securityModel == secmod) {
            return sptr->secDef;
        }
    }
    /*
     * not registered
     */
    return NULL;
}

/* try to pick a reasonable security module default based on what was
   compiled into the net-snmp package */
#define SECMOD_DEFAULT_MODEL  USM_SEC_MODEL_NUMBER


static int _Secmod_setDefaultSecmod(int major, int minor, void *serverarg, void *clientarg)
{
    Types_Session *sess = (Types_Session *) serverarg;
    char           *cptr;
    int             model;

    if (!sess)
        return ErrorCode_GENERR;
    if (sess->securityModel == API_DEFAULT_SECMODEL) {
        if ((cptr = DefaultStore_getString(DsStorage_LIBRARY_ID,
                      DsStr_SECMODEL)) != NULL) {
            if ((model = Enum_seFindValueInSlist("priotSecmods", cptr))
        != ENUM_SE_DNE) {
                sess->securityModel = model;
            } else {
                Logger_log(LOGGER_PRIORITY_ERR,
                         "unknown security model name: %s.  Forcing USM instead.\n",
                         cptr);
                sess->securityModel = SECMOD_DEFAULT_MODEL;
                return ErrorCode_GENERR;
            }
        } else {
            sess->securityModel = SECMOD_DEFAULT_MODEL;
        }
    }
    return ErrorCode_SUCCESS;
}
