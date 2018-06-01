#include "EngineTime.h"
#include "Api.h"
#include "Scapi.h"
#include "System/Util/Trace.h"
#include "System/Util/Utilities.h"
#include "Usm.h"
#include "V3.h"

/**
 * Global static hashlist to contain Enginetime entries.
 * New records are prepended to the appropriate list at the hash index.
 */
static Enginetime_p _engineTime_list[ engineLIST_SIZE ];

int EngineTime_get( const u_char* engineId,
    u_int engineIdLength,
    u_int* engineBoot,
    u_int* engineTime, u_int authenticatedFlag )
{
    int rval = ErrorCode_SUCCESS;
    int timediff = 0;
    Enginetime_p e = NULL;

    /*
     * Sanity check.
     */
    if ( !engineTime || !engineBoot ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_getEnginetimeQuit );
    }

    /*
     * Compute estimated current engine_time tuple at engineID if
     * a record is cached for it.
     */
    *engineTime = *engineBoot = 0;

    if ( !engineId || ( engineIdLength <= 0 ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_getEnginetimeQuit );
    }

    if ( !( e = EngineTime_searchInList( engineId, engineIdLength ) ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_getEnginetimeQuit );
    }

    if ( !authenticatedFlag || e->authenticatedFlag ) {
        *engineTime = e->engineTime;
        *engineBoot = e->engineBoot;

        timediff = ( int )( V3_localEngineTime() - e->lastReceivedEngineTime );
    }

    if ( timediff > ( int )( UTILITIES_ENGINETIME_MAX - *engineTime ) ) {
        *engineTime = ( timediff - ( UTILITIES_ENGINETIME_MAX - *engineTime ) );

        /*
         * FIX -- move this check up... should not change anything
         * * if engineboot is already locked.  ???
         */
        if ( *engineBoot < UTILITIES_ENGINEBOOT_MAX ) {
            *engineBoot += 1;
        }

    } else {
        *engineTime += timediff;
    }

    DEBUG_MSGTL( ( "LcdTime_getEnginetime", "engineID " ) );
    DEBUG_MSGHEX( ( "LcdTime_getEnginetime", engineId, engineIdLength ) );
    DEBUG_MSG( ( "LcdTime_getEnginetime", ": boots=%d, time=%d\n", *engineBoot,
        *engineTime ) );

goto_getEnginetimeQuit:
    return rval;
}

int EngineTime_getEx( u_char* engineId,
    u_int engineIdLength,
    u_int* engineBoot,
    u_int* engineTime,
    u_int* lastEngineTime, u_int authenticatedFlag )
{
    int rval = ErrorCode_SUCCESS;
    int timediff = 0;
    Enginetime_p e = NULL;

    /*
     * Sanity check.
     */
    if ( !engineTime || !engineBoot || !lastEngineTime ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_getEnginetimeExQuit );
    }

    /*
     * Compute estimated current engine_time tuple at engineID if
     * a record is cached for it.
     */
    *lastEngineTime = *engineTime = *engineBoot = 0;

    if ( !engineId || ( engineIdLength <= 0 ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_getEnginetimeExQuit );
    }

    if ( !( e = EngineTime_searchInList( engineId, engineIdLength ) ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_getEnginetimeExQuit );
    }

    if ( !authenticatedFlag || e->authenticatedFlag ) {
        *lastEngineTime = *engineTime = e->engineTime;
        *engineBoot = e->engineBoot;

        timediff = ( int )( V3_localEngineTime() - e->lastReceivedEngineTime );
    }

    if ( timediff > ( int )( UTILITIES_ENGINETIME_MAX - *engineTime ) ) {
        *engineTime = ( timediff - ( UTILITIES_ENGINETIME_MAX - *engineTime ) );

        /*
         * FIX -- move this check up... should not change anything
         * * if engineboot is already locked.  ???
         */
        if ( *engineBoot < UTILITIES_ENGINEBOOT_MAX ) {
            *engineBoot += 1;
        }

    } else {
        *engineTime += timediff;
    }

    DEBUG_MSGTL( ( "LcdTime_getEnginetimeEx", "engineID " ) );
    DEBUG_MSGHEX( ( "LcdTime_getEnginetimeEx", engineId, engineIdLength ) );
    DEBUG_MSG( ( "LcdTime_getEnginetimeEx", ": boots=%d, time=%d\n",
        *engineBoot, *engineTime ) );

goto_getEnginetimeExQuit:
    return rval;
}

void EngineTime_free( unsigned char* engineId, size_t engineIdLength )
{
    Enginetime_p e = NULL;
    int rval = 0;

    rval = EngineTime_hashEngineId( engineId, engineIdLength );
    if ( rval < 0 )
        return;

    e = _engineTime_list[ rval ];

    while ( e != NULL ) {
        _engineTime_list[ rval ] = e->next;
        MEMORY_FREE( e->engineId );
        MEMORY_FREE( e );
        e = _engineTime_list[ rval ];
    }
}

void EngineTime_clear( void )
{
    int index = 0;
    Enginetime_p e = NULL;
    Enginetime_p nextE = NULL;

    for ( ; index < engineLIST_SIZE; ++index ) {
        e = _engineTime_list[ index ];

        while ( e != NULL ) {
            nextE = e->next;
            MEMORY_FREE( e->engineId );
            MEMORY_FREE( e );
            e = nextE;
        }

        _engineTime_list[ index ] = NULL;
    }
    return;
}

int EngineTime_set( const u_char* engineId,
    u_int engineIdLength,
    u_int engineBoot, u_int engineTime, u_int authenticatedFlag )
{
    int rval = ErrorCode_SUCCESS, iindex;
    Enginetime_p e = NULL;

    /*
     * Sanity check.
     */
    if ( !engineId || ( engineIdLength <= 0 ) ) {
        return rval;
    }

    /*
     * Store the given <engine_time, engineboot> tuple in the record
     * for engineID.  Create a new record if necessary.
     */
    if ( !( e = EngineTime_searchInList( engineId, engineIdLength ) ) ) {
        if ( ( iindex = EngineTime_hashEngineId( engineId, engineIdLength ) ) < 0 ) {
            UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_setEnginetimeQuit );
        }

        e = ( Enginetime_p )calloc( 1, sizeof( *e ) );

        e->next = _engineTime_list[ iindex ];
        _engineTime_list[ iindex ] = e;

        e->engineId = ( u_char* )calloc( 1, engineIdLength );
        memcpy( e->engineId, engineId, engineIdLength );

        e->engineIdLength = engineIdLength;
    }

    if ( authenticatedFlag || !e->authenticatedFlag ) {
        e->authenticatedFlag = authenticatedFlag;

        e->engineTime = engineTime;
        e->engineBoot = engineBoot;
        e->lastReceivedEngineTime = V3_localEngineTime();
    }

    e = NULL; /* Indicates a successful update. */

    DEBUG_MSGTL( ( "LcdTime_setEnginetime", "engineID " ) );
    DEBUG_MSGHEX( ( "LcdTime_setEnginetime", engineId, engineIdLength ) );
    DEBUG_MSG( ( "LcdTime_setEnginetime", ": boots=%d, time=%d\n", engineBoot,
        engineTime ) );

goto_setEnginetimeQuit:
    MEMORY_FREE( e );

    return rval;
}

Enginetime_p EngineTime_searchInList( const u_char* engineId, u_int engineIdLength )
{
    int rval = ErrorCode_SUCCESS;
    Enginetime_p e = NULL;

    /*
     * Sanity check.
     */
    if ( !engineId || ( engineIdLength <= 0 ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_searchEnginetimeListQuit );
    }

    /*
     * Find the entry for engineID if there be one.
     */
    rval = EngineTime_hashEngineId( engineId, engineIdLength );
    if ( rval < 0 ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_searchEnginetimeListQuit );
    }
    e = _engineTime_list[ rval ];

    for ( ; e; e = e->next ) {
        if ( ( engineIdLength == e->engineIdLength )
            && !memcmp( e->engineId, engineId, engineIdLength ) ) {
            break;
        }
    }

goto_searchEnginetimeListQuit:
    return e;
}

int EngineTime_hashEngineId( const u_char* engineId, u_int engineIdLength )
{
    int rval = ErrorCode_GENERR;
    size_t buf_len = UTILITIES_MAX_BUFFER;
    u_int additive = 0;
    u_char *bufp, buf[ UTILITIES_MAX_BUFFER ];
    void* context = NULL;

    /*
     * Sanity check.
     */
    if ( !engineId || ( engineIdLength <= 0 ) ) {
        UTILITIES_QUIT_FUN( ErrorCode_GENERR, goto_hashEngineIDQuit );
    }

    /*
     * Hash engineID into a list index.
     */
    rval = Scapi_hash( usm_hMACMD5AuthProtocol,
        sizeof( usm_hMACMD5AuthProtocol ) / sizeof( oid ),
        engineId, engineIdLength, buf, &buf_len );

    if ( rval == ErrorCode_SC_NOT_CONFIGURED ) {
        /* fall back to sha1 */
        rval = Scapi_hash( usm_hMACSHA1AuthProtocol,
            sizeof( usm_hMACSHA1AuthProtocol ) / sizeof( oid ),
            engineId, engineIdLength, buf, &buf_len );
    }

    UTILITIES_QUIT_FUN( rval, goto_hashEngineIDQuit );

    for ( bufp = buf; ( bufp - buf ) < ( int )buf_len; bufp += 4 ) {
        additive += ( u_int )*bufp;
    }

goto_hashEngineIDQuit:
    MEMORY_FREE( context );
    memset( buf, 0, UTILITIES_MAX_BUFFER );

    return ( rval < 0 ) ? rval : ( int )( additive % engineLIST_SIZE );
}
