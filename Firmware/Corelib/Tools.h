#ifndef TOOLS_H
#define TOOLS_H

#include <stddef.h>
#include <sys/types.h>
/*
 * General acros and constants.
*/
#define TOOLS_MAXPATH                 1024	/* Should be safe enough */
#define TOOLS_MAXBUF                  (1024 * 4)
#define TOOLS_MAXBUF_MEDIUM           1024
#define TOOLS_MAXBUF_SMALL            512
#define TOOLS_MAXBUF_MESSAGE	      1500
#define TOOLS_MAXOID                  64
#define TOOLS_MAX_CMDLINE_OIDS	      128
#define TOOLS_FILEMODE_CLOSED         0600
#define TOOLS_FILEMODE_OPEN           0644
#define TOOLS_BYTESIZE(bitsize)       ((bitsize + 7) >> 3)
#define TOOLS_ROUNDUP8(x)             ( ( (x+7) >> 3 ) * 8 )
#define TOOLS_STRORNULL(x)            ( x ? x : "(null)")


/** @def TOOLS_FREE(s)
    Frees a pointer only if it is !NULL and sets its value to NULL */
#define TOOLS_FREE(s)    do { if (s) { free((void *)s); s=NULL; } } while(0)

/** @def TOOLS_SWIPE_MEM(n, s)
    Frees pointer n only if it is !NULL, sets n to s and sets s to NULL */
#define TOOLS_SWIPE_MEM(n,s) do { if (n) free((void *)n); n = s; s=NULL; } while(0)

/*
 * XXX Not optimal everywhere.
 */
/** @def TOOLS_MALLOC_STRUCT(s)
    Mallocs memory of sizeof(struct s), zeros it and returns a pointer to it. */
#define TOOLS_MALLOC_STRUCT(s)   (struct s *) calloc(1, sizeof(struct s))

/** @def TOOLS_MALLOC_TYPEDEF(t)
    Mallocs memory of sizeof(t), zeros it and returns a pointer to it. */
#define TOOLS_MALLOC_TYPEDEF(td)  (td *) calloc(1, sizeof(td))

/** @def TOOLS_ZERO(s,l)
    Zeros l bytes of memory starting at s. */
#define TOOLS_ZERO(s,l)	do { if (s) memset(s, 0, l); } while(0)



/**
 * @def TOOLS_REMOVE_CONST(t, e)
 *
 * Cast away constness without that gcc -Wcast-qual prints a compiler warning,
 * similar to const_cast<> in C++.
 *
 * @param[in] t A pointer type.
 * @param[in] e An expression of a type that can be assigned to the type (const t).
 */
#define TOOLS_REMOVE_CONST(t, e)   (__extension__ ({ const t tmp = (e); (t)(uintptr_t)tmp; }))



#define TOOLS_TOUPPER(c)	(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c)
#define TOOLS_TOLOWER(c)	(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c)

#define TOOLS_HEX2VAL(s)    ((isalpha(s) ? (TOOLS_TOLOWER(s)-'a'+10) : (TOOLS_TOLOWER(s)-'0')) & 0xf)

#define TOOLS_VAL2HEX(s)	( (s) + (((s) >= 10) ? ('a'-10) : '0') )


/** @def TOOLS_MAX(a, b)
    Computers the maximum of a and b. */
#define TOOLS_MAX(a,b) ((a) > (b) ? (a) : (b))

/** @def TOOLS_MIN(a, b)
    Computers the minimum of a and b. */
#define TOOLS_MIN(a,b) ((a) > (b) ? (b) : (a))

/** @def TOOLS_MACRO_VAL_TO_STR(s)
 *  Expands to string with value of the s.
 *  If s is macro, the resulting string is value of the macro.
 *  Example:
 *   \#define TEST 1234
 *   TOOLS_MACRO_VAL_TO_STR(TEST) expands to "1234"
 *   TOOLS_MACRO_VAL_TO_STR(TEST+1) expands to "1234+1"
 */
#define TOOLS_MACRO_VAL_TO_STR(s) TOOLS_MACRO_VAL_TO_STR_PRIV(s)
#define TOOLS_MACRO_VAL_TO_STR_PRIV(s) #s

#define TOOLS_FALSE 0
#define TOOLS_TRUE  1


/*
 * TOOLS_QUIT the FUNction:
 *      e       Error code variable
 *      l       Label to goto to cleanup and get out of the function.
 *
 * XXX  It would be nice if the label could be constructed by the
 *      preprocessor in context.  Limited to a single error return value.
 *      Temporary hack at best.
 */
#define TOOLS_QUITFUN(e, l)             \
{                                       \
    if ( (e) != ErrorCode_SUCCESS) {      \
        rval = ErrorCode_GENERR;          \
        goto l ;                        \
    }                                   \
}


/**
 * Compute res = a + b.
 *
 * @pre a and b must be normalized 'struct timeval' values.
 *
 * @note res may be the same variable as one of the operands. In other
 *   words, &a == &res || &b == &res may hold.
 */
#define TOOLS_TIMERADD(a, b, res)                    \
{                                                    \
    (res)->tv_sec  = (a)->tv_sec  + (b)->tv_sec;     \
    (res)->tv_usec = (a)->tv_usec + (b)->tv_usec;    \
    if ((res)->tv_usec >= 1000000L) {                \
        (res)->tv_usec -= 1000000L;                  \
        (res)->tv_sec++;                             \
    }                                                \
}

/**
 * Compute res = a - b.
 *
 * @pre a and b must be normalized 'struct timeval' values.
 *
 * @note res may be the same variable as one of the operands. In other
 *   words, &a == &res || &b == &res may hold.
 */
#define TOOLS_TIMERSUB(a, b, res)                               \
{                                                               \
    (res)->tv_sec  = (a)->tv_sec  - (b)->tv_sec - 1;            \
    (res)->tv_usec = (a)->tv_usec - (b)->tv_usec + 1000000L;    \
    if ((res)->tv_usec >= 1000000L) {                           \
        (res)->tv_usec -= 1000000L;                             \
        (res)->tv_sec++;                                        \
    }                                                           \
}


/*
 * ISTRANSFORM
 * ASSUMES the minimum length for ttype and toid.
 */
#define TOOLS_USM_LENGTH_OID_TRANSFORM	10

#define TOOLS_ISTRANSFORM(ttype, toid)                       \
    !Api_oidCompare(ttype, TOOLS_USM_LENGTH_OID_TRANSFORM,   \
    usm_ ## toid ## Protocol, TOOLS_USM_LENGTH_OID_TRANSFORM)


#define TOOLS_ENGINETIME_MAX	2147483647      /* ((2^31)-1) */
#define TOOLS_ENGINEBOOT_MAX	2147483647      /* ((2^31)-1) */


/*
 * Prototypes.
 */

/** A pointer to an opaque time marker value. */
typedef void *  markerT;
typedef const void* constMarkerT;


char *   Tools_strdup( const char * ptr);
void *   Tools_calloc(size_t nmemb, size_t size);
void *   Tools_malloc(size_t size);
void *   Tools_realloc( void * ptr, size_t size);
void     Tools_free( void * ptr);
int      Tools_realloc2(u_char ** buf, size_t * buf_len);
int      Tools_strcat(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc, const u_char * s);
void     Tools_freeZero(void *buf, size_t size);
u_char * Tools_mallocRandom(size_t * size);
void *   Tools_memdup(const void *from, size_t size);
char *   Tools_strdupAndNull(const u_char * from, size_t from_len);
u_int    Tools_binaryToHex2(u_char ** dest, size_t *dest_len, int allow_realloc, const u_char * input, size_t len);
u_int    Tools_binaryToHex(const u_char * input, size_t len, char **output);
int      Tools_decimalToBinary(u_char ** buf, size_t * buf_len, size_t * out_len,int allow_realloc, const char *decimal);
int      Tools_hexToBinary(u_char ** buf, size_t * buf_len, size_t * offset, int allow_realloc, const char *hex, const char *delim);
int      Tools_hexToBinary1(u_char ** buf, size_t * buf_len, size_t * offset, int allow_realloc, const char *hex);
int      Tools_hexToBinary2(const u_char * input, size_t len, char **output);

void     Tools_dumpChunk(const char *debugtoken, const char *title, const u_char * buf, int size);
markerT  Tools_atimeNewMarker(void);
void     Tools_atimeSetMarker(markerT pm);
void     Tools_getMonotonicClock(struct timeval* tv);
void     Tools_setMonotonicMarker(markerT *pm);
long     Tools_atimeDiff(constMarkerT first, constMarkerT second);
u_long   Tools_uatimeDiff(constMarkerT first, constMarkerT second);
u_long   Tools_uatimeHdiff(constMarkerT first, constMarkerT second);
int      Tools_atimeReady(constMarkerT pm, int delta_ms);
int      Tools_uatimeReady(constMarkerT pm, unsigned int delta_ms);
int      Tools_readyMonotonic(constMarkerT pm, int delta_ms);
int      Tools_markerTticks(constMarkerT pm);
int      Tools_timevalTticks(const struct timeval *tv);
char *   Tools_getenv(const char *name);
int      Tools_setenv(const char *envname, const char *envval, int overwrite);
int      Tools_addrstrHton(char *ptr, size_t len);
int      Tools_stringTimeToSecs(const char *time_string);

#define  Tools_cstrcat(b,l,o,a,s) Tools_strcat(b,l,o,a,(const u_char *)s)


#endif // TOOLS_H
