#include "DefaultStore.h"
#include <string.h>

#include "String.h"
#include "Numbers/Integer.h"
#include "Debug.h"
#include "Logger.h"
#include "Tools.h"
#include "ReadConfig.h"
#include "Api.h"



typedef struct DefaultStore_ReadConfig_s {
    _u8           type;
    _s8 *         token;
    _s8 *         storename;
    _i32          storeid;
    _i32          which;
    struct DefaultStore_ReadConfig_s *next;
} DefaultStore_ReadConfig;


static DefaultStore_ReadConfig * defaultStore_readConfig = NULL;

static const _s8 *  defaultStore_stores [DEFAULTSTORE_MAX_IDS] = { "LIB", "APP", "TOK" };
static _i32         defaultStore_integers[DEFAULTSTORE_MAX_IDS][DEFAULTSTORE_MAX_SUBIDS];
static _s8          defaultStore_booleans[DEFAULTSTORE_MAX_IDS][DEFAULTSTORE_MAX_SUBIDS/8];
static _s8  *       defaultStore_strings[DEFAULTSTORE_MAX_IDS][DEFAULTSTORE_MAX_SUBIDS];
static void *       defaultStore_voids[DEFAULTSTORE_MAX_IDS][DEFAULTSTORE_MAX_SUBIDS];



/**
 * Stores "true" or "false" given an _i32 value for value into
 * defaultStore_booleans[store][which] slot.
 *
 * @param storeid an index to the boolean storage container's first index(store)
 *
 * @param which an index to the boolean storage container's second index(which)
 *
 * @param value if > 0, "true" is set into the slot otherwise "false"
 *
 * @return Returns SNMPPERR_GENERR if the storeid and which parameters do not
 * correspond to a valid slot, or  API_ERR_SUCCESS otherwise.
 */
static _bool DefaultStore_isParamsOk(_i32 storeid, _i32 which){
    if (storeid < 0 || storeid >= DEFAULTSTORE_MAX_IDS ||
            which   < 0 || which   >= DEFAULTSTORE_MAX_SUBIDS) {
        return false;
    }
    return true;
}

_i32 DefaultStore_setBoolean(_i32 storeid, _i32 which, _i32 value)
{
    if(!DefaultStore_isParamsOk(storeid, which)){
        return API_ERR_GENERR;
    }

    DEBUG_MSGTL(("DefaultStore_setBoolean", "Setting %s:%d = %d/%s\n",
                defaultStore_stores[storeid], which, value, ((value) ? "True" : "False")));

    if (value > 0) {
        defaultStore_booleans[storeid][which/8] |= (1 << (which % 8));
    } else {
        defaultStore_booleans[storeid][which/8] &= (0xff7f >> (7 - (which % 8)));
    }

    return API_ERR_SUCCESS;
}

_i32 DefaultStore_toggleBoolean(_i32 storeid, _i32 which)
{
    if(!DefaultStore_isParamsOk(storeid, which)){
        return API_ERR_GENERR;
    }

    if ((defaultStore_booleans[storeid][which/8] & (1 << (which % 8))) == 0) {
        defaultStore_booleans[storeid][which/8] |= (1 << (which % 8));
    } else {
        defaultStore_booleans[storeid][which/8] &= (0xff7f >> (7 - (which % 8)));
    }

    DEBUG_MSGTL(("DefaultStore_toggleBoolean", "Setting %s:%d = %d/%s\n",
                defaultStore_stores[storeid], which, defaultStore_booleans[storeid][which/8],
               ((defaultStore_booleans[storeid][which/8]) ? "True" : "False")));

    return API_ERR_SUCCESS;
}

_i32 DefaultStore_getBoolean(_i32 storeid, _i32 which)
{
    if(!DefaultStore_isParamsOk(storeid, which)){
        return API_ERR_GENERR;
    }

    return (defaultStore_booleans[storeid][which/8] & (1 << (which % 8))) ? 1:0;
}

_i32 DefaultStore_setInt(_i32 storeid, _i32 which, _i32 value)
{
    if(!DefaultStore_isParamsOk(storeid, which)){
        return API_ERR_GENERR;
    }

    DEBUG_MSGTL(("DefaultStore_setInt", "Setting %s:%d = %d\n",
                defaultStore_stores[storeid], which, value));

    defaultStore_integers[storeid][which] = value;
    return API_ERR_SUCCESS;
}

_i32 DefaultStore_getInt(_i32 storeid, _i32 which)
{
    if(!DefaultStore_isParamsOk(storeid, which)){
        return API_ERR_GENERR;
    }

    return defaultStore_integers[storeid][which];
}

_i32 DefaultStore_setString(_i32 storeid, _i32 which,  const _s8 *value)
{
    if(!DefaultStore_isParamsOk(storeid, which)){
        return API_ERR_GENERR;
    }

    DEBUG_MSGTL(("DefaultStore_setString", "Setting %s:%d = \"%s\"\n",
                defaultStore_stores[storeid], which, (value ? value : "(null)")));

    /*
     * is some silly person is calling us with our own pointer?
     */
    if (defaultStore_strings[storeid][which] == value)
        return API_ERR_SUCCESS;

    if (defaultStore_strings[storeid][which] != NULL) {
        free(defaultStore_strings[storeid][which]);
        defaultStore_strings[storeid][which] = NULL;
    }

    if (value) {
        defaultStore_strings[storeid][which] = strdup(value);
    } else {
        defaultStore_strings[storeid][which] = NULL;
    }

    return API_ERR_SUCCESS;
}

_s8 * DefaultStore_getString(_i32 storeid, _i32 which)
{
    if(!DefaultStore_isParamsOk(storeid, which)){
        return NULL;
    }

    return defaultStore_strings[storeid][which];
}

_i32 DefaultStore_setVoid(_i32 storeid, _i32 which, void *value)
{
    if(!DefaultStore_isParamsOk(storeid, which)){
        return API_ERR_GENERR;
    }

    DEBUG_MSGTL(("DefaultStore_setVoid", "Setting %s:%d = %p\n",
                defaultStore_stores[storeid], which, value));

    defaultStore_voids[storeid][which] = value;

    return API_ERR_SUCCESS;
}

void * DefaultStore_getVoid(_i32 storeid, _i32 which)
{
    if(!DefaultStore_isParamsOk(storeid, which)){
        return NULL;
    }

    return defaultStore_voids[storeid][which];
}


_i32 DefaultStore_parseBoolean(_s8 * line)
{
    char           *value, *endptr;
    int             itmp;
    char           *st;

    value = strtok_r(line, " \t\n", &st);
    if (strcasecmp(value, "yes") == 0 ||
            strcasecmp(value, "true") == 0) {
        return 1;
    } else if (strcasecmp(value, "no") == 0 ||
               strcasecmp(value, "false") == 0) {
        return 0;
    } else {
        itmp = strtol(value, &endptr, 10);
        if (*endptr != 0 || itmp < 0 || itmp > 1) {
            ReadConfig_configPerror("Should be yes|no|true|false|0|1");
            return -1;
        }
        return itmp;
    }
}

void DefaultStore_handleConfig(const _s8 *token, _s8 *line)
{
    DefaultStore_ReadConfig *drsp;
    _s8            buf[TOOLS_MAXBUF];
    _s8           *value, *endptr;
    _i32          itmp;
    _s8           *st;

    DEBUG_MSGTL(("DefaultStore_handleConfig", "handling %s\n", token));

    for (drsp = defaultStore_readConfig;
         drsp != NULL && strcasecmp(token, drsp->token) != 0;
         drsp = drsp->next);

    if (drsp != NULL) {
        DEBUG_MSGTL(("DefaultStore_handleConfig",
                    "setting: token=%s, type=%d, id=%s, which=%d\n",
                    drsp->token, drsp->type, defaultStore_stores[drsp->storeid],
                   drsp->which));

        switch (drsp->type) {
        case DATATYPE_BOOLEAN:
            itmp = DefaultStore_parseBoolean(line);
            if ( itmp != -1 )
                DefaultStore_setBoolean(drsp->storeid, drsp->which, itmp);
            DEBUG_MSGTL(("DefaultStore_handleConfig", "bool: %d\n", itmp));
            break;

        case DATATYPE_INTEGER:
            value = strtok_r(line, " \t\n", &st);
            itmp = strtol(value, &endptr, 10);
            if (*endptr != 0) {
                ReadConfig_configPerror("Bad integer value");
            } else {
                DefaultStore_setInt(drsp->storeid, drsp->which, itmp);
            }
            DEBUG_MSGTL(("DefaultStore_handleConfig", "int: %d\n", itmp));
            break;

        case DATATYPE_OCTETSTRING:
            if (*line == '"') {
                ReadConfig_copyNword(line, buf, sizeof(buf));
                DefaultStore_setString(drsp->storeid, drsp->which, buf);
            } else {
                DefaultStore_setString(drsp->storeid, drsp->which, line);
            }
            DEBUG_MSGTL(("DefaultStore_handleConfig", "string: %s\n", line));
            break;

        default:
            Logger_log(LOGGER_PRIORITY_ERR, "DefaultStore_handleConfig: type %d (0x%02x)\n", drsp->type, drsp->type);
            break;
        }
    } else {
        Logger_log(LOGGER_PRIORITY_ERR, "DefaultStore_handleConfig: no registration for %s\n", token);
    }
}


_i32    DefaultStore_registerConfig(_u8 type, const char * storename,  const char * token,  _i32 storeid, _i32 which)
{
    DefaultStore_ReadConfig *drsp;

    if(!DefaultStore_isParamsOk(storeid, which)){
        return API_ERR_GENERR;
    }

    if (defaultStore_readConfig == NULL) {
        defaultStore_readConfig = TOOLS_MALLOC_TYPEDEF(DefaultStore_ReadConfig);
        if (defaultStore_readConfig == NULL)
            return API_ERR_GENERR;
        drsp = defaultStore_readConfig;
    } else {
        for (drsp = defaultStore_readConfig; drsp->next != NULL; drsp = drsp->next);
        drsp->next = TOOLS_MALLOC_TYPEDEF(DefaultStore_ReadConfig);
        if (drsp->next == NULL)
            return API_ERR_GENERR;
        drsp = drsp->next;
    }

    drsp->type    = type;
    drsp->storename = strdup(storename);
    drsp->token   = strdup(token);
    drsp->storeid = storeid;
    drsp->which   = which;

    switch (type) {
    case DATATYPE_BOOLEAN:
        ReadConfig_registerConfigHandler(storename, token, DefaultStore_handleConfig, NULL,
                                "(1|yes|true|0|no|false)");
        break;

    case DATATYPE_INTEGER:
        ReadConfig_registerConfigHandler(storename, token, DefaultStore_handleConfig, NULL,
                                "integerValue");
        break;

    case DATATYPE_OCTETSTRING:
        ReadConfig_registerConfigHandler(storename, token, DefaultStore_handleConfig, NULL,
                                "string");
        break;

    }
    return API_ERR_SUCCESS;
}

_i32        DefaultStore_registerPremib(_u8 type, const char * storename,  const char * token, _i32 storeid, _i32 which)
{
    DefaultStore_ReadConfig *drsp;

    if(!DefaultStore_isParamsOk(storeid, which)){
        return API_ERR_GENERR;
    }


    if (defaultStore_readConfig == NULL) {
        defaultStore_readConfig = TOOLS_MALLOC_TYPEDEF(DefaultStore_ReadConfig);
        if (defaultStore_readConfig == NULL)
            return API_ERR_GENERR;
        drsp = defaultStore_readConfig;
    } else {
        for (drsp = defaultStore_readConfig; drsp->next != NULL; drsp = drsp->next);
        drsp->next = TOOLS_MALLOC_TYPEDEF(DefaultStore_ReadConfig);
        if (drsp->next == NULL)
            return API_ERR_GENERR;
        drsp = drsp->next;
    }

    drsp->type    = type;
    drsp->storename   = strdup(storename);
    drsp->token   = strdup(token);
    drsp->storeid = storeid;
    drsp->which   = which;

    switch (type) {
    case DATATYPE_BOOLEAN:
        ReadConfig_registerConfigHandler(storename, token, DefaultStore_handleConfig, NULL,
                                "(1|yes|true|0|no|false)");
        break;

    case DATATYPE_INTEGER:
        ReadConfig_registerConfigHandler(storename, token, DefaultStore_handleConfig, NULL,
                                "integerValue");
        break;

    case DATATYPE_OCTETSTRING:
        ReadConfig_registerConfigHandler(storename, token, DefaultStore_handleConfig, NULL,
                                "string");
        break;

    }
    return API_ERR_SUCCESS;
}

void DefaultStore_shutdown(void)
{
    DefaultStore_ReadConfig *drsp;
    int             i, j;

    for (drsp = defaultStore_readConfig; drsp; drsp = defaultStore_readConfig) {
        defaultStore_readConfig = drsp->next;

        if (drsp->storename && drsp->token) {
            ReadConfig_unregisterConfigHandler(drsp->storename, drsp->token);
        }
        if (drsp->storename != NULL) {
            free(drsp->storename);
        }
        if (drsp->token != NULL) {
            free(drsp->token);
        }
        free(drsp);
    }

    for (i = 0; i < DEFAULTSTORE_MAX_IDS; i++) {
        for (j = 0; j < DEFAULTSTORE_MAX_SUBIDS; j++) {
            if (defaultStore_strings[i][j] != NULL) {
                free(defaultStore_strings[i][j]);
                defaultStore_strings[i][j] = NULL;
            }
        }
    }
}
