#include "FileParser.h"
#include "ReadConfig.h"
#include "System/Util/Assert.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"

/** ==================[ Private Functions Prototypes ]================== */

/**
 * @internal
 * parse mode:
 */
static void _FileParser_parseModeSaveIndexStringString( FILE* f, Container_Container* cin, int flags );

/**
 * @internal
 * parse mode: save everything
 */
static void _FileParser_parseModeSaveEverything( FILE* f, Container_Container* cin, int flags );

/**
 * @internal
 * parse mode:
 */
static void _FileParser_parseModeUserFunction( FILE* file, Container_Container* container,
    TextLineProcessInfo_t* textLineProcessInfo, int flags );

/**
 * @internal
 * process token value index line
 */
static int _FileParser_processLineTvi( TextLineInfo_t* lineInfo, void* mem, struct TextLineProcessInfo_s* lpi );

/** =============================[ Public Functions ]================== */

Container_Container*
FileParser_parse( File* file, Container_Container* container,
    int parseMode, u_int flags, void* textLineProcessInfo )
{
    Container_Container* c = container;
    FILE* fin;
    int rc;

    if ( NULL == file )
        return NULL;

    if ( ( NULL == c ) && ( !( flags & fpFLAG_NO_CONTAINER ) ) ) {
        c = Container_find( "textParse:binaryArray" );
        if ( NULL == c )
            return NULL;
    }

    rc = File_open( file );
    if ( rc < 0 ) { /** error already logged */
        if ( ( NULL != c ) && ( c != container ) )
            CONTAINER_FREE( c );
        return NULL;
    }

    /*
     * get a stream from the file descriptor. This DOES NOT rewind the
     * file (if fd was previously opened).
     */
    fin = fdopen( file->fileDescriptor, "r" );
    if ( NULL == fin ) {
        if ( FILE_IS_AUTOCLOSE( file->priotFlags ) )
            close( file->fileDescriptor );
        if ( ( NULL != c ) && ( c != container ) )
            CONTAINER_FREE( c );
        return NULL;
    }

    switch ( parseMode ) {

    case fpPARSE_MODE_SAVE_EVERYTHING:
        _FileParser_parseModeSaveEverything( fin, c, flags );
        break;

    case fpPARSE_MODE_INDEX_STRING_STRING:
        _FileParser_parseModeSaveIndexStringString( fin, c, flags );
        break;

    case fpPARSE_MODE_USER_FUNCTION:
        if ( NULL != textLineProcessInfo )
            _FileParser_parseModeUserFunction( fin, c, ( TextLineProcessInfo_t* )textLineProcessInfo,
                flags );
        break;

    default:
        Logger_log( LOGGER_PRIORITY_ERR, "unknown parse mode %d\n", parseMode );
        break;
    }

    /*
     * close the stream, which will have the side effect of also closing
     * the file descriptor, so we need to reset it.
     */
    fclose( fin );
    file->fileDescriptor = -1;

    return c;
}

Container_Container*
FileParser_parseTokenContainerFromFile( const char* file, u_int flags,
    Container_Container* container, void* context )
{
    TextLineProcessInfo_t lpi;
    Container_Container *c = container, *c_rc;
    File* fp;

    if ( NULL == file )
        return NULL;

    /*
     * allocate file resources
     */
    fp = File_fill( NULL, file, O_RDONLY, 0, 0 );
    if ( NULL == fp ) /** msg already logged */
        return NULL;

    memset( &lpi, 0x0, sizeof( lpi ) );
    lpi.memSize = sizeof( TextTokenValueIndex_t );
    lpi.processFunc = _FileParser_processLineTvi;
    lpi.userContext = context;

    if ( NULL == c ) {
        c = Container_find( "string:binaryArray" );
        if ( NULL == c ) {
            Logger_log( LOGGER_PRIORITY_ERR, "malloc failed\n" );
            File_release( fp );
            return NULL;
        }
    }

    c_rc = FileParser_parse( fp, c, fpPARSE_MODE_USER_FUNCTION, 0, &lpi );

    /*
     * if we got a bad return and the user didn't pass us a container,
     * we need to release the container we allocated.
     */
    if ( ( NULL == c_rc ) && ( NULL == container ) ) {
        CONTAINER_FREE( c );
        c = NULL;
    } else
        c = c_rc;

    /*
     * release file resources
     */
    File_release( fp );

    return c;
}

/** =============================[ Private Functions ]================== */

static void _FileParser_parseModeSaveIndexStringString( FILE* f, Container_Container* cin,
    int flags )
{
    char line[ READCONFIG_STRINGMAX ], *ptr;
    TextTokenValueIndex_t* tvi;
    size_t count = 0, len;

    Assert_assert( NULL != f );
    Assert_assert( NULL != cin );

    while ( fgets( line, sizeof( line ), f ) != NULL ) {

        ++count;
        ptr = line;
        len = strlen( line ) - 1;
        if ( line[ len ] == '\n' )
            line[ len ] = 0;

        /*
         * save blank line or comment?
         */
        if ( flags & fpFLAG_SKIP_WHITESPACE ) {
            if ( NULL == ( ptr = ReadConfig_skipWhite( ptr ) ) )
                continue;
        }

        tvi = MEMORY_MALLOC_TYPEDEF( TextTokenValueIndex_t );
        if ( NULL == tvi ) {
            Logger_log( LOGGER_PRIORITY_ERR, "malloc failed\n" );
            break;
        }

        /*
         * copy whole line, then set second pointer to
         * after token. One malloc, 2 strings!
         */
        tvi->index = count;
        tvi->token = strdup( line );
        if ( NULL == tvi->token ) {
            Logger_log( LOGGER_PRIORITY_ERR, "malloc failed\n" );
            free( tvi );
            break;
        }
        tvi->value.cp = ReadConfig_skipNotWhite( tvi->token );
        if ( NULL != tvi->value.cp ) {
            *( tvi->value.cp ) = 0;
            ++( tvi->value.cp );
        }
        CONTAINER_INSERT( cin, tvi );
    }
}

static void _FileParser_parseModeSaveEverything( FILE* f, Container_Container* cin, int flags )
{
    char line[ READCONFIG_STRINGMAX ], *ptr;
    size_t len;

    Assert_assert( NULL != f );
    Assert_assert( NULL != cin );

    while ( fgets( line, sizeof( line ), f ) != NULL ) {

        ptr = line;
        len = strlen( line ) - 1;
        if ( line[ len ] == '\n' )
            line[ len ] = 0;

        /*
         * save blank line or comment?
         */
        if ( flags & fpFLAG_SKIP_WHITESPACE ) {
            if ( NULL == ( ptr = ReadConfig_skipWhite( ptr ) ) )
                continue;
        }

        ptr = strdup( line );
        if ( NULL == ptr ) {
            Logger_log( LOGGER_PRIORITY_ERR, "malloc failed\n" );
            break;
        }

        CONTAINER_INSERT( cin, ptr );
    }
}

static void _FileParser_parseModeUserFunction( FILE* file, Container_Container* container,
    TextLineProcessInfo_t* textLineProcessInfo, int flags )
{
    char buf[ READCONFIG_STRINGMAX ];
    TextLineInfo_t li;
    void* mem = NULL;
    int rc;

    Assert_assert( NULL != file );
    Assert_assert( NULL != container );

    /*
     * static buf, or does the user want the memory?
     */
    if ( flags & fpFLAG_ALLOC_LINE ) {
        if ( 0 != textLineProcessInfo->lineMax )
            li.lineMax = textLineProcessInfo->lineMax;
        else
            li.lineMax = READCONFIG_STRINGMAX;
        li.line = ( char* )Memory_calloc( li.lineMax, 1 );
        if ( NULL == li.line ) {
            Logger_log( LOGGER_PRIORITY_ERR, "malloc failed\n" );
            return;
        }
    } else {
        li.line = buf;
        li.lineMax = sizeof( buf );
    }

    li.index = 0;
    while ( fgets( li.line, li.lineMax, file ) != NULL ) {

        ++li.index;
        li.start = li.line;
        li.lineLength = String_length( li.line ) - 1;
        if ( ( !( textLineProcessInfo->flags & fpFLAG_LEAVE_NEWLINE ) ) && ( li.line[ li.lineLength ] == '\n' ) )
            li.line[ li.lineLength ] = 0;

        /*
         * save blank line or comment?
         */
        if ( !( textLineProcessInfo->flags & fpFLAG_PROCESS_WHITESPACE ) ) {
            if ( NULL == ( li.start = ReadConfig_skipWhite( li.start ) ) )
                continue;
        }

        /*
         *  do we need to allocate memory for the use?
         * if the last call didn't use the memory we allocated,
         * re-use it. Otherwise, allocate new chunk.
         */
        if ( ( 0 != textLineProcessInfo->memSize ) && ( NULL == mem ) ) {
            mem = Memory_calloc( textLineProcessInfo->memSize, 1 );
            if ( NULL == mem ) {
                Logger_log( LOGGER_PRIORITY_ERR, "malloc failed\n" );
                break;
            }
        }

        /*
         * do they want a copy ot the line?
         */
        if ( textLineProcessInfo->flags & fpFLAG_STRDUP_LINE ) {
            li.start = String_new( li.start );
            if ( NULL == li.start ) {
                Logger_log( LOGGER_PRIORITY_ERR, "malloc failed\n" );
                break;
            }
        } else if ( textLineProcessInfo->flags & fpFLAG_ALLOC_LINE ) {
            li.start = li.line;
        }

        /*
         * call the user function. If the function wants to save
         * the memory chunk, insert it in the container, the clear
         * pointer so we reallocate next time.
         */
        li.startLength = String_length( li.start );
        rc = ( *textLineProcessInfo->processFunc )( &li, mem, textLineProcessInfo );
        if ( fpRETURN_CODE_MEMORY_USED == rc ) {

            if ( !( textLineProcessInfo->flags & fpFLAG_LINE_PROCESS_NO_CONTAINER ) )
                CONTAINER_INSERT( container, mem );

            mem = NULL;

            if ( textLineProcessInfo->flags & fpFLAG_ALLOC_LINE ) {
                li.line = ( char* )Memory_calloc( li.lineMax, 1 );
                if ( NULL == li.line ) {
                    Logger_log( LOGGER_PRIORITY_ERR, "malloc failed\n" );
                    break;
                }
            }
        } else if ( fpRETURN_CODE_MEMORY_UNUSED == rc ) {
            /*
             * they didn't use the memory. if li.start was a strdup, we have
             * to release it. leave mem, we can re-use it (its a fixed size).
             */
            if ( textLineProcessInfo->flags & fpFLAG_STRDUP_LINE )
                free( li.start ); /* no point in MEMORY_FREE */
        } else {
            if ( fpRETURN_CODE_STOP_PROCESSING != rc )
                Logger_log( LOGGER_PRIORITY_ERR, "unknown rc %d from text processor\n", rc );
            break;
        }
    }
    MEMORY_FREE( mem );
}

static int _FileParser_processLineTvi( TextLineInfo_t* line_info, void* mem,
    struct TextLineProcessInfo_s* lpi )
{
    TextTokenValueIndex_t* tvi = ( TextTokenValueIndex_t* )mem;
    char* ptr;

    /*
     * get token
     */
    ptr = ReadConfig_skipNotWhite( line_info->start );
    if ( NULL == ptr ) {
        DEBUG_MSGTL( ( "text:util:tvi", "no value after token '%s'\n",
            line_info->start ) );
        return fpRETURN_CODE_MEMORY_UNUSED;
    }

    /*
     * null terminate, search for value;
     */
    *( ptr++ ) = 0;
    ptr = ReadConfig_skipWhite( ptr );
    if ( NULL == ptr ) {
        DEBUG_MSGTL( ( "text:util:tvi", "no value after token '%s'\n",
            line_info->start ) );
        return fpRETURN_CODE_MEMORY_UNUSED;
    }

    /*
     * get value
     */
    switch ( ( int )( intptr_t )lpi->userContext ) {

    case fpTYPE_UNSIGNED:
        tvi->value.ul = strtoul( ptr, NULL, 0 );
        if ( ( errno == ERANGE ) && ( ULONG_MAX == tvi->value.ul ) )
            Logger_log( LOGGER_PRIORITY_WARNING, "value overflow\n" );
        break;

    case fpTYPE_INTEGER:
        tvi->value.sl = strtol( ptr, NULL, 0 );
        if ( ( errno == ERANGE ) && ( ( LONG_MAX == tvi->value.sl ) || ( LONG_MIN == tvi->value.sl ) ) )
            Logger_log( LOGGER_PRIORITY_WARNING, "value over/under-flow\n" );
        break;

    case fpTYPE_STRING:
        tvi->value.cp = strdup( ptr );
        break;

    case fpTYPE_BOOLEAN:
        if ( isdigit( ( unsigned char )( *ptr ) ) )
            tvi->value.ul = strtoul( ptr, NULL, 0 );
        else if ( strcasecmp( ptr, "true" ) == 0 )
            tvi->value.ul = 1;
        else if ( strcasecmp( ptr, "false" ) == 0 )
            tvi->value.ul = 0;
        else {
            Logger_log( LOGGER_PRIORITY_WARNING, "bad value for boolean\n" );
            return fpRETURN_CODE_MEMORY_UNUSED;
        }
        break;

    default:
        Logger_log( LOGGER_PRIORITY_ERR, "unsupported value type %d\n",
            ( int )( intptr_t )lpi->userContext );
        break;
    }

    /*
     * save token and value
     */
    tvi->token = strdup( line_info->start );
    tvi->index = line_info->index;

    return fpRETURN_CODE_MEMORY_USED;
}
