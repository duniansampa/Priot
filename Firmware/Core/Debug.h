#ifndef DEBUG_H
#define DEBUG_H

#include "Api.h"

/*
* Debug.h:
*
* - prototypes for debug routines.
* - easy to use macros to wrap around the functions.  This also provides
* the ability to remove debugging code easily from the applications at
* compile time.
*/

#define DEBUG_MAX_TOKENS 256
#define DEBUG_MAX_TOKEN_LEN 128
#define DEBUG_TOKEN_DELIMITER ","
#define DEBUG_ALWAYS_TOKEN "all"

/*
 * internal:
 * You probably shouldn't be using this information unless the word
 * "expert" applies to you.  I know it looks tempting.
 */
typedef struct Debug_tokenDescr_s {
    char* tokenName;
    char enabled;
} Debug_tokenDescr;

extern int debug_numTokens;
extern Debug_tokenDescr debug_tokens[];

/*
 * These functions should not be used, if at all possible.  Instead, use
 * the macros below.
 */

int Debug_indentGet( void );

void Debug_indentAdd( int amount );

void Debug_configRegisterTokens( const char* configtoken, char* tokens );

void Debug_configTurnOnDebugging( const char* configtoken, char* line );

void Debug_registerTokens( const char* tokens );

void Debug_printRegisteredTokens( void );

int Debug_enableTokenLogs( const char* token );

int Debug_disableTokenLogs( const char* token );

int Debug_isTokenRegistered( const char* token );

void Debug_msg( const char* token, const char* format, ... );

void Debug_msgOid( const char* token, const oid* theoid, size_t len );

void Debug_msgSuboid( const char* token, const oid* theoid, size_t len );

void Debug_msgVar( const char* token, Types_VariableList* var );

void Debug_msgOidRange( const char* token, const oid* theoid, size_t len,
    size_t var_subid, oid range_ubound );

void Debug_msgHex( const char* token, const u_char* thedata, size_t len );

void Debug_msgHextli( const char* token, const u_char* thedata, size_t len );

void Debug_msgToken( const char* token, const char* format, ... );

void Debug_comboNc( const char* token, const char* format, ... );

void Debug_setDoDebugging( int val );

int Debug_getDoDebugging( void );

void Debug_debugInit( void );

/*
 * Use these macros instead of the functions above to allow them to be
 * re-defined at compile time to NOP for speed optimization.
 *
 * They need to be called enclosing all the arguments in a single set of ()s.
 * Example:
 * DEBUG_MSGTL(("token", "debugging of something %s related\n", "priot"));
 *
 * Usage:
 * All of the functions take a "token" argument that helps determine when
 * the output in question should be printed.  See the manual page
 * on the -D flag to turn on/off output for a given token on the command line.
 *
 * DEBUG_MSG((token, format, ...)):      equivalent to printf(format, ...)
 * (if "token" debugging output
 * is requested by the user)
 *
 * DEBUG_MSGT((token, format, ...)):     equivalent to DEBUG_MSG, but prints
 * "token: " at the beginning of the
 * line for you.
 *
 * DEBUG_TRACE                           Insert this token anywhere you want
 * tracing output displayed when the
 * "trace" debugging token is selected.
 *
 * DEBUG_MSGL((token, format, ...)):     equivalent to DEBUG_MSG, but includes
 * DEBUG_TRACE debugging line just before
 * yours.
 *
 * DEBUG_MSGTL((token, format, ...)):    Same as DEBUG_MSGL and DEBUG_MSGT
 * combined.
 *
 * Important:
 * It is considered best if you use DEBUG_MSGTL() everywhere possible, as it
 * gives the nicest format output and provides tracing support just before
 * every debugging statement output.
 *
 * To print multiple pieces to a single line in one call, use:
 *
 * DEBUG_MSGTL(("token", "line part 1"));
 * DEBUG_MSG  (("token", " and part 2\n"));
 *
 * to get:
 *
 * token: line part 1 and part 2
 *
 * as debugging output.
 *
 *
 * Each of these macros also have a version with a suffix of '_NC'. The
 * NC suffix stands for 'No Check', which means that no check will be
 * performed to see if debug is enabled or if the token has been turned
 * on. These NC versions are intended for use within a DEBUG_IF {} block,
 * where the debug/token check has already been performed.
 */

/*
 * define two macros : one macro with, one without,
 *                     a test if debugging is enabled.
 *
 * Generally, use the macro with _DBG_IF_
 */

/******************* Start private macros ************************/
#define _DBG_IF_ Debug_getDoDebugging()
#define DEBUG_IF( x ) if ( _DBG_IF_ && Debug_isTokenRegistered( x ) == ErrorCode_SUCCESS )

#define __DBGMSGT( x ) Debug_msgToken x, Debug_msg x
#define __DBGMSG_NC( x ) Debug_msg x
#define __DBGMSGT_NC( x ) Debug_comboNc x
#define __DBGMSGL_NC( x ) \
    __DBGTRACE;           \
    Debug_msg x
#define __DBGMSGTL_NC( x ) \
    __DBGTRACE;            \
    Debug_comboNc x

#define __DBGTRACE __DBGMSGT( ( "trace", " %s, %d:\n", __FILE__, __LINE__ ) )
#define __DBGTRACETOK( x ) __DBGMSGT( ( x, " %s, %d:\n", __FILE__, __LINE__ ) )

#define __DBGMSGL( x ) __DBGTRACE, Debug_msg x
#define __DBGMSGTL( x ) __DBGTRACE, Debug_msgToken x, Debug_msg x
#define __DBGMSGOID( x ) Debug_msgOid x
#define __DBGMSGSUBOID( x ) Debug_msgSuboid x
#define __DBGMSGVAR( x ) Debug_msgVar x
#define __DBGMSGOIDRANGE( x ) Debug_msgOidRange x
#define __DBGMSGHEX( x ) Debug_msgHex x
#define __DBGMSGHEXTLI( x ) Debug_msgHextli x
#define __DBGINDENT() Debug_indentGet()
#define __DBGINDENTADD( x ) Debug_indentAdd( x )
#define __DBGINDENTMORE() Debug_indentAdd( 2 )
#define __DBGINDENTLESS() Debug_indentAdd( -2 )
#define __DBGPRINTINDENT( token ) __DBGMSGTL( ( token, "%*s", __DBGINDENT(), "" ) )

#define __DBGDUMPHEADER( token, x )                                                                                                                                                                                                                                                 \
    __DBGPRINTINDENT( "dumph_" token );                                                                                                                                                                                                                                             \
    Debug_msg( "dumph_" token, x );                                                                                                                                                                                                                                                 \
    if ( Debug_isTokenRegistered( "dumpx" token ) == ErrorCode_SUCCESS || Debug_isTokenRegistered( "dumpv" token ) == ErrorCode_SUCCESS || ( Debug_isTokenRegistered( "dumpx_" token ) != ErrorCode_SUCCESS && Debug_isTokenRegistered( "dumpv_" token ) != ErrorCode_SUCCESS ) ) { \
        Debug_msg( "dumph_" token, "\n" );                                                                                                                                                                                                                                          \
    } else {                                                                                                                                                                                                                                                                        \
        Debug_msg( "dumph_" token, "  " );                                                                                                                                                                                                                                          \
    }                                                                                                                                                                                                                                                                               \
    __DBGINDENTMORE()

#define __DBGDUMPSECTION( token, x )        \
    __DBGPRINTINDENT( "dumph_" token );     \
    Debug_msg( "dumph_" token, "%s\n", x ); \
    __DBGINDENTMORE()

#define __DBGDUMPSETUP( token, buf, len )                                                                                                    \
    Debug_msg( "dumpx" token, "dumpx_%s:%*s", token, __DBGINDENT(), "" );                                                                    \
    __DBGMSGHEX( ( "dumpx_" token, buf, len ) );                                                                                             \
    if ( Debug_isTokenRegistered( "dumpv" token ) == ErrorCode_SUCCESS || Debug_isTokenRegistered( "dumpv_" token ) != ErrorCode_SUCCESS ) { \
        Debug_msg( "dumpx_" token, "\n" );                                                                                                   \
    } else {                                                                                                                                 \
        Debug_msg( "dumpx_" token, "  " );                                                                                                   \
    }                                                                                                                                        \
    Debug_msg( "dumpv" token, "dumpv_%s:%*s", token, __DBGINDENT(), "" );

/******************* End   private macros ************************/

/*****************************************************************/

/******************* Start public macros ************************/

#define DEBUG_MSG( x )    \
    do {                  \
        if ( _DBG_IF_ ) { \
            Debug_msg x;  \
        }                 \
    } while ( 0 )
#define DEBUG_MSGT( x )     \
    do {                    \
        if ( _DBG_IF_ ) {   \
            __DBGMSGT( x ); \
        }                   \
    } while ( 0 )
#define DEBUG_TRACE       \
    do {                  \
        if ( _DBG_IF_ ) { \
            __DBGTRACE;   \
        }                 \
    } while ( 0 )
#define DEBUG_TRACETOK( x )     \
    do {                        \
        if ( _DBG_IF_ ) {       \
            __DBGTRACETOK( x ); \
        }                       \
    } while ( 0 )
#define DEBUG_MSGL( x )     \
    do {                    \
        if ( _DBG_IF_ ) {   \
            __DBGMSGL( x ); \
        }                   \
    } while ( 0 )
#define DEBUG_MSGTL( x )     \
    do {                     \
        if ( _DBG_IF_ ) {    \
            __DBGMSGTL( x ); \
        }                    \
    } while ( 0 )
#define DEBUG_MSGOID( x )     \
    do {                      \
        if ( _DBG_IF_ ) {     \
            __DBGMSGOID( x ); \
        }                     \
    } while ( 0 )
#define DEBUG_MSGSUBOID( x )     \
    do {                         \
        if ( _DBG_IF_ ) {        \
            __DBGMSGSUBOID( x ); \
        }                        \
    } while ( 0 )
#define DEBUG_MSGVAR( x )     \
    do {                      \
        if ( _DBG_IF_ ) {     \
            __DBGMSGVAR( x ); \
        }                     \
    } while ( 0 )
#define DEBUG_MSGOIDRANGE( x )     \
    do {                           \
        if ( _DBG_IF_ ) {          \
            __DBGMSGOIDRANGE( x ); \
        }                          \
    } while ( 0 )
#define DEBUG_MSGHEX( x )     \
    do {                      \
        if ( _DBG_IF_ ) {     \
            __DBGMSGHEX( x ); \
        }                     \
    } while ( 0 )
#define DEBUG_MSGHEXTLI( x )     \
    do {                         \
        if ( _DBG_IF_ ) {        \
            __DBGMSGHEXTLI( x ); \
        }                        \
    } while ( 0 )
#define DEBUG_INDENTADD( x )     \
    do {                         \
        if ( _DBG_IF_ ) {        \
            __DBGINDENTADD( x ); \
        }                        \
    } while ( 0 )
#define DEBUG_INDENTMORE()     \
    do {                       \
        if ( _DBG_IF_ ) {      \
            __DBGINDENTMORE(); \
        }                      \
    } while ( 0 )
#define DEBUG_INDENTLESS()     \
    do {                       \
        if ( _DBG_IF_ ) {      \
            __DBGINDENTLESS(); \
        }                      \
    } while ( 0 )

#define DEBUG_PRINTINDENT( token )     \
    do {                               \
        if ( _DBG_IF_ ) {              \
            __DBGPRINTINDENT( token ); \
        }                              \
    } while ( 0 )
#define DEBUG_DUMPHEADER( token, x )     \
    do {                                 \
        if ( _DBG_IF_ ) {                \
            __DBGDUMPHEADER( token, x ); \
        }                                \
    } while ( 0 )
#define DEBUG_DUMPSECTION( token, x )     \
    do {                                  \
        if ( _DBG_IF_ ) {                 \
            __DBGDUMPSECTION( token, x ); \
        }                                 \
    } while ( 0 )
#define DEBUG_DUMPSETUP( token, buf, len )     \
    do {                                       \
        if ( _DBG_IF_ ) {                      \
            __DBGDUMPSETUP( token, buf, len ); \
        }                                      \
    } while ( 0 )
#define DEBUG_MSG_NC( x ) \
    do {                  \
        __DBGMSG_NC( x ); \
    } while ( 0 )
#define DEBUG_MSGT_NC( x ) \
    do {                   \
        __DBGMSGT_NC( x ); \
    } while ( 0 )
/******************* End   public macros ************************/

#endif //DEBUG_H
