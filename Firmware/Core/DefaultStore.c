#include "DefaultStore.h"
#include <string.h>
#include "System/String.h"
#include "System/Numerics/Integer.h"
#include "Debug.h"
#include "Logger.h"
#include "Tools.h"
#include "ReadConfig.h"
#include "Api.h"


typedef struct DefaultStore_ReadConfig_s {
    uchar        type;
    char *       token;
    char *       storename;
    int          storeid;
    int          which;
    struct DefaultStore_ReadConfig_s *next;
} DefaultStore_ReadConfig;



static DefaultStore_ReadConfig * _defaultStore_configs = NULL;

static const char *  _defaultStore_stores [DEFAULTSTORE_MAX_IDS] = { "LIB", "APP", "TOK" };
static int           _defaultStore_integers[DEFAULTSTORE_MAX_IDS][DEFAULTSTORE_MAX_SUBIDS];
static char          _defaultStore_booleans[DEFAULTSTORE_MAX_IDS][DEFAULTSTORE_MAX_SUBIDS/8];
static char *        _defaultStore_strings[DEFAULTSTORE_MAX_IDS][DEFAULTSTORE_MAX_SUBIDS];
static void *        _defaultStore_voids[DEFAULTSTORE_MAX_IDS][DEFAULTSTORE_MAX_SUBIDS];



/**
 * Stores "true" or "false" given an int value for value into
 * defaultStore_booleans[store][which] slot.
 *
 * @param storeid an index to the boolean storage container's first index(store)
 *
 * @param which an index to the boolean storage container's second index(which)
 *
 * @param value if > 0, "true" is set into the slot otherwise "false"
 *
 * @return Returns SNMPPERR_GENERR if the storeid and which parameters do not
 * correspond to a valid slot, or  ErrorCode_SUCCESS otherwise.
 */
static bool _DefaultStore_isParamsOk(int storeid, int which){
    if ( storeid < 0 || storeid >= DEFAULTSTORE_MAX_IDS ||
         which   < 0 || which   >= DEFAULTSTORE_MAX_SUBIDS) {
        return 0;
    }
    return 1;
}

int DefaultStore_setBoolean(int storeid, int which, int value)
{
    if(!_DefaultStore_isParamsOk(storeid, which)){
        return ErrorCode_GENERR;
    }

    DEBUG_MSGTL(("DefaultStore_setBoolean", "Setting %s:%d = %d/%s\n",
                _defaultStore_stores[storeid], which, value, ((value) ? "True" : "False")));

    if (value > 0) {
        _defaultStore_booleans[storeid][which/8] |= (1 << (which % 8));
    } else {
        _defaultStore_booleans[storeid][which/8] &= (0xff7f >> (7 - (which % 8)));
    }

    return ErrorCode_SUCCESS;
}

int DefaultStore_toggleBoolean(int storeid, int which)
{
    if(!_DefaultStore_isParamsOk(storeid, which)){
        return ErrorCode_GENERR;
    }

    if ((_defaultStore_booleans[storeid][which/8] & (1 << (which % 8))) == 0) {
        _defaultStore_booleans[storeid][which/8] |= (1 << (which % 8));
    } else {
        _defaultStore_booleans[storeid][which/8] &= (0xff7f >> (7 - (which % 8)));
    }

    DEBUG_MSGTL(("DefaultStore_toggleBoolean", "Setting %s:%d = %d/%s\n",
                _defaultStore_stores[storeid], which, _defaultStore_booleans[storeid][which/8],
               ((_defaultStore_booleans[storeid][which/8]) ? "True" : "False")));

    return ErrorCode_SUCCESS;
}

int DefaultStore_getBoolean(int storeid, int which)
{
    if(!_DefaultStore_isParamsOk(storeid, which)){
        return ErrorCode_GENERR;
    }

    return (_defaultStore_booleans[storeid][which/8] & (1 << (which % 8))) ? 1:0;
}

int DefaultStore_setInt(int storeid, int which, int value)
{
    if(!_DefaultStore_isParamsOk(storeid, which)){
        return ErrorCode_GENERR;
    }

    DEBUG_MSGTL(("DefaultStore_setInt", "Setting %s:%d = %d\n",
                _defaultStore_stores[storeid], which, value));

    _defaultStore_integers[storeid][which] = value;
    return ErrorCode_SUCCESS;
}

int DefaultStore_getInt(int storeid, int which)
{
    if(!_DefaultStore_isParamsOk(storeid, which)){
        return ErrorCode_GENERR;
    }

    return _defaultStore_integers[storeid][which];
}

int DefaultStore_setString(int storeid, int which,  const char *value)
{
    if(!_DefaultStore_isParamsOk(storeid, which)){
        return ErrorCode_GENERR;
    }

    DEBUG_MSGTL(("DefaultStore_setString", "Setting %s:%d = \"%s\"\n",
                _defaultStore_stores[storeid], which, (value ? value : "(null)")));

    /*
     * is some silly person is calling us with our own pointer?
     */
    if (_defaultStore_strings[storeid][which] == value)
        return ErrorCode_SUCCESS;

    if (_defaultStore_strings[storeid][which] != NULL) {
        free(_defaultStore_strings[storeid][which]);
        _defaultStore_strings[storeid][which] = NULL;
    }

    if (value) {
        _defaultStore_strings[storeid][which] = strdup(value);
    } else {
        _defaultStore_strings[storeid][which] = NULL;
    }

    return ErrorCode_SUCCESS;
}

char * DefaultStore_getString(int storeid, int which)
{
    if(!_DefaultStore_isParamsOk(storeid, which)){
        return NULL;
    }

    return _defaultStore_strings[storeid][which];
}

int DefaultStore_setVoid(int storeid, int which, void *value)
{
    if(!_DefaultStore_isParamsOk(storeid, which)){
        return ErrorCode_GENERR;
    }

    DEBUG_MSGTL(("DefaultStore_setVoid", "Setting %s:%d = %p\n",
                _defaultStore_stores[storeid], which, value));

    _defaultStore_voids[storeid][which] = value;

    return ErrorCode_SUCCESS;
}

void * DefaultStore_getVoid(int storeid, int which)
{
    if(!_DefaultStore_isParamsOk(storeid, which)){
        return NULL;
    }

    return _defaultStore_voids[storeid][which];
}


int DefaultStore_parseBoolean(char * line)
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

void DefaultStore_handleConfig(const char *token, char *line)
{
    DefaultStore_ReadConfig *drsp;
    char            buf[TOOLS_MAXBUF];
    char           *value, *endptr;
    int          itmp;
    char           *st;

    DEBUG_MSGTL(("DefaultStore_handleConfig", "handling %s\n", token));

    for (drsp = _defaultStore_configs;
         drsp != NULL && strcasecmp(token, drsp->token) != 0;
         drsp = drsp->next);

    if (drsp != NULL) {
        DEBUG_MSGTL(("DefaultStore_handleConfig",
                    "setting: token=%s, type=%d, id=%s, which=%d\n",
                    drsp->token, drsp->type, _defaultStore_stores[drsp->storeid],
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


int    DefaultStore_registerConfig(uchar type, const char * storename,  const char * token,  int storeid, int which)
{
    DefaultStore_ReadConfig *drsp;

    if(!_DefaultStore_isParamsOk(storeid, which)){
        return ErrorCode_GENERR;
    }

    if (_defaultStore_configs == NULL) {
        _defaultStore_configs = TOOLS_MALLOC_TYPEDEF(DefaultStore_ReadConfig);
        if (_defaultStore_configs == NULL)
            return ErrorCode_GENERR;
        drsp = _defaultStore_configs;
    } else {
        for (drsp = _defaultStore_configs; drsp->next != NULL; drsp = drsp->next);
        drsp->next = TOOLS_MALLOC_TYPEDEF(DefaultStore_ReadConfig);
        if (drsp->next == NULL)
            return ErrorCode_GENERR;
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
    return ErrorCode_SUCCESS;
}

int        DefaultStore_registerPremib(uchar type, const char * storename,  const char * token, int storeid, int which)
{
    DefaultStore_ReadConfig *drsp;

    if(!_DefaultStore_isParamsOk(storeid, which)){
        return ErrorCode_GENERR;
    }


    if (_defaultStore_configs == NULL) {
        _defaultStore_configs = TOOLS_MALLOC_TYPEDEF(DefaultStore_ReadConfig);
        if (_defaultStore_configs == NULL)
            return ErrorCode_GENERR;
        drsp = _defaultStore_configs;
    } else {
        for (drsp = _defaultStore_configs; drsp->next != NULL; drsp = drsp->next);
        drsp->next = TOOLS_MALLOC_TYPEDEF(DefaultStore_ReadConfig);
        if (drsp->next == NULL)
            return ErrorCode_GENERR;
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
    return ErrorCode_SUCCESS;
}

void DefaultStore_shutdown(void)
{
    DefaultStore_ReadConfig *drsp;
    int             i, j;

    for (drsp = _defaultStore_configs; drsp; drsp = _defaultStore_configs) {
        _defaultStore_configs = drsp->next;

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
            if (_defaultStore_strings[i][j] != NULL) {
                free(_defaultStore_strings[i][j]);
                _defaultStore_strings[i][j] = NULL;
            }
        }
    }
}
