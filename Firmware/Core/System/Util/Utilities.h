#ifndef IOT_UTILITIES_H
#define IOT_UTILITIES_H

#include "System/Convert.h"
#include "System/String.h"
#include "System/Util/Memory.h"
#include "System/Util/Time.h"
#include <stddef.h>

/** ============================[ Macros ]============================ */

/*
 * General acros and constants.
*/
#define UTILITIES_MAX_PATH 1024 /* Should be safe enough */
#define UTILITIES_MAX_BUFFER ( 1024 * 4 )
#define UTILITIES_MAX_BUFFER_MEDIUM 1024
#define UTILITIES_MAX_BUFFER_SMALL 512
#define UTILITIES_BYTE_SIZE( bitsize ) ( ( bitsize + 7 ) >> 3 )
#define UTILITIES_ROUND_UP8( x ) ( ( ( x + 7 ) >> 3 ) * 8 )
#define UTILITIES_STRING_OR_NULL( x ) ( x ? x : "(null)" )

/**
 * \def UTILITIES_REMOVE_CONST(t, e)
 *
 * Cast away constness without that gcc -Wcast-qual prints a compiler warning,
 * similar to const_cast<> in C++.
 *
 * @param[in] t A pointer type.
 * @param[in] e An expression of a type that can be assigned to the type (const t).
 */
#define UTILITIES_REMOVE_CONST( t, e ) ( __extension__( { const t tmp = (e); (t)(uintptr_t)tmp; } ) )

/** \def UTILITIES_MAX_VALUE(a, b)
 * Computers the maximum of a and b.
 */
#define UTILITIES_MAX_VALUE( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )

/** @def UTILITIES_MIN_VALUE(a, b)
 * Computers the minimum of a and b.
 */
#define UTILITIES_MIN_VALUE( a, b ) ( ( a ) > ( b ) ? ( b ) : ( a ) )

/*
 * UTILITIES_QUIT_FUN the Function:
 *      e       Error code variable
 *      l       Label to goto to cleanup and get out of the function.
 *
 * XXX  It would be nice if the label could be constructed by the
 *      preprocessor in context.  Limited to a single error return value.
 *      Temporary hack at best.
 */
#define UTILITIES_QUIT_FUN( e, l )          \
    {                                       \
        if ( ( e ) != ErrorCode_SUCCESS ) { \
            rval = ErrorCode_GENERR;        \
            goto l;                         \
        }                                   \
    }

/*
 * ISTRANSFORM
 * ASSUMES the minimum length for ttype and toid.
 */
#define UTILITIES_USM_LENGTH_OID_TRANSFORM 10

#define UTILITIES_ISTRANSFORM( ttype, toid )                    \
    !Api_oidCompare( ttype, UTILITIES_USM_LENGTH_OID_TRANSFORM, \
        usm_##toid##Protocol, UTILITIES_USM_LENGTH_OID_TRANSFORM )

#define UTILITIES_ENGINETIME_MAX 2147483647 /* ((2^31)-1) */
#define UTILITIES_ENGINEBOOT_MAX 2147483647 /* ((2^31)-1) */

/** =============================[ Functions Prototypes ]================== */

/*
 * swap the order of an inet addr string
 */
int Utilities_addrStringHton( char* ptr, size_t len );

#endif // IOT_UTILITIES_H
