#include "Trace.h"
#include "Impl.h"
#include "Mib.h"
#include "ReadConfig.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"

#define DEBUG_DISABLED 0
#define DEBUG_ACTIVE 1
#define DEBUG_EXCLUDED 2

static int debug_doDebug = DEBUG_ALWAYS_DEBUG;
int debug_numTokens = 0;
static int debug_printEverything = 0;

Debug_tokenDescr debug_tokens[ DEBUG_MAX_TOKENS ];

/*
 * Number of spaces to indent debug outpur. Valid range is [0,INT_MAX]
 */
static int debug_indent = 0;

int Debug_indentGet( void )
{
    return debug_indent;
}

void Debug_indentAdd( int amount )
{
    if ( -debug_indent <= amount && amount <= INT_MAX - debug_indent )
        debug_indent += amount;
}

void Debug_configRegisterTokens( const char* configtoken, char* tokens )
{

    Debug_registerTokens( tokens );
}

void Debug_configTurnOnDebugging( const char* configtoken, char* line )
{
    Debug_setDoDebugging( atoi( line ) );
}

void Debug_registerTokens( const char* tokens )
{
    char *newp, *cp;
    char* st = NULL;
    int status;

    if ( tokens == NULL || *tokens == 0 )
        return;

    newp = strdup( tokens ); /* strtok_r messes it up */
    cp = strtok_r( newp, DEBUG_TOKEN_DELIMITER, &st );
    while ( cp ) {
        if ( strlen( cp ) < DEBUG_MAX_TOKEN_LEN ) {
            if ( strcasecmp( cp, DEBUG_ALWAYS_TOKEN ) == 0 ) {
                debug_printEverything = 1;
            } else if ( debug_numTokens < DEBUG_MAX_TOKENS ) {
                if ( '-' == *cp ) {
                    ++cp;
                    status = DEBUG_EXCLUDED;
                } else
                    status = DEBUG_ACTIVE;
                debug_tokens[ debug_numTokens ].tokenName = strdup( cp );
                debug_tokens[ debug_numTokens++ ].enabled = status;
                Logger_log( LOGGER_PRIORITY_NOTICE, "registered debug token %s, %d\n", cp, status );
            } else {
                Logger_log( LOGGER_PRIORITY_NOTICE, "Unable to register debug token %s\n", cp );
            }
        } else {
            Logger_log( LOGGER_PRIORITY_NOTICE, "Debug token %s over length\n", cp );
        }
        cp = strtok_r( NULL, DEBUG_TOKEN_DELIMITER, &st );
    }
    free( newp );
}

/*
 * Print all registered tokens along with their current status
 */
void Debug_printRegisteredTokens( void )
{
    int i;

    Logger_log( LOGGER_PRIORITY_INFO, "%d tokens registered :\n", debug_numTokens );
    for ( i = 0; i < debug_numTokens; i++ ) {
        Logger_log( LOGGER_PRIORITY_INFO, "%d) %s : %d\n", i, debug_tokens[ i ].tokenName, debug_tokens[ i ].enabled );
    }
}

/*
 * Enable logs on a given token
 */
int Debug_enableTokenLogs( const char* token )
{
    int i;

    /* debugging flag is on or off */
    if ( !debug_doDebug )
        return ErrorCode_GENERR;

    if ( debug_numTokens == 0 || debug_printEverything ) {
        /* no tokens specified, print everything */
        return ErrorCode_SUCCESS;
    } else {
        for ( i = 0; i < debug_numTokens; i++ ) {
            if ( debug_tokens[ i ].tokenName && strncmp( debug_tokens[ i ].tokenName, token, strlen( debug_tokens[ i ].tokenName ) ) == 0 ) {
                debug_tokens[ i ].enabled = DEBUG_ACTIVE;
                return ErrorCode_SUCCESS;
            }
        }
    }
    return ErrorCode_GENERR;
}

/*
 * Diable logs on a given token
 */
int Debug_disableTokenLogs( const char* token )
{
    int i;

    /* debugging flag is on or off */
    if ( !debug_doDebug )
        return ErrorCode_GENERR;

    if ( debug_numTokens == 0 || debug_printEverything ) {
        /* no tokens specified, print everything */
        return ErrorCode_SUCCESS;
    } else {
        for ( i = 0; i < debug_numTokens; i++ ) {
            if ( strncmp( debug_tokens[ i ].tokenName, token,
                     strlen( debug_tokens[ i ].tokenName ) )
                == 0 ) {
                debug_tokens[ i ].enabled = DEBUG_DISABLED;
                return ErrorCode_SUCCESS;
            }
        }
    }
    return ErrorCode_GENERR;
}

/*
 * debug_is_token_registered(char *TOKEN):
 *
 * returns ErrorCode_SUCCESS
 * or ErrorCode_GENERR
 *
 * if TOKEN has been registered and debugging support is turned on.
 */
int Debug_isTokenRegistered( const char* token )
{
    int i, rc;

    /*
     * debugging flag is on or off
     */
    if ( !debug_doDebug )
        return ErrorCode_GENERR;

    if ( debug_numTokens == 0 || debug_printEverything ) {
        /*
         * no tokens specified, print everything
         */
        return ErrorCode_SUCCESS;
    } else
        rc = ErrorCode_GENERR; /* ! found = err */

    for ( i = 0; i < debug_numTokens; i++ ) {
        if ( DEBUG_DISABLED == debug_tokens[ i ].enabled )
            continue;
        if ( debug_tokens[ i ].tokenName && strncmp( debug_tokens[ i ].tokenName, token, strlen( debug_tokens[ i ].tokenName ) ) == 0 ) {
            if ( DEBUG_ACTIVE == debug_tokens[ i ].enabled )
                return ErrorCode_SUCCESS; /* active */
            else
                return ErrorCode_GENERR; /* excluded */
        }
    }
    return rc;
}

void Debug_msg( const char* token, const char* format, ... )
{
    if ( Debug_isTokenRegistered( token ) == ErrorCode_SUCCESS ) {
        va_list debugargs;

        va_start( debugargs, format );
        Logger_vlog( LOGGER_PRIORITY_DEBUG, format, debugargs );
        va_end( debugargs );
    }
}

void Debug_msgOid( const char* token, const oid* theoid, size_t len )
{
    u_char* buf = NULL;
    size_t buf_len = 0, out_len = 0;

    if ( Mib_sprintReallocObjid2( &buf, &buf_len, &out_len, 1, theoid, len ) ) {
        if ( buf != NULL ) {
            Debug_msg( token, "%s", buf );
        }
    } else {
        if ( buf != NULL ) {
            Debug_msg( token, "%s [TRUNCATED]", buf );
        }
    }

    if ( buf != NULL ) {
        free( buf );
    }
}

void Debug_msgSuboid( const char* token, const oid* theoid, size_t len )
{
    u_char* buf = NULL;
    size_t buf_len = 0, out_len = 0;
    int buf_overflow = 0;

    Mib_sprintReallocObjid( &buf, &buf_len, &out_len, 1,
        &buf_overflow, theoid, len );
    if ( buf_overflow ) {
        if ( buf != NULL ) {
            Debug_msg( token, "%s [TRUNCATED]", buf );
        }
    } else {
        if ( buf != NULL ) {
            Debug_msg( token, "%s", buf );
        }
    }

    if ( buf != NULL ) {
        free( buf );
    }
}

void Debug_msgVar( const char* token, VariableList* var )
{
    u_char* buf = NULL;
    size_t buf_len = 0, out_len = 0;

    if ( var == NULL || token == NULL ) {
        return;
    }

    if ( Mib_sprintReallocVariable( &buf, &buf_len, &out_len, 1,
             var->name, var->nameLength, var ) ) {
        if ( buf != NULL ) {
            Debug_msg( token, "%s", buf );
        }
    } else {
        if ( buf != NULL ) {
            Debug_msg( token, "%s [TRUNCATED]", buf );
        }
    }

    if ( buf != NULL ) {
        free( buf );
    }
}

void Debug_msgOidRange( const char* token, const oid* theoid, size_t len,
    size_t var_subid, oid range_ubound )
{
    u_char* buf = NULL;
    size_t buf_len = 0, out_len = 0, i = 0;
    int rc = 0;

    if ( var_subid == 0 ) {
        rc = Mib_sprintReallocObjid2( &buf, &buf_len, &out_len, 1, theoid,
            len );
    } else {
        char tmpbuf[ 128 ];
        /* XXX - ? check for 0 == var_subid -1 ? */
        rc = Mib_sprintReallocObjid2( &buf, &buf_len, &out_len, 1, theoid,
            var_subid - 1 ); /* Adjust for C's 0-based array indexing */
        if ( rc ) {
            sprintf( tmpbuf, ".%lu--%lu", theoid[ var_subid - 1 ], range_ubound );
            rc = String_appendRealloc( &buf, &buf_len, &out_len, 1, ( const u_char* )tmpbuf );
            if ( rc ) {
                for ( i = var_subid; i < len; i++ ) {
                    sprintf( tmpbuf, ".%lu", theoid[ i ] );
                    if ( !String_appendRealloc( &buf, &buf_len, &out_len, 1, ( const u_char* )tmpbuf ) ) {
                        break;
                    }
                }
            }
        }
    }

    if ( buf != NULL ) {
        Debug_msg( token, "%s%s", buf, rc ? "" : " [TRUNCATED]" );
        free( buf );
    }
}

void Debug_msgHex( const char* token, const u_char* thedata, size_t len )
{
    u_char* buf = NULL;
    size_t buf_len = 0, out_len = 0;

    if ( Mib_sprintReallocHexString( &buf, &buf_len, &out_len, 1, thedata, len ) ) {
        if ( buf != NULL ) {
            Debug_msg( token, "%s", buf );
        }
    } else {
        if ( buf != NULL ) {
            Debug_msg( token, "%s [TRUNCATED]", buf );
        }
    }

    if ( buf != NULL ) {
        free( buf );
    }
}

void Debug_msgHextli( const char* token, const u_char* thedata, size_t len )
{
    char buf[ IMPL_SPRINT_MAX_LEN ], token2[ IMPL_SPRINT_MAX_LEN ];
    u_char* b3 = NULL;
    size_t b3_len = 0, o3_len = 0;
    int incr;
    sprintf( token2, "dumpx_%s", token );

    /*
     * XX tracing lines removed from this function DEBUG_TRACE;
     */
    DEBUG_IF( token2 )
    {
        for ( incr = 16; len > 0; len -= incr, thedata += incr ) {
            if ( ( int )len < incr ) {
                incr = len;
            }
            /*
             * XXnext two lines were DEBUGPRINTINDENT(token);
             */
            sprintf( buf, "dumpx%s", token );
            Debug_msg( buf, "%s: %*s", token2, Debug_indentGet(), "" );
            if ( Mib_sprintReallocHexString( &b3, &b3_len, &o3_len, 1, thedata, incr ) ) {
                if ( b3 != NULL ) {
                    Debug_msg( token2, "%s", b3 );
                }
            } else {
                if ( b3 != NULL ) {
                    Debug_msg( token2, "%s [TRUNCATED]", b3 );
                }
            }
            o3_len = 0;
        }
    }
    if ( b3 != NULL ) {
        free( b3 );
    }
}

void Debug_msgToken( const char* token, const char* format, ... )
{
    va_list debugargs;

    va_start( debugargs, format );
    Debug_msg( token, "%s: ", token );
    va_end( debugargs );
}

void Debug_comboNc( const char* token, const char* format, ... )
{
    va_list debugargs;

    va_start( debugargs, format );
    Logger_log( LOGGER_PRIORITY_DEBUG, "%s: ", token );
    Logger_vlog( LOGGER_PRIORITY_DEBUG, format, debugargs );
    va_end( debugargs );
}

/*
 * for speed, these shouldn't be in default_storage space
 */
void Debug_setDoDebugging( int val )
{
    debug_doDebug = val;
}

int Debug_getDoDebugging( void )
{
    return debug_doDebug;
}

void Debug_debugInit( void )
{
    ReadConfig_registerPrenetMibHandler( "priot", "doDebugging",
        Debug_configTurnOnDebugging, NULL,
        "(1|0)" );
    ReadConfig_registerPrenetMibHandler( "priot", "debugTokens",
        Debug_configRegisterTokens, NULL,
        "token[,token...]" );
}
