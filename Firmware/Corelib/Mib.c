#include "Mib.h"

#include "DefaultStore.h"
#include "Tools.h"
#include "Asn01.h"
#include "Int64.h"
#include "Debug.h"
#include "Parse.h"
#include "System.h"
#include "Priot.h"
#include "Logger.h"
#include "Parse.h"
#include "ReadConfig.h"
#include "Strlcat.h"
#include "Strlcpy.h"
#include "Api.h"
#include "Assert.h"
#include "Client.h"
#include "Impl.h"



#define MIB_NAMLEN(dirent) strlen((dirent)->d_name)





/** @defgroup mib_utilities mib parsing and datatype manipulation routines.
 *  @ingroup library
 *
 */


static char * Mib_uptimeString2(u_long, char *, size_t);

static struct Parse_Tree_s * Mib_getReallocSymbol(const oid * objid, size_t objidlen,
                                        struct Parse_Tree_s *subtree,
                                        u_char ** buf, size_t * buf_len,
                                        size_t * out_len,
                                        int allow_realloc,
                                        int *buf_overflow,
                                        struct Parse_IndexList_s *in_dices,
                                        size_t * end_of_known);

static int  Mib_printTreeNode(u_char ** buf, size_t * buf_len,
                                size_t * out_len, int allow_realloc,
                                struct Parse_Tree_s *tp, int width);

static void Mib_handleMibDirsConf(const char *token, char *line);
static void Mib_handleMibsConf(const char *token, char *line);
static void Mib_handleMibFileConf(const char *token, char *line);

static void Mib_oidFinishPrinting( const oid * objid, size_t objidlen,
                                     u_char ** buf, size_t * buf_len,
                                     size_t * out_len,
                                     int allow_realloc, int *buf_overflow);

/*
 * helper functions for get_module_node
 */
static int  Mib_nodeToOid(struct Parse_Tree_s *, oid *, size_t *);

static int  Mib_addStringsToOid(struct Parse_Tree_s *, char *,
                                oid *, size_t *, size_t);

static struct Parse_Tree_s * mib_treeTop;

struct Parse_Tree_s * mib_mib;  /* Backwards compatibility */

extern struct Parse_Tree_s *parse_treeHead;

oid   mib_rFC1213_MIB[] = { 1, 3, 6, 1, 2, 1 };
static char mib_standardPrefix[] = ".1.3.6.1.2.1";

/*
 * Set default here as some uses of read_objid require valid pointer.
 */
static char *mib_prefix = &mib_standardPrefix[0];

typedef struct Mib_PrefixList_s {
    const char     *str;
    int             len;
} *Mib_PrefixListPTR, Mib_PrefixList;

/*
 * Here are the prefix strings.
 * Note that the first one finds the value of Prefix or Standard_Prefix.
 * Any of these MAY start with period; all will NOT end with period.
 * Period is added where needed.  See use of Prefix in this module.
 */
Mib_PrefixList  mib_prefixes[] = {
    {&mib_standardPrefix[0]},      /* placeholder for Prefix data */
    {".iso.org.dod.internet.mgmt.mib-2"},
    {".iso.org.dod.internet.experimental"},
    {".iso.org.dod.internet.private"},
    {".iso.org.dod.internet.snmpParties"},
    {".iso.org.dod.internet.snmpSecrets"},
    {NULL, 0}                   /* end of list */
};

enum Mib_InetAddressType {
    IPV4   = 1,
    IPV6   = 2,
    IPV4Z  = 3,
    IPV6Z  = 4,
    DNS    = 16
};

/**
 * @internal
 * Converts timeticks to hours, minutes, seconds string.
 *
 * @param timeticks    The timeticks to convert.
 * @param buf          Buffer to write to, has to be at
 *                     least 40 Bytes large.
 *
 * @return The buffer.
 */
static char * Mib_uptimeString2(u_long timeticks, char *buf, size_t buflen)
{
    int centisecs, seconds, minutes, hours, days;

    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.NUMERIC_TIMETICKS)) {
        snprintf(buf, buflen, "%lu", timeticks);
        return buf;
    }


    centisecs = timeticks % 100;
    timeticks /= 100;
    days = timeticks / (60 * 60 * 24);
    timeticks %= (60 * 60 * 24);

    hours = timeticks / (60 * 60);
    timeticks %= (60 * 60);

    minutes = timeticks / 60;
    seconds = timeticks % 60;

    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT))
        snprintf(buf, buflen, "%d:%d:%02d:%02d.%02d", days, hours, minutes, seconds, centisecs);
    else {
        if (days == 0) {
            snprintf(buf, buflen, "%d:%02d:%02d.%02d",
                    hours, minutes, seconds, centisecs);
        } else if (days == 1) {
            snprintf(buf, buflen, "%d day, %d:%02d:%02d.%02d",
                    days, hours, minutes, seconds, centisecs);
        } else {
            snprintf(buf, buflen, "%d days, %d:%02d:%02d.%02d",
                    days, hours, minutes, seconds, centisecs);
        }
    }
    return buf;
}


/**
 * @internal
 * Prints the character pointed to if in human-readable ASCII range,
 * otherwise prints a dot.
 *
 * @param buf Buffer to print the character to.
 * @param ch  Character to print.
 */
static void Mib_sprintChar(char *buf, const u_char ch)
{
    if (isprint(ch) || isspace(ch)) {
        sprintf(buf, "%c", (int) ch);
    } else {
        sprintf(buf, ".");
    }
}


/**
 * Prints a hexadecimal string into a buffer.
 *
 * The characters pointed by *cp are encoded as hexadecimal string.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      address of the buffer to print to.
 * @param buf_len  address to an integer containing the size of buf.
 * @param out_len  incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param cp       the array of characters to encode.
 * @param line_len the array length of cp.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintHexStringLine(u_char ** buf, size_t * buf_len, size_t * out_len,
                       int allow_realloc, const u_char * cp, size_t line_len)
{
    const u_char   *tp;
    const u_char   *cp2 = cp;
    size_t          lenleft = line_len;

    /*
     * Make sure there's enough room for the hex output....
     */
    while ((*out_len + line_len*3+1) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
    }

    /*
     * .... and display the hex values themselves....
     */
    for (; lenleft >= 8; lenleft-=8) {
        sprintf((char *) (*buf + *out_len),
                "%02X %02X %02X %02X %02X %02X %02X %02X ", cp[0], cp[1],
                cp[2], cp[3], cp[4], cp[5], cp[6], cp[7]);
        *out_len += strlen((char *) (*buf + *out_len));
        cp       += 8;
    }
    for (; lenleft > 0; lenleft--) {
        sprintf((char *) (*buf + *out_len), "%02X ", *cp++);
        *out_len += strlen((char *) (*buf + *out_len));
    }

    /*
     * .... plus (optionally) do the same for the ASCII equivalent.
     */
    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.PRINT_HEX_TEXT)) {
        while ((*out_len + line_len+5) >= *buf_len) {
            if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
                return 0;
            }
        }
        sprintf((char *) (*buf + *out_len), "  [");
        *out_len += strlen((char *) (*buf + *out_len));
        for (tp = cp2; tp < cp; tp++) {
            Mib_sprintChar((char *) (*buf + *out_len), *tp);
            (*out_len)++;
        }
        sprintf((char *) (*buf + *out_len), "]");
        *out_len += strlen((char *) (*buf + *out_len));
    }
    return 1;
}


int Mib_sprintReallocHexString(u_char ** buf, size_t * buf_len, size_t * out_len,
                             int allow_realloc, const u_char * cp, size_t len)
{
    int line_len = DefaultStore_getInt(DSSTORAGE.LIBRARY_ID,
                                       DSLIB_INTEGER.HEX_OUTPUT_LENGTH);
    if (!line_len)
        line_len=len;

    for (; (int)len > line_len; len -= line_len) {
        if(!Mib_sprintHexStringLine(buf, buf_len, out_len, allow_realloc, cp, line_len))
            return 0;
        *(*buf + (*out_len)++) = '\n';
        *(*buf + *out_len) = 0;
        cp += line_len;
    }
    if(!Mib_sprintHexStringLine(buf, buf_len, out_len, allow_realloc, cp, len))
        return 0;
    *(*buf + *out_len) = 0;
    return 1;
}

/**
 * Prints an ascii string into a buffer.
 *
 * The characters pointed by *cp are encoded as an ascii string.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      address of the buffer to print to.
 * @param buf_len  address to an integer containing the size of buf.
 * @param out_len  incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param cp       the array of characters to encode.
 * @param len      the array length of cp.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocAsciiString(u_char ** buf, size_t * buf_len,
                           size_t * out_len, int allow_realloc,
                           const u_char * cp, size_t len)
{
    int             i;

    for (i = 0; i < (int) len; i++) {
        if (isprint(*cp) || isspace(*cp)) {
            if (*cp == '\\' || *cp == '"') {
                if ((*out_len >= *buf_len) &&
                    !(allow_realloc && Tools_realloc2(buf, buf_len))) {
                    return 0;
                }
                *(*buf + (*out_len)++) = '\\';
            }
            if ((*out_len >= *buf_len) &&
                !(allow_realloc && Tools_realloc2(buf, buf_len))) {
                return 0;
            }
            *(*buf + (*out_len)++) = *cp++;
        } else {
            if ((*out_len >= *buf_len) &&
                !(allow_realloc && Tools_realloc2(buf, buf_len))) {
                return 0;
            }
            *(*buf + (*out_len)++) = '.';
            cp++;
        }
    }
    if ((*out_len >= *buf_len) &&
        !(allow_realloc && Tools_realloc2(buf, buf_len))) {
        return 0;
    }
    *(*buf + *out_len) = '\0';
    return 1;
}


/**
 * Prints an octet string into a buffer.
 *
 * The variable var is encoded as octet string.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocOctetString(u_char ** buf, size_t * buf_len,
                            size_t * out_len, int allow_realloc,
                            const Types_VariableList * var,
                            const struct Parse_EnumList_s *enums, const char *hint,
                            const char *units)
{
    size_t          saved_out_len = *out_len;
    const char     *saved_hint = hint;
    int             hex = 0, x = 0;
    u_char         *cp;
    int             output_format, cnt;

    if (var->type != ASN01_OCTET_STR) {
        if (!DefaultStore_getBoolean( DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            const char      str[] = "Wrong Type (should be OCTET STRING): ";
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }


    if (hint) {
        int             repeat, width = 1;
        long            value;
        char            code = 'd', separ = 0, term = 0, ch, intbuf[32];
#define HEX2DIGIT_NEED_INIT 3
        char            hex2digit = HEX2DIGIT_NEED_INIT;
        u_char         *ecp;

        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "STRING: ")) {
                return 0;
            }
        }
        cp = var->val.string;
        ecp = cp + var->valLen;

        while (cp < ecp) {
            repeat = 1;
            if (*hint) {
                if (*hint == '*') {
                    repeat = *cp++;
                    hint++;
                }
                width = 0;
                while ('0' <= *hint && *hint <= '9')
                    width = (width * 10) + (*hint++ - '0');
                code = *hint++;
                if ((ch = *hint) && ch != '*' && (ch < '0' || ch > '9')
                    && (width != 0
                        || (ch != 'x' && ch != 'd' && ch != 'o')))
                    separ = *hint++;
                else
                    separ = 0;
                if ((ch = *hint) && ch != '*' && (ch < '0' || ch > '9')
                    && (width != 0
                        || (ch != 'x' && ch != 'd' && ch != 'o')))
                    term = *hint++;
                else
                    term = 0;
                if (width == 0)  /* Handle malformed hint strings */
                    width = 1;
            }

            while (repeat && cp < ecp) {
                value = 0;
                if (code != 'a' && code != 't') {
                    for (x = 0; x < width; x++) {
                        value = value * 256 + *cp++;
                    }
                }
                switch (code) {
                case 'x':
                    if (HEX2DIGIT_NEED_INIT == hex2digit)
                        hex2digit = DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                                                           DSLIB_BOOLEAN.LIB_2DIGIT_HEX_OUTPUT);
                    /*
                     * if value is < 16, it will be a single hex digit. If the
                     * width is 1 (we are outputting a byte at a time), pat it
                     * to 2 digits if NETSNMP_DS_LIB_2DIGIT_HEX_OUTPUT is set
                     * or all of the following are true:
                     *  - we do not have a separation character
                     *  - there is no hint left (or there never was a hint)
                     *
                     * e.g. for the data 0xAA01BB, would anyone really ever
                     * want the string "AA1BB"??
                     */
                    if (((value < 16) && (1 == width)) &&
                        (hex2digit || ((0 == separ) && (0 == *hint)))) {
                        sprintf(intbuf, "0%lx", value);
                    } else {
                        sprintf(intbuf, "%lx", value);
                    }
                    if (!Tools_cstrcat
                        (buf, buf_len, out_len, allow_realloc, intbuf)) {
                        return 0;
                    }
                    break;
                case 'd':
                    sprintf(intbuf, "%ld", value);
                    if (!Tools_cstrcat
                        (buf, buf_len, out_len, allow_realloc, intbuf)) {
                        return 0;
                    }
                    break;
                case 'o':
                    sprintf(intbuf, "%lo", value);
                    if (!Tools_cstrcat
                        (buf, buf_len, out_len, allow_realloc, intbuf)) {
                        return 0;
                    }
                    break;
                case 't': /* new in rfc 3411 */
                case 'a':
                    cnt = TOOLS_MIN(width, ecp - cp);
                    if (!Mib_sprintReallocAsciiString(buf, buf_len, out_len,
                                                    allow_realloc, cp, cnt))
                        return 0;
                    cp += cnt;
                    break;
                default:
                    *out_len = saved_out_len;
                    if (Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                                     "(Bad hint ignored: ")
                        && Tools_cstrcat(buf, buf_len, out_len,
                                       allow_realloc, saved_hint)
                        && Tools_cstrcat(buf, buf_len, out_len,
                                       allow_realloc, ") ")) {
                        return Mib_sprintReallocOctetString(buf, buf_len,
                                                           out_len,
                                                           allow_realloc,
                                                           var, enums,
                                                           NULL, NULL);
                    } else {
                        return 0;
                    }
                }

                if (cp < ecp && separ) {
                    while ((*out_len + 1) >= *buf_len) {
                        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
                            return 0;
                        }
                    }
                    *(*buf + *out_len) = separ;
                    (*out_len)++;
                    *(*buf + *out_len) = '\0';
                }
                repeat--;
            }

            if (term && cp < ecp) {
                while ((*out_len + 1) >= *buf_len) {
                    if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
                        return 0;
                    }
                }
                *(*buf + *out_len) = term;
                (*out_len)++;
                *(*buf + *out_len) = '\0';
            }
        }

        if (units) {
            return (Tools_cstrcat
                    (buf, buf_len, out_len, allow_realloc, " ")
                    && Tools_cstrcat(buf, buf_len, out_len, allow_realloc, units));
        }
        if ((*out_len >= *buf_len) &&
            !(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
        *(*buf + *out_len) = '\0';

        return 1;
    }

    output_format = DefaultStore_getInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.STRING_OUTPUT_FORMAT);
    if (0 == output_format) {
        output_format = MIB_STRING_OUTPUT_GUESS;
    }
    switch (output_format) {
    case MIB_STRING_OUTPUT_GUESS:
        hex = 0;
        for (cp = var->val.string, x = 0; x < (int) var->valLen; x++, cp++) {
            if (!isprint(*cp) && !isspace(*cp)) {
                hex = 1;
            }
        }
        break;

    case MIB_STRING_OUTPUT_ASCII:
        hex = 0;
        break;

    case MIB_STRING_OUTPUT_HEX:
        hex = 1;
        break;
    }

    if (var->valLen == 0) {
        return Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\"\"");
    }

    if (hex) {
        if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\"")) {
                return 0;
            }
        } else {
            if (!Tools_cstrcat
                (buf, buf_len, out_len, allow_realloc, "Hex-STRING: ")) {
                return 0;
            }
        }

        if (!Mib_sprintReallocHexString(buf, buf_len, out_len, allow_realloc,
                                      var->val.string, var->valLen)) {
            return 0;
        }

        if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\"")) {
                return 0;
            }
        }
    } else {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                             "STRING: ")) {
                return 0;
            }
        }
        if (!Tools_cstrcat
            (buf, buf_len, out_len, allow_realloc, "\"")) {
            return 0;
        }
        if (!Mib_sprintReallocAsciiString
            (buf, buf_len, out_len, allow_realloc, var->val.string,
             var->valLen)) {
            return 0;
        }
        if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\"")) {
            return 0;
        }
    }

    if (units) {
        return (Tools_cstrcat(buf, buf_len, out_len, allow_realloc, " ")
                && Tools_cstrcat(buf, buf_len, out_len, allow_realloc, units));
    }
    return 1;
}


/**
 * Prints a float into a buffer.
 *
 * The variable var is encoded as a floating point value.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocFloat(u_char ** buf, size_t * buf_len,
                     size_t * out_len, int allow_realloc,
                     const Types_VariableList * var,
                     const struct Parse_EnumList_s *enums,
                     const char *hint, const char *units)
{
    if (var->type != ASN01_OPAQUE_FLOAT) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be Float): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len, allow_realloc, var, NULL, NULL, NULL);
    }

    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        if (!Tools_cstrcat
            (buf, buf_len, out_len, allow_realloc, "Opaque: Float: ")) {
            return 0;
        }
    }


    /*
     * How much space needed for max. length float?  128 is overkill.
     */

    while ((*out_len + 128 + 1) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
    }

    sprintf((char *) (*buf + *out_len), "%f", *var->val.floatVal);
    *out_len += strlen((char *) (*buf + *out_len));

    if (units) {
        return (Tools_cstrcat(buf, buf_len, out_len, allow_realloc, " ")
                && Tools_cstrcat(buf, buf_len, out_len, allow_realloc, units));
    }
    return 1;
}


/**
 * Prints a double into a buffer.
 *
 * The variable var is encoded as a double precision floating point value.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocDouble(u_char ** buf, size_t * buf_len,
                      size_t * out_len, int allow_realloc,
                      const Types_VariableList * var,
                      const struct Parse_EnumList_s *enums,
                      const char *hint, const char *units)
{
    if (var->type != ASN01_OPAQUE_DOUBLE) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be Double): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        if (!Tools_cstrcat
            (buf, buf_len, out_len, allow_realloc, "Opaque: Float: ")) {
            return 0;
        }
    }

    /*
     * How much space needed for max. length double?  128 is overkill.
     */

    while ((*out_len + 128 + 1) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
    }

    sprintf((char *) (*buf + *out_len), "%f", *var->val.doubleVal);
    *out_len += strlen((char *) (*buf + *out_len));

    if (units) {
        return (Tools_cstrcat
                (buf, buf_len, out_len, allow_realloc, " ")
                && Tools_cstrcat(buf, buf_len, out_len, allow_realloc, units));
    }
    return 1;
}

/**
 * Prints a counter into a buffer.
 *
 * The variable var is encoded as a counter value.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocCounter64(u_char ** buf, size_t * buf_len, size_t * out_len,
                         int allow_realloc,
                         const Types_VariableList * var,
                         const struct Parse_EnumList_s *enums,
                         const char *hint, const char *units)
{
    char            a64buf[INT64_I64CHARSZ + 1];

    if (var->type != ASN01_COUNTER64
        && var->type != ASN01_OPAQUE_COUNTER64
        && var->type != ASN01_OPAQUE_I64 && var->type != ASN01_OPAQUE_U64
        ) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be Counter64): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        if (var->type != ASN01_COUNTER64) {
            if (!Tools_cstrcat
                (buf, buf_len, out_len, allow_realloc, "Opaque: ")) {
                return 0;
            }
        }
        switch (var->type) {
        case ASN01_OPAQUE_U64:
            if (!Tools_cstrcat
                (buf, buf_len, out_len, allow_realloc, "UInt64: ")) {
                return 0;
            }
            break;
        case ASN01_OPAQUE_I64:
            if (!Tools_cstrcat
                (buf, buf_len, out_len, allow_realloc, "Int64: ")) {
                return 0;
            }
            break;
        case ASN01_COUNTER64:
        case ASN01_OPAQUE_COUNTER64:
            if (!Tools_cstrcat
                (buf, buf_len, out_len, allow_realloc, "Counter64: ")) {
                return 0;
            }
        }
    }
    if (var->type == ASN01_OPAQUE_I64) {
        Int64_printI64(a64buf, var->val.counter64);
        if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, a64buf)) {
            return 0;
        }
    } else {
        Int64_printU64(a64buf, var->val.counter64);
        if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, a64buf)) {
            return 0;
        }
    }
    if (units) {
        return (Tools_cstrcat(buf, buf_len, out_len, allow_realloc, " ")
                && Tools_cstrcat(buf, buf_len, out_len, allow_realloc, units));
    }
    return 1;
}


/**
 * Prints an object identifier into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocOpaque(u_char ** buf, size_t * buf_len,
                      size_t * out_len, int allow_realloc,
                      const Types_VariableList * var,
                      const struct Parse_EnumList_s *enums,
                      const char *hint, const char *units)
{
    if (var->type != ASN01_OPAQUE
        && var->type != ASN01_OPAQUE_COUNTER64
        && var->type != ASN01_OPAQUE_U64
        && var->type != ASN01_OPAQUE_I64
        && var->type != ASN01_OPAQUE_FLOAT && var->type != ASN01_OPAQUE_DOUBLE
        ) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be Opaque): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len, allow_realloc, var, NULL, NULL, NULL);
    }
    switch (var->type) {
    case ASN01_OPAQUE_COUNTER64:
    case ASN01_OPAQUE_U64:
    case ASN01_OPAQUE_I64:
        return Mib_sprintReallocCounter64(buf, buf_len, out_len, allow_realloc, var, enums, hint, units);
        break;

    case ASN01_OPAQUE_FLOAT:
        return Mib_sprintReallocFloat(buf, buf_len, out_len, allow_realloc,
                                    var, enums, hint, units);
        break;

    case ASN01_OPAQUE_DOUBLE:
        return Mib_sprintReallocDouble(buf, buf_len, out_len, allow_realloc,
                                     var, enums, hint, units);
        break;

    case ASN01_OPAQUE:
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
            u_char          str[] = "OPAQUE: ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str)) {
                return 0;
            }
        }
        if (!Mib_sprintReallocHexString(buf, buf_len, out_len, allow_realloc,
                                      var->val.string, var->valLen)) {
            return 0;
        }
    }
    if (units) {
        return (Tools_strcat
                (buf, buf_len, out_len, allow_realloc,
                 (const u_char *) " ")
                && Tools_strcat(buf, buf_len, out_len, allow_realloc,
                               (const u_char *) units));
    }
    return 1;
}


/**
 * Prints an object identifier into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocObjectIdentifier(u_char ** buf, size_t * buf_len,
                                 size_t * out_len, int allow_realloc,
                                 const Types_VariableList * var,
                                 const struct Parse_EnumList_s *enums,
                                 const char *hint, const char *units)
{
    int buf_overflow = 0;

    if (var->type != ASN01_OBJECT_ID) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be OBJECT IDENTIFIER): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        u_char          str[] = "OID: ";
        if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str)) {
            return 0;
        }
    }

    Mib_sprintReallocObjidTree(buf, buf_len, out_len, allow_realloc,
                                      &buf_overflow,
                                      (oid *) (var->val.objid),
                                      var->valLen / sizeof(oid));

    if (buf_overflow) {
        return 0;
    }

    if (units) {
        return (Tools_strcat
                (buf, buf_len, out_len, allow_realloc,
                 (const u_char *) " ")
                && Tools_strcat(buf, buf_len, out_len, allow_realloc,
                               (const u_char *) units));
    }
    return 1;
}


/**
 * Prints a timetick variable into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocTimeTicks(u_char ** buf, size_t * buf_len, size_t * out_len,
                         int allow_realloc,
                         const Types_VariableList * var,
                         const struct Parse_EnumList_s *enums,
                         const char *hint, const char *units)
{
    char timebuf[40];

    if (var->type != ASN01_TIMETICKS) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be Timeticks): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.NUMERIC_TIMETICKS)) {
        char            str[32];
        sprintf(str, "%lu", *(u_long *) var->val.integer);
        if (!Tools_strcat
            (buf, buf_len, out_len, allow_realloc, (const u_char *) str)) {
            return 0;
        }
        return 1;
    }
    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        char            str[32];
        sprintf(str, "Timeticks: (%lu) ", *(u_long *) var->val.integer);
        if (!Tools_strcat
            (buf, buf_len, out_len, allow_realloc, (const u_char *) str)) {
            return 0;
        }
    }
    Mib_uptimeString2(*(u_long *) (var->val.integer), timebuf, sizeof(timebuf));
    if (!Tools_strcat (buf, buf_len, out_len, allow_realloc, (const u_char *) timebuf)) {
        return 0;
    }
    if (units) {
        return (Tools_strcat
                (buf, buf_len, out_len, allow_realloc,
                 (const u_char *) " ")
                && Tools_strcat(buf, buf_len, out_len, allow_realloc,
                               (const u_char *) units));
    }
    return 1;
}



/**
 * Prints an integer according to the hint into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param val      The variable to encode.
 * @param decimaltype 'd' or 'u' depending on integer type
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may _NOT_ be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocHintedInteger(u_char ** buf, size_t * buf_len,
                              size_t * out_len, int allow_realloc,
                              long val, const char decimaltype,
                              const char *hint, const char *units)
{
    char            fmt[10] = "%l@", tmp[256];
    int             shift = 0, len, negative = 0;

    if (hint[0] == 'd') {
        /*
         * We might *actually* want a 'u' here.
         */
        if (hint[1] == '-')
            shift = atoi(hint + 2);
        fmt[2] = decimaltype;
        if (val < 0) {
            negative = 1;
            val = -val;
        }
    } else {
        /*
         * DISPLAY-HINT character is 'b', 'o', or 'x'.
         */
        fmt[2] = hint[0];
    }

    if (hint[0] == 'b') {
    unsigned long int bit = 0x80000000LU;
    char *bp = tmp;
    while (bit) {
        *bp++ = val & bit ? '1' : '0';
        bit >>= 1;
    }
    *bp = 0;
    }
    else
    sprintf(tmp, fmt, val);

    if (shift != 0) {
        len = strlen(tmp);
        if (shift <= len) {
            tmp[len + 1] = 0;
            while (shift--) {
                tmp[len] = tmp[len - 1];
                len--;
            }
            tmp[len] = '.';
        } else {
            tmp[shift + 1] = 0;
            while (shift) {
                if (len-- > 0) {
                    tmp[shift] = tmp[len];
                } else {
                    tmp[shift] = '0';
                }
                shift--;
            }
            tmp[0] = '.';
        }
    }
    if (negative) {
        len = strlen(tmp)+1;
        while (len) {
            tmp[len] = tmp[len-1];
            len--;
        }
        tmp[0] = '-';
    }
    return Tools_strcat(buf, buf_len, out_len, allow_realloc, (u_char *)tmp);
}


/**
 * Prints an integer into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocInteger(u_char ** buf, size_t * buf_len, size_t * out_len,
                       int allow_realloc,
                       const Types_VariableList * var,
                       const struct Parse_EnumList_s *enums,
                       const char *hint, const char *units)
{
    char           *enum_string = NULL;

    if (var->type != ASN01_INTEGER) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be INTEGER): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    for (; enums; enums = enums->next) {
        if (enums->value == *var->val.integer) {
            enum_string = enums->label;
            break;
        }
    }

    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        if (!Tools_strcat(buf, buf_len, out_len, allow_realloc,
                         (const u_char *) "INTEGER: ")) {
            return 0;
        }
    }

    if (enum_string == NULL || DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.PRINT_NUMERIC_ENUM)) {
        if (hint) {
            if (!(Mib_sprintReallocHintedInteger(buf, buf_len, out_len,
                                                allow_realloc,
                                                *var->val.integer, 'd',
                                                hint, units))) {
                return 0;
            }
        } else {
            char            str[32];
            sprintf(str, "%ld", *var->val.integer);
            if (!Tools_strcat
                (buf, buf_len, out_len, allow_realloc,
                 (const u_char *) str)) {
                return 0;
            }
        }
    } else if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        if (!Tools_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) enum_string)) {
            return 0;
        }
    } else {
        char            str[32];
        sprintf(str, "(%ld)", *var->val.integer);
        if (!Tools_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) enum_string)) {
            return 0;
        }
        if (!Tools_strcat
            (buf, buf_len, out_len, allow_realloc, (const u_char *) str)) {
            return 0;
        }
    }

    if (units) {
        return (Tools_strcat
                (buf, buf_len, out_len, allow_realloc,
                 (const u_char *) " ")
                && Tools_strcat(buf, buf_len, out_len, allow_realloc,
                               (const u_char *) units));
    }
    return 1;
}


/**
 * Prints an unsigned integer into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocUinteger(u_char ** buf, size_t * buf_len, size_t * out_len,
                        int allow_realloc,
                        const Types_VariableList * var,
                        const struct Parse_EnumList_s *enums,
                        const char *hint, const char *units)
{
    char           *enum_string = NULL;

    if (var->type != ASN01_UINTEGER) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be UInteger32): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    for (; enums; enums = enums->next) {
        if (enums->value == *var->val.integer) {
            enum_string = enums->label;
            break;
        }
    }

    if (enum_string == NULL ||
        DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.PRINT_NUMERIC_ENUM)) {
        if (hint) {
            if (!(Mib_sprintReallocHintedInteger(buf, buf_len, out_len,
                                                allow_realloc,
                                                *var->val.integer, 'u',
                                                hint, units))) {
                return 0;
            }
        } else {
            char            str[32];
            sprintf(str, "%lu", *var->val.integer);
            if (!Tools_strcat
                (buf, buf_len, out_len, allow_realloc,
                 (const u_char *) str)) {
                return 0;
            }
        }
    } else if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        if (!Tools_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) enum_string)) {
            return 0;
        }
    } else {
        char            str[32];
        sprintf(str, "(%lu)", *var->val.integer);
        if (!Tools_strcat
            (buf, buf_len, out_len, allow_realloc,
             (const u_char *) enum_string)) {
            return 0;
        }
        if (!Tools_strcat
            (buf, buf_len, out_len, allow_realloc, (const u_char *) str)) {
            return 0;
        }
    }

    if (units) {
        return (Tools_strcat
                (buf, buf_len, out_len, allow_realloc,
                 (const u_char *) " ")
                && Tools_strcat(buf, buf_len, out_len, allow_realloc,
                               (const u_char *) units));
    }
    return 1;
}


/**
 * Prints a gauge value into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocGauge(u_char ** buf, size_t * buf_len, size_t * out_len,
                     int allow_realloc,
                     const Types_VariableList * var,
                     const struct Parse_EnumList_s *enums,
                     const char *hint, const char *units)
{
    char            tmp[32];

    if (var->type != ASN01_GAUGE) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be Gauge32 or Unsigned32): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        u_char          str[] = "Gauge32: ";
        if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str)) {
            return 0;
        }
    }
    if (hint) {
        if (!Mib_sprintReallocHintedInteger(buf, buf_len, out_len,
                                           allow_realloc,
                                           *var->val.integer, 'u', hint,
                                           units)) {
            return 0;
        }
    } else {
        sprintf(tmp, "%u", (unsigned int)(*var->val.integer & 0xffffffff));
        if (!Tools_strcat
            (buf, buf_len, out_len, allow_realloc, (const u_char *) tmp)) {
            return 0;
        }
    }
    if (units) {
        return (Tools_strcat
                (buf, buf_len, out_len, allow_realloc,
                 (const u_char *) " ")
                && Tools_strcat(buf, buf_len, out_len, allow_realloc,
                               (const u_char *) units));
    }
    return 1;
}


/**
 * Prints a counter value into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocCounter(u_char ** buf, size_t * buf_len, size_t * out_len,
                       int allow_realloc,
                       const Types_VariableList * var,
                       const struct Parse_EnumList_s *enums,
                       const char *hint, const char *units)
{
    char            tmp[32];

    if (var->type != ASN01_COUNTER) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be Counter32): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        u_char          str[] = "Counter32: ";
        if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str)) {
            return 0;
        }
    }
    sprintf(tmp, "%u", (unsigned int)(*var->val.integer & 0xffffffff));
    if (!Tools_strcat
        (buf, buf_len, out_len, allow_realloc, (const u_char *) tmp)) {
        return 0;
    }
    if (units) {
        return (Tools_strcat
                (buf, buf_len, out_len, allow_realloc,
                 (const u_char *) " ")
                && Tools_strcat(buf, buf_len, out_len, allow_realloc,
                               (const u_char *) units));
    }
    return 1;
}


/**
 * Prints a network address into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocNetworkAddress(u_char ** buf, size_t * buf_len,
                              size_t * out_len, int allow_realloc,
                              const Types_VariableList * var,
                              const struct Parse_EnumList_s *enums, const char *hint,
                              const char *units)
{
    size_t          i;

    if (var->type != ASN01_IPADDRESS) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be NetworkAddress): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        u_char          str[] = "Network Address: ";
        if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str)) {
            return 0;
        }
    }

    while ((*out_len + (var->valLen * 3) + 2) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
    }

    for (i = 0; i < var->valLen; i++) {
        sprintf((char *) (*buf + *out_len), "%02X", var->val.string[i]);
        *out_len += 2;
        if (i < var->valLen - 1) {
            *(*buf + *out_len) = ':';
            (*out_len)++;
        }
    }
    return 1;
}


/**
 * Prints an ip-address into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocIpAddress(u_char ** buf, size_t * buf_len, size_t * out_len,
                         int allow_realloc,
                         const Types_VariableList * var,
                         const struct Parse_EnumList_s *enums,
                         const char *hint, const char *units)
{
    u_char         *ip = var->val.string;

    if (var->type != ASN01_IPADDRESS) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be IpAddress): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        u_char          str[] = "IpAddress: ";
        if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str)) {
            return 0;
        }
    }
    while ((*out_len + 17) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
    }
    if (ip)
        sprintf((char *) (*buf + *out_len), "%d.%d.%d.%d",
                                            ip[0], ip[1], ip[2], ip[3]);
    *out_len += strlen((char *) (*buf + *out_len));
    return 1;
}


/**
 * Prints a null value into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocNull(u_char ** buf, size_t * buf_len, size_t * out_len,
                    int allow_realloc,
                    const Types_VariableList * var,
                    const struct Parse_EnumList_s *enums,
                    const char *hint, const char *units)
{
    u_char          str[] = "NULL";

    if (var->type != ASN01_NULL) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be NULL): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    return Tools_strcat(buf, buf_len, out_len, allow_realloc, str);
}


/**
 * Prints a bit string into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocBitString(u_char ** buf, size_t * buf_len, size_t * out_len,
                         int allow_realloc,
                         const Types_VariableList * var,
                         const struct Parse_EnumList_s *enums,
                         const char *hint, const char *units)
{
    int             len, bit;
    u_char         *cp;
    char           *enum_string;

    if (var->type != ASN01_BIT_STR && var->type != ASN01_OCTET_STR) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be BITS): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        u_char          str[] = "\"";
        if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str)) {
            return 0;
        }
    } else {
        u_char          str[] = "BITS: ";
        if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str)) {
            return 0;
        }
    }
    if (!Mib_sprintReallocHexString(buf, buf_len, out_len, allow_realloc,
                                  var->val.bitstring, var->valLen)) {
        return 0;
    }

    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        u_char          str[] = "\"";
        if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str)) {
            return 0;
        }
    } else {
        cp = var->val.bitstring;
        for (len = 0; len < (int) var->valLen; len++) {
            for (bit = 0; bit < 8; bit++) {
                if (*cp & (0x80 >> bit)) {
                    enum_string = NULL;
                    for (; enums; enums = enums->next) {
                        if (enums->value == (len * 8) + bit) {
                            enum_string = enums->label;
                            break;
                        }
                    }
                    if (enum_string == NULL ||
                        DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.PRINT_NUMERIC_ENUM)) {
                        char            str[32];
                        sprintf(str, "%d ", (len * 8) + bit);
                        if (!Tools_strcat
                            (buf, buf_len, out_len, allow_realloc,
                             (const u_char *) str)) {
                            return 0;
                        }
                    } else {
                        char            str[32];
                        sprintf(str, "(%d) ", (len * 8) + bit);
                        if (!Tools_strcat
                            (buf, buf_len, out_len, allow_realloc,
                             (const u_char *) enum_string)) {
                            return 0;
                        }
                        if (!Tools_strcat
                            (buf, buf_len, out_len, allow_realloc,
                             (const u_char *) str)) {
                            return 0;
                        }
                    }
                }
            }
            cp++;
        }
    }
    return 1;
}

int Mib_sprintReallocNsapAddress(u_char ** buf, size_t * buf_len,
                           size_t * out_len, int allow_realloc,
                           const Types_VariableList * var,
                           const struct Parse_EnumList_s *enums, const char *hint,
                           const char *units)
{
    if (var->type != ASN01_NSAP) {
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            u_char          str[] = "Wrong Type (should be NsapAddress): ";
            if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str))
                return 0;
        }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, var, NULL, NULL,
                                          NULL);
    }

    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
        u_char          str[] = "NsapAddress: ";
        if (!Tools_strcat(buf, buf_len, out_len, allow_realloc, str)) {
            return 0;
        }
    }

    return Mib_sprintReallocHexString(buf, buf_len, out_len, allow_realloc,
                                    var->val.string, var->valLen);
}


/**
 * Fallback routine for a bad type, prints "Variable has bad type" into a buffer.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocBadType(u_char ** buf, size_t * buf_len, size_t * out_len,
                       int allow_realloc,
                       const Types_VariableList * var,
                       const struct Parse_EnumList_s *enums,
                       const char *hint, const char *units)
{
    u_char          str[] = "Variable has bad type";

    return Tools_strcat(buf, buf_len, out_len, allow_realloc, str);
}



/**
 * Universal print routine, prints a variable into a buffer according to the variable
 * type.
 *
 * If allow_realloc is true the buffer will be (re)allocated to fit in the
 * needed size. (Note: *buf may change due to this.)
 *
 * @param buf      Address of the buffer to print to.
 * @param buf_len  Address to an integer containing the size of buf.
 * @param out_len  Incremented by the number of characters printed.
 * @param allow_realloc if not zero reallocate the buffer to fit the
 *                      needed size.
 * @param var      The variable to encode.
 * @param enums    The enumeration ff this variable is enumerated. may be NULL.
 * @param hint     Contents of the DISPLAY-HINT clause of the MIB.
 *                 See RFC 1903 Section 3.1 for details. may be NULL.
 * @param units    Contents of the UNITS clause of the MIB. may be NULL.
 *
 * @return 1 on success, or 0 on failure (out of memory, or buffer to
 *         small when not allowed to realloc.)
 */
int Mib_sprintReallocByType(u_char ** buf, size_t * buf_len, size_t * out_len,
                       int allow_realloc,
                       const Types_VariableList * var,
                       const struct Parse_EnumList_s *enums,
                       const char *hint, const char *units)
{
    DEBUG_MSGTL(("output", "sprint_by_type, type %d\n", var->type));

    switch (var->type) {
    case ASN01_INTEGER:
        return Mib_sprintReallocInteger(buf, buf_len, out_len, allow_realloc,
                                      var, enums, hint, units);
    case ASN01_OCTET_STR:
        return Mib_sprintReallocOctetString(buf, buf_len, out_len,
                                           allow_realloc, var, enums, hint,
                                           units);
    case ASN01_BIT_STR:
        return Mib_sprintReallocBitString(buf, buf_len, out_len,
                                        allow_realloc, var, enums, hint,
                                        units);
    case ASN01_OPAQUE:
        return Mib_sprintReallocOpaque(buf, buf_len, out_len, allow_realloc,
                                     var, enums, hint, units);
    case ASN01_OBJECT_ID:
        return Mib_sprintReallocObjectIdentifier(buf, buf_len, out_len,
                                                allow_realloc, var, enums,
                                                hint, units);
    case ASN01_TIMETICKS:
        return Mib_sprintReallocTimeTicks(buf, buf_len, out_len,
                                        allow_realloc, var, enums, hint,
                                        units);
    case ASN01_GAUGE:
        return Mib_sprintReallocGauge(buf, buf_len, out_len, allow_realloc,
                                    var, enums, hint, units);
    case ASN01_COUNTER:
        return Mib_sprintReallocCounter(buf, buf_len, out_len, allow_realloc,
                                      var, enums, hint, units);
    case ASN01_IPADDRESS:
        return Mib_sprintReallocIpAddress(buf, buf_len, out_len,
                                        allow_realloc, var, enums, hint,
                                        units);
    case ASN01_NULL:
        return Mib_sprintReallocNull(buf, buf_len, out_len, allow_realloc,
                                   var, enums, hint, units);
    case ASN01_UINTEGER:
        return Mib_sprintReallocUinteger(buf, buf_len, out_len,
                                       allow_realloc, var, enums, hint,
                                       units);
    case ASN01_COUNTER64:
    case ASN01_OPAQUE_U64:
    case ASN01_OPAQUE_I64:
    case ASN01_OPAQUE_COUNTER64:
        return Mib_sprintReallocCounter64(buf, buf_len, out_len,
                                        allow_realloc, var, enums, hint,
                                        units);
    case ASN01_OPAQUE_FLOAT:
        return Mib_sprintReallocFloat(buf, buf_len, out_len, allow_realloc,
                                    var, enums, hint, units);
    case ASN01_OPAQUE_DOUBLE:
        return Mib_sprintReallocDouble(buf, buf_len, out_len, allow_realloc,
                                     var, enums, hint, units);
    default:
        DEBUG_MSGTL(("sprint_by_type", "bad type: %d\n", var->type));
        return Mib_sprintReallocBadType(buf, buf_len, out_len, allow_realloc,
                                      var, enums, hint, units);
    }
}


/**
 * Retrieves the tree head.
 *
 * @return the tree head.
 */
struct Parse_Tree_s * Mib_getTreeHead(void)
{
    return (parse_treeHead);
}

static char    *mib_confmibdir = NULL;
static char    *mib_confmibs = NULL;

static void Mib_handleMibDirsConf(const char *token, char *line)
{
    char           *ctmp;

    if (mib_confmibdir) {
        if ((*line == '+') || (*line == '-')) {
            ctmp = (char *) malloc(strlen(mib_confmibdir) + strlen(line) + 2);
            if (!ctmp) {
                DEBUG_MSGTL(("read_config:initmib",
                            "mibdir conf malloc failed"));
                return;
            }
            if(*line++ == '+')
                sprintf(ctmp, "%s%c%s", mib_confmibdir, ENV_SEPARATOR_CHAR, line);
            else
                sprintf(ctmp, "%s%c%s", line, ENV_SEPARATOR_CHAR, mib_confmibdir);
        } else {
            ctmp = strdup(line);
            if (!ctmp) {
                DEBUG_MSGTL(("read_config:initmib", "mibs conf malloc failed"));
                return;
            }
        }
        TOOLS_FREE(mib_confmibdir);
    } else {
        ctmp = strdup(line);
        if (!ctmp) {
            DEBUG_MSGTL(("read_config:initmib", "mibs conf malloc failed"));
            return;
        }
    }
    mib_confmibdir = ctmp;
    DEBUG_MSGTL(("read_config:initmib", "using mibdirs: %s\n", mib_confmibdir));
}

static void Mib_handleMibsConf(const char *token, char *line)
{
    char           *ctmp;

    if (mib_confmibs) {
        if ((*line == '+') || (*line == '-')) {
            ctmp = (char *) malloc(strlen(mib_confmibs) + strlen(line) + 2);
            if (!ctmp) {
                DEBUG_MSGTL(("read_config:initmib", "mibs conf malloc failed"));
                return;
            }
            if(*line++ == '+')
                sprintf(ctmp, "%s%c%s", mib_confmibs, ENV_SEPARATOR_CHAR, line);
            else
                sprintf(ctmp, "%s%c%s", line, ENV_SEPARATOR_CHAR, mib_confmibdir);
        } else {
            ctmp = strdup(line);
            if (!ctmp) {
                DEBUG_MSGTL(("read_config:initmib", "mibs conf malloc failed"));
                return;
            }
        }
       TOOLS_FREE(mib_confmibs);
    } else {
        ctmp = strdup(line);
        if (!ctmp) {
            DEBUG_MSGTL(("read_config:initmib", "mibs conf malloc failed"));
            return;
        }
    }
    mib_confmibs = ctmp;
    DEBUG_MSGTL(("read_config:initmib", "using mibs: %s\n", mib_confmibs));
}


static void Mib_handleMibFileConf(const char *token, char *line)
{
    DEBUG_MSGTL(("read_config:initmib", "reading mibfile: %s\n", line));
    Parse_readMib(line);
}

static void Mib_handlePrintNumeric(const char *token, char *line)
{
    const char *value;
    char       *st;

    value = strtok_r(line, " \t\n", &st);
    if (value && (
        (strcasecmp(value, "yes")  == 0) ||
        (strcasecmp(value, "true") == 0) ||
        (*value == '1') )) {

        DefaultStore_setInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.OID_OUTPUT_FORMAT, MIB_OID_OUTPUT_NUMERIC);
    }
}

char * Mib_outToggleOptions(char *options)
{
    while (*options) {
        switch (*options++) {
        case '0':
           DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.LIB_2DIGIT_HEX_OUTPUT);
            break;
        case 'a':
            DefaultStore_setInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.STRING_OUTPUT_FORMAT,
                                                      MIB_STRING_OUTPUT_ASCII);
            break;
        case 'b':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.DONT_BREAKDOWN_OIDS);
            break;
        case 'e':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.PRINT_NUMERIC_ENUM);
            break;
        case 'E':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.ESCAPE_QUOTES);
            break;
        case 'f':
            DefaultStore_setInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.OID_OUTPUT_FORMAT,
                                                      MIB_OID_OUTPUT_FULL);
            break;
        case 'n':
            DefaultStore_setInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.OID_OUTPUT_FORMAT,
                                                      MIB_OID_OUTPUT_NUMERIC);
            break;
        case 'q':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT);
            break;
        case 'Q':
            DefaultStore_setBoolean(DSSTORAGE.LIBRARY_ID,  DSLIB_BOOLEAN.QUICKE_PRINT, 1);
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT);
            break;
        case 's':
            DefaultStore_setInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.OID_OUTPUT_FORMAT,
                                                      MIB_OID_OUTPUT_SUFFIX);
            break;
        case 'S':
            DefaultStore_setInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.OID_OUTPUT_FORMAT,
                                                      MIB_OID_OUTPUT_MODULE);
            break;
        case 't':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.NUMERIC_TIMETICKS);
            break;
        case 'T':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.PRINT_HEX_TEXT);
            break;
        case 'u':
            DefaultStore_setInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.OID_OUTPUT_FORMAT,
                                                      MIB_OID_OUTPUT_UCD);
            break;
        case 'U':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.DONT_PRINT_UNITS);
            break;
        case 'v':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.PRINT_BARE_VALUE);
            break;
        case 'x':
            DefaultStore_setInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.STRING_OUTPUT_FORMAT,
                                                      MIB_STRING_OUTPUT_HEX);
            break;
        case 'X':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.EXTENDED_INDEX);
            break;
        default:
            return options - 1;
        }
    }
    return NULL;
}

void Mib_outToggleOptionsUsage(const char *lead, FILE * outf)
{
    fprintf(outf, "%s0:  print leading 0 for single-digit hex characters\n", lead);
    fprintf(outf, "%sa:  print all strings in ascii format\n", lead);
    fprintf(outf, "%sb:  do not break OID indexes down\n", lead);
    fprintf(outf, "%se:  print enums numerically\n", lead);
    fprintf(outf, "%sE:  escape quotes in string indices\n", lead);
    fprintf(outf, "%sf:  print full OIDs on output\n", lead);
    fprintf(outf, "%sn:  print OIDs numerically\n", lead);
    fprintf(outf, "%sq:  quick print for easier parsing\n", lead);
    fprintf(outf, "%sQ:  quick print with equal-signs\n", lead);    /* @@JDW */
    fprintf(outf, "%ss:  print only last symbolic element of OID\n", lead);
    fprintf(outf, "%sS:  print MIB module-id plus last element\n", lead);
    fprintf(outf, "%st:  print timeticks unparsed as numeric integers\n",
            lead);
    fprintf(outf,
            "%sT:  print human-readable text along with hex strings\n",
            lead);
    fprintf(outf, "%su:  print OIDs using UCD-style prefix suppression\n",
            lead);
    fprintf(outf, "%sU:  don't print units\n", lead);
    fprintf(outf, "%sv:  print values only (not OID = value)\n", lead);
    fprintf(outf, "%sx:  print all strings in hex format\n", lead);
    fprintf(outf, "%sX:  extended index format\n", lead);
}

char * Mib_inOptions(char *optarg, int argc, char *const *argv)
{
    char *cp;

    for (cp = optarg; *cp; cp++) {
        switch (*cp) {
        case 'b':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.REGEX_ACCESS);
            break;
        case 'R':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.RANDOM_ACCESS);
            break;
        case 'r':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.DONT_CHECK_RANGE);
            break;
        case 'h':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.NO_DISPLAY_HINT);
            break;
        case 'u':
            DefaultStore_toggleBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.READ_UCD_STYLE_OID);
            break;
        case 's':
            /* What if argc/argv are null ? */
            if (!*(++cp))
                cp = argv[optind++];
            DefaultStore_setString(DSSTORAGE.LIBRARY_ID,
                                  DSLIB_STRING.OIDSUFFIX, cp);
            return NULL;

        case 'S':
            /* What if argc/argv are null ? */
            if (!*(++cp))
                cp = argv[optind++];
            DefaultStore_setString(DSSTORAGE.LIBRARY_ID,
                                  DSLIB_STRING.OIDPREFIX, cp);
            return NULL;

        default:
           /*
            *  Here?  Or in snmp_parse_args?
            snmp_log(LOG_ERR, "Unknown input option passed to -I: %c.\n", *cp);
            */
            return cp;
        }
    }
    return NULL;
}

char  * Mib_inToggleOptions(char *options)
{
    return Mib_inOptions( options, 0, NULL );
}


/**
 * Prints out a help usage for the in* toggle options.
 *
 * @param lead      The lead to print for every line.
 * @param outf      The file descriptor to write to.
 *
 */
void Mib_inToggleOptionsUsage(const char *lead, FILE * outf)
{
    fprintf(outf, "%sb:  do best/regex matching to find a MIB node\n", lead);
    fprintf(outf, "%sh:  don't apply DISPLAY-HINTs\n", lead);
    fprintf(outf, "%sr:  do not check values for range/type legality\n", lead);
    fprintf(outf, "%sR:  do random access to OID labels\n", lead);
    fprintf(outf, "%su:  top-level OIDs must have '.' prefix (UCD-style)\n", lead);
    fprintf(outf, "%ss SUFFIX:  Append all textual OIDs with SUFFIX before parsing\n",lead);
    fprintf(outf, "%sS PREFIX:  Prepend all textual OIDs with PREFIX before parsing\n",lead);
}

/***
 *
 */
void Mib_registerMibHandlers(void)
{
    ReadConfig_registerPrenetMibHandler("snmp", "mibdirs",
                                    Mib_handleMibDirsConf, NULL,
                                    "[mib-dirs|+mib-dirs|-mib-dirs]");
    ReadConfig_registerPrenetMibHandler("snmp", "mibs",
                                    Mib_handleMibsConf, NULL,
                                    "[mib-tokens|+mib-tokens]");
    ReadConfig_registerConfigHandler("snmp", "mibfile",
                            Mib_handleMibFileConf, NULL, "mibfile-to-read");
    /*
     * register the snmp.conf configuration handlers for default
     * parsing behaviour
     */

    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "showMibErrors", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.MIB_ERRORS);
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "commentToEOL",  DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.MIB_COMMENT_TERM);  /* Describes actual behaviour */
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "strictCommentTerm", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.MIB_COMMENT_TERM); /* Backward compatibility */
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "mibAllowUnderline", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.MIB_PARSE_LABEL);
    DefaultStore_registerPremib(ASN01_INTEGER, "snmp", "mibWarningLevel", DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.MIB_WARNINGS);
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "mibReplaceWithLatest", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.MIB_REPLACE);

    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "printNumericEnums", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.PRINT_NUMERIC_ENUM);
    ReadConfig_registerPrenetMibHandler("snmp", "printNumericOids", Mib_handlePrintNumeric, NULL, "(1|yes|true|0|no|false)");
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "escapeQuotes", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.ESCAPE_QUOTES);
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "dontBreakdownOids", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.DONT_BREAKDOWN_OIDS);
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "quickPrinting", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT);
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "numericTimeticks", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.NUMERIC_TIMETICKS);
    DefaultStore_registerPremib(ASN01_INTEGER, "snmp", "oidOutputFormat", DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.OID_OUTPUT_FORMAT);
    DefaultStore_registerPremib(ASN01_INTEGER, "snmp", "suffixPrinting", DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.OID_OUTPUT_FORMAT);
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "extendedIndex", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.EXTENDED_INDEX);
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "printHexText", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.PRINT_HEX_TEXT);
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "printValueOnly", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.PRINT_BARE_VALUE);
    DefaultStore_registerPremib(ASN01_BOOLEAN, "snmp", "dontPrintUnits", DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.DONT_PRINT_UNITS);
    DefaultStore_registerPremib(ASN01_INTEGER, "snmp", "hexOutputLength", DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.HEX_OUTPUT_LENGTH);
}

/*
 * function : Mib_setMibDirectory
 *            - This function sets the string of the directories
 *              from which the MIB modules will be searched or
 *              loaded.
 * arguments: const char *dir, which are the directories
 *              from which the MIB modules will be searched or
 *              loaded.
 * returns  : -
 */
void Mib_setMibDirectory(const char *dir)
{
    const char *newdir;
    char *olddir, *tmpdir = NULL;

    DEBUG_TRACE;
    if (NULL == dir) {
        return;
    }

    olddir = DefaultStore_getString(DSSTORAGE.LIBRARY_ID, DSLIB_STRING.MIBDIRS);
    if (olddir) {
        if ((*dir == '+') || (*dir == '-')) {
            /** New dir starts with '+', thus we add it. */
            tmpdir = (char *)malloc(strlen(dir) + strlen(olddir) + 2);
            if (!tmpdir) {
                DEBUG_MSGTL(("read_config:initmib", "set mibdir malloc failed"));
                return;
            }
            if (*dir++ == '+')
                sprintf(tmpdir, "%s%c%s", olddir, ENV_SEPARATOR_CHAR, dir);
            else
                sprintf(tmpdir, "%s%c%s", dir, ENV_SEPARATOR_CHAR, olddir);
            newdir = tmpdir;
        } else {
            newdir = dir;
        }
    } else {
        /** If dir starts with '+' skip '+' it. */
        newdir = ((*dir == '+') ? ++dir : dir);
    }
    DefaultStore_setString(DSSTORAGE.LIBRARY_ID, DSLIB_STRING.MIBDIRS, newdir);

    /** set_string calls strdup, so if we allocated memory, free it */
    if (tmpdir == newdir) {
        TOOLS_FREE(tmpdir);
    }
}

/*
 * function : Mib_getMibDirectory
 *            - This function returns a string of the directories
 *              from which the MIB modules will be searched or
 *              loaded.
 *              If the value still does not exists, it will be made
 *              from the evironment variable 'MIBDIRS' and/or the
 *              default.
 * arguments: -
 * returns  : char * of the directories in which the MIB modules
 *            will be searched/loaded.
 */

char * Mib_getMibDirectory(void)
{
    char *dir;

    DEBUG_TRACE;
    dir = DefaultStore_getString(DSSTORAGE.LIBRARY_ID, DSLIB_STRING.MIBDIRS);
    if (dir == NULL) {
        DEBUG_MSGTL(("get_mib_directory", "no mib directories set\n"));

        /** Check if the environment variable is set */
        dir = Tools_getenv("MIBDIRS");
        if (dir == NULL) {
            DEBUG_MSGTL(("get_mib_directory", "no mib directories set by environment\n"));
            /** Not set use hard coded path */
            if (mib_confmibdir == NULL) {
                DEBUG_MSGTL(("get_mib_directory", "no mib directories set by config\n"));
                Mib_setMibDirectory(DEFAULT_MIBDIRS);
            }
            else if ((*mib_confmibdir == '+') || (*mib_confmibdir == '-')) {
                DEBUG_MSGTL(("get_mib_directory", "mib directories set by config (but added)\n"));
                Mib_setMibDirectory(DEFAULT_MIBDIRS);
                Mib_setMibDirectory(mib_confmibdir);
            }
            else {
                DEBUG_MSGTL(("get_mib_directory", "mib directories set by config\n"));
                Mib_setMibDirectory(mib_confmibdir);
            }
        } else if ((*dir == '+') || (*dir == '-')) {
            DEBUG_MSGTL(("get_mib_directory", "mib directories set by environment (but added)\n"));
            Mib_setMibDirectory(DEFAULT_MIBDIRS);
            Mib_setMibDirectory(dir);
        } else {
            DEBUG_MSGTL(("get_mib_directory", "mib directories set by environment\n"));
            Mib_setMibDirectory(dir);
        }
        dir = DefaultStore_getString(DSSTORAGE.LIBRARY_ID, DSLIB_STRING.MIBDIRS);
    }
    DEBUG_MSGTL(("get_mib_directory", "mib directories set '%s'\n", dir));
    return(dir);
}

/*
 * function : netsnmp_fixup_mib_directory
 * arguments: -
 * returns  : -
 */
void Mib_fixupMibDirectory(void)
{
    char *homepath = Tools_getenv("HOME");
    char *mibpath = Mib_getMibDirectory();
    char *oldmibpath = NULL;
    char *ptr_home;
    char *new_mibpath;

    DEBUG_TRACE;
    if (homepath && mibpath) {
        DEBUG_MSGTL(("fixup_mib_directory", "mib directories '%s'\n", mibpath));
        while ((ptr_home = strstr(mibpath, "$HOME"))) {
            new_mibpath = (char *)malloc(strlen(mibpath) - strlen("$HOME") +
                     strlen(homepath)+1);
            if (new_mibpath) {
                *ptr_home = 0; /* null out the spot where we stop copying */
                sprintf(new_mibpath, "%s%s%s", mibpath, homepath,
            ptr_home + strlen("$HOME"));
                /** swap in the new value and repeat */
                mibpath = new_mibpath;
        if (oldmibpath != NULL) {
            TOOLS_FREE(oldmibpath);
        }
        oldmibpath = new_mibpath;
            } else {
                break;
            }
        }

        Mib_setMibDirectory(mibpath);

    /*  The above copies the mibpath for us, so...  */

    if (oldmibpath != NULL) {
        TOOLS_FREE(oldmibpath);
    }

    }

}

/**
 * Initialises the mib reader.
 *
 * Reads in all settings from the environment.
 */
void Mib_initMib(void)
{
    const char     *prefix;
    char           *env_var, *entry;
    Mib_PrefixListPTR   pp = &mib_prefixes[0];
    char           *st = NULL;

    if (mib_mib)
        return;
    Parse_initMibInternals();

    /*
     * Initialise the MIB directory/ies
     */
    Mib_fixupMibDirectory();
    env_var = strdup(Mib_getMibDirectory());
    Mib_mibIndexLoad();

    DEBUG_MSGTL(("init_mib",
                "Seen MIBDIRS: Looking in '%s' for mib dirs ...\n",
                env_var));

    entry = strtok_r(env_var, ENV_SEPARATOR, &st);
    while (entry) {
        Parse_addMibdir(entry);
        entry = strtok_r(NULL, ENV_SEPARATOR, &st);
    }
    TOOLS_FREE(env_var);

    env_var = Tools_getenv("MIBFILES");
    if (env_var != NULL) {
        if (*env_var == '+')
            entry = strtok_r(env_var+1, ENV_SEPARATOR, &st);
        else
            entry = strtok_r(env_var, ENV_SEPARATOR, &st);
        while (entry) {
            Parse_addMibfile(entry, NULL, NULL);
            entry = strtok_r(NULL, ENV_SEPARATOR, &st);
        }
    }

    Parse_initMibInternals();

    /*
     * Read in any modules or mibs requested
     */

    env_var = Tools_getenv("MIBS");
    if (env_var == NULL) {
        if (mib_confmibs != NULL)
            env_var = strdup(mib_confmibs);
        else
            env_var = strdup(PRIOT_DEFAULT_MIBS);
    } else {
        env_var = strdup(env_var);
    }
    if (env_var && ((*env_var == '+') || (*env_var == '-'))) {
        entry =
            (char *) malloc(strlen(PRIOT_DEFAULT_MIBS) + strlen(env_var) + 2);
        if (!entry) {
            DEBUG_MSGTL(("init_mib", "env mibs malloc failed"));
            TOOLS_FREE(env_var);
            return;
        } else {
            if (*env_var == '+')
                sprintf(entry, "%s%c%s", PRIOT_DEFAULT_MIBS, ENV_SEPARATOR_CHAR,
                        env_var+1);
            else
                sprintf(entry, "%s%c%s", env_var+1, ENV_SEPARATOR_CHAR,
                        PRIOT_DEFAULT_MIBS );
        }
        TOOLS_FREE(env_var);
        env_var = entry;
    }

    DEBUG_MSGTL(("init_mib",
                "Seen MIBS: Looking in '%s' for mib files ...\n",
                env_var));
    entry = strtok_r(env_var, ENV_SEPARATOR, &st);
    while (entry) {
        if (strcasecmp(entry, DEBUG_ALWAYS_TOKEN) == 0) {
            Parse_readAllMibs();
        } else if (strstr(entry, "/") != NULL) {
            Parse_readMib(entry);
        } else {
            Parse_readModule(entry);
        }
        entry = strtok_r(NULL, ENV_SEPARATOR, &st);
    }
    Parse_adoptOrphans();
    TOOLS_FREE(env_var);

    env_var = Tools_getenv("MIBFILES");
    if (env_var != NULL) {
        if ((*env_var == '+') || (*env_var == '-')) {
            env_var = strdup(env_var + 1);
        } else {
            env_var = strdup(env_var);
        }
    } else {
    }

    if (env_var != NULL) {
        DEBUG_MSGTL(("init_mib",
                    "Seen MIBFILES: Looking in '%s' for mib files ...\n",
                    env_var));
        entry = strtok_r(env_var, ENV_SEPARATOR, &st);
        while (entry) {
            Parse_readMib(entry);
            entry = strtok_r(NULL, ENV_SEPARATOR, &st);
        }
        TOOLS_FREE(env_var);
    }

    prefix = Tools_getenv("PREFIX");

    if (!prefix)
        prefix = mib_standardPrefix;

    mib_prefix = (char *) malloc(strlen(prefix) + 2);
    if (!mib_prefix)
        DEBUG_MSGTL(("init_mib", "Prefix malloc failed"));
    else
        strcpy(mib_prefix, prefix);

    DEBUG_MSGTL(("init_mib",
                "Seen PREFIX: Looking in '%s' for prefix ...\n", mib_prefix));

    /*
     * remove trailing dot
     */
    if (mib_prefix) {
        env_var = &mib_prefix[strlen(mib_prefix) - 1];
        if (*env_var == '.')
            *env_var = '\0';
    }

    pp->str = mib_prefix;           /* fixup first mib_prefix entry */
    /*
     * now that the list of prefixes is built, save each string length.
     */
    while (pp->str) {
        pp->len = strlen(pp->str);
        pp++;
    }

    mib_mib = parse_treeHead;            /* Backwards compatibility */
    mib_treeTop = (struct Parse_Tree_s *) calloc(1, sizeof(struct Parse_Tree_s));
    /*
     * XX error check ?
     */
    if (mib_treeTop) {
        mib_treeTop->label = strdup("(top)");
        mib_treeTop->child_list = parse_treeHead;
    }
}


/*
 * Handle MIB indexes centrally
 */
static int mib_mibIndex     = 0;   /* Last index in use */
static int mib_mibIndexMax  = 0;   /* Size of index array */
char     **mib_mibIndexes   = NULL;

int Mib_mibIndexAdd( const char *dirname, int i );

void Mib_mibIndexLoad( void )
{
    DIR *dir;
    struct dirent *file;
    FILE *fp;
    char tmpbuf[ 300];
    char tmpbuf2[300];
    int  i;
    char *cp;

    /*
     * Open the MIB index directory, or create it (empty)
     */
    snprintf( tmpbuf, sizeof(tmpbuf), "%s/mib_indexes",
              ReadConfig_getPersistentDirectory());
    tmpbuf[sizeof(tmpbuf)-1] = 0;
    dir = opendir( tmpbuf );
    if ( dir == NULL ) {
        DEBUG_MSGTL(("mibindex", "load: (new)\n"));
        System_mkdirhier( tmpbuf, AGENT_DIRECTORY_MODE, 0);
        return;
    }

    /*
     * Create a list of which directory each file refers to
     */
    while ((file = readdir( dir ))) {
        if ( !isdigit((unsigned char)(file->d_name[0])))
            continue;
        i = atoi( file->d_name );

        snprintf( tmpbuf, sizeof(tmpbuf), "%s/mib_indexes/%d",
              ReadConfig_getPersistentDirectory(), i );
        tmpbuf[sizeof(tmpbuf)-1] = 0;
        fp = fopen( tmpbuf, "r" );
        if (!fp)
            continue;
        cp = fgets( tmpbuf2, sizeof(tmpbuf2), fp );
        if ( !cp ) {
            DEBUG_MSGTL(("mibindex", "Empty MIB index (%d)\n", i));
            fclose(fp);
            continue;
        }
        tmpbuf2[strlen(tmpbuf2)-1] = 0;
        DEBUG_MSGTL(("mibindex", "load: (%d) %s\n", i, tmpbuf2));
        (void)Mib_mibIndexAdd( tmpbuf2+4, i );  /* Skip 'DIR ' */
        fclose( fp );
    }
    closedir( dir );
}

char * Mib_mibIndexLookup( const char *dirname )
{
    int i;
    static char tmpbuf[300];

    for (i=0; i<mib_mibIndex; i++) {
        if ( mib_mibIndexes[i] &&
             strcmp( mib_mibIndexes[i], dirname ) == 0) {
             snprintf(tmpbuf, sizeof(tmpbuf), "%s/mib_indexes/%d",
                      ReadConfig_getPersistentDirectory(), i);
             tmpbuf[sizeof(tmpbuf)-1] = 0;
             DEBUG_MSGTL(("mibindex", "lookup: %s (%d) %s\n", dirname, i, tmpbuf ));
             return tmpbuf;
        }
    }
    DEBUG_MSGTL(("mibindex", "lookup: (none)\n"));
    return NULL;
}

int Mib_mibIndexAdd( const char *dirname, int i )
{
    const int old_mibindex_max = mib_mibIndexMax;

    DEBUG_MSGTL(("mibindex", "add: %s (%d)\n", dirname, i ));
    if ( i == -1 )
        i = mib_mibIndex++;
    if ( i >= mib_mibIndexMax ) {
        /*
         * If the index array is full (or non-existent)
         *   then expand (or create) it
         */
        mib_mibIndexMax = i + 10;
        mib_mibIndexes = (char **)realloc(mib_mibIndexes,
                              mib_mibIndexMax * sizeof(mib_mibIndexes[0]));
        Assert_assert(mib_mibIndexes);
        memset(mib_mibIndexes + old_mibindex_max, 0,
               (mib_mibIndexMax - old_mibindex_max) * sizeof(mib_mibIndexes[0]));
    }

    mib_mibIndexes[ i ] = strdup( dirname );
    if ( i >= mib_mibIndex )
        mib_mibIndex = i+1;

    DEBUG_MSGTL(("mibindex", "add: %d/%d/%d\n", i, mib_mibIndex, mib_mibIndexMax ));
    return i;
}

FILE * Mib_mibIndexNew( const char *dirname )
{
    FILE *fp;
    char  tmpbuf[300];
    char *cp;
    int   i;

    cp = Mib_mibIndexLookup( dirname );
    if (!cp) {
        i  = Mib_mibIndexAdd( dirname, -1 );
        snprintf( tmpbuf, sizeof(tmpbuf), "%s/mib_indexes/%d",
                  ReadConfig_getPersistentDirectory(), i );
        tmpbuf[sizeof(tmpbuf)-1] = 0;
        cp = tmpbuf;
    }
    DEBUG_MSGTL(("mibindex", "new: %s (%s)\n", dirname, cp ));
    fp = fopen( cp, "w" );
    if (fp)
        fprintf( fp, "DIR %s\n", dirname );
    return fp;
}


/**
 * Unloads all mibs.
 */
void Mib_shutdownMib(void)
{
    Parse_unloadAllMibs();
    if (mib_treeTop) {
        if (mib_treeTop->label)
            TOOLS_FREE(mib_treeTop->label);
        TOOLS_FREE(mib_treeTop);
    }
    parse_treeHead = NULL;
    mib_mib = NULL;
    if (mib_mibIndexes) {
        int i;
        for (i = 0; i < mib_mibIndex; ++i)
            TOOLS_FREE(mib_mibIndexes[i]);
        free(mib_mibIndexes);
        mib_mibIndex = 0;
        mib_mibIndexMax = 0;
        mib_mibIndexes = NULL;
    }
    if (mib_prefix != NULL && mib_prefix != &mib_standardPrefix[0])
        TOOLS_FREE(mib_prefix);
    if (mib_prefix)
        mib_prefix = NULL;
    TOOLS_FREE(mib_confmibs);
    TOOLS_FREE(mib_confmibdir);
}

/**
 * Prints the MIBs to the file fp.
 *
 * @param fp   The file descriptor to print to.
 */
void Mib_printMib(FILE * fp)
{
    Parse_printSubtree(fp, parse_treeHead, 0);
}

void Mib_printAsciiDump(FILE * fp)
{
    fprintf(fp, "dump DEFINITIONS .= BEGIN\n");
    Parse_printAsciiDumpTree(fp, parse_treeHead, 0);
    fprintf(fp, "END\n");
}


/**
 * Set's the printing function printomat in a subtree according
 * it's type
 *
 * @param subtree    The subtree to set.
 */
void Mib_setFunction(struct Parse_Tree_s *subtree)
{
    subtree->printer = NULL;
    switch (subtree->type) {
    case PARSE_TYPE_OBJID:
        subtree->printomat = Mib_sprintReallocObjectIdentifier;
        break;
    case PARSE_TYPE_OCTETSTR:
        subtree->printomat = Mib_sprintReallocOctetString;
        break;
    case PARSE_TYPE_INTEGER:
        subtree->printomat = Mib_sprintReallocInteger;
        break;
    case PARSE_TYPE_INTEGER32:
        subtree->printomat = Mib_sprintReallocInteger;
        break;
    case PARSE_TYPE_NETADDR:
        subtree->printomat = Mib_sprintReallocNetworkAddress;
        break;
    case PARSE_TYPE_IPADDR:
        subtree->printomat = Mib_sprintReallocIpAddress;
        break;
    case PARSE_TYPE_COUNTER:
        subtree->printomat = Mib_sprintReallocCounter;
        break;
    case PARSE_TYPE_GAUGE:
        subtree->printomat = Mib_sprintReallocGauge;
        break;
    case PARSE_TYPE_TIMETICKS:
        subtree->printomat = Mib_sprintReallocTimeTicks;
        break;
    case PARSE_TYPE_OPAQUE:
        subtree->printomat = Mib_sprintReallocOpaque;
        break;
    case PARSE_TYPE_NULL:
        subtree->printomat = Mib_sprintReallocNull;
        break;
    case PARSE_TYPE_BITSTRING:
        subtree->printomat = Mib_sprintReallocBitString;
        break;
    case PARSE_TYPE_NSAPADDRESS:
        subtree->printomat = Mib_sprintReallocNsapAddress;
        break;
    case PARSE_TYPE_COUNTER64:
        subtree->printomat = Mib_sprintReallocCounter64;
        break;
    case PARSE_TYPE_UINTEGER:
        subtree->printomat = Mib_sprintReallocUinteger;
        break;
    case PARSE_TYPE_UNSIGNED32:
        subtree->printomat = Mib_sprintReallocGauge;
        break;
    case PARSE_TYPE_OTHER:
    default:
        subtree->printomat = Mib_sprintReallocByType;
        break;
    }
}

/**
 * Reads an object identifier from an input string into internal OID form.
 *
 * When called, out_len must hold the maximum length of the output array.
 *
 * @param input     the input string.
 * @param output    the oid wirte.
 * @param out_len   number of subid's in output.
 *
 * @return 1 if successful.
 *
 * If an error occurs, this function returns 0 and MAY set snmp_errno.
 * snmp_errno is NOT set if PRIOTAPI_SET_PRIOT_ERROR evaluates to nothing.
 * This can make multi-threaded use a tiny bit more robust.
 */
int Mib_readObjid(const char *input, oid * output, size_t * out_len)
{                               /* number of subid's in "output" */

    struct Parse_Tree_s    *root = mib_treeTop;
    char            buf[IMPL_SPRINT_MAX_LEN];
    int             ret, max_out_len;
    char           *name, ch;
    const char     *cp;

    cp = input;
    while ((ch = *cp)) {
        if (('0' <= ch && ch <= '9')
            || ('a' <= ch && ch <= 'z')
            || ('A' <= ch && ch <= 'Z')
            || ch == '-')
            cp++;
        else
            break;
    }
    if (ch == ':')
        return Mib_getNode(input, output, out_len);
    if (*input == '.')
        input++;
    else if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.READ_UCD_STYLE_OID)) {
        /*
         * get past leading '.', append '.' to Prefix.
         */
        if (*mib_prefix == '.')
            Strlcpy_strlcpy(buf, mib_prefix + 1, sizeof(buf));
        else
            Strlcpy_strlcpy(buf, mib_prefix, sizeof(buf));
        Strlcat_strlcat(buf, ".", sizeof(buf));
        Strlcat_strlcat(buf, input, sizeof(buf));
        input = buf;
    }

    if ((root == NULL) && (parse_treeHead != NULL)) {
        root = parse_treeHead;
    }
    else if (root == NULL) {
        API_SET_PRIOT_ERROR(ErrorCode_NOMIB);
        *out_len = 0;
        return 0;
    }
    name = strdup(input);
    max_out_len = *out_len;
    *out_len = 0;
    if ((ret =
         Mib_addStringsToOid(root, name, output, out_len,
                             max_out_len)) <= 0)
    {
        if (ret == 0)
            ret = ErrorCode_UNKNOWN_OBJID;
        API_SET_PRIOT_ERROR(ret);
        TOOLS_FREE(name);
        return 0;
    }
    TOOLS_FREE(name);

    return 1;
}

/**
 *
 */
void Mib_sprintReallocObjid(u_char ** buf, size_t * buf_len,
                             size_t * out_len, int allow_realloc,
                             int *buf_overflow,
                             const oid * objid, size_t objidlen)
{
    u_char         *tbuf = NULL, *cp = NULL;
    size_t          tbuf_len = 256, tout_len = 0;
    int             tbuf_overflow = 0;
    int             output_format;

    if ((tbuf = (u_char *) calloc(tbuf_len, 1)) == NULL) {
        tbuf_overflow = 1;
    } else {
        *tbuf = '.';
        tout_len = 1;
    }

    Mib_oidFinishPrinting(objid, objidlen,
                         &tbuf, &tbuf_len, &tout_len,
                         allow_realloc, &tbuf_overflow);

    if (tbuf_overflow) {
        if (!*buf_overflow) {
            Tools_strcat(buf, buf_len, out_len, allow_realloc, tbuf);
            *buf_overflow = 1;
        }
        TOOLS_FREE(tbuf);
        return;
    }

    output_format = DefaultStore_getInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.OID_OUTPUT_FORMAT);
    if (0 == output_format) {
        output_format = MIB_OID_OUTPUT_NUMERIC;
    }
    switch (output_format) {
    case MIB_OID_OUTPUT_FULL:
    case MIB_OID_OUTPUT_NUMERIC:
    case MIB_OID_OUTPUT_SUFFIX:
    case MIB_OID_OUTPUT_MODULE:
        cp = tbuf;
        break;

    case MIB_OID_OUTPUT_NONE:
    default:
        cp = NULL;
    }

    if (!*buf_overflow &&
        !Tools_strcat(buf, buf_len, out_len, allow_realloc, cp)) {
        *buf_overflow = 1;
    }
    TOOLS_FREE(tbuf);
}

/**
 *
 */
struct Parse_Tree_s    * Mib_sprintReallocObjidTree(u_char ** buf, size_t * buf_len,
                                  size_t * out_len, int allow_realloc,
                                  int *buf_overflow,
                                  const oid * objid, size_t objidlen)
{
    u_char         *tbuf = NULL, *cp = NULL;
    size_t          tbuf_len = 512, tout_len = 0;
    struct Parse_Tree_s    *subtree = parse_treeHead;
    size_t          midpoint_offset = 0;
    int             tbuf_overflow = 0;
    int             output_format;

    if ((tbuf = (u_char *) calloc(tbuf_len, 1)) == NULL) {
        tbuf_overflow = 1;
    } else {
        *tbuf = '.';
        tout_len = 1;
    }

    subtree = Mib_getReallocSymbol(objid, objidlen, subtree,
                                  &tbuf, &tbuf_len, &tout_len,
                                  allow_realloc, &tbuf_overflow, NULL,
                                  &midpoint_offset);

    if (tbuf_overflow) {
        if (!*buf_overflow) {
            Tools_strcat(buf, buf_len, out_len, allow_realloc, tbuf);
            *buf_overflow = 1;
        }
        TOOLS_FREE(tbuf);
        return subtree;
    }

    output_format = DefaultStore_getInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.OID_OUTPUT_FORMAT);
    if (0 == output_format) {
        output_format = MIB_OID_OUTPUT_MODULE;
    }
    switch (output_format) {
    case MIB_OID_OUTPUT_FULL:
    case MIB_OID_OUTPUT_NUMERIC:
        cp = tbuf;
        break;

    case MIB_OID_OUTPUT_SUFFIX:
    case MIB_OID_OUTPUT_MODULE:
        for (cp = tbuf; *cp; cp++);

        if (midpoint_offset != 0) {
            cp = tbuf + midpoint_offset - 2;    /*  beyond the '.'  */
        } else {
            while (cp >= tbuf) {
                if (isalpha(*cp)) {
                    break;
                }
                cp--;
            }
        }

        while (cp >= tbuf) {
            if (*cp == '.') {
                break;
            }
            cp--;
        }

        cp++;

        if ((MIB_OID_OUTPUT_MODULE == output_format)
            && cp > tbuf) {
            char            modbuf[256] = { 0 }, *mod =
                Parse_moduleName(subtree->modid, modbuf);

            /*
             * Don't add the module ID if it's just numeric (i.e. we couldn't look
             * it up properly.
             */

            if (!*buf_overflow && modbuf[0] != '#') {
                if (!Tools_strcat
                    (buf, buf_len, out_len, allow_realloc,
                     (const u_char *) mod)
                    || !Tools_strcat(buf, buf_len, out_len, allow_realloc,
                                    (const u_char *) ".")) {
                    *buf_overflow = 1;
                }
            }
        }
        break;

    case MIB_OID_OUTPUT_UCD:
    {
        Mib_PrefixListPTR   pp = &mib_prefixes[0];
        size_t          ilen, tlen;
        const char     *testcp;

        cp = tbuf;
        tlen = strlen((char *) tbuf);

        while (pp->str) {
            ilen = pp->len;
            testcp = pp->str;

            if ((tlen > ilen) && memcmp(tbuf, testcp, ilen) == 0) {
                cp += (ilen + 1);
                break;
            }
            pp++;
        }
        break;
    }

    case MIB_OID_OUTPUT_NONE:
    default:
        cp = NULL;
    }

    if (!*buf_overflow &&
        !Tools_strcat(buf, buf_len, out_len, allow_realloc, cp)) {
        *buf_overflow = 1;
    }
    TOOLS_FREE(tbuf);
    return subtree;
}

int Mib_sprintReallocObjid2(u_char ** buf, size_t * buf_len,
                     size_t * out_len, int allow_realloc,
                     const oid * objid, size_t objidlen)
{
    int             buf_overflow = 0;

    Mib_sprintReallocObjidTree(buf, buf_len, out_len, allow_realloc,
                                      &buf_overflow, objid, objidlen);
    return !buf_overflow;
}

int Mib_snprintObjid(char *buf, size_t buf_len,
              const oid * objid, size_t objidlen)
{
    size_t          out_len = 0;

    if (Mib_sprintReallocObjid2((u_char **) & buf, &buf_len, &out_len, 0,
                             objid, objidlen)) {
        return (int) out_len;
    } else {
        return -1;
    }
}

/**
 * Prints an oid to stdout.
 *
 * @param objid      The oid to print
 * @param objidlen   The length of oidid.
 */
void Mib_printObjid(const oid * objid, size_t objidlen)
{                               /* number of subidentifiers */
    Mib_fprintObjid(stdout, objid, objidlen);
}


/**
 * Prints an oid to a file descriptor.
 *
 * @param f          The file descriptor to print to.
 * @param objid      The oid to print
 * @param objidlen   The length of oidid.
 */
void Mib_fprintObjid(FILE * f, const oid * objid, size_t objidlen)
{                               /* number of subidentifiers */
    u_char         *buf = NULL;
    size_t          buf_len = 256, out_len = 0;
    int             buf_overflow = 0;

    if ((buf = (u_char *) calloc(buf_len, 1)) == NULL) {
        fprintf(f, "[TRUNCATED]\n");
        return;
    } else {
        Mib_sprintReallocObjidTree(&buf, &buf_len, &out_len, 1,
                                          &buf_overflow, objid, objidlen);
        if (buf_overflow) {
            fprintf(f, "%s [TRUNCATED]\n", buf);
        } else {
            fprintf(f, "%s\n", buf);
        }
    }

    TOOLS_FREE(buf);
}

int Mib_sprintReallocVariable(u_char ** buf, size_t * buf_len,
                        size_t * out_len, int allow_realloc,
                        const oid * objid, size_t objidlen,
                        const Types_VariableList * variable)
{
    int             buf_overflow = 0;

    struct Parse_Tree_s    *subtree = parse_treeHead;

    subtree = Mib_sprintReallocObjidTree(buf, buf_len, out_len,
                                          allow_realloc, &buf_overflow,
                                          objid, objidlen);

    if (buf_overflow) {
        return 0;
    }
    if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.PRINT_BARE_VALUE)) {
        if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICKE_PRINT)) {
            if (!Tools_strcat
                (buf, buf_len, out_len, allow_realloc,
                 (const u_char *) " = ")) {
                return 0;
            }
        } else {
            if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.QUICK_PRINT)) {
                if (!Tools_strcat
                    (buf, buf_len, out_len, allow_realloc,
                     (const u_char *) " ")) {
                    return 0;
                }
            } else {
                if (!Tools_strcat
                    (buf, buf_len, out_len, allow_realloc,
                     (const u_char *) " = ")) {
                    return 0;
                }
            }                   /* end if-else NETSNMP_DS_LIB_QUICK_PRINT */
        }                       /* end if-else NETSNMP_DS_LIB_QUICKE_PRINT */
    } else {
        *out_len = 0;
    }

    if (variable->type == PRIOT_NOSUCHOBJECT) {
        return Tools_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *)
                           "No Such Object available on this agent at this OID");
    } else if (variable->type == PRIOT_NOSUCHINSTANCE) {
        return Tools_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *)
                           "No Such Instance currently exists at this OID");
    } else if (variable->type == PRIOT_ENDOFMIBVIEW) {
        return Tools_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *)
                           "No more variables left in this MIB View (It is past the end of the MIB tree)");
    } else if (subtree) {
        const char *units = NULL;
        const char *hint = NULL;
        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.DONT_PRINT_UNITS)) {
            units = subtree->units;
        }

        if (!DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.NO_DISPLAY_HINT)) {
            hint = subtree->hint;
        }

        if (subtree->printomat) {
            return (*subtree->printomat) (buf, buf_len, out_len,
                                          allow_realloc, variable,
                                          subtree->enums, hint,
                                          units);
        } else {
            return Mib_sprintReallocByType(buf, buf_len, out_len,
                                          allow_realloc, variable,
                                          subtree->enums, hint,
                                          units);
        }
    } else {
        /*
         * Handle rare case where tree is empty.
         */
        return Mib_sprintReallocByType(buf, buf_len, out_len, allow_realloc,
                                      variable, NULL, NULL, NULL);
    }
}

int Mib_snprintVariable(char *buf, size_t buf_len,
                 const oid * objid, size_t objidlen,
                 const Types_VariableList * variable)
{
    size_t          out_len = 0;

    if (Mib_sprintReallocVariable((u_char **) & buf, &buf_len, &out_len, 0,
                                objid, objidlen, variable)) {
        return (int) out_len;
    } else {
        return -1;
    }
}

/**
 * Prints a variable to stdout.
 *
 * @param objid     The object id.
 * @param objidlen  The length of teh object id.
 * @param variable  The variable to print.
 */
void Mib_printVariable(const oid * objid,
               size_t objidlen, const Types_VariableList * variable)
{
    Mib_fprintVariable(stdout, objid, objidlen, variable);
}


/**
 * Prints a variable to a file descriptor.
 *
 * @param f         The file descriptor to print to.
 * @param objid     The object id.
 * @param objidlen  The length of teh object id.
 * @param variable  The variable to print.
 */
void Mib_fprintVariable(FILE * f,
                const oid * objid,
                size_t objidlen, const Types_VariableList * variable)
{
    u_char         *buf = NULL;
    size_t          buf_len = 256, out_len = 0;

    if ((buf = (u_char *) calloc(buf_len, 1)) == NULL) {
        fprintf(f, "[TRUNCATED]\n");
        return;
    } else {
        if (Mib_sprintReallocVariable(&buf, &buf_len, &out_len, 1,
                                    objid, objidlen, variable)) {
            fprintf(f, "%s\n", buf);
        } else {
            fprintf(f, "%s [TRUNCATED]\n", buf);
        }
    }

    TOOLS_FREE(buf);
}

int Mib_sprintReallocValue(u_char ** buf, size_t * buf_len,
                     size_t * out_len, int allow_realloc,
                     const oid * objid, size_t objidlen,
                     const Types_VariableList * variable)
{
    if (variable->type == PRIOT_NOSUCHOBJECT) {
        return Tools_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *)
                           "No Such Object available on this agent at this OID");
    } else if (variable->type == PRIOT_NOSUCHINSTANCE) {
        return Tools_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *)
                           "No Such Instance currently exists at this OID");
    } else if (variable->type == PRIOT_ENDOFMIBVIEW) {
        return Tools_strcat(buf, buf_len, out_len, allow_realloc,
                           (const u_char *)
                           "No more variables left in this MIB View (It is past the end of the MIB tree)");
    } else {
        const char *units = NULL;
        struct Parse_Tree_s *subtree = parse_treeHead;
    subtree = Mib_getTree(objid, objidlen, subtree);
        if (subtree && !DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.DONT_PRINT_UNITS)) {
            units = subtree->units;
        }
        if (subtree) {
        if(subtree->printomat) {
        return (*subtree->printomat) (buf, buf_len, out_len,
                          allow_realloc, variable,
                          subtree->enums, subtree->hint,
                          units);
        } else {
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                          allow_realloc, variable,
                          subtree->enums, subtree->hint,
                          units);
        }
    }
        return Mib_sprintReallocByType(buf, buf_len, out_len,
                                      allow_realloc, variable,
                                      NULL, NULL, NULL);
    }
}

/* used in the perl module */
int Mib_snprintValue(char *buf, size_t buf_len,
              const oid * objid, size_t objidlen,
              const Types_VariableList * variable)
{
    size_t          out_len = 0;

    if (Mib_sprintReallocValue((u_char **) & buf, &buf_len, &out_len, 0,
                             objid, objidlen, variable)) {
        return (int) out_len;
    } else {
        return -1;
    }
}

void Mib_printValue(const oid * objid,
            size_t objidlen, const Types_VariableList * variable)
{
    Mib_fprintValue(stdout, objid, objidlen, variable);
}

void Mib_fprintValue(FILE * f,
             const oid * objid,
             size_t objidlen, const Types_VariableList * variable)
{
    u_char         *buf = NULL;
    size_t          buf_len = 256, out_len = 0;

    if ((buf = (u_char *) calloc(buf_len, 1)) == NULL) {
        fprintf(f, "[TRUNCATED]\n");
        return;
    } else {
        if (Mib_sprintReallocValue(&buf, &buf_len, &out_len, 1,
                                 objid, objidlen, variable)) {
            fprintf(f, "%s\n", buf);
        } else {
            fprintf(f, "%s [TRUNCATED]\n", buf);
        }
    }

    TOOLS_FREE(buf);
}


/**
 * Takes the value in VAR and turns it into an OID segment in var->name.
 *
 * @param var    The variable.
 *
 * @return ErrorCode_SUCCESS or ErrorCode_GENERR
 */
int Mib_buildOidSegment(Types_VariableList * var)
{
    int             i;
    uint32_t        ipaddr;

    if (var->name && var->name != var->nameLoc)
        TOOLS_FREE(var->name);
    switch (var->type) {
    case ASN01_INTEGER:
    case ASN01_COUNTER:
    case ASN01_GAUGE:
    case ASN01_TIMETICKS:
        var->nameLength = 1;
        var->name = var->nameLoc;
        var->name[0] = *(var->val.integer);
        break;

    case ASN01_IPADDRESS:
        var->nameLength = 4;
        var->name = var->nameLoc;
        memcpy(&ipaddr, var->val.string, sizeof(ipaddr));
        var->name[0] = (ipaddr >> 24) & 0xff;
        var->name[1] = (ipaddr >> 16) & 0xff;
        var->name[2] = (ipaddr >>  8) & 0xff;
        var->name[3] = (ipaddr >>  0) & 0xff;
        break;

    case ASN01_PRIV_IMPLIED_OBJECT_ID:
        var->nameLength = var->valLen / sizeof(oid);
        if (var->nameLength > (sizeof(var->nameLoc) / sizeof(oid)))
            var->name = (oid *) malloc(sizeof(oid) * (var->nameLength));
        else
            var->name = var->nameLoc;
        if (var->name == NULL)
            return ErrorCode_GENERR;

        for (i = 0; i < (int) var->nameLength; i++)
            var->name[i] = var->val.objid[i];
        break;

    case ASN01_OBJECT_ID:
        var->nameLength = var->valLen / sizeof(oid) + 1;
        if (var->nameLength > (sizeof(var->nameLoc) / sizeof(oid)))
            var->name = (oid *) malloc(sizeof(oid) * (var->nameLength));
        else
            var->name = var->nameLoc;
        if (var->name == NULL)
            return ErrorCode_GENERR;

        var->name[0] = var->nameLength - 1;
        for (i = 0; i < (int) var->nameLength - 1; i++)
            var->name[i + 1] = var->val.objid[i];
        break;

    case ASN01_PRIV_IMPLIED_OCTET_STR:
        var->nameLength = var->valLen;
        if (var->nameLength > (sizeof(var->nameLoc) / sizeof(oid)))
            var->name = (oid *) malloc(sizeof(oid) * (var->nameLength));
        else
            var->name = var->nameLoc;
        if (var->name == NULL)
            return ErrorCode_GENERR;

        for (i = 0; i < (int) var->valLen; i++)
            var->name[i] = (oid) var->val.string[i];
        break;

    case ASN01_OPAQUE:
    case ASN01_OCTET_STR:
        var->nameLength = var->valLen + 1;
        if (var->nameLength > (sizeof(var->nameLoc) / sizeof(oid)))
            var->name = (oid *) malloc(sizeof(oid) * (var->nameLength));
        else
            var->name = var->nameLoc;
        if (var->name == NULL)
            return ErrorCode_GENERR;

        var->name[0] = (oid) var->valLen;
        for (i = 0; i < (int) var->valLen; i++)
            var->name[i + 1] = (oid) var->val.string[i];
        break;

    default:
        DEBUG_MSGTL(("build_oid_segment",
                    "invalid asn type: %d\n", var->type));
        return ErrorCode_GENERR;
    }

    if (var->nameLength >TYPES_MAX_OID_LEN) {
        DEBUG_MSGTL(("build_oid_segment",
                    "Something terribly wrong, namelen = %lu\n",
                    (unsigned long)var->nameLength));
        return ErrorCode_GENERR;
    }

    return ErrorCode_SUCCESS;
}


int Mib_buildOidNoalloc(oid * in, size_t in_len, size_t * out_len,
                  oid * prefix, size_t prefix_len,
                  Types_VariableList * indexes)
{
    Types_VariableList *var;

    if (prefix) {
        if (in_len < prefix_len)
            return ErrorCode_GENERR;
        memcpy(in, prefix, prefix_len * sizeof(oid));
        *out_len = prefix_len;
    } else {
        *out_len = 0;
    }

    for (var = indexes; var != NULL; var = var->nextVariable) {
        if (Mib_buildOidSegment(var) != ErrorCode_SUCCESS)
            return ErrorCode_GENERR;
        if (var->nameLength + *out_len <= in_len) {
            memcpy(&(in[*out_len]), var->name,
                   sizeof(oid) * var->nameLength);
            *out_len += var->nameLength;
        } else {
            return ErrorCode_GENERR;
        }
    }

    DEBUG_MSGTL(("Mib_buildOidNoalloc", "generated: "));
    DEBUG_MSGOID(("Mib_buildOidNoalloc", in, *out_len));
    DEBUG_MSG(("Mib_buildOidNoalloc", "\n"));
    return ErrorCode_SUCCESS;
}

int Mib_buildOid(oid ** out, size_t * out_len,
          oid * prefix, size_t prefix_len, Types_VariableList * indexes)
{
    oid             tmpout[TYPES_MAX_OID_LEN];

    /*
     * xxx-rks: inefficent. try only building segments to find index len:
     *   for (var = indexes; var != NULL; var = var->next_variable) {
     *      if (build_oid_segment(var) != ErrorCode_SUCCESS)
     *         return ErrorCode_GENERR;
     *      *out_len += var->name_length;
     *
     * then see if it fits in existing buffer, or realloc buffer.
     */
    if (Mib_buildOidNoalloc(tmpout, sizeof(tmpout), out_len,
                          prefix, prefix_len, indexes) != ErrorCode_SUCCESS)
        return ErrorCode_GENERR;

    /** xxx-rks: should free previous value? */
    Client_cloneMem((void **) out, (void *) tmpout, *out_len * sizeof(oid));

    return ErrorCode_SUCCESS;
}

/*
 * vblist_out must contain a pre-allocated string of variables into
 * which indexes can be extracted based on the previously existing
 * types in the variable chain
 * returns:
 * ErrorCode_GENERR  on error
 * ErrorCode_SUCCESS on success
 */

int Mib_parseOidIndexes(oid * oidIndex, size_t oidLen,
                  Types_VariableList * data)
{
    Types_VariableList *var = data;

    while (var && oidLen > 0) {

        if (Mib_parseOneOidIndex(&oidIndex, &oidLen, var, 0) !=
            ErrorCode_SUCCESS)
            break;

        var = var->nextVariable;
    }

    if (var != NULL || oidLen != 0)
        return ErrorCode_GENERR;
    return ErrorCode_SUCCESS;
}


int Mib_parseOneOidIndex(oid ** oidStart, size_t * oidLen,
                    Types_VariableList * data, int complete)
{
    Types_VariableList *var = data;
    oid             tmpout[TYPES_MAX_OID_LEN];
    unsigned int    i;
    unsigned int    uitmp = 0;

    oid            *oidIndex = *oidStart;

    if (var == NULL || ((*oidLen == 0) && (complete == 0)))
        return ErrorCode_GENERR;
    else {
        switch (var->type) {
        case ASN01_INTEGER:
        case ASN01_COUNTER:
        case ASN01_GAUGE:
        case ASN01_TIMETICKS:
            if (*oidLen) {
                Client_setVarValue(var, (u_char *) oidIndex++,
                                   sizeof(oid));
                --(*oidLen);
            } else {
                Client_setVarValue(var, (u_char *) oidLen, sizeof(long));
            }
            DEBUG_MSGTL(("parse_oid_indexes",
                        "Parsed int(%d): %ld\n", var->type,
                        *var->val.integer));
            break;

        case ASN01_IPADDRESS:
            if ((4 > *oidLen) && (complete == 0))
                return ErrorCode_GENERR;

            for (i = 0; i < 4 && i < *oidLen; ++i) {
                if (oidIndex[i] > 255) {
                    DEBUG_MSGTL(("parse_oid_indexes",
                                "illegal oid in index: %" "l" "d\n",
                                oidIndex[0]));
                        return ErrorCode_GENERR;  /* sub-identifier too large */
                    }
                    uitmp = uitmp + (oidIndex[i] << (8*(3-i)));
                }
            if (4 > (int) (*oidLen)) {
                oidIndex += *oidLen;
                (*oidLen) = 0;
            } else {
                oidIndex += 4;
                (*oidLen) -= 4;
            }
            uitmp = htonl(uitmp); /* put it in proper order for byte copies */
            uitmp = Client_setVarValue(var, (u_char *) &uitmp, 4);
            DEBUG_MSGTL(("parse_oid_indexes",
                        "Parsed ipaddr(%d): %d.%d.%d.%d\n", var->type,
                        var->val.string[0], var->val.string[1],
                        var->val.string[2], var->val.string[3]));
            break;

        case ASN01_OBJECT_ID:
        case ASN01_PRIV_IMPLIED_OBJECT_ID:
            if (var->type == ASN01_PRIV_IMPLIED_OBJECT_ID) {
                /*
                 * might not be implied, might be fixed len. check if
                 * caller set up val len, and use it if they did.
                 */
                if (0 == var->valLen)
                    uitmp = *oidLen;
                else {
                    DEBUG_MSGTL(("parse_oid_indexes:fix", "fixed len oid\n"));
                    uitmp = var->valLen;
                }
            } else {
                if (*oidLen) {
                    uitmp = *oidIndex++;
                    --(*oidLen);
                } else {
                    uitmp = 0;
                }
                if ((uitmp > *oidLen) && (complete == 0))
                    return ErrorCode_GENERR;
            }

            if (uitmp > TYPES_MAX_OID_LEN)
                return ErrorCode_GENERR;  /* too big and illegal */

            if (uitmp > *oidLen) {
                memcpy(tmpout, oidIndex, sizeof(oid) * (*oidLen));
                memset(&tmpout[*oidLen], 0x00,
                       sizeof(oid) * (uitmp - *oidLen));
                Client_setVarValue(var, (u_char *) tmpout,
                                   sizeof(oid) * uitmp);
                oidIndex += *oidLen;
                (*oidLen) = 0;
            } else {
                Client_setVarValue(var, (u_char *) oidIndex,
                                   sizeof(oid) * uitmp);
                oidIndex += uitmp;
                (*oidLen) -= uitmp;
            }

            DEBUG_MSGTL(("parse_oid_indexes", "Parsed oid: "));
            DEBUG_MSGOID(("parse_oid_indexes",
                         var->val.objid, var->valLen / sizeof(oid)));
            DEBUG_MSG(("parse_oid_indexes", "\n"));
            break;

        case ASN01_OPAQUE:
        case ASN01_OCTET_STR:
        case ASN01_PRIV_IMPLIED_OCTET_STR:
            if (var->type == ASN01_PRIV_IMPLIED_OCTET_STR) {
                /*
                 * might not be implied, might be fixed len. check if
                 * caller set up val len, and use it if they did.
                 */
                if (0 == var->valLen)
                    uitmp = *oidLen;
                else {
                    DEBUG_MSGTL(("parse_oid_indexes:fix", "fixed len str\n"));
                    uitmp = var->valLen;
                }
            } else {
                if (*oidLen) {
                    uitmp = *oidIndex++;
                    --(*oidLen);
                } else {
                    uitmp = 0;
                }
                if ((uitmp > *oidLen) && (complete == 0))
                    return ErrorCode_GENERR;
            }

            /*
             * we handle this one ourselves since we don't have
             * pre-allocated memory to copy from using
             * PriotClient_setVarValue()
             */

            if (uitmp == 0)
                break;          /* zero length strings shouldn't malloc */

            if (uitmp > TYPES_MAX_OID_LEN)
                return ErrorCode_GENERR;  /* too big and illegal */

            /*
             * malloc by size+1 to allow a null to be appended.
             */
            var->valLen = uitmp;
            var->val.string = (u_char *) calloc(1, uitmp + 1);
            if (var->val.string == NULL)
                return ErrorCode_GENERR;

            if ((size_t)uitmp > (*oidLen)) {
                for (i = 0; i < *oidLen; ++i)
                    var->val.string[i] = (u_char) * oidIndex++;
                for (i = *oidLen; i < uitmp; ++i)
                    var->val.string[i] = '\0';
                (*oidLen) = 0;
            } else {
                for (i = 0; i < uitmp; ++i)
                    var->val.string[i] = (u_char) * oidIndex++;
                (*oidLen) -= uitmp;
            }
            var->val.string[uitmp] = '\0';

            DEBUG_MSGTL(("parse_oid_indexes",
                        "Parsed str(%d): %s\n", var->type,
                        var->val.string));
            break;

        default:
            DEBUG_MSGTL(("parse_oid_indexes",
                        "invalid asn type: %d\n", var->type));
            return ErrorCode_GENERR;
        }
    }
    (*oidStart) = oidIndex;
    return ErrorCode_SUCCESS;
}

/*
 * dump_realloc_oid_to_inetaddress:
 *   return 0 for failure,
 *   return 1 for success,
 *   return 2 for not handled
 */

int Mib_dumpReallocOidToInetAddress(const int addr_type, const oid * objid, size_t objidlen,
                                u_char ** buf, size_t * buf_len,
                                size_t * out_len, int allow_realloc,
                                char quotechar)
{
    int             i, len;
    char            intbuf[64], *p;
    char *const     end = intbuf + sizeof(intbuf);
    unsigned char  *zc;
    unsigned long   zone;

    if (!buf)
        return 1;

    for (i = 0; i < objidlen; i++)
        if (objid[i] < 0 || objid[i] > 255)
            return 2;

    p = intbuf;
    *p++ = quotechar;

    switch (addr_type) {
    case IPV4:
    case IPV4Z:
        if ((addr_type == IPV4  && objidlen != 4) ||
            (addr_type == IPV4Z && objidlen != 8))
            return 2;

        len = snprintf(p, end - p, "%" "l" "u.%" "l" "u."
                      "%" "l" "u.%" "l" "u",
                      objid[0], objid[1], objid[2], objid[3]);
        p += len;
        if (p >= end)
            return 2;
        if (addr_type == IPV4Z) {
            zc = (unsigned char*)&zone;
            zc[0] = objid[4];
            zc[1] = objid[5];
            zc[2] = objid[6];
            zc[3] = objid[7];
            zone = ntohl(zone);
            len = snprintf(p, end - p, "%%%lu", zone);
            p += len;
            if (p >= end)
                return 2;
        }

        break;

    case IPV6:
    case IPV6Z:
        if ((addr_type == IPV6 && objidlen != 16) ||
            (addr_type == IPV6Z && objidlen != 20))
            return 2;

        len = 0;
        for (i = 0; i < 16; i ++) {
            len = snprintf(p, end - p, "%s%02" "l" "x", i ? ":" : "",
                           objid[i]);
            p += len;
            if (p >= end)
                return 2;
        }

        if (addr_type == IPV6Z) {
            zc = (unsigned char*)&zone;
            zc[0] = objid[16];
            zc[1] = objid[17];
            zc[2] = objid[18];
            zc[3] = objid[19];
            zone = ntohl(zone);
            len = snprintf(p, end - p, "%%%lu", zone);
            p += len;
            if (p >= end)
                return 2;
        }

        break;

    case DNS:
    default:
        /* DNS can just be handled by dump_realloc_oid_to_string() */
        return 2;
    }

    *p++ = quotechar;
    if (p >= end)
        return 2;

    *p++ = '\0';
    if (p >= end)
        return 2;

    return Tools_cstrcat(buf, buf_len, out_len, allow_realloc, intbuf);
}

int Mib_dumpReallocOidToString(const oid * objid, size_t objidlen,
                           u_char ** buf, size_t * buf_len,
                           size_t * out_len, int allow_realloc,
                           char quotechar)
{
    if (buf) {
        int             i, alen;

        for (i = 0, alen = 0; i < (int) objidlen; i++) {
            oid             tst = objid[i];
            if ((tst > 254) || (!isprint(tst))) {
                tst = (oid) '.';
            }

            if (alen == 0) {
                if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.ESCAPE_QUOTES)) {
                    while ((*out_len + 2) >= *buf_len) {
                        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
                            return 0;
                        }
                    }
                    *(*buf + *out_len) = '\\';
                    (*out_len)++;
                }
                while ((*out_len + 2) >= *buf_len) {
                    if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
                        return 0;
                    }
                }
                *(*buf + *out_len) = quotechar;
                (*out_len)++;
            }

            while ((*out_len + 2) >= *buf_len) {
                if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
                    return 0;
                }
            }
            *(*buf + *out_len) = (char) tst;
            (*out_len)++;
            alen++;
        }

        if (alen) {
            if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.ESCAPE_QUOTES)) {
                while ((*out_len + 2) >= *buf_len) {
                    if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
                        return 0;
                    }
                }
                *(*buf + *out_len) = '\\';
                (*out_len)++;
            }
            while ((*out_len + 2) >= *buf_len) {
                if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
                    return 0;
                }
            }
            *(*buf + *out_len) = quotechar;
            (*out_len)++;
        }

        *(*buf + *out_len) = '\0';
    }

    return 1;
}

void Mib_oidFinishPrinting(const oid * objid, size_t objidlen,
                     u_char ** buf, size_t * buf_len, size_t * out_len,
                     int allow_realloc, int *buf_overflow) {
    char            intbuf[64];
    if (*buf != NULL && *(*buf + *out_len - 1) != '.') {
        if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                           allow_realloc,
                                           (const u_char *) ".")) {
            *buf_overflow = 1;
        }
    }

    while (objidlen-- > 0) {    /* output rest of name, uninterpreted */
        sprintf(intbuf, "%" "l" "u.", *objid++);
        if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                           allow_realloc,
                                           (const u_char *) intbuf)) {
            *buf_overflow = 1;
        }
    }

    if (*buf != NULL) {
        *(*buf + *out_len - 1) = '\0';  /* remove trailing dot */
        *out_len = *out_len - 1;
    }
}

static struct Parse_Tree_s * Mib_getReallocSymbol(const oid * objid, size_t objidlen,
                    struct Parse_Tree_s *subtree,
                    u_char ** buf, size_t * buf_len, size_t * out_len,
                    int allow_realloc, int *buf_overflow,
                    struct Parse_IndexList_s *in_dices, size_t * end_of_known)
{
    struct Parse_Tree_s    *return_tree = NULL;
    int             extended_index =
        DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.EXTENDED_INDEX);
    int             output_format =
        DefaultStore_getInt(DSSTORAGE.LIBRARY_ID, DSLIB_INTEGER.OID_OUTPUT_FORMAT);
    char            intbuf[64];

    if (!objid || !buf) {
        return NULL;
    }

    for (; subtree; subtree = subtree->next_peer) {
        if (*objid == subtree->subid) {
        while (subtree->next_peer && subtree->next_peer->subid == *objid)
        subtree = subtree->next_peer;
            if (subtree->indexes) {
                in_dices = subtree->indexes;
            } else if (subtree->augments) {
                struct Parse_Tree_s    *tp2 =
                    Parse_findTreeNode(subtree->augments, -1);
                if (tp2) {
                    in_dices = tp2->indexes;
                }
            }

            if (!strncmp(subtree->label, PARSE_ANON, PARSE_ANON_LEN) ||
                (MIB_OID_OUTPUT_NUMERIC == output_format)) {
                sprintf(intbuf, "%lu", subtree->subid);
                if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                                   allow_realloc,
                                                   (const u_char *)
                                                   intbuf)) {
                    *buf_overflow = 1;
                }
            } else {
                if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                                   allow_realloc,
                                                   (const u_char *)
                                                   subtree->label)) {
                    *buf_overflow = 1;
                }
            }

            if (objidlen > 1) {
                if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                                   allow_realloc,
                                                   (const u_char *) ".")) {
                    *buf_overflow = 1;
                }

                return_tree = Mib_getReallocSymbol(objid + 1, objidlen - 1,
                                                  subtree->child_list,
                                                  buf, buf_len, out_len,
                                                  allow_realloc,
                                                  buf_overflow, in_dices,
                                                  end_of_known);
            }

            if (return_tree != NULL) {
                return return_tree;
            } else {
                return subtree;
            }
        }
    }


    if (end_of_known) {
        *end_of_known = *out_len;
    }

    /*
     * Subtree not found.
     */

    while (in_dices && (objidlen > 0) &&
           (MIB_OID_OUTPUT_NUMERIC != output_format) &&
           !DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.DONT_BREAKDOWN_OIDS)) {
        size_t          numids;
        struct Parse_Tree_s    *tp;

        tp = Parse_findTreeNode(in_dices->ilabel, -1);

        if (!tp) {
            /*
             * Can't find an index in the mib tree.  Bail.
             */
            goto goto_finishIt;
        }

        if (extended_index) {
            if (*buf != NULL && *(*buf + *out_len - 1) == '.') {
                (*out_len)--;
            }
            if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                               allow_realloc,
                                               (const u_char *) "[")) {
                *buf_overflow = 1;
            }
        }

        switch (tp->type) {
        case PARSE_TYPE_OCTETSTR:
            if (extended_index && tp->hint) {
                Types_VariableList var;
                u_char          buffer[1024];
                int             i;

                memset(&var, 0, sizeof var);
                if (in_dices->isimplied) {
                    numids = objidlen;
                    if (numids > objidlen)
                        goto goto_finishIt;
                } else if (tp->ranges && !tp->ranges->next
                           && tp->ranges->low == tp->ranges->high) {
                    numids = tp->ranges->low;
                    if (numids > objidlen)
                        goto goto_finishIt;
                } else {
                    numids = *objid;
                    if (numids >= objidlen)
                        goto goto_finishIt;
                    objid++;
                    objidlen--;
                }
                if (numids > objidlen)
                    goto goto_finishIt;
                for (i = 0; i < (int) numids; i++)
                    buffer[i] = (u_char) objid[i];
                var.type = ASN01_OCTET_STR;
                var.val.string = buffer;
                var.valLen = numids;
                if (!*buf_overflow) {
                    if (!Mib_sprintReallocOctetString(buf, buf_len, out_len,
                                                     allow_realloc, &var,
                                                     NULL, tp->hint,
                                                     NULL)) {
                        *buf_overflow = 1;
                    }
                }
            } else if (in_dices->isimplied) {
                numids = objidlen;
                if (numids > objidlen)
                    goto goto_finishIt;

                if (!*buf_overflow) {
                    if (!Mib_dumpReallocOidToString
                        (objid, numids, buf, buf_len, out_len,
                         allow_realloc, '\'')) {
                        *buf_overflow = 1;
                    }
                }
            } else if (tp->ranges && !tp->ranges->next
                       && tp->ranges->low == tp->ranges->high) {
                /*
                 * a fixed-length octet string
                 */
                numids = tp->ranges->low;
                if (numids > objidlen)
                    goto goto_finishIt;

                if (!*buf_overflow) {
                    if (! Mib_dumpReallocOidToString
                        (objid, numids, buf, buf_len, out_len,
                         allow_realloc, '\'')) {
                        *buf_overflow = 1;
                    }
                }
            } else {
                numids = (size_t) * objid + 1;
                if (numids > objidlen)
                    goto goto_finishIt;
                if (numids == 1) {
                    if (DefaultStore_getBoolean (DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.ESCAPE_QUOTES)) {
                        if (!*buf_overflow
                            && !Tools_strcat(buf, buf_len, out_len,
                                            allow_realloc,
                                            (const u_char *) "\\")) {
                            *buf_overflow = 1;
                        }
                    }
                    if (!*buf_overflow
                        && !Tools_strcat(buf, buf_len, out_len,
                                        allow_realloc,
                                        (const u_char *) "\"")) {
                        *buf_overflow = 1;
                    }
                    if (DefaultStore_getBoolean (DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.ESCAPE_QUOTES)) {
                        if (!*buf_overflow
                            && !Tools_strcat(buf, buf_len, out_len,
                                            allow_realloc,
                                            (const u_char *) "\\")) {
                            *buf_overflow = 1;
                        }
                    }
                    if (!*buf_overflow
                        && !Tools_strcat(buf, buf_len, out_len,
                                        allow_realloc,
                                        (const u_char *) "\"")) {
                        *buf_overflow = 1;
                    }
                } else {
                    if (!*buf_overflow) {
                        struct Parse_Tree_s * next_peer;
                        int normal_handling = 1;

                        if (tp->next_peer) {
                            next_peer = tp->next_peer;
                        }

                        /* Try handling the InetAddress in the OID, in case of failure,
                         * use the normal_handling.
                         */
                        if (tp->next_peer &&
                            tp->tc_index != -1 &&
                            next_peer->tc_index != -1 &&
                            strcmp(Parse_getTcDescriptor(tp->tc_index), "InetAddress") == 0 &&
                            strcmp(Parse_getTcDescriptor(next_peer->tc_index),
                                    "InetAddressType") == 0 ) {

                            int ret;
                            int addr_type = *(objid - 1);

                            ret = Mib_dumpReallocOidToInetAddress(addr_type,
                                        objid + 1, numids - 1, buf, buf_len, out_len,
                                        allow_realloc, '"');
                            if (ret != 2) {
                                normal_handling = 0;
                                if (ret == 0) {
                                    *buf_overflow = 1;
                                }

                            }
                        }
                        if (normal_handling && !Mib_dumpReallocOidToString
                            (objid + 1, numids - 1, buf, buf_len, out_len,
                             allow_realloc, '"')) {
                            *buf_overflow = 1;
                        }
                    }
                }
            }
            objid += numids;
            objidlen -= numids;
            break;

        case PARSE_TYPE_INTEGER32:
        case PARSE_TYPE_UINTEGER:
        case PARSE_TYPE_UNSIGNED32:
        case PARSE_TYPE_GAUGE:
        case PARSE_TYPE_INTEGER:
            if (tp->enums) {
                struct Parse_EnumList_s *ep = tp->enums;
                while (ep && ep->value != (int) (*objid)) {
                    ep = ep->next;
                }
                if (ep) {
                    if (!*buf_overflow
                        && !Tools_strcat(buf, buf_len, out_len,
                                        allow_realloc,
                                        (const u_char *) ep->label)) {
                        *buf_overflow = 1;
                    }
                } else {
                    sprintf(intbuf, "%" "l" "u", *objid);
                    if (!*buf_overflow
                        && !Tools_strcat(buf, buf_len, out_len,
                                        allow_realloc,
                                        (const u_char *) intbuf)) {
                        *buf_overflow = 1;
                    }
                }
            } else {
                sprintf(intbuf, "%" "l" "u", *objid);
                if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                                   allow_realloc,
                                                   (const u_char *)
                                                   intbuf)) {
                    *buf_overflow = 1;
                }
            }
            objid++;
            objidlen--;
            break;

        case PARSE_TYPE_TIMETICKS:
            /* In an index, this is probably a timefilter */
            if (extended_index) {
                Mib_uptimeString2( *objid, intbuf, sizeof( intbuf ) );
            } else {
                sprintf(intbuf, "%" "l" "u", *objid);
            }
            if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                               allow_realloc,
                                               (const u_char *)
                                               intbuf)) {
                *buf_overflow = 1;
            }
            objid++;
            objidlen--;
            break;

        case PARSE_TYPE_OBJID:
            if (in_dices->isimplied) {
                numids = objidlen;
            } else {
                numids = (size_t) * objid + 1;
            }
            if (numids > objidlen)
                goto goto_finishIt;
            if (extended_index) {
                if (in_dices->isimplied) {
                    if (!*buf_overflow
                        && !Mib_sprintReallocObjidTree(buf, buf_len,
                                                              out_len,
                                                              allow_realloc,
                                                              buf_overflow,
                                                              objid,
                                                              numids)) {
                        *buf_overflow = 1;
                    }
                } else {
                    if (!*buf_overflow
                        && !Mib_sprintReallocObjidTree(buf, buf_len,
                                                              out_len,
                                                              allow_realloc,
                                                              buf_overflow,
                                                              objid + 1,
                                                              numids -
                                                              1)) {
                        *buf_overflow = 1;
                    }
                }
            } else {
                Mib_getReallocSymbol(objid, numids, NULL, buf, buf_len,
                                    out_len, allow_realloc, buf_overflow,
                                    NULL, NULL);
            }
            objid += (numids);
            objidlen -= (numids);
            break;

        case PARSE_TYPE_IPADDR:
            if (objidlen < 4)
                goto goto_finishIt;
            sprintf(intbuf, "%" "l" "u.%" "l" "u."
                    "%" "l" "u.%" "l" "u",
                    objid[0], objid[1], objid[2], objid[3]);
            objid += 4;
            objidlen -= 4;
            if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                               allow_realloc,
                                               (const u_char *) intbuf)) {
                *buf_overflow = 1;
            }
            break;

        case PARSE_TYPE_NETADDR:{
                oid             ntype = *objid++;

                objidlen--;
                sprintf(intbuf, "%" "l" "u.", ntype);
                if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                                   allow_realloc,
                                                   (const u_char *)
                                                   intbuf)) {
                    *buf_overflow = 1;
                }

                if (ntype == 1 && objidlen >= 4) {
                    sprintf(intbuf, "%" "l" "u.%" "l" "u."
                            "%" "l" "u.%" "l" "u",
                            objid[0], objid[1], objid[2], objid[3]);
                    if (!*buf_overflow
                        && !Tools_strcat(buf, buf_len, out_len,
                                        allow_realloc,
                                        (const u_char *) intbuf)) {
                        *buf_overflow = 1;
                    }
                    objid += 4;
                    objidlen -= 4;
                } else {
                    goto goto_finishIt;
                }
            }
            break;

        case PARSE_TYPE_NSAPADDRESS:
        default:
            goto goto_finishIt;
            break;
        }

        if (extended_index) {
            if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                               allow_realloc,
                                               (const u_char *) "]")) {
                *buf_overflow = 1;
            }
        } else {
            if (!*buf_overflow && !Tools_strcat(buf, buf_len, out_len,
                                               allow_realloc,
                                               (const u_char *) ".")) {
                *buf_overflow = 1;
            }
        }
        in_dices = in_dices->next;
    }

goto_finishIt:
    Mib_oidFinishPrinting(objid, objidlen,
                         buf, buf_len, out_len,
                         allow_realloc, buf_overflow);
    return NULL;
}

struct Parse_Tree_s * Mib_getTree(const oid * objid, size_t objidlen, struct Parse_Tree_s *subtree)
{
    struct Parse_Tree_s    *return_tree = NULL;

    for (; subtree; subtree = subtree->next_peer) {
        if (*objid == subtree->subid)
            goto goto_found;
    }

    return NULL;

goto_found:
    while (subtree->next_peer && subtree->next_peer->subid == *objid)
    subtree = subtree->next_peer;
    if (objidlen > 1)
        return_tree =
            Mib_getTree(objid + 1, objidlen - 1, subtree->child_list);
    if (return_tree != NULL)
        return return_tree;
    else
        return subtree;
}

/**
 * Prints on oid description on stdout.
 *
 * @see fprint_description
 */
void Mib_printDescription(oid * objid, size_t objidlen, int width) /* number of subidentifiers */
{
    Mib_fprintDescription(stdout, objid, objidlen, width);
}


/**
 * Prints on oid description into a file descriptor.
 *
 * @param f         The file descriptor to print to.
 * @param objid     The object identifier.
 * @param objidlen  The object id length.
 * @param width     Number of subidentifiers.
 */
void Mib_fprintDescription(FILE * f, oid * objid, size_t objidlen, int width)
{
    u_char         *buf = NULL;
    size_t          buf_len = 256, out_len = 0;

    if ((buf = (u_char *) calloc(buf_len, 1)) == NULL) {
        fprintf(f, "[TRUNCATED]\n");
        return;
    } else {
        if (!Mib_sprintReallocDescription(&buf, &buf_len, &out_len, 1,
                                   objid, objidlen, width)) {
            fprintf(f, "%s [TRUNCATED]\n", buf);
        } else {
            fprintf(f, "%s\n", buf);
        }
    }

    TOOLS_FREE(buf);
}

int Mib_snprintDescription(char *buf, size_t buf_len,
                    oid * objid, size_t objidlen, int width)
{
    size_t          out_len = 0;

    if (Mib_sprintReallocDescription((u_char **) & buf, &buf_len, &out_len, 0,
                                    objid, objidlen, width)) {
        return (int) out_len;
    } else {
        return -1;
    }
}

int Mib_sprintReallocDescription(u_char ** buf, size_t * buf_len,
                     size_t * out_len, int allow_realloc,
                     oid * objid, size_t objidlen, int width)
{
    struct Parse_Tree_s    *tp = Mib_getTree(objid, objidlen, parse_treeHead);
    struct Parse_Tree_s    *subtree = parse_treeHead;
    int             pos, len;
    char            tmpbuf[128];
    const char     *cp;

    if (NULL == tp)
        return 0;

    if (tp->type <= PARSE_TYPE_SIMPLE_LAST)
        cp = " OBJECT-TYPE";
    else
        switch (tp->type) {
        case PARSE_TYPE_TRAPTYPE:
            cp = " TRAP-TYPE";
            break;
        case PARSE_TYPE_NOTIFTYPE:
            cp = " NOTIFICATION-TYPE";
            break;
        case PARSE_TYPE_OBJGROUP:
            cp = " OBJECT-GROUP";
            break;
        case PARSE_TYPE_AGENTCAP:
            cp = " AGENT-CAPABILITIES";
            break;
        case PARSE_TYPE_MODID:
            cp = " MODULE-IDENTITY";
            break;
        case PARSE_TYPE_OBJIDENTITY:
            cp = " OBJECT-IDENTITY";
            break;
        case PARSE_TYPE_MODCOMP:
            cp = " MODULE-COMPLIANCE";
            break;
        default:
            sprintf(tmpbuf, " type_%d", tp->type);
            cp = tmpbuf;
        }

    if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, tp->label) ||
        !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, cp) ||
        !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\n")) {
        return 0;
    }
    if (!Mib_printTreeNode(buf, buf_len, out_len, allow_realloc, tp, width))
        return 0;
    if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, ".= {"))
        return 0;
    pos = 5;
    while (objidlen > 1) {
        for (; subtree; subtree = subtree->next_peer) {
            if (*objid == subtree->subid) {
                while (subtree->next_peer && subtree->next_peer->subid == *objid)
                    subtree = subtree->next_peer;
                if (strncmp(subtree->label, PARSE_ANON, PARSE_ANON_LEN)) {
                    snprintf(tmpbuf, sizeof(tmpbuf), " %s(%lu)", subtree->label, subtree->subid);
                    tmpbuf[ sizeof(tmpbuf)-1 ] = 0;
                } else
                    sprintf(tmpbuf, " %lu", subtree->subid);
                len = strlen(tmpbuf);
                if (pos + len + 2 > width) {
                    if (!Tools_cstrcat(buf, buf_len, out_len,
                                     allow_realloc, "\n     "))
                        return 0;
                    pos = 5;
                }
                if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, tmpbuf))
                    return 0;
                pos += len;
                objid++;
                objidlen--;
                break;
            }
        }
        if (subtree)
            subtree = subtree->child_list;
        else
            break;
    }
    while (objidlen > 1) {
        sprintf(tmpbuf, " %" "l" "u", *objid);
        len = strlen(tmpbuf);
        if (pos + len + 2 > width) {
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\n     "))
                return 0;
            pos = 5;
        }
        if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, tmpbuf))
            return 0;
        pos += len;
        objid++;
        objidlen--;
    }
    sprintf(tmpbuf, " %" "l" "u }", *objid);
    len = strlen(tmpbuf);
    if (pos + len + 2 > width) {
        if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\n     "))
            return 0;
        pos = 5;
    }
    if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, tmpbuf))
        return 0;
    return 1;
}

static int Mib_printTreeNode(u_char ** buf, size_t * buf_len,
                     size_t * out_len, int allow_realloc,
                     struct Parse_Tree_s *tp, int width)
{
    const char     *cp;
    char            str[PARSE_MAXTOKEN];
    int             i, prevmod, pos, len;

    if (tp) {
        Parse_moduleName(tp->modid, str);
        if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "  -- FROM\t") ||
            !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, str))
            return 0;
        pos = 16+strlen(str);
        for (i = 1, prevmod = tp->modid; i < tp->number_modules; i++) {
            if (prevmod != tp->module_list[i]) {
                Parse_moduleName(tp->module_list[i], str);
                len = strlen(str);
                if (pos + len + 2 > width) {
                    if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                                     ",\n  --\t\t"))
                        return 0;
                    pos = 16;
                }
                else {
                    if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, ", "))
                        return 0;
                    pos += 2;
                }
                if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, str))
                    return 0;
                pos += len;
            }
            prevmod = tp->module_list[i];
        }
        if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\n"))
            return 0;
        if (tp->tc_index != -1) {
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                              "  -- TEXTUAL CONVENTION ") ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                              Parse_getTcDescriptor(tp->tc_index)) ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\n"))
                return 0;
        }
        switch (tp->type) {
        case PARSE_TYPE_OBJID:
            cp = "OBJECT IDENTIFIER";
            break;
        case PARSE_TYPE_OCTETSTR:
            cp = "OCTET STRING";
            break;
        case PARSE_TYPE_INTEGER:
            cp = "INTEGER";
            break;
        case PARSE_TYPE_NETADDR:
            cp = "NetworkAddress";
            break;
        case PARSE_TYPE_IPADDR:
            cp = "IpAddress";
            break;
        case PARSE_TYPE_COUNTER:
            cp = "Counter32";
            break;
        case PARSE_TYPE_GAUGE:
            cp = "Gauge32";
            break;
        case PARSE_TYPE_TIMETICKS:
            cp = "TimeTicks";
            break;
        case PARSE_TYPE_OPAQUE:
            cp = "Opaque";
            break;
        case PARSE_TYPE_NULL:
            cp = "NULL";
            break;
        case PARSE_TYPE_COUNTER64:
            cp = "Counter64";
            break;
        case PARSE_TYPE_BITSTRING:
            cp = "BITS";
            break;
        case PARSE_TYPE_NSAPADDRESS:
            cp = "NsapAddress";
            break;
        case PARSE_TYPE_UINTEGER:
            cp = "UInteger32";
            break;
        case PARSE_TYPE_UNSIGNED32:
            cp = "Unsigned32";
            break;
        case PARSE_TYPE_INTEGER32:
            cp = "Integer32";
            break;
        default:
            cp = NULL;
            break;
        }

        if (cp)
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                             "  SYNTAX\t") ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, cp))
                return 0;
        if (tp->ranges) {
            struct Parse_RangeList_s *rp = tp->ranges;
            int             first = 1;
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, " ("))
                return 0;
            while (rp) {
                switch (tp->type) {
                case PARSE_TYPE_INTEGER:
                case PARSE_TYPE_INTEGER32:
                    if (rp->low == rp->high)
                        sprintf(str, "%s%d", (first ? "" : " | "), rp->low );
                    else
                        sprintf(str, "%s%d..%d", (first ? "" : " | "),
                                rp->low, rp->high);
                    break;
                case PARSE_TYPE_UNSIGNED32:
                case PARSE_TYPE_OCTETSTR:
                case PARSE_TYPE_GAUGE:
                case PARSE_TYPE_UINTEGER:
                    if (rp->low == rp->high)
                        sprintf(str, "%s%u", (first ? "" : " | "),
                                (unsigned)rp->low );
                    else
                        sprintf(str, "%s%u..%u", (first ? "" : " | "),
                                (unsigned)rp->low, (unsigned)rp->high);
                    break;
                default:
                    /* No other range types allowed */
                    break;
                }
                if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, str))
                    return 0;
                if (first)
                    first = 0;
                rp = rp->next;
            }
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, ") "))
                return 0;
        }
        if (tp->enums) {
            struct Parse_EnumList_s *ep = tp->enums;
            int             first = 1;
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, " {"))
                return 0;
            pos = 16 + strlen(cp) + 2;
            while (ep) {
                if (first)
                    first = 0;
                else
                    if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, ", "))
                        return 0;
                snprintf(str, sizeof(str), "%s(%d)", ep->label, ep->value);
                str[ sizeof(str)-1 ] = 0;
                len = strlen(str);
                if (pos + len + 2 > width) {
                    if (!Tools_cstrcat(buf, buf_len, out_len,
                                     allow_realloc, "\n\t\t  "))
                        return 0;
                    pos = 18;
                }
                if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, str))
                    return 0;
                pos += len + 2;
                ep = ep->next;
            }
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "} "))
                return 0;
        }
        if (cp)
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\n"))
                return 0;
        if (tp->hint)
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                             "  DISPLAY-HINT\t\"") ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, tp->hint) ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\"\n"))
                return 0;
        if (tp->units)
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                             "  UNITS\t\t\"") ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, tp->units) ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\"\n"))
                return 0;
        switch (tp->access) {
        case PARSE_MIB_ACCESS_READONLY:
            cp = "read-only";
            break;
        case PARSE_MIB_ACCESS_READWRITE:
            cp = "read-write";
            break;
        case PARSE_MIB_ACCESS_WRITEONLY:
            cp = "write-only";
            break;
        case PARSE_MIB_ACCESS_NOACCESS:
            cp = "not-accessible";
            break;
        case PARSE_MIB_ACCESS_NOTIFY:
            cp = "accessible-for-notify";
            break;
        case PARSE_MIB_ACCESS_CREATE:
            cp = "read-create";
            break;
        case 0:
            cp = NULL;
            break;
        default:
            sprintf(str, "access_%d", tp->access);
            cp = str;
        }
        if (cp)
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                             "  MAX-ACCESS\t") ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, cp) ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\n"))
                return 0;
        switch (tp->status) {
        case PARSE_MIB_STATUS_MANDATORY:
            cp = "mandatory";
            break;
        case PARSE_MIB_STATUS_OPTIONAL:
            cp = "optional";
            break;
        case PARSE_MIB_STATUS_OBSOLETE:
            cp = "obsolete";
            break;
        case PARSE_MIB_STATUS_DEPRECATED:
            cp = "deprecated";
            break;
        case PARSE_MIB_STATUS_CURRENT:
            cp = "current";
            break;
        case 0:
            cp = NULL;
            break;
        default:
            sprintf(str, "status_%d", tp->status);
            cp = str;
        }

        if (cp)
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                             "  STATUS\t") ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, cp) ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\n"))
                return 0;
        if (tp->augments)
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                             "  AUGMENTS\t{ ") ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, tp->augments) ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, " }\n"))
                return 0;
        if (tp->indexes) {
            struct Parse_IndexList_s *ip = tp->indexes;
            int             first = 1;
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                             "  INDEX\t\t{ "))
                return 0;
            pos = 16 + 2;
            while (ip) {
                if (first)
                    first = 0;
                else
                    if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, ", "))
                        return 0;
                snprintf(str, sizeof(str), "%s%s",
                        ip->isimplied ? "IMPLIED " : "",
                        ip->ilabel);
                str[ sizeof(str)-1 ] = 0;
                len = strlen(str);
                if (pos + len + 2 > width) {
                    if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\n\t\t  "))
                        return 0;
                    pos = 16 + 2;
                }
                if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, str))
                    return 0;
                pos += len + 2;
                ip = ip->next;
            }
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, " }\n"))
                return 0;
        }
        if (tp->varbinds) {
            struct Parse_VarbindList_s *vp = tp->varbinds;
            int             first = 1;

            if (tp->type == PARSE_TYPE_TRAPTYPE) {
                if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                    "  VARIABLES\t{ "))
                    return 0;
            } else {
                if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                    "  OBJECTS\t{ "))
                    return 0;
            }
            pos = 16 + 2;
            while (vp) {
                if (first)
                    first = 0;
                else
                    if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, ", "))
                        return 0;
                Strlcpy_strlcpy(str, vp->vblabel, sizeof(str));
                len = strlen(str);
                if (pos + len + 2 > width) {
                    if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                                    "\n\t\t  "))
                        return 0;
                    pos = 16 + 2;
                }
                if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, str))
                    return 0;
                pos += len + 2;
                vp = vp->next;
            }
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, " }\n"))
                return 0;
        }
        if (tp->description)
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                              "  DESCRIPTION\t\"") ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, tp->description) ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "\"\n"))
                return 0;
        if (tp->defaultValue)
            if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc,
                              "  DEFVAL\t{ ") ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, tp->defaultValue) ||
                !Tools_cstrcat(buf, buf_len, out_len, allow_realloc, " }\n"))
                return 0;
    } else
        if (!Tools_cstrcat(buf, buf_len, out_len, allow_realloc, "No description\n"))
            return 0;
    return 1;
}

int Mib_getModuleNode(const char *fname,
                const char *module, oid * objid, size_t * objidlen)
{
    int             modid, rc = 0;
    struct Parse_Tree_s    *tp;
    char           *name, *cp;

    if (!strcmp(module, "ANY"))
        modid = -1;
    else {
        Parse_readModule(module);
        modid = Parse_whichModule(module);
        if (modid == -1)
            return 0;
    }

    /*
     * Isolate the first component of the name ...
     */
    name = strdup(fname);
    cp = strchr(name, '.');
    if (cp != NULL) {
        *cp = '\0';
        cp++;
    }
    /*
     * ... and locate it in the tree.
     */
    tp = Parse_findTreeNode(name, modid);
    if (tp) {
        size_t          maxlen = *objidlen;

        /*
         * Set the first element of the object ID
         */
        if (Mib_nodeToOid(tp, objid, objidlen)) {
            rc = 1;

            /*
             * If the name requested was more than one element,
             * tag on the rest of the components
             */
            if (cp != NULL)
                rc = Mib_addStringsToOid(tp, cp, objid, objidlen, maxlen);
        }
    }

    TOOLS_FREE(name);
    return (rc);
}


/**
 * @internal
 *
 * Populates the object identifier from a node in the MIB hierarchy.
 * Builds up the object ID, working backwards,
 * starting from the end of the objid buffer.
 * When the top of the MIB tree is reached, the buffer is adjusted.
 *
 * The buffer length is set to the number of subidentifiers
 * for the object identifier associated with the MIB node.
 *
 * @return the number of subidentifiers copied.
 *
 * If 0 is returned, the objid buffer is too small,
 * and the buffer contents are indeterminate.
 * The buffer length can be used to create a larger buffer.
 */
static int Mib_nodeToOid(struct Parse_Tree_s *tp, oid * objid, size_t * objidlen)
{
    int             numids, lenids;
    oid            *op;

    if (!tp || !objid || !objidlen)
        return 0;

    lenids = (int) *objidlen;
    op = objid + lenids;        /* points after the last element */

    for (numids = 0; tp; tp = tp->parent, numids++) {
        if (numids >= lenids)
            continue;
        --op;
        *op = tp->subid;
    }

    *objidlen = (size_t) numids;
    if (numids > lenids) {
        return 0;
    }

    if (numids < lenids)
        memmove(objid, op, numids * sizeof(oid));

    return (numids);
}

/*
 * Replace \x with x stop at eos_marker
 * return NULL if eos_marker not found
 */
static char *Mib_applyEscapes(char *src, char eos_marker)
{
    char *dst;
    int backslash = 0;

    dst = src;
    while (*src) {
    if (backslash) {
        backslash = 0;
        *dst++ = *src;
    } else {
        if (eos_marker == *src) break;
        if ('\\' == *src) {
        backslash = 1;
        } else {
        *dst++ = *src;
        }
    }
    src++;
    }
    if (!*src) {
    /* never found eos_marker */
    return NULL;
    } else {
    *dst = 0;
    return src;
    }
}

static int Mib_addStringsToOid(struct Parse_Tree_s *tp, char *cp,
                    oid * objid, size_t * objidlen, size_t maxlen)
{
    oid             subid;
    int             len_index = 1000000;
    struct Parse_Tree_s    *tp2 = NULL;
    struct Parse_IndexList_s *in_dices = NULL;
    char           *fcp, *ecp, *cp2 = NULL;
    char            doingquote;
    int             len = -1, pos = -1;
    int             check = !DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.DONT_CHECK_RANGE);
    int             do_hint = !DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.NO_DISPLAY_HINT);

    while (cp && tp && tp->child_list) {
        fcp = cp;
        tp2 = tp->child_list;
        /*
         * Isolate the next entry
         */
        cp2 = strchr(cp, '.');
        if (cp2)
            *cp2++ = '\0';

        /*
         * Search for the appropriate child
         */
        if (isdigit((unsigned char)(*cp))) {
            subid = strtoul(cp, &ecp, 0);
            if (*ecp)
                goto bad_id;
            while (tp2 && tp2->subid != subid)
                tp2 = tp2->next_peer;
        } else {
            while (tp2 && strcmp(tp2->label, fcp))
                tp2 = tp2->next_peer;
            if (!tp2)
                goto bad_id;
            subid = tp2->subid;
        }
        if (*objidlen >= maxlen)
            goto bad_id;
    while (tp2 && tp2->next_peer && tp2->next_peer->subid == subid)
        tp2 = tp2->next_peer;
        objid[*objidlen] = subid;
        (*objidlen)++;

        cp = cp2;
        if (!tp2)
            break;
        tp = tp2;
    }

    if (tp && !tp->child_list) {
        if ((tp2 = tp->parent)) {
            if (tp2->indexes)
                in_dices = tp2->indexes;
            else if (tp2->augments) {
                tp2 = Parse_findTreeNode(tp2->augments, -1);
                if (tp2)
                    in_dices = tp2->indexes;
            }
        }
        tp = NULL;
    }

    while (cp && in_dices) {
        fcp = cp;

        tp = Parse_findTreeNode(in_dices->ilabel, -1);
        if (!tp)
            break;
        switch (tp->type) {
        case PARSE_TYPE_INTEGER:
        case PARSE_TYPE_INTEGER32:
        case PARSE_TYPE_UINTEGER:
        case PARSE_TYPE_UNSIGNED32:
        case PARSE_TYPE_TIMETICKS:
            /*
             * Isolate the next entry
             */
            cp2 = strchr(cp, '.');
            if (cp2)
                *cp2++ = '\0';
            if (isdigit((unsigned char)(*cp))) {
                subid = strtoul(cp, &ecp, 0);
                if (*ecp)
                    goto bad_id;
            } else {
                if (tp->enums) {
                    struct Parse_EnumList_s *ep = tp->enums;
                    while (ep && strcmp(ep->label, cp))
                        ep = ep->next;
                    if (!ep)
                        goto bad_id;
                    subid = ep->value;
                } else
                    goto bad_id;
            }
            if (check && tp->ranges) {
                struct Parse_RangeList_s *rp = tp->ranges;
                int             ok = 0;
                if (tp->type == PARSE_TYPE_INTEGER ||
                    tp->type == PARSE_TYPE_INTEGER32) {
                  while (!ok && rp) {
                    if ((rp->low <= (int) subid)
                        && ((int) subid <= rp->high))
                        ok = 1;
                    else
                        rp = rp->next;
                  }
                } else { /* check unsigned range */
                  while (!ok && rp) {
                    if (((unsigned int)rp->low <= subid)
                        && (subid <= (unsigned int)rp->high))
                        ok = 1;
                    else
                        rp = rp->next;
                  }
                }
                if (!ok)
                    goto bad_id;
            }
            if (*objidlen >= maxlen)
                goto bad_id;
            objid[*objidlen] = subid;
            (*objidlen)++;
            break;
        case PARSE_TYPE_IPADDR:
            if (*objidlen + 4 > maxlen)
                goto bad_id;
            for (subid = 0; cp && subid < 4; subid++) {
                fcp = cp;
                cp2 = strchr(cp, '.');
                if (cp2)
                    *cp2++ = 0;
                objid[*objidlen] = strtoul(cp, &ecp, 0);
                if (*ecp)
                    goto bad_id;
                if (check && objid[*objidlen] > 255)
                    goto bad_id;
                (*objidlen)++;
                cp = cp2;
            }
            break;
        case PARSE_TYPE_OCTETSTR:
            if (tp->ranges && !tp->ranges->next
                && tp->ranges->low == tp->ranges->high)
                len = tp->ranges->low;
            else
                len = -1;
            pos = 0;
            if (*cp == '"' || *cp == '\'') {
                doingquote = *cp++;
                /*
                 * insert length if requested
                 */
                if (!in_dices->isimplied && len == -1) {
                    if (doingquote == '\'') {
                        Api_setDetail
                            ("'-quote is for fixed length strings");
                        return 0;
                    }
                    if (*objidlen >= maxlen)
                        goto bad_id;
                    len_index = *objidlen;
                    (*objidlen)++;
                } else if (doingquote == '"') {
                    Api_setDetail
                        ("\"-quote is for variable length strings");
                    return 0;
                }

        cp2 = Mib_applyEscapes(cp, doingquote);
        if (!cp2) goto bad_id;
        else {
            unsigned char *new_val;
            int new_val_len;
            int parsed_hint = 0;
            const char *parsed_value;

            if (do_hint && tp->hint) {
            parsed_value = Mib_parseOctetHint(tp->hint, cp,
                                            &new_val, &new_val_len);
            parsed_hint = parsed_value == NULL;
            }
            if (parsed_hint) {
            int i;
            for (i = 0; i < new_val_len; i++) {
                if (*objidlen >= maxlen) goto bad_id;
                objid[ *objidlen ] = new_val[i];
                (*objidlen)++;
                pos++;
            }
            TOOLS_FREE(new_val);
            } else {
            while(*cp) {
                if (*objidlen >= maxlen) goto bad_id;
                objid[ *objidlen ] = *cp++;
                (*objidlen)++;
                pos++;
            }
            }
        }

        cp2++;
                if (!*cp2)
                    cp2 = NULL;
                else if (*cp2 != '.')
                    goto bad_id;
                else
                    cp2++;
        if (check) {
                    if (len == -1) {
                        struct Parse_RangeList_s *rp = tp->ranges;
                        int             ok = 0;
                        while (rp && !ok)
                            if (rp->low <= pos && pos <= rp->high)
                                ok = 1;
                            else
                                rp = rp->next;
                        if (!ok)
                            goto bad_id;
                        if (!in_dices->isimplied)
                            objid[len_index] = pos;
                    } else if (pos != len)
                        goto bad_id;
        }
        else if (len == -1 && !in_dices->isimplied)
            objid[len_index] = pos;
            } else {
                if (!in_dices->isimplied && len == -1) {
                    fcp = cp;
                    cp2 = strchr(cp, '.');
                    if (cp2)
                        *cp2++ = 0;
                    len = strtoul(cp, &ecp, 0);
                    if (*ecp)
                        goto bad_id;
                    if (*objidlen + len + 1 >= maxlen)
                        goto bad_id;
                    objid[*objidlen] = len;
                    (*objidlen)++;
                    cp = cp2;
                }
                while (len && cp) {
                    fcp = cp;
                    cp2 = strchr(cp, '.');
                    if (cp2)
                        *cp2++ = 0;
                    objid[*objidlen] = strtoul(cp, &ecp, 0);
                    if (*ecp)
                        goto bad_id;
                    if (check && objid[*objidlen] > 255)
                        goto bad_id;
                    (*objidlen)++;
                    len--;
                    cp = cp2;
                }
            }
            break;
        case PARSE_TYPE_OBJID:
            in_dices = NULL;
            cp2 = cp;
            break;
    case PARSE_TYPE_NETADDR:
        fcp = cp;
        cp2 = strchr(cp, '.');
        if (cp2)
        *cp2++ = 0;
        subid = strtoul(cp, &ecp, 0);
        if (*ecp)
        goto bad_id;
        if (*objidlen + 1 >= maxlen)
        goto bad_id;
        objid[*objidlen] = subid;
        (*objidlen)++;
        cp = cp2;
        if (subid == 1) {
        for (len = 0; cp && len < 4; len++) {
            fcp = cp;
            cp2 = strchr(cp, '.');
            if (cp2)
            *cp2++ = 0;
            subid = strtoul(cp, &ecp, 0);
            if (*ecp)
            goto bad_id;
            if (*objidlen + 1 >= maxlen)
            goto bad_id;
            if (check && subid > 255)
            goto bad_id;
            objid[*objidlen] = subid;
            (*objidlen)++;
            cp = cp2;
        }
        }
        else {
        in_dices = NULL;
        }
        break;
        default:
            Logger_log(LOGGER_PRIORITY_ERR, "Unexpected index type: %d %s %s\n",
                     tp->type, in_dices->ilabel, cp);
            in_dices = NULL;
            cp2 = cp;
            break;
        }
        cp = cp2;
        if (in_dices)
            in_dices = in_dices->next;
    }

    while (cp) {
        fcp = cp;
        switch (*cp) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            cp2 = strchr(cp, '.');
            if (cp2)
                *cp2++ = 0;
            subid = strtoul(cp, &ecp, 0);
            if (*ecp)
                goto bad_id;
            if (*objidlen >= maxlen)
                goto bad_id;
            objid[*objidlen] = subid;
            (*objidlen)++;
            break;
        case '"':
        case '\'':
            doingquote = *cp++;
            /*
             * insert length if requested
             */
            if (doingquote == '"') {
                if (*objidlen >= maxlen)
                    goto bad_id;
                objid[*objidlen] = len = strchr(cp, doingquote) - cp;
                (*objidlen)++;
            }

            if (!cp)
                goto bad_id;
            while (*cp && *cp != doingquote) {
                if (*objidlen >= maxlen)
                    goto bad_id;
                objid[*objidlen] = *cp++;
                (*objidlen)++;
            }
            cp2 = cp + 1;
            if (!*cp2)
                cp2 = NULL;
            else if (*cp2 == '.')
                cp2++;
            else
                goto bad_id;
            break;
        default:
            goto bad_id;
        }
        cp = cp2;
    }
    return 1;

  bad_id:
    {
        char            buf[256];
        if (in_dices)
            snprintf(buf, sizeof(buf), "Index out of range: %s (%s)",
                    fcp, in_dices->ilabel);
        else if (tp)
            snprintf(buf, sizeof(buf), "Sub-id not found: %s -> %s", tp->label, fcp);
        else
            snprintf(buf, sizeof(buf), "%s", fcp);
        buf[ sizeof(buf)-1 ] = 0;

        Api_setDetail(buf);
    }
    return 0;
}


/**
 * @see comments on find_best_tree_node for usage after first time.
 */
int Mib_getWildNode(const char *name, oid * objid, size_t * objidlen)
{
    struct Parse_Tree_s    *tp = Parse_findBestTreeNode(name, parse_treeHead, NULL);
    if (!tp)
        return 0;
    return Mib_getNode(tp->label, objid, objidlen);
}

int Mib_getNode(const char *name, oid * objid, size_t * objidlen)
{
    const char     *cp;
    char            ch;
    int             res;

    cp = name;
    while ((ch = *cp))
        if (('0' <= ch && ch <= '9')
            || ('a' <= ch && ch <= 'z')
            || ('A' <= ch && ch <= 'Z')
            || ch == '-')
            cp++;
        else
            break;
    if (ch != ':')
        if (*name == '.')
            res = Mib_getModuleNode(name + 1, "ANY", objid, objidlen);
        else
            res = Mib_getModuleNode(name, "ANY", objid, objidlen);
    else {
        char           *module;
        /*
         *  requested name is of the form
         *      "module:subidentifier"
         */
        module = (char *) malloc((size_t) (cp - name + 1));
        if (!module)
            return ErrorCode_GENERR;
        sprintf(module, "%.*s", (int) (cp - name), name);
        cp++;                   /* cp now point to the subidentifier */
        if (*cp == ':')
            cp++;

        /*
         * 'cp' and 'name' *do* go that way round!
         */
        res = Mib_getModuleNode(cp, module, objid, objidlen);
        TOOLS_FREE(module);
    }
    if (res == 0) {
        API_SET_PRIOT_ERROR(ErrorCode_UNKNOWN_OBJID);
    }

    return res;
}

/*
 * initialize: no peers included in the report.
 */
void Mib_clearTreeFlags(register struct Parse_Tree_s *tp)
{
    for (; tp; tp = tp->next_peer) {
        tp->reported = 0;
        if (tp->child_list)
            Mib_clearTreeFlags(tp->child_list);
     /*RECURSE*/}
}

/*
 * Update: 1998-07-17 <jhy@gsu.edu>
 * Added print_oid_report* functions.
 */
static int      mib_printSubtreeOidReportLabeledOid = 0;
static int      mib_printSubtreeOidReportOid = 0;
static int      mib_printSubtreeOidReportSymbolic = 0;
static int      mib_printSubtreeOidReportMibChildOid = 0;
static int      mib_printSubtreeOidReportSuffix = 0;

/*
 * These methods recurse.
 */
static void     Mib_printParentLabeledOid(FILE *, struct Parse_Tree_s *);
static void     Mib_printParentOid(FILE *, struct Parse_Tree_s *);
static void     Mib_printParentMibChildOid(FILE *, struct Parse_Tree_s *);
static void     Mib_printParentLabel(FILE *, struct Parse_Tree_s *);
static void     Mib_printSubtreeOidReport(FILE *, struct Parse_Tree_s *, int);


void Mib_printOidReport(FILE * fp)
{
    struct Parse_Tree_s    *tp;
    Mib_clearTreeFlags(parse_treeHead);
    for (tp = parse_treeHead; tp; tp = tp->next_peer)
        Mib_printSubtreeOidReport(fp, tp, 0);
}

void Mib_printOidReportEnableLabeledoid(void)
{
    mib_printSubtreeOidReportLabeledOid = 1;
}

void Mib_printOidReportEnableOid(void)
{
    mib_printSubtreeOidReportOid = 1;
}

void Mib_printOidReportEnableSuffix(void)
{
    mib_printSubtreeOidReportSuffix = 1;
}

void Mib_printOidReportEnableSymbolic(void)
{
    mib_printSubtreeOidReportSymbolic = 1;
}

void Mib_printOidReportEnableMibChildOid(void)
{
    mib_printSubtreeOidReportMibChildOid = 1;
}

/*
 * helper methods for print_subtree_oid_report()
 * each one traverses back up the node tree
 * until there is no parent.  Then, the label combination
 * is output, such that the parent is displayed first.
 *
 * Warning: these methods are all recursive.
 */

static void Mib_printParentLabeledOid(FILE * f, struct Parse_Tree_s *tp)
{
    if (tp) {
        if (tp->parent) {
            Mib_printParentLabeledOid(f, tp->parent);
         /*RECURSE*/}
        fprintf(f, ".%s(%lu)", tp->label, tp->subid);
    }
}

static void Mib_printParentOid(FILE * f, struct Parse_Tree_s *tp)
{
    if (tp) {
        if (tp->parent) {
            Mib_printParentOid(f, tp->parent);
         /*RECURSE*/}
        fprintf(f, ".%lu", tp->subid);
    }
}


static void Mib_printParentMibChildOid(FILE * f, struct Parse_Tree_s *tp)
{
    static struct Parse_Tree_s *temp;
    unsigned long elems[100];
    int elem_cnt = 0;
    int i = 0;
    temp = tp;
    if (temp) {
        while (temp->parent) {
                elems[elem_cnt++] = temp->subid;
                temp = temp->parent;
        }
        elems[elem_cnt++] = temp->subid;
    }
    for (i = elem_cnt - 1; i >= 0; i--) {
        if (i == elem_cnt - 1) {
            fprintf(f, "%lu", elems[i]);
            } else {
            fprintf(f, ".%lu", elems[i]);
        }
    }
}

static void Mib_printParentLabel(FILE * f, struct Parse_Tree_s *tp)
{
    if (tp) {
        if (tp->parent) {
            Mib_printParentLabel(f, tp->parent);
         /*RECURSE*/}
        fprintf(f, ".%s", tp->label);
    }
}

/**
 * @internal
 * This methods generates variations on the original print_subtree() report.
 * Traverse the tree depth first, from least to greatest sub-identifier.
 * Warning: this methods recurses and calls methods that recurse.
 *
 * @param f       File descriptor to print to.
 * @param tree    ???
 * @param count   ???
 */

static void Mib_printSubtreeOidReport(FILE * f, struct Parse_Tree_s *tree, int count)
{
    struct Parse_Tree_s    *tp;

    count++;

    /*
     * sanity check
     */
    if (!tree) {
        return;
    }

    /*
     * find the not reported peer with the lowest sub-identifier.
     * if no more, break the loop and cleanup.
     * set "reported" flag, and create report for this peer.
     * recurse using the children of this peer, if any.
     */
    while (1) {
        register struct Parse_Tree_s *ntp;

        tp = NULL;
        for (ntp = tree->child_list; ntp; ntp = ntp->next_peer) {
            if (ntp->reported)
                continue;

            if (!tp || (tp->subid > ntp->subid))
                tp = ntp;
        }
        if (!tp)
            break;

        tp->reported = 1;

        if (mib_printSubtreeOidReportLabeledOid) {
            Mib_printParentLabeledOid(f, tp);
            fprintf(f, "\n");
        }
        if (mib_printSubtreeOidReportOid) {
            Mib_printParentOid(f, tp);
            fprintf(f, "\n");
        }
        if (mib_printSubtreeOidReportSymbolic) {
            Mib_printParentLabel(f, tp);
            fprintf(f, "\n");
        }
        if (mib_printSubtreeOidReportMibChildOid) {
        fprintf(f, "\"%s\"\t", tp->label);
            fprintf(f, "\t\t\"");
            Mib_printParentMibChildOid(f, tp);
            fprintf(f, "\"\n");
        }
        if (mib_printSubtreeOidReportSuffix) {
            int             i;
            for (i = 0; i < count; i++)
                fprintf(f, "  ");
            fprintf(f, "%s(%ld) type=%d", tp->label, tp->subid, tp->type);
            if (tp->tc_index != -1)
                fprintf(f, " tc=%d", tp->tc_index);
            if (tp->hint)
                fprintf(f, " hint=%s", tp->hint);
            if (tp->units)
                fprintf(f, " units=%s", tp->units);

            fprintf(f, "\n");
        }
        Mib_printSubtreeOidReport(f, tp, count);
     /*RECURSE*/}
}


/**
 * Converts timeticks to hours, minutes, seconds string.
 *
 * @param timeticks    The timeticks to convert.
 * @param buf          Buffer to write to, has to be at
 *                     least 40 Bytes large.
 *
 * @return The buffer
 *
 * @see uptimeString
 */
char * Mib_uptimeString(u_long timeticks, char *buf)
{
    return Mib_uptimeStringN( timeticks, buf, 40);
}

char * Mib_uptimeStringN(u_long timeticks, char *buf, size_t buflen)
{
    Mib_uptimeString2(timeticks, buf, buflen);
    return buf;
}

/**
 * Given a string, parses an oid out of it (if possible).
 * It will try to parse it based on predetermined configuration if
 * present or by every method possible otherwise.
 * If a suffix has been registered using NETSNMP_DS_LIB_OIDSUFFIX, it
 * will be appended to the input string before processing.
 *
 * @param argv    The OID to string parse
 * @param root    An OID array where the results are stored.
 * @param rootlen The max length of the array going in and the data
 *                length coming out.
 *
 * @return        The root oid pointer if successful, or NULL otherwise.
 */

oid * Mib_parseOid(const char *argv, oid * root, size_t * rootlen)
{
    size_t          savlen = *rootlen;
    static size_t   tmpbuf_len = 0;
    static char    *tmpbuf;
    const char     *suffix, *prefix;

    suffix = DefaultStore_getString(DSSTORAGE.LIBRARY_ID, DSLIB_STRING.OIDSUFFIX);
    prefix = DefaultStore_getString(DSSTORAGE.LIBRARY_ID, DSLIB_STRING.OIDPREFIX);

    if ((suffix && suffix[0]) || (prefix && prefix[0])) {
        if (!suffix)
            suffix = "";
        if (!prefix)
            prefix = "";
        if ((strlen(suffix) + strlen(prefix) + strlen(argv) + 2) > tmpbuf_len) {
            tmpbuf_len = strlen(suffix) + strlen(argv) + strlen(prefix) + 2;
            tmpbuf = (char *)realloc(tmpbuf, tmpbuf_len);
        }
        snprintf(tmpbuf, tmpbuf_len, "%s%s%s%s", prefix, argv,
                 ((suffix[0] == '.' || suffix[0] == '\0') ? "" : "."),
                 suffix);
        argv = tmpbuf;
        DEBUG_MSGTL(("snmp_parse_oid","Parsing: %s\n",argv));
    }

    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.RANDOM_ACCESS)
        || strchr(argv, ':')) {
        if (Mib_getNode(argv, root, rootlen)) {
            return root;
        }
    } else if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID, DSLIB_BOOLEAN.REGEX_ACCESS)) {
    Mib_clearTreeFlags(parse_treeHead);
        if (Mib_getWildNode(argv, root, rootlen)) {
            return root;
        }
    } else {
        if (Mib_readObjid(argv, root, rootlen)) {
            return root;
        }
        *rootlen = savlen;
        if (Mib_getNode(argv, root, rootlen)) {
            return root;
        }
        *rootlen = savlen;
        DEBUG_MSGTL(("parse_oid", "wildly parsing\n"));
    Mib_clearTreeFlags(parse_treeHead);
        if (Mib_getWildNode(argv, root, rootlen)) {
            return root;
        }
    }
    return NULL;
}

/*
 * Use DISPLAY-HINT to parse a value into an octet string.
 *
 * note that "1d1d", "11" could have come from an octet string that
 * looked like { 1, 1 } or an octet string that looked like { 11 }
 * because of this, it's doubtful that anyone would use such a display
 * string. Therefore, the parser ignores this case.
 */

struct Mib_ParseHints_s {
    int length;
    int repeat;
    int format;
    int separator;
    int terminator;
    unsigned char *result;
    int result_max;
    int result_len;
};

static void Mib_parseHintsReset(struct Mib_ParseHints_s *ph)
{
    ph->length = 0;
    ph->repeat = 0;
    ph->format = 0;
    ph->separator = 0;
    ph->terminator = 0;
}

static void Mib_parseHintsCtor(struct Mib_ParseHints_s *ph)
{
    Mib_parseHintsReset(ph);
    ph->result = NULL;
    ph->result_max = 0;
    ph->result_len = 0;
}

static int Mib_parseHintsAddResultOctet(struct Mib_ParseHints_s *ph, unsigned char octet)
{
    if (!(ph->result_len < ph->result_max)) {
    ph->result_max = ph->result_len + 32;
    if (!ph->result) {
        ph->result = (unsigned char *)malloc(ph->result_max);
    } else {
        ph->result = (unsigned char *)realloc(ph->result, ph->result_max);
    }
    }

    if (!ph->result) {
    return 0;		/* failed */
    }

    ph->result[ph->result_len++] = octet;
    return 1;			/* success */
}

static int Mib_parseHintsParse(struct Mib_ParseHints_s *ph, const char **v_in_out)
{
    const char *v = *v_in_out;
    char *nv;
    int base;
    int repeats = 0;
    int repeat_fixup = ph->result_len;

    if (ph->repeat) {
    if (!Mib_parseHintsAddResultOctet(ph, 0)) {
        return 0;
    }
    }
    do {
    base = 0;
    switch (ph->format) {
    case 'x': base += 6;	/* fall through */
    case 'd': base += 2;	/* fall through */
    case 'o': base += 8;	/* fall through */
        {
        int i;
        unsigned long number = strtol(v, &nv, base);
        if (nv == v) return 0;
        v = nv;
        for (i = 0; i < ph->length; i++) {
            int shift = 8 * (ph->length - 1 - i);
            if (!Mib_parseHintsAddResultOctet(ph, (u_char)(number >> shift) )) {
            return 0; /* failed */
            }
        }
        }
        break;

    case 'a':
        {
        int i;

        for (i = 0; i < ph->length && *v; i++) {
            if (!Mib_parseHintsAddResultOctet(ph, *v++)) {
            return 0;	/* failed */
            }
        }
        }
        break;
    }

    repeats++;

    if (ph->separator && *v) {
        if (*v == ph->separator) {
        v++;
        } else {
        return 0;		/* failed */
        }
    }

    if (ph->terminator) {
        if (*v == ph->terminator) {
        v++;
        break;
        }
    }
    } while (ph->repeat && *v);
    if (ph->repeat) {
    ph->result[repeat_fixup] = repeats;
    }

    *v_in_out = v;
    return 1;
}

static void Mib_parseHintsLengthAddDigit(struct Mib_ParseHints_s *ph, int digit)
{
    ph->length *= 10;
    ph->length += digit - '0';
}

const char * Mib_parseOctetHint(const char *hint, const char *value, unsigned char **new_val, int *new_val_len)
{
    const char *h = hint;
    const char *v = value;
    struct Mib_ParseHints_s ph;
    int retval = 1;
    /* See RFC 1443 */
    enum {
    HINT_1_2,
    HINT_2_3,
    HINT_1_2_4,
    HINT_1_2_5
    } state = HINT_1_2;

    Mib_parseHintsCtor(&ph);
    while (*h && *v && retval) {
    switch (state) {
    case HINT_1_2:
        if ('*' == *h) {
        ph.repeat = 1;
        state = HINT_2_3;
        } else if (isdigit((unsigned char)(*h))) {
        Mib_parseHintsLengthAddDigit(&ph, *h);
        state = HINT_2_3;
        } else {
        return v;	/* failed */
        }
        break;

    case HINT_2_3:
        if (isdigit((unsigned char)(*h))) {
        Mib_parseHintsLengthAddDigit(&ph, *h);
        /* state = HINT_2_3 */
        } else if ('x' == *h || 'd' == *h || 'o' == *h || 'a' == *h) {
        ph.format = *h;
        state = HINT_1_2_4;
        } else {
        return v;	/* failed */
        }
        break;

    case HINT_1_2_4:
        if ('*' == *h) {
        retval = Mib_parseHintsParse(&ph, &v);
        Mib_parseHintsReset(&ph);

        ph.repeat = 1;
        state = HINT_2_3;
        } else if (isdigit((unsigned char)(*h))) {
        retval = Mib_parseHintsParse(&ph, &v);
        Mib_parseHintsReset(&ph);

        Mib_parseHintsLengthAddDigit(&ph, *h);
        state = HINT_2_3;
        } else {
        ph.separator = *h;
        state = HINT_1_2_5;
        }
        break;

    case HINT_1_2_5:
        if ('*' == *h) {
        retval = Mib_parseHintsParse(&ph, &v);
        Mib_parseHintsReset(&ph);

        ph.repeat = 1;
        state = HINT_2_3;
        } else if (isdigit((unsigned char)(*h))) {
        retval = Mib_parseHintsParse(&ph, &v);
        Mib_parseHintsReset(&ph);

        Mib_parseHintsLengthAddDigit(&ph, *h);
        state = HINT_2_3;
        } else {
        ph.terminator = *h;

        retval = Mib_parseHintsParse(&ph, &v);
        Mib_parseHintsReset(&ph);

        state = HINT_1_2;
        }
        break;
    }
    h++;
    }
    while (*v && retval) {
    retval = Mib_parseHintsParse(&ph, &v);
    }
    if (retval) {
    *new_val = ph.result;
    *new_val_len = ph.result_len;
    } else {
    if (ph.result) {
        TOOLS_FREE(ph.result);
    }
    *new_val = NULL;
    *new_val_len = 0;
    }
    return retval ? NULL : v;
}


u_char Mib_mibToAsnType(int mib_type)
{
    switch (mib_type) {
    case PARSE_TYPE_OBJID:
        return ASN01_OBJECT_ID;

    case PARSE_TYPE_OCTETSTR:
        return ASN01_OCTET_STR;

    case PARSE_TYPE_NETADDR:
    case PARSE_TYPE_IPADDR:
        return ASN01_IPADDRESS;

    case PARSE_TYPE_INTEGER32:
    case PARSE_TYPE_INTEGER:
        return ASN01_INTEGER;

    case PARSE_TYPE_COUNTER:
        return ASN01_COUNTER;

    case PARSE_TYPE_GAUGE:
        return ASN01_GAUGE;

    case PARSE_TYPE_TIMETICKS:
        return ASN01_TIMETICKS;

    case PARSE_TYPE_OPAQUE:
        return ASN01_OPAQUE;

    case PARSE_TYPE_NULL:
        return ASN01_NULL;

    case PARSE_TYPE_COUNTER64:
        return ASN01_COUNTER64;

    case PARSE_TYPE_BITSTRING:
        return ASN01_BIT_STR;

    case PARSE_TYPE_UINTEGER:
    case PARSE_TYPE_UNSIGNED32:
        return ASN01_UNSIGNED;

    case PARSE_TYPE_NSAPADDRESS:
        return ASN01_NSAP;

    }
    return -1;
}

/**
 * Converts a string to its OID form.
 * in example  "hello" = 5 . 'h' . 'e' . 'l' . 'l' . 'o'
 *
 * @param S   The string.
 * @param O   The oid.
 * @param L   The length of the oid.
 *
 * @return 0 on Sucess, 1 on failure.
 */
int Mib_str2oid(const char *S, oid * O, int L)
{
    const char     *c = S;
    oid            *o = &O[1];

    --L;                        /* leave room for length prefix */

    for (; *c && L; --L, ++o, ++c)
        *o = *c;

    /*
     * make sure we got to the end of the string
     */
    if (*c != 0)
        return 1;

    /*
     * set the length of the oid
     */
    *O = c - S;

    return 0;
}

/**
 * Converts an OID to its character form.
 * in example  5 . 1 . 2 . 3 . 4 . 5 = 12345
 *
 * @param C   The character buffer.
 * @param L   The length of the buffer.
 * @param O   The oid.
 *
 * @return 0 on Sucess, 1 on failure.
 */
int Mib_oid2chars(char *C, int L, const oid * O)
{
    char           *c = C;
    const oid      *o = &O[1];

    if (L < (int)*O)
        return 1;

    L = *O; /** length */
    for (; L; --L, ++o, ++c) {
        if (*o > 0xFF)
            return 1;
        *c = (char)*o;
    }
    return 0;
}

/**
 * Converts an OID to its string form.
 * in example  5 . 'h' . 'e' . 'l' . 'l' . 'o' = "hello\0" (null terminated)
 *
 * @param S   The character string buffer.
 * @param L   The length of the string buffer.
 * @param O   The oid.
 *
 * @return 0 on Sucess, 1 on failure.
 */
int Mib_oid2str(char *S, int L, oid * O)
{
    int            rc;

    if (L <= (int)*O)
        return 1;

    rc = Mib_oid2chars(S, L, O);
    if (rc)
        return 1;

    S[ *O ] = 0;

    return 0;
}


int Mib_snprintByType(char *buf, size_t buf_len,
                Types_VariableList * var,
                const struct Parse_EnumList_s *enums,
                const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocByType((u_char **) & buf, &buf_len, &out_len, 0,
                               var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintHexString(char *buf, size_t buf_len, const u_char * cp, size_t len)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocHexString((u_char **) & buf, &buf_len, &out_len, 0,
                                 cp, len))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintAsciiString(char *buf, size_t buf_len,
                    const u_char * cp, size_t len)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocAsciiString
        ((u_char **) & buf, &buf_len, &out_len, 0, cp, len))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintOctetString(char *buf, size_t buf_len,
                     const Types_VariableList * var, const struct Parse_EnumList_s *enums,
                     const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocOctetString
        ((u_char **) & buf, &buf_len, &out_len, 0, var, enums, hint,
         units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintOpaque(char *buf, size_t buf_len,
               const Types_VariableList * var, const struct Parse_EnumList_s *enums,
               const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocOpaque((u_char **) & buf, &buf_len, &out_len, 0,
                              var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintObjectIdentifier(char *buf, size_t buf_len,
                          const Types_VariableList * var,
                          const struct Parse_EnumList_s *enums, const char *hint,
                          const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocObjectIdentifier
        ((u_char **) & buf, &buf_len, &out_len, 0, var, enums, hint,
         units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintTimeTicks(char *buf, size_t buf_len,
                  const Types_VariableList * var, const struct Parse_EnumList_s *enums,
                  const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocTimeTicks((u_char **) & buf, &buf_len, &out_len, 0,
                                 var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintHintedInteger(char *buf, size_t buf_len,
                       long val, const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocHintedInteger
        ((u_char **) & buf, &buf_len, &out_len, 0, val, 'd', hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintInteger(char *buf, size_t buf_len,
                const Types_VariableList * var, const struct Parse_EnumList_s *enums,
                const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocInteger((u_char **) & buf, &buf_len, &out_len, 0,
                               var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintUinteger(char *buf, size_t buf_len,
                 const Types_VariableList * var, const struct Parse_EnumList_s *enums,
                 const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocUinteger((u_char **) & buf, &buf_len, &out_len, 0,
                                var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintGauge(char *buf, size_t buf_len,
              const Types_VariableList * var, const struct Parse_EnumList_s *enums,
              const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocGauge((u_char **) & buf, &buf_len, &out_len, 0,
                             var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintCounter(char *buf, size_t buf_len,
                const Types_VariableList * var, const struct Parse_EnumList_s *enums,
                const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocCounter((u_char **) & buf, &buf_len, &out_len, 0,
                               var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintNetworkAddress(char *buf, size_t buf_len,
                       const Types_VariableList * var,
                       const struct Parse_EnumList_s *enums, const char *hint,
                       const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocNetworkAddress
        ((u_char **) & buf, &buf_len, &out_len, 0, var, enums, hint,
         units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintIpAddress(char *buf, size_t buf_len,
                  const Types_VariableList * var, const struct Parse_EnumList_s *enums,
                  const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocIpAddress((u_char **) & buf, &buf_len, &out_len, 0,
                                 var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintNull(char *buf, size_t buf_len,
             const Types_VariableList * var, const struct Parse_EnumList_s *enums,
             const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocNull((u_char **) & buf, &buf_len, &out_len, 0,
                            var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintBitString(char *buf, size_t buf_len,
                  const Types_VariableList * var, const struct Parse_EnumList_s *enums,
                  const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocBitString((u_char **) & buf, &buf_len, &out_len, 0,
                                 var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintNsapAddress(char *buf, size_t buf_len,
                    const Types_VariableList * var, const struct Parse_EnumList_s *enums,
                    const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocNsapAddress
        ((u_char **) & buf, &buf_len, &out_len, 0, var, enums, hint,
         units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintCounter64(char *buf, size_t buf_len,
                  const Types_VariableList * var, const struct Parse_EnumList_s *enums,
                  const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocCounter64((u_char **) & buf, &buf_len, &out_len, 0,
                                 var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintBadType(char *buf, size_t buf_len,
                const Types_VariableList * var, const struct Parse_EnumList_s *enums,
                const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocBadType((u_char **) & buf, &buf_len, &out_len, 0,
                               var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintFloat(char *buf, size_t buf_len,
              const Types_VariableList * var, const struct Parse_EnumList_s *enums,
              const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocFloat((u_char **) & buf, &buf_len, &out_len, 0,
                             var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}

int Mib_snprintDouble(char *buf, size_t buf_len,
               const Types_VariableList * var, const struct Parse_EnumList_s *enums,
               const char *hint, const char *units)
{
    size_t          out_len = 0;
    if (Mib_sprintReallocDouble((u_char **) & buf, &buf_len, &out_len, 0,
                              var, enums, hint, units))
        return (int) out_len;
    else
        return -1;
}



