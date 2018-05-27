#include "System/Util/DefaultStore.h"
#include "Api.h"
#include "ReadConfig.h"
#include "System/String.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"
#include <string.h>

/** ============================[ Types ]================== */

typedef struct DefaultStoreConfig_s {
    unsigned char asnType;
    char* token;
    char* storeName;
    int storeId;
    int registryId;
} DefaultStoreConfig;

/** ============================[ Private Variables ]================== */

/**
 * @brief _defaultStore_configs
 *        config list.
 */
static List* _defaultStore_configs = NULL;

/**
 * @brief _defaultStore_stores - the vector of storage names
 */
static const char* _defaultStore_stores[ DEFAULTSTORE_MAX_IDS ] = { "LIB", "APP", "TOK" };

/**
 * @brief _defaultStore_integers - storage container of integer
 */
static int _defaultStore_integers[ DEFAULTSTORE_MAX_IDS ][ DEFAULTSTORE_MAX_SUBIDS ];

/**
 * @brief _defaultStore_booleans - storage container of booleans
 */
static char _defaultStore_booleans[ DEFAULTSTORE_MAX_IDS ][ DEFAULTSTORE_MAX_SUBIDS / 8 ];

/**
 * @brief _defaultStore_strings - storage container of string
 */
static char* _defaultStore_strings[ DEFAULTSTORE_MAX_IDS ][ DEFAULTSTORE_MAX_SUBIDS ];

/**
 * @brief _defaultStore_voids - storage container of voids
 */
static void* _defaultStore_voids[ DEFAULTSTORE_MAX_IDS ][ DEFAULTSTORE_MAX_SUBIDS ];

/** ==================[ Private Functions Prototypes ]================== */

/** @brief _DefaultStore_isParamsOk
 *         checks whether the record in a given index (storeId, registryId) is valid or not:
 *         defaultStore_booleans[storeId][registryId].
 *
 * @param storeId - an index to storage container's first index(storeId)
 * @param registryId - an index to storage container's second index(registryId)
 *
 * @retval 0 : record is invalid
 * @retval 1 : record is valid
 */
static bool _DefaultStore_isParamsOk( int storeId, int registryId );

/**
 * @brief _DefaultStore_parseBoolean
 *        converts string to boolean.
 *
 * @param line - the string
 * @return a boolean value
 */
static int _DefaultStore_parseBoolean( char* line );

/**
 * @brief _DefaultStore_handlerConfig
 *        The parser. The handler function pointer that use  the specified
 *        token and the rest of the line to do whatever is required.
 *
 * @param token - the token
 * @param line - the rest of line.
 */
static void _DefaultStore_handlerConfig( const char* token, char* line );

/**
 * @brief _List_freeFunction
 *        is the function to release the List->data.
 *        Must know how to free List->data.
 *
 * @param data -  the data
 */
static void _List_freeFunction( void* data );

/** =============================[ Public Functions ]================== */

int DefaultStore_setBoolean( int storeId, int registryId, int value )
{
    if ( !_DefaultStore_isParamsOk( storeId, registryId ) ) {
        return ErrorCode_GENERR;
    }

    DEBUG_MSGTL( ( "DefaultStore_setBoolean", "Setting %s:%d = %d/%s\n",
        _defaultStore_stores[ storeId ], registryId, value, ( ( value ) ? "True" : "False" ) ) );

    if ( value > 0 ) {
        _defaultStore_booleans[ storeId ][ registryId / 8 ] |= ( 1 << ( registryId % 8 ) );
    } else {
        _defaultStore_booleans[ storeId ][ registryId / 8 ] &= ( 0xff7f >> ( 7 - ( registryId % 8 ) ) );
    }

    return ErrorCode_SUCCESS;
}

int DefaultStore_toggleBoolean( int storeId, int registryId )
{
    if ( !_DefaultStore_isParamsOk( storeId, registryId ) ) {
        return ErrorCode_GENERR;
    }

    if ( ( _defaultStore_booleans[ storeId ][ registryId / 8 ] & ( 1 << ( registryId % 8 ) ) ) == 0 ) {
        _defaultStore_booleans[ storeId ][ registryId / 8 ] |= ( 1 << ( registryId % 8 ) );
    } else {
        _defaultStore_booleans[ storeId ][ registryId / 8 ] &= ( 0xff7f >> ( 7 - ( registryId % 8 ) ) );
    }

    DEBUG_MSGTL( ( "DefaultStore_toggleBoolean", "Setting %s:%d = %d/%s\n",
        _defaultStore_stores[ storeId ], registryId, _defaultStore_booleans[ storeId ][ registryId / 8 ],
        ( ( _defaultStore_booleans[ storeId ][ registryId / 8 ] ) ? "True" : "False" ) ) );

    return ErrorCode_SUCCESS;
}

int DefaultStore_getBoolean( int storeId, int registryId )
{
    if ( !_DefaultStore_isParamsOk( storeId, registryId ) ) {
        return ErrorCode_GENERR;
    }

    return ( _defaultStore_booleans[ storeId ][ registryId / 8 ] & ( 1 << ( registryId % 8 ) ) ) ? 1 : 0;
}

int DefaultStore_setInt( int storeId, int registryId, int value )
{
    if ( !_DefaultStore_isParamsOk( storeId, registryId ) ) {
        return ErrorCode_GENERR;
    }

    DEBUG_MSGTL( ( "DefaultStore_setInt", "Setting %s:%d = %d\n",
        _defaultStore_stores[ storeId ], registryId, value ) );

    _defaultStore_integers[ storeId ][ registryId ] = value;
    return ErrorCode_SUCCESS;
}

int DefaultStore_getInt( int storeId, int registryId )
{
    if ( !_DefaultStore_isParamsOk( storeId, registryId ) ) {
        return ErrorCode_GENERR;
    }

    return _defaultStore_integers[ storeId ][ registryId ];
}

int DefaultStore_setString( int storeId, int registryId, const char* value )
{
    if ( !_DefaultStore_isParamsOk( storeId, registryId ) ) {
        return ErrorCode_GENERR;
    }

    DEBUG_MSGTL( ( "DefaultStore_setString", "Setting %s:%d = \"%s\"\n",
        _defaultStore_stores[ storeId ], registryId, ( value ? value : "(null)" ) ) );

    /*
     * is some silly person is calling us with our own pointer?
     */
    if ( _defaultStore_strings[ storeId ][ registryId ] == value )
        return ErrorCode_SUCCESS;

    if ( _defaultStore_strings[ storeId ][ registryId ] != NULL ) {
        free( _defaultStore_strings[ storeId ][ registryId ] );
        _defaultStore_strings[ storeId ][ registryId ] = NULL;
    }

    if ( value ) {
        _defaultStore_strings[ storeId ][ registryId ] = strdup( value );
    } else {
        _defaultStore_strings[ storeId ][ registryId ] = NULL;
    }

    return ErrorCode_SUCCESS;
}

char* DefaultStore_getString( int storeId, int registryId )
{
    if ( !_DefaultStore_isParamsOk( storeId, registryId ) ) {
        return NULL;
    }

    return _defaultStore_strings[ storeId ][ registryId ];
}

int DefaultStore_setVoid( int storeId, int registryId, void* value )
{
    if ( !_DefaultStore_isParamsOk( storeId, registryId ) ) {
        return ErrorCode_GENERR;
    }

    DEBUG_MSGTL( ( "DefaultStore_setVoid", "Setting %s:%d = %p\n",
        _defaultStore_stores[ storeId ], registryId, value ) );

    _defaultStore_voids[ storeId ][ registryId ] = value;

    return ErrorCode_SUCCESS;
}

void* DefaultStore_getVoid( int storeId, int registryId )
{
    if ( !_DefaultStore_isParamsOk( storeId, registryId ) ) {
        return NULL;
    }

    return _defaultStore_voids[ storeId ][ registryId ];
}

int DefaultStore_registerConfig( unsigned char asnType, const char* storeName, const char* token, int storeId, int registryId )
{
    DefaultStoreConfig* newElement = NULL;

    if ( !_DefaultStore_isParamsOk( storeId, registryId ) ) {
        return ErrorCode_GENERR;
    }

    newElement = MEMORY_MALLOC_TYPEDEF( DefaultStoreConfig );

    if ( newElement == NULL ) {
        return ErrorCode_GENERR;
    }

    newElement->asnType = asnType;
    newElement->storeName = String_new( storeName );
    newElement->token = String_new( token );
    newElement->storeId = storeId;
    newElement->registryId = registryId;

    List_emplace( &_defaultStore_configs, newElement, _List_freeFunction );

    switch ( asnType ) {
    case DATATYPE_BOOLEAN:
        ReadConfig_registerConfigHandler( storeName, token, _DefaultStore_handlerConfig, NULL,
            "(1|yes|true|0|no|false)" );
        break;

    case DATATYPE_INTEGER:
        ReadConfig_registerConfigHandler( storeName, token, _DefaultStore_handlerConfig, NULL,
            "integerValue" );
        break;

    case DATATYPE_OCTETSTRING:
        ReadConfig_registerConfigHandler( storeName, token, _DefaultStore_handlerConfig, NULL,
            "string" );
        break;
    }
    return ErrorCode_SUCCESS;
}

int DefaultStore_registerPremib( unsigned char asnType, const char* storeName, const char* token, int storeId, int registryId )
{
    return DefaultStore_registerConfig( asnType, storeName, token, storeId, registryId );
}

void DefaultStore_clear( void )
{
    int i, j;

    List_clear( _defaultStore_configs );

    for ( i = 0; i < DEFAULTSTORE_MAX_IDS; i++ ) {
        for ( j = 0; j < DEFAULTSTORE_MAX_SUBIDS; j++ ) {
            if ( _defaultStore_strings[ i ][ j ] != NULL ) {
                Memory_free( _defaultStore_strings[ i ][ j ] );
                _defaultStore_strings[ i ][ j ] = NULL;
            }
        }
    }
}

/** =============================[ Private Functions ]================== */

static bool _DefaultStore_isParamsOk( int storeId, int registryId )
{
    if ( storeId < 0 || storeId >= DEFAULTSTORE_MAX_IDS || registryId < 0 || registryId >= DEFAULTSTORE_MAX_SUBIDS ) {
        return 0;
    }
    return 1;
}

static int _DefaultStore_parseBoolean( char* line )
{
    char *value, *endptr;
    int itmp;
    char* st;

    value = strtok_r( line, " \t\n", &st );
    if ( String_equalsIgnoreCase( value, "yes" ) || String_equalsIgnoreCase( value, "true" ) ) {
        return 1;
    } else if ( String_equalsIgnoreCase( value, "no" ) || String_equalsIgnoreCase( value, "false" ) ) {
        return 0;
    } else {
        itmp = strtol( value, &endptr, 10 );
        if ( *endptr != 0 || itmp < 0 || itmp > 1 ) {
            ReadConfig_configPerror( "Should be yes|no|true|false|0|1" );
            return -1;
        }
        return itmp;
    }
}

static void _DefaultStore_handlerConfig( const char* token, char* line )
{
    List* list;
    DefaultStoreConfig* element;
    char buf[ UTILITIES_MAX_BUFFER ];
    char *value, *endptr;
    int itmp;
    char* st;

    DEBUG_MSGTL( ( "DefaultStore_handleConfig", "handling %s\n", token ) );

    /** find */
    for ( list = _defaultStore_configs; list != NULL; list = list->next ) {
        element = list->data;
        if ( element && String_equals( token, element->token ) ) {
            break;
        }
    }

    if ( list != NULL ) {
        DEBUG_MSGTL( ( "DefaultStore_handleConfig", "setting: token=%s, type=%d, id=%s, registryId=%d\n",
            element->token, element->asnType, _defaultStore_stores[ element->storeId ], element->registryId ) );

        switch ( element->asnType ) {
        case DATATYPE_BOOLEAN:
            itmp = _DefaultStore_parseBoolean( line );
            if ( itmp != -1 )
                DefaultStore_setBoolean( element->storeId, element->registryId, itmp );
            DEBUG_MSGTL( ( "DefaultStore_handleConfig", "bool: %d\n", itmp ) );
            break;

        case DATATYPE_INTEGER:
            value = strtok_r( line, " \t\n", &st );
            itmp = strtol( value, &endptr, 10 );
            if ( *endptr != 0 ) {
                ReadConfig_configPerror( "Bad integer value" );
            } else {
                DefaultStore_setInt( element->storeId, element->registryId, itmp );
            }
            DEBUG_MSGTL( ( "DefaultStore_handleConfig", "int: %d\n", itmp ) );
            break;

        case DATATYPE_OCTETSTRING:
            if ( *line == '"' ) {
                ReadConfig_copyNword( line, buf, sizeof( buf ) );
                DefaultStore_setString( element->storeId, element->registryId, buf );
            } else {
                DefaultStore_setString( element->storeId, element->registryId, line );
            }
            DEBUG_MSGTL( ( "DefaultStore_handleConfig", "string: %s\n", line ) );
            break;

        default:
            Logger_log( LOGGER_PRIORITY_ERR, "DefaultStore_handleConfig: type %d (0x%02x)\n", element->asnType, element->asnType );
            break;
        }
    } else {
        Logger_log( LOGGER_PRIORITY_ERR, "DefaultStore_handleConfig: no registration for %s\n", token );
    }
}

/** =============================[ Private Functions ]================== */

static void _List_freeFunction( void* data )
{
    DefaultStoreConfig* element = data;
    if ( element->storeName && element->token ) {
        ReadConfig_unregisterConfigHandler( element->storeName, element->token );
    }
    Memory_free( element->storeName );
    Memory_free( element->token );
    Memory_free( element );
}
