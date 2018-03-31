#include "Tools.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/time.h>
#include "Config.h"
#include "Debug.h"
#include "Logger.h"
#include "Scapi.h"
#include "Assert.h"

/**
 * This function is a wrapper for the strdup function.
 *
 * @note The strdup() implementation calls _malloc_dbg() when linking with
 * MSVCRT??D.dll and malloc() when linking with MSVCRT??.dll
 */
char * Tools_strdup( const char * ptr)
{
    return strdup(ptr);
}

/**
 * This function is a wrapper for the calloc function.
 */
void * Tools_calloc(size_t nmemb, size_t size)
{
    return calloc(nmemb, size);
}

/**
 * This function is a wrapper for the malloc function.
 */
void * Tools_malloc(size_t size)
{
    return malloc(size);
}

/**
 * This function is a wrapper for the realloc function.
 */
void * Tools_realloc( void * ptr, size_t size)
{
    return realloc(ptr, size);
}

/**
 * This function is a wrapper for the free function.
 * It calls free only if the calling parameter has a non-zero value.
 */
void Tools_free( void * ptr)
{
    if (ptr)
        free(ptr);
}


/**
 * This function increase the size of the buffer pointed at by *buf, which is
 * initially of size *buf_len.  Contents are preserved **AT THE BOTTOM END OF
 * THE BUFFER**.  If memory can be (re-)allocated then it returns 1, else it
 * returns 0.
 *
 * @param buf  pointer to a buffer pointer
 * @param buf_len      pointer to current size of buffer in bytes
 *
 * @note
 * The current re-allocation algorithm is to increase the buffer size by
 * whichever is the greater of 256 bytes or the current buffer size, up to
 * a maximum increase of 8192 bytes.
 */
int Tools_realloc2(u_char ** buf, size_t * buf_len)
{
    u_char         *new_buf = NULL;
    size_t          new_buf_len = 0;

    if (buf == NULL) {
        return 0;
    }

    if (*buf_len <= 255) {
        new_buf_len = *buf_len + 256;
    } else if (*buf_len > 255 && *buf_len <= 8191) {
        new_buf_len = *buf_len * 2;
    } else if (*buf_len > 8191) {
        new_buf_len = *buf_len + 8192;
    }

    if (*buf == NULL) {
        new_buf = (u_char *) malloc(new_buf_len);
    } else {
        new_buf = (u_char *) realloc(*buf, new_buf_len);
    }

    if (new_buf != NULL) {
        *buf = new_buf;
        *buf_len = new_buf_len;
        return 1;
    } else {
        return 0;
    }
}

int Tools_strcat(u_char ** buf, size_t * buf_len, size_t * out_len,
            int allow_realloc, const u_char * s)
{
    if (buf == NULL || buf_len == NULL || out_len == NULL) {
        return 0;
    }

    if (s == NULL) {
        /*
         * Appending a NULL string always succeeds since it is a NOP.
         */
        return 1;
    }

    while ((*out_len + strlen((const char *) s) + 1) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
    }

    if (!*buf)
        return 0;

    strcpy((char *) (*buf + *out_len), (const char *) s);
    *out_len += strlen((char *) (*buf + *out_len));
    return 1;
}


/** zeros memory before freeing it.
 *
 *	@param *buf	Pointer at bytes to free.
 *	@param size	Number of bytes in buf.
 */
void Tools_freeZero(void *buf, size_t size)
{
    if (buf) {
        memset(buf, 0, size);
        free(buf);
    }

}


/**
 * Returns pointer to allocaed & set buffer on success, size contains
 * number of random bytes filled.  buf is NULL and *size set to KMT
 * error value upon failure.
 *
 *	@param size	Number of bytes to malloc() and fill with random bytes.
 *
 * @return a malloced buffer
 *
 */
u_char * Tools_mallocRandom(size_t * size)
{
    int             rval = ErrorCode_SUCCESS;
    u_char         *buf = (u_char *) calloc(1, *size);

    if (buf) {
        rval = Scapi_random(buf, size);

        if (rval < 0) {
            Tools_freeZero(buf, *size);
            buf = NULL;
        } else {
            *size = rval;
        }
    }

    return buf;

}


/**
 * Duplicates a memory block.
 *
 * @param[in] from Pointer to copy memory from.
 * @param[in] size Size of the data to be copied.
 *
 * @return Pointer to the duplicated memory block, or NULL if memory allocation
 * failed.
 */
void * Tools_memdup(const void *from, size_t size)
{
    void *to = NULL;

    if (from) {
        to = malloc(size);
        if (to)
            memcpy(to, from, size);
    }
    return to;
}                               /* end netsnmp_memdup() */




/** copies a (possible) unterminated string of a given length into a
 *  new buffer and null terminates it as well (new buffer MAY be one
 *  byte longer to account for this */
char * Tools_strdupAndNull(const u_char * from, size_t from_len)
{
    char         *ret;

    if (from_len == 0 || from[from_len - 1] != '\0') {
        ret = (char *)malloc(from_len + 1);
        if (!ret)
            return NULL;
        ret[from_len] = '\0';
    } else {
        ret = (char *)malloc(from_len);
        if (!ret)
            return NULL;
        ret[from_len - 1] = '\0';
    }
    memcpy(ret, from, from_len);
    return ret;
}


/** converts binary to hexidecimal
 *
 *     @param *input            Binary data.
 *     @param len               Length of binary data.
 *     @param **dest            NULL terminated string equivalent in hex.
 *     @param *dest_len         size of destination buffer
 *     @param allow_realloc     flag indicating if buffer can be realloc'd
 *
 * @return olen	Length of output string not including NULL terminator.
 */
u_int Tools_binaryToHex2(u_char ** dest, size_t *dest_len, int allow_realloc,
                      const u_char * input, size_t len)
{
    u_int           olen = (len * 2) + 1;
    u_char         *s, *op;
    const u_char   *ip = input;

    if (dest == NULL || dest_len == NULL || input == NULL)
        return 0;

    if (NULL == *dest) {
        s = (unsigned char *) calloc(1, olen);
        *dest_len = olen;
    }
    else
        s = *dest;

    if (*dest_len < olen) {
        if (!allow_realloc)
            return 0;
        *dest_len = olen;
        if (Tools_realloc2(dest, dest_len))
            return 0;
    }

    op = s;
    while (ip - input < (int) len) {
        *op++ = TOOLS_VAL2HEX((*ip >> 4) & 0xf);
        *op++ = TOOLS_VAL2HEX(*ip & 0xf);
        ip++;
    }
    *op = '\0';

    if (s != *dest)
        *dest = s;
    *dest_len = olen;

    return olen;
}


/** converts binary to hexidecimal
 *
 *	@param *input		Binary data.
 *	@param len		Length of binary data.
 *	@param **output	NULL terminated string equivalent in hex.
 *
 * @return olen	Length of output string not including NULL terminator.
 *
 * FIX	Is there already one of these in the UCD SNMP codebase?
 *	The old one should be used, or this one should be moved to
 *	snmplib/snmp_api.c.
 */
u_int Tools_binaryToHex(const u_char * input, size_t len, char **output)
{
    size_t out_len = 0;

    *output = NULL; /* will alloc new buffer */

    return Tools_binaryToHex2((u_char**)output, &out_len, 1, input, len);
}                               /* end binary_to_hex() */



/**
 * Tools_hexToBinary2
 *	@param *input		Printable data in base16.
 *	@param len		Length in bytes of data.
 *	@param **output	Binary data equivalent to input.
 *
 * @return SNMPERR_GENERR on failure, otherwise length of allocated string.
 *
 * Input of an odd length is right aligned.
 *
 * FIX	Another version of "hex-to-binary" which takes odd length input
 *	strings.  It also allocates the memory to hold the binary data.
 *	Should be integrated with the official hex_to_binary() function.
 */
int Tools_hexToBinary2(const u_char * input, size_t len, char **output)
{
    u_int           olen = (len / 2) + (len % 2);
    char           *s = (char *)calloc(1, olen ? olen : 1), *op = s;
    const u_char   *ip = input;


    *output = NULL;
    if (!s)
        goto hex_to_binary2_quit;

    *op = 0;
    if (len % 2) {
        if (!isxdigit(*ip))
            goto hex_to_binary2_quit;
        *op++ = TOOLS_HEX2VAL(*ip);
        ip++;
    }

    while (ip < input + len) {
        if (!isxdigit(*ip))
            goto hex_to_binary2_quit;
        *op = TOOLS_HEX2VAL(*ip) << 4;
        ip++;

        if (!isxdigit(*ip))
            goto hex_to_binary2_quit;
        *op++ += TOOLS_HEX2VAL(*ip);
        ip++;
    }

    *output = s;
    return olen;

  hex_to_binary2_quit:
    Tools_freeZero(s, olen);
    return -1;

}                               /* end hex_to_binary2() */

int Tools_decimalToBinary(u_char ** buf, size_t * buf_len, size_t * out_len,
                       int allow_realloc, const char *decimal)
{
    int             subid = 0;
    const char     *cp = decimal;

    if (buf == NULL || buf_len == NULL || out_len == NULL
        || decimal == NULL) {
        return 0;
    }

    while (*cp != '\0') {
        if (isspace((int) *cp) || *cp == '.') {
            cp++;
            continue;
        }
        if (!isdigit((int) *cp)) {
            return 0;
        }
        if ((subid = atoi(cp)) > 255) {
            return 0;
        }
        if ((*out_len >= *buf_len) &&
            !(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
        *(*buf + *out_len) = (u_char) subid;
        (*out_len)++;
        while (isdigit((int) *cp)) {
            cp++;
        }
    }
    return 1;
}


/**
 * convert an ASCII hex string (with specified delimiters) to binary
 *
 * @param buf     address of a pointer (pointer to pointer) for the output buffer.
 *                If allow_realloc is set, the buffer may be grown via snmp_realloc
 *                to accomodate the data.
 *
 * @param buf_len pointer to a size_t containing the initial size of buf.
 *
 * @param offset On input, a pointer to a size_t indicating an offset into buf.
 *                The  binary data will be stored at this offset.
 *                On output, this pointer will have updated the offset to be
 *                the first byte after the converted data.
 *
 * @param allow_realloc If true, the buffer can be reallocated. If false, and
 *                      the buffer is not large enough to contain the string,
 *                      an error will be returned.
 *
 * @param hex     pointer to hex string to be converted. May be prefixed by
 *                "0x" or "0X".
 *
 * @param delim   point to a string of allowed delimiters between bytes.
 *                If not specified, any non-hex characters will be an error.
 *
 * @retval 1  success
 * @retval 0  error
 */
int Tools_hexToBinary(u_char ** buf, size_t * buf_len, size_t * offset,
                      int allow_realloc, const char *hex, const char *delim)
{
    unsigned int    subid = 0;
    const char     *cp = hex;

    if (buf == NULL || buf_len == NULL || offset == NULL || hex == NULL) {
        return 0;
    }

    if ((*cp == '0') && ((*(cp + 1) == 'x') || (*(cp + 1) == 'X'))) {
        cp += 2;
    }

    while (*cp != '\0') {
        if (!isxdigit((int) *cp) ||
            !isxdigit((int) *(cp+1))) {
            if ((NULL != delim) && (NULL != strchr(delim, *cp))) {
                cp++;
                continue;
            }
            return 0;
        }
        if (sscanf(cp, "%2x", &subid) == 0) {
            return 0;
        }
        /*
         * if we dont' have enough space, realloc.
         * (snmp_realloc will adjust buf_len to new size)
         */
        if ((*offset >= *buf_len) &&
            !(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
        *(*buf + *offset) = (u_char) subid;
        (*offset)++;
        if (*++cp == '\0') {
            /*
             * Odd number of hex digits is an error.
             */
            return 0;
        } else {
            cp++;
        }
    }
    return 1;
}


/**
 * convert an ASCII hex string to binary
 *
 * @note This is a wrapper which calls Tools_hexToBinary with a
 * delimiter string of " ".
 *
 * See netsnmp_hex_to_binary for parameter descriptions.
 *
 * @retval 1  success
 * @retval 0  error
 */
int Tools_hexToBinary1(u_char ** buf, size_t * buf_len, size_t * offset,
                   int allow_realloc, const char *hex)
{
    return Tools_hexToBinary(buf, buf_len, offset, allow_realloc, hex, " ");
}


/*******************************************************************-o-******
 * dump_chunk
 *
 * Parameters:
 *	*title	(May be NULL.)
 *	*buf
 *	 size
 */
void Tools_dumpChunk(const char *debugtoken, const char *title, const u_char * buf, int size)
{
    int             printunit = 64;     /* XXX  Make global. */
    char            chunk[TOOLS_MAXBUF], *s, *sp;

    if (title && (*title != '\0')) {
        DEBUG_MSGTL((debugtoken, "%s\n", title));
    }


    memset(chunk, 0, TOOLS_MAXBUF);
    size = Tools_binaryToHex(buf, size, &s);
    sp = s;

    while (size > 0) {
        if (size > printunit) {
            memcpy(chunk, sp, printunit);
            chunk[printunit] = '\0';
            DEBUG_MSGTL((debugtoken, "\t%s\n", chunk));
        } else {
            DEBUG_MSGTL((debugtoken, "\t%s\n", sp));
        }

        sp += printunit;
        size -= printunit;
    }

    TOOLS_FREE(s);

}                               /* end dump_chunk() */


/**
 * Create a new real-time marker.
 *
 * \deprecated Use netsnmp_set_monotonic_marker() instead.
 *
 * @note Caller must free time marker when no longer needed.
 */
markerT Tools_atimeNewMarker(void)
{
    markerT        pm = (markerT) calloc(1, sizeof(struct timeval));
    gettimeofday((struct timeval *) pm, NULL);
    return pm;
}

/**
 * Set a time marker to the current value of the real-time clock.
 * \deprecated Use netsnmp_set_monotonic_marker() instead.
 */
void Tools_atimeSetMarker(markerT pm)
{
    if (!pm)
        return;

    gettimeofday((struct timeval *) pm, NULL);
}


/**
 * Query the current value of the monotonic clock.
 *
 * Returns the current value of a monotonic clock if such a clock is provided by
 * the operating system or the wall clock time if no such clock is provided by
 * the operating system. A monotonic clock is a clock that is never adjusted
 * backwards and that proceeds at the same rate as wall clock time.
 *
 * @param[out] tv Pointer to monotonic clock time.
 */
void Tools_getMonotonicClock(struct timeval* tv)
{

    /* At least FreeBSD 4 doesn't provide monotonic clock support. */
#warning Not sure how to query a monotonically increasing clock on your system. \
Timers will not work correctly if the system clock is adjusted by e.g. ntpd.
    gettimeofday(tv, NULL);
}

/**
 * Set a time marker to the current value of the monotonic clock.
 */
void Tools_setMonotonicMarker(markerT *pm)
{
    if (!*pm)
        *pm = malloc(sizeof(struct timeval));
    if (*pm)
        Tools_getMonotonicClock((struct timeval* )*pm);
}


/**
 * Returns the difference (in msec) between the two markers
 *
 * \deprecated Don't use in new code.
 */
long Tools_atimeDiff(constMarkerT first, constMarkerT second)
{
    struct timeval diff;

    TOOLS_TIMERSUB((const struct timeval *) second, (const struct timeval *) first, &diff);

    return (long)(diff.tv_sec * 1000 + diff.tv_usec / 1000);
}

/**
 * Returns the difference (in u_long msec) between the two markers
 *
 * \deprecated Don't use in new code.
 */
u_long Tools_uatimeDiff(constMarkerT first, constMarkerT second)
{
    struct timeval diff;

    TOOLS_TIMERSUB((const struct timeval *) second, (const struct timeval *) first, &diff);

    return (((u_long) diff.tv_sec) * 1000 + diff.tv_usec / 1000);
}

/**
 * Returns the difference (in u_long 1/100th secs) between the two markers
 * (functionally this is what sysUpTime needs)
 *
 * \deprecated Don't use in new code.
 */
u_long Tools_uatimeHdiff(constMarkerT first, constMarkerT second)
{
    struct timeval diff;

    TOOLS_TIMERSUB((const struct timeval *) second, (const struct timeval *) first, &diff);
    return ((u_long) diff.tv_sec) * 100 + diff.tv_usec / 10000;
}

/**
 * Test: Has (marked time plus delta) exceeded current time ?
 * Returns 0 if test fails or cannot be tested (no marker).
 *
 * \deprecated Use netsnmp_ready_monotonic() instead.
 */
int Tools_atimeReady(constMarkerT pm, int delta_ms)
{
    markerT        now;
    long            diff;
    if (!pm)
        return 0;

    now = Tools_atimeNewMarker();

    diff = Tools_atimeDiff(pm, now);
    free(now);
    if (diff < delta_ms)
        return 0;

    return 1;
}


/**
 * Test: Has (marked time plus delta) exceeded current time ?
 * Returns 0 if test fails or cannot be tested (no marker).
 *
 * \deprecated Use netsnmp_ready_monotonic() instead.
 */
int Tools_uatimeReady(constMarkerT pm, unsigned int delta_ms)
{
    markerT        now;
    u_long          diff;
    if (!pm)
        return 0;

    now = Tools_atimeNewMarker();

    diff = Tools_uatimeDiff(pm, now);
    free(now);
    if (diff < delta_ms)
        return 0;

    return 1;
}


/**
 * Is the current time past (marked time plus delta) ?
 *
 * @param[in] pm Pointer to marked time as obtained via
 *   netsnmp_set_monotonic_marker().
 * @param[in] delta_ms Time delta in milliseconds.
 *
 * @return pm != NULL && now >= (*pm + delta_ms)
 */
int Tools_readyMonotonic(constMarkerT pm, int delta_ms)
{
    struct timeval  now, diff, delta;

    Assert_assert(delta_ms >= 0);
    if (pm) {
        Tools_getMonotonicClock(&now);
        TOOLS_TIMERSUB(&now, (const struct timeval *) pm, &diff);
        delta.tv_sec = delta_ms / 1000;
        delta.tv_usec = (delta_ms % 1000) * 1000UL;
        return timercmp(&diff, &delta, >=) ? TRUE : FALSE;
    } else {
        return FALSE;
    }
}



/*
 * Time-related utility functions
 */

/**
* Return the number of timeTicks since the given marker
*
* \deprecated Don't use in new code.
*/
int Tools_markerTticks(constMarkerT pm)
{
    int             res;
    markerT        now = Tools_atimeNewMarker();

    res = Tools_atimeDiff(pm, now);
    free(now);
    return res / 10;            /* atime_diff works in msec, not csec */
}


/**
 * \deprecated Don't use in new code.
 */
int Tools_timevalTticks(const struct timeval *tv)
{
    return Tools_markerTticks((constMarkerT) tv);
}

/**
 * Non Windows:  Returns a pointer to the desired environment variable
 *               or NULL if the environment variable does not exist.
 *
 * Windows:      Returns a pointer to the desired environment variable
 *               if it exists.  If it does not, the variable is looked up
 *               in the registry in HKCU\\Net-SNMP or HKLM\\Net-SNMP
 *               (whichever it finds first) and stores the result in the
 *               environment variable.  It then returns a pointer to
 *               environment variable.
 */

char * Tools_getenv(const char *name)
{
  return (getenv(name));
}

/**
 * Set an environment variable.
 *
 * This function is only necessary on Windows for the MSVC and MinGW
 * environments. If the process that uses the Net-SNMP DLL (e.g. a Perl
 * interpreter) and the Net-SNMP have been built with a different compiler
 * version then each will have a separate set of environment variables.
 * This function allows to set an environment variable such that it gets
 * noticed by the Net-SNMP DLL.
 */
int Tools_setenv(const char *envname, const char *envval, int overwrite)
{
    return setenv(envname, envval, overwrite);
}


/*
 * swap the order of an inet addr string
 */
int Tools_addrstrHton(char *ptr, size_t len)
{
    char tmp[8];

    if (8 == len) {
        tmp[0] = ptr[6];
        tmp[1] = ptr[7];
        tmp[2] = ptr[4];
        tmp[3] = ptr[5];
        tmp[4] = ptr[2];
        tmp[5] = ptr[3];
        tmp[6] = ptr[0];
        tmp[7] = ptr[1];
        memcpy (ptr, &tmp, 8);
    }
    else if (32 == len) {
        Tools_addrstrHton(ptr   , 8);
        Tools_addrstrHton(ptr+8 , 8);
        Tools_addrstrHton(ptr+16, 8);
        Tools_addrstrHton(ptr+24, 8);
    }
    else
        return -1;


    return 0;
}


/**
 * Takes a time string like 4h and converts it to seconds.
 * The string time given may end in 's' for seconds (the default
 * anyway if no suffix is specified),
 * 'm' for minutes, 'h' for hours, 'd' for days, or 'w' for weeks.  The
 * upper case versions are also accepted.
 *
 * @param time_string The time string to convert.
 *
 * @return seconds converted from the string
 * @return -1  : on failure
 */
int Tools_stringTimeToSecs(const char *time_string) {
    int secs = -1;
    if (!time_string || !time_string[0])
        return secs;

    secs = atoi(time_string);

    if (isdigit((unsigned char)time_string[strlen(time_string)-1]))
        return secs; /* no letter specified, it's already in seconds */

    switch (time_string[strlen(time_string)-1]) {
    case 's':
    case 'S':
        /* already in seconds */
        break;

    case 'm':
    case 'M':
        secs = secs * 60;
        break;

    case 'h':
    case 'H':
        secs = secs * 60 * 60;
        break;

    case 'd':
    case 'D':
        secs = secs * 60 * 60 * 24;
        break;

    case 'w':
    case 'W':
        secs = secs * 60 * 60 * 24 * 7;
        break;

    default:
        Logger_log(LOGGER_PRIORITY_ERR, "time string %s contains an invalid suffix letter\n",
                 time_string);
        return -1;
    }

    DEBUG_MSGTL(("string_time_to_secs", "Converted time string %s to %d\n",
                time_string, secs));
    return secs;
}
