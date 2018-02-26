#include "Parse.h"
#include "DefaultStore.h"
#include "Debug.h"
#include "Logger.h"
#include "Tools.h"
#include "Mib.h"
#include <regex.h>
#include "Strlcat.h"
#include "Strlcpy.h"
#include "ReadConfig.h"


/*
 * A linked list of nodes.
 */
struct Parse_Node_s {
    struct Parse_Node_s    *next;
    char           *label;  /* This node's (unique) textual name */
    u_long          subid;  /* This node's integer subidentifier */
    int             modid;  /* The module containing this node */
    char           *parent; /* The parent's textual name */
    int             tc_index; /* index into tclist (-1 if NA) */
    int             type;   /* The type of object this represents */
    int             access;
    int             status;
    struct Parse_EnumList_s *enums; /* (optional) list of enumerated integers */
    struct Parse_RangeList_s *ranges;
    struct Parse_IndexList_s *indexes;
    char           *augments;
    struct Parse_VarbindList_s *varbinds;
    char           *hint;
    char           *units;
    char           *description; /* description (a quoted string) */
    char           *reference; /* references (a quoted string) */
    char           *defaultValue;
    char           *filename;
    int             lineno;
};

/*
 * This is one element of an object identifier with either an integer
 * subidentifier, or a textual string label, or both.
 * The subid is -1 if not present, and label is NULL if not present.
 */
struct Parse_Subid_s {
    int             subid;
    int             modid;
    char           *label;
};

#define PARSE_MAXTC   4096

struct Parse_Tc_s {                     /* textual conventions */
    int             type;
    int             modid;
    char           *descriptor;
    char           *hint;
    struct Parse_EnumList_s *enums;
    struct Parse_RangeList_s *ranges;
    char           *description;
} parse_tclist[PARSE_MAXTC];

int             parse_mibLine = 0;
const char     *parse_file = "(none)";
static int      parse_anonymous = 0;

struct Parse_Objgroup_s {
    char           *name;
    int             line;
    struct Parse_Objgroup_s *next;
} *parse_objgroups = NULL, *parse_objects = NULL, *parse_notifs = NULL;

#define PARSE_SYNTAX_MASK     0x80
/*
 * types of tokens
 * Tokens wiht the SYNTAX_MASK bit set are syntax tokens
 */
#define PARSE_CONTINUE    -1
#define PARSE_ENDOFFILE   0
#define PARSE_LABEL       1
#define PARSE_SUBTREE     2
#define PARSE_SYNTAX      3
#define PARSE_OBJID       (4 | PARSE_SYNTAX_MASK)
#define PARSE_OCTETSTR    (5 | PARSE_SYNTAX_MASK)
#define PARSE_INTEGER     (6 | PARSE_SYNTAX_MASK)
#define PARSE_NETADDR     (7 | PARSE_SYNTAX_MASK)
#define PARSE_IPADDR      (8 | PARSE_SYNTAX_MASK)
#define PARSE_COUNTER     (9 | PARSE_SYNTAX_MASK)
#define PARSE_GAUGE       (10 | PARSE_SYNTAX_MASK)
#define PARSE_TIMETICKS   (11 | PARSE_SYNTAX_MASK)
#define PARSE_KW_OPAQUE   (12 | PARSE_SYNTAX_MASK)
#define PARSE_NUL         (13 | PARSE_SYNTAX_MASK)
#define PARSE_SEQUENCE    14
#define PARSE_OF          15          /* SEQUENCE OF */
#define PARSE_OBJTYPE     16
#define PARSE_ACCESS      17
#define PARSE_READONLY    18
#define PARSE_READWRITE   19
#define PARSE_WRITEONLY   20

#define PARSE_NOACCESS    21
#define PARSE_STATUS      22
#define PARSE_MANDATORY   23
#define PARSE_KW_OPTIONAL 24
#define PARSE_OBSOLETE    25
/*
 * #define RECOMMENDED 26
 */
#define PARSE_PUNCT       27
#define PARSE_EQUALS      28
#define PARSE_NUMBER      29
#define PARSE_LEFTBRACKET 30
#define PARSE_RIGHTBRACKET 31
#define PARSE_LEFTPAREN   32
#define PARSE_RIGHTPAREN  33
#define PARSE_COMMA       34
#define PARSE_DESCRIPTION 35
#define PARSE_QUOTESTRING 36
#define PARSE_INDEX       37
#define PARSE_DEFVAL      38
#define PARSE_DEPRECATED  39
#define PARSE_SIZE        40
#define PARSE_BITSTRING   (41 | PARSE_SYNTAX_MASK)
#define PARSE_NSAPADDRESS (42 | PARSE_SYNTAX_MASK)
#define PARSE_COUNTER64   (43 | PARSE_SYNTAX_MASK)
#define PARSE_OBJGROUP    44
#define PARSE_NOTIFTYPE   45
#define PARSE_AUGMENTS    46
#define PARSE_COMPLIANCE  47
#define PARSE_READCREATE  48
#define PARSE_UNITS       49
#define PARSE_REFERENCE   50
#define PARSE_NUM_ENTRIES 51
#define PARSE_MODULEIDENTITY 52
#define PARSE_LASTUPDATED 53
#define PARSE_ORGANIZATION 54
#define PARSE_CONTACTINFO 55
#define PARSE_UINTEGER32 (56 | PARSE_SYNTAX_MASK)
#define PARSE_CURRENT     57
#define PARSE_DEFINITIONS 58
#define PARSE_END         59
#define PARSE_SEMI        60
#define PARSE_TRAPTYPE    61
#define PARSE_ENTERPRISE  62
/*
 * #define DISPLAYSTR (63 | SYNTAX_MASK)
 */
#define PARSE_BEGIN       64
#define PARSE_IMPORTS     65
#define PARSE_EXPORTS     66
#define PARSE_ACCNOTIFY   67
#define PARSE_BAR         68
#define PARSE_RANGE       69
#define PARSE_CONVENTION  70
#define PARSE_DISPLAYHINT 71
#define PARSE_FROM        72
#define PARSE_AGENTCAP    73
#define PARSE_MACRO       74
#define PARSE_IMPLIED     75
#define PARSE_SUPPORTS    76
#define PARSE_INCLUDES    77
#define PARSE_VARIATION   78
#define PARSE_REVISION    79
#define PARSE_NOTIMPL	    80
#define PARSE_OBJECTS	    81
#define PARSE_NOTIFICATIONS	82
#define PARSE_MODULE	    83
#define PARSE_MINACCESS     84
#define PARSE_PRODREL	    85
#define PARSE_WRSYNTAX      86
#define PARSE_CREATEREQ     87
#define PARSE_NOTIFGROUP    88
#define PARSE_MANDATORYGROUPS	89
#define PARSE_GROUP	        90
#define PARSE_OBJECT	    91
#define PARSE_IDENTIFIER    92
#define PARSE_CHOICE	    93
#define PARSE_LEFTSQBRACK	95
#define PARSE_RIGHTSQBRACK	96
#define PARSE_IMPLICIT      97
#define PARSE_APPSYNTAX	    (98 | PARSE_SYNTAX_MASK)
#define PARSE_OBJSYNTAX	    (99 | PARSE_SYNTAX_MASK)
#define PARSE_SIMPLESYNTAX	(100 | PARSE_SYNTAX_MASK)
#define PARSE_OBJNAME		(101 | PARSE_SYNTAX_MASK)
#define PARSE_NOTIFNAME	    (102 | PARSE_SYNTAX_MASK)
#define PARSE_VARIABLES	    103
#define PARSE_UNSIGNED32	(104 | PARSE_SYNTAX_MASK)
#define PARSE_INTEGER32	    (105 | PARSE_SYNTAX_MASK)
#define PARSE_OBJIDENTITY	106
/*
 * Beware of reaching SYNTAX_MASK (0x80)
 */

struct Parse_Tok_s {
    const char     *name;       /* token name */
    int             len;        /* length not counting nul */
    int             token;      /* value */
    int             hash;       /* hash of name */
    struct Parse_Tok_s *next;       /* pointer to next in hash table */
};


static struct Parse_Tok_s parse_tokens[] = {
    {"obsolete", sizeof("obsolete") - 1, PARSE_OBSOLETE}
    ,
    {"Opaque", sizeof("Opaque") - 1, PARSE_KW_OPAQUE}
    ,
    {"optional", sizeof("optional") - 1, PARSE_KW_OPTIONAL}
    ,
    {"LAST-UPDATED", sizeof("LAST-UPDATED") - 1, PARSE_LASTUPDATED}
    ,
    {"ORGANIZATION", sizeof("ORGANIZATION") - 1, PARSE_ORGANIZATION}
    ,
    {"CONTACT-INFO", sizeof("CONTACT-INFO") - 1, PARSE_CONTACTINFO}
    ,
    {"MODULE-IDENTITY", sizeof("MODULE-IDENTITY") - 1, PARSE_MODULEIDENTITY}
    ,
    {"MODULE-COMPLIANCE", sizeof("MODULE-COMPLIANCE") - 1, PARSE_COMPLIANCE}
    ,
    {"DEFINITIONS", sizeof("DEFINITIONS") - 1, PARSE_DEFINITIONS}
    ,
    {"END", sizeof("END") - 1, PARSE_END}
    ,
    {"AUGMENTS", sizeof("AUGMENTS") - 1, PARSE_AUGMENTS}
    ,
    {"not-accessible", sizeof("not-accessible") - 1, PARSE_NOACCESS}
    ,
    {"write-only", sizeof("write-only") - 1, PARSE_WRITEONLY}
    ,
    {"NsapAddress", sizeof("NsapAddress") - 1, PARSE_NSAPADDRESS}
    ,
    {"UNITS", sizeof("Units") - 1, PARSE_UNITS}
    ,
    {"REFERENCE", sizeof("REFERENCE") - 1, PARSE_REFERENCE}
    ,
    {"NUM-ENTRIES", sizeof("NUM-ENTRIES") - 1, PARSE_NUM_ENTRIES}
    ,
    {"BITSTRING", sizeof("BITSTRING") - 1, PARSE_BITSTRING}
    ,
    {"BIT", sizeof("BIT") - 1, PARSE_CONTINUE}
    ,
    {"BITS", sizeof("BITS") - 1, PARSE_BITSTRING}
    ,
    {"Counter64", sizeof("Counter64") - 1, PARSE_COUNTER64}
    ,
    {"TimeTicks", sizeof("TimeTicks") - 1, PARSE_TIMETICKS}
    ,
    {"NOTIFICATION-TYPE", sizeof("NOTIFICATION-TYPE") - 1, PARSE_NOTIFTYPE}
    ,
    {"OBJECT-GROUP", sizeof("OBJECT-GROUP") - 1, PARSE_OBJGROUP}
    ,
    {"OBJECT-IDENTITY", sizeof("OBJECT-IDENTITY") - 1, PARSE_OBJIDENTITY}
    ,
    {"IDENTIFIER", sizeof("IDENTIFIER") - 1, PARSE_IDENTIFIER}
    ,
    {"OBJECT", sizeof("OBJECT") - 1, PARSE_OBJECT}
    ,
    {"NetworkAddress", sizeof("NetworkAddress") - 1, PARSE_NETADDR}
    ,
    {"Gauge", sizeof("Gauge") - 1, PARSE_GAUGE}
    ,
    {"Gauge32", sizeof("Gauge32") - 1, PARSE_GAUGE}
    ,
    {"Unsigned32", sizeof("Unsigned32") - 1, PARSE_UNSIGNED32}
    ,
    {"read-write", sizeof("read-write") - 1, PARSE_READWRITE}
    ,
    {"read-create", sizeof("read-create") - 1, PARSE_READCREATE}
    ,
    {"OCTETSTRING", sizeof("OCTETSTRING") - 1, PARSE_OCTETSTR}
    ,
    {"OCTET", sizeof("OCTET") - 1, PARSE_CONTINUE}
    ,
    {"OF", sizeof("OF") - 1, PARSE_OF}
    ,
    {"SEQUENCE", sizeof("SEQUENCE") - 1, PARSE_SEQUENCE}
    ,
    {"NULL", sizeof("NULL") - 1, PARSE_NUL}
    ,
    {"IpAddress", sizeof("IpAddress") - 1, PARSE_IPADDR}
    ,
    {"UInteger32", sizeof("UInteger32") - 1, PARSE_UINTEGER32}
    ,
    {"INTEGER", sizeof("INTEGER") - 1, PARSE_INTEGER}
    ,
    {"Integer32", sizeof("Integer32") - 1, PARSE_INTEGER32}
    ,
    {"Counter", sizeof("Counter") - 1, PARSE_COUNTER}
    ,
    {"Counter32", sizeof("Counter32") - 1, PARSE_COUNTER}
    ,
    {"read-only", sizeof("read-only") - 1, PARSE_READONLY}
    ,
    {"DESCRIPTION", sizeof("DESCRIPTION") - 1, PARSE_DESCRIPTION}
    ,
    {"INDEX", sizeof("INDEX") - 1, PARSE_INDEX}
    ,
    {"DEFVAL", sizeof("DEFVAL") - 1, PARSE_DEFVAL}
    ,
    {"deprecated", sizeof("deprecated") - 1, PARSE_DEPRECATED}
    ,
    {"SIZE", sizeof("SIZE") - 1, PARSE_SIZE}
    ,
    {"MAX-ACCESS", sizeof("MAX-ACCESS") - 1, PARSE_ACCESS}
    ,
    {"ACCESS", sizeof("ACCESS") - 1, PARSE_ACCESS}
    ,
    {"mandatory", sizeof("mandatory") - 1, PARSE_MANDATORY}
    ,
    {"current", sizeof("current") - 1, PARSE_CURRENT}
    ,
    {"STATUS", sizeof("STATUS") - 1, PARSE_STATUS}
    ,
    {"SYNTAX", sizeof("SYNTAX") - 1, PARSE_SYNTAX}
    ,
    {"OBJECT-TYPE", sizeof("OBJECT-TYPE") - 1, PARSE_OBJTYPE}
    ,
    {"TRAP-TYPE", sizeof("TRAP-TYPE") - 1, PARSE_TRAPTYPE}
    ,
    {"ENTERPRISE", sizeof("ENTERPRISE") - 1, PARSE_ENTERPRISE}
    ,
    {"BEGIN", sizeof("BEGIN") - 1, PARSE_BEGIN}
    ,
    {"IMPORTS", sizeof("IMPORTS") - 1, PARSE_IMPORTS}
    ,
    {"EXPORTS", sizeof("EXPORTS") - 1, PARSE_EXPORTS}
    ,
    {"accessible-for-notify", sizeof("accessible-for-notify") - 1,
    PARSE_ACCNOTIFY}
    ,
    {"TEXTUAL-CONVENTION", sizeof("TEXTUAL-CONVENTION") - 1, PARSE_CONVENTION}
    ,
    {"NOTIFICATION-GROUP", sizeof("NOTIFICATION-GROUP") - 1, PARSE_NOTIFGROUP}
    ,
    {"DISPLAY-HINT", sizeof("DISPLAY-HINT") - 1, PARSE_DISPLAYHINT}
    ,
    {"FROM", sizeof("FROM") - 1, PARSE_FROM}
    ,
    {"AGENT-CAPABILITIES", sizeof("AGENT-CAPABILITIES") - 1, PARSE_AGENTCAP}
    ,
    {"MACRO", sizeof("MACRO") - 1, PARSE_MACRO}
    ,
    {"IMPLIED", sizeof("IMPLIED") - 1, PARSE_IMPLIED}
    ,
    {"SUPPORTS", sizeof("SUPPORTS") - 1, PARSE_SUPPORTS}
    ,
    {"INCLUDES", sizeof("INCLUDES") - 1, PARSE_INCLUDES}
    ,
    {"VARIATION", sizeof("VARIATION") - 1, PARSE_VARIATION}
    ,
    {"REVISION", sizeof("REVISION") - 1, PARSE_REVISION}
    ,
    {"not-implemented", sizeof("not-implemented") - 1, PARSE_NOTIMPL}
    ,
    {"OBJECTS", sizeof("OBJECTS") - 1, PARSE_OBJECTS}
    ,
    {"NOTIFICATIONS", sizeof("NOTIFICATIONS") - 1, PARSE_NOTIFICATIONS}
    ,
    {"MODULE", sizeof("MODULE") - 1, PARSE_MODULE}
    ,
    {"MIN-ACCESS", sizeof("MIN-ACCESS") - 1, PARSE_MINACCESS}
    ,
    {"PRODUCT-RELEASE", sizeof("PRODUCT-RELEASE") - 1, PARSE_PRODREL}
    ,
    {"WRITE-SYNTAX", sizeof("WRITE-SYNTAX") - 1, PARSE_WRSYNTAX}
    ,
    {"CREATION-REQUIRES", sizeof("CREATION-REQUIRES") - 1, PARSE_CREATEREQ}
    ,
    {"MANDATORY-GROUPS", sizeof("MANDATORY-GROUPS") - 1, PARSE_MANDATORYGROUPS}
    ,
    {"GROUP", sizeof("GROUP") - 1, PARSE_GROUP}
    ,
    {"CHOICE", sizeof("CHOICE") - 1, PARSE_CHOICE}
    ,
    {"IMPLICIT", sizeof("IMPLICIT") - 1, PARSE_IMPLICIT}
    ,
    {"ObjectSyntax", sizeof("ObjectSyntax") - 1, PARSE_OBJSYNTAX}
    ,
    {"SimpleSyntax", sizeof("SimpleSyntax") - 1, PARSE_SIMPLESYNTAX}
    ,
    {"ApplicationSyntax", sizeof("ApplicationSyntax") - 1, PARSE_APPSYNTAX}
    ,
    {"ObjectName", sizeof("ObjectName") - 1, PARSE_OBJNAME}
    ,
    {"NotificationName", sizeof("NotificationName") - 1, PARSE_NOTIFNAME}
    ,
    {"VARIABLES", sizeof("VARIABLES") - 1, PARSE_VARIABLES}
    ,
    {NULL}
};

static struct Parse_ModuleCompatability_s * parse_moduleMapHead;
static struct Parse_ModuleCompatability_s parse_moduleMap[] = {
    {"RFC1065-SMI", "RFC1155-SMI", NULL, 0},
    {"RFC1066-MIB", "RFC1156-MIB", NULL, 0},
    /*
     * 'mib' -> 'mib-2'
     */
    {"RFC1156-MIB", "RFC1158-MIB", NULL, 0},
    /*
     * 'snmpEnableAuthTraps' -> 'snmpEnableAuthenTraps'
     */
    {"RFC1158-MIB", "RFC1213-MIB", NULL, 0},
    /*
     * 'nullOID' -> 'zeroDotZero'
     */
    {"RFC1155-SMI", "SNMPv2-SMI", NULL, 0},
    {"RFC1213-MIB", "SNMPv2-SMI", "mib-2", 0},
    {"RFC1213-MIB", "SNMPv2-MIB", "sys", 3},
    {"RFC1213-MIB", "IF-MIB", "if", 2},
    {"RFC1213-MIB", "IP-MIB", "ip", 2},
    {"RFC1213-MIB", "IP-MIB", "icmp", 4},
    {"RFC1213-MIB", "TCP-MIB", "tcp", 3},
    {"RFC1213-MIB", "UDP-MIB", "udp", 3},
    {"RFC1213-MIB", "SNMPv2-SMI", "transmission", 0},
    {"RFC1213-MIB", "SNMPv2-MIB", "snmp", 4},
    {"RFC1231-MIB", "TOKENRING-MIB", NULL, 0},
    {"RFC1271-MIB", "RMON-MIB", NULL, 0},
    {"RFC1286-MIB", "SOURCE-ROUTING-MIB", "dot1dSr", 7},
    {"RFC1286-MIB", "BRIDGE-MIB", NULL, 0},
    {"RFC1315-MIB", "FRAME-RELAY-DTE-MIB", NULL, 0},
    {"RFC1316-MIB", "CHARACTER-MIB", NULL, 0},
    {"RFC1406-MIB", "DS1-MIB", NULL, 0},
    {"RFC-1213", "RFC1213-MIB", NULL, 0},
};

#define PARSE_MODULE_NOT_FOUND	0
#define PARSE_MODULE_LOADED_OK	1
#define PARSE_MODULE_ALREADY_LOADED	2
/*
 * #define MODULE_LOAD_FAILED   3
 */
#define PARSE_MODULE_LOAD_FAILED	PARSE_MODULE_NOT_FOUND
#define PARSE_MODULE_SYNTAX_ERROR     4

int   parse_gMibError = 0, parse_gLoop = 0;
char *parse_gpMibErrorString = NULL;
char parse_gMibNames[READCONFIG_STRINGMAX];

#define PARSE_HASHSIZE        32
#define PARSE_BUCKET(x)       (x & (PARSE_HASHSIZE-1))

#define PARSE_NHASHSIZE    128
#define PARSE_NBUCKET(x)   (x & (PARSE_NHASHSIZE-1))

static struct Parse_Tok_s *parse_buckets[PARSE_HASHSIZE];

static struct Parse_Node_s *parse_nbuckets[PARSE_NHASHSIZE];
static struct Parse_Tree_s *parse_tbuckets[PARSE_NHASHSIZE];
static struct Parse_Module_s *parse_moduleHead = NULL;

static struct Parse_Node_s *  parse_orphanNodes = NULL;

struct Parse_Tree_s * parse_treeHead = NULL;

#define	PARSE_NUMBER_OF_ROOT_NODES	3
static struct Parse_ModuleImport_s parse_rootImports[PARSE_NUMBER_OF_ROOT_NODES];

static int      parse_currentModule = 0;
static int      parse_maxModule = 0;
static int      parse_firstErrModule = 1;
static char    *parse_lastErrModule = NULL; /* no repeats on "Cannot find module..." */


static void     Parse_treeFromNode(struct Parse_Tree_s *tp, struct Parse_Node_s *np);
static void     Parse_doSubtree(struct Parse_Tree_s *, struct Parse_Node_s **);
static void     Parse_doLinkup(struct Parse_Module_s *, struct Parse_Node_s *);
static void     Parse_dumpModuleList(void);
static int      Parse_parseQuoteString(FILE *, char *, int);
static int      Parse_tossObjectIdentifier(FILE *);
static int      Parse_nameHash(const char *);
static void     Parse_initNodeHash(struct Parse_Node_s *);
static void     Parse_printError(const char *, const char *, int);
static void     Parse_freeTree(struct Parse_Tree_s *);
static void     Parse_freePartialTree(struct Parse_Tree_s *, int);
static void     Parse_freeNode(struct Parse_Node_s *);
static void     Parse_buildTranslationTable(void);
static void     Parse_initTreeRoots(void);
static void     Parse_mergeAnonChildren(struct Parse_Tree_s *, struct Parse_Tree_s *);
static void     Parse_unlinkTbucket(struct Parse_Tree_s *);
static void     Parse_unlinkTree(struct Parse_Tree_s *);
static int      Parse_getoid(FILE *, struct Parse_Subid_s *, int);
static struct Parse_Node_s *Parse_parseObjectid(FILE *, char *);
static int      Parse_getTc(const char *, int, int *, struct Parse_EnumList_s **,
                       struct Parse_RangeList_s **, char **);
static int      Parse_getTcIndex(const char *, int);
static struct Parse_EnumList_s *Parse_parseEnumlist(FILE *, struct Parse_EnumList_s **);
static struct Parse_RangeList_s *Parse_parseRanges(FILE * fp, struct Parse_RangeList_s **);
static struct Parse_Node_s *Parse_parseAsntype(FILE *, char *, int *, char *);
static struct Parse_Node_s *Parse_parseObjecttype(FILE *, char *);
static struct Parse_Node_s *Parse_parseObjectgroup(FILE *, char *, int,
                                      struct Parse_Objgroup_s **);
static struct Parse_Node_s *Parse_parseNotificationDefinition(FILE *, char *);
static struct Parse_Node_s *Parse_parseTrapDefinition(FILE *, char *);
static struct Parse_Node_s *Parse_parseCompliance(FILE *, char *);
static struct Parse_Node_s *Parse_parseCapabilities(FILE *, char *);
static struct Parse_Node_s *Parse_parseModuleIdentity(FILE *, char *);
static struct Parse_Node_s *Parse_parseMacro(FILE *, char *);
static void     Parse_parseImports(FILE *);
static struct Parse_Node_s * Parse_parse(FILE *, struct Parse_Node_s *);

static int     Parse_readModuleInternal(const char *);
static int     Parse_readModuleReplacements(const char *);
static int     Parse_readImportReplacements(const char *,
                                         struct Parse_ModuleImport_s *);

static void     Parse_newModule(const char *, const char *);

static struct Parse_Node_s *Parse_mergeParseObjectid(struct Parse_Node_s *, FILE *, char *);
static struct Parse_IndexList_s *Parse_getIndexes(FILE * fp, struct Parse_IndexList_s **);
static struct Parse_VarbindList_s *Parse_getVarbinds(FILE * fp, struct Parse_VarbindList_s **);
static void     Parse_freeIndexes(struct Parse_IndexList_s **);
static void     Parse_freeVarbinds(struct Parse_VarbindList_s **);
static void     Parse_freeRanges(struct Parse_RangeList_s **);
static void     Parse_freeEnums(struct Parse_EnumList_s **);
static struct Parse_RangeList_s *Parse_copyRanges(struct Parse_RangeList_s *);
static struct Parse_EnumList_s *Parse_copyEnums(struct Parse_EnumList_s *);

static u_int    Parse_computeMatch(const char *search_base, const char *key);

void Parse_mibToggleOptionsUsage(const char *lead, FILE * outf)
{
    fprintf(outf, "%su:  %sallow the use of underlines in MIB symbols\n",
            lead, ((DefaultStore_getBoolean(DsStorage_LIBRARY_ID, DsBool_MIB_PARSE_LABEL)) ?
           "dis" : ""));
    fprintf(outf, "%sc:  %sallow the use of \"--\" to terminate comments\n",
            lead, ((DefaultStore_getBoolean(DsStorage_LIBRARY_ID, DsBool_MIB_COMMENT_TERM)) ?
           "" : "dis"));

    fprintf(outf, "%sd:  %ssave the DESCRIPTIONs of the MIB objects\n",
            lead, ((DefaultStore_getBoolean(DsStorage_LIBRARY_ID, DsBool_SAVE_MIB_DESCRS)) ?
           "do not " : ""));

    fprintf(outf, "%se:  disable errors when MIB symbols conflict\n", lead);

    fprintf(outf, "%sw:  enable warnings when MIB symbols conflict\n", lead);

    fprintf(outf, "%sW:  enable detailed warnings when MIB symbols conflict\n",
            lead);

    fprintf(outf, "%sR:  replace MIB symbols from latest module\n", lead);
}

char * Parse_mibToggleOptions(char *options)
{
    if (options) {
        while (*options) {
            switch (*options) {
            case 'u':
                DefaultStore_setBoolean(DsStorage_LIBRARY_ID, DsBool_MIB_PARSE_LABEL,
                               !DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                                               DsBool_MIB_PARSE_LABEL));
                break;

            case 'c':
                DefaultStore_toggleBoolean(DsStorage_LIBRARY_ID, DsBool_MIB_COMMENT_TERM);
                break;

            case 'e':
                DefaultStore_toggleBoolean(DsStorage_LIBRARY_ID, DsBool_MIB_ERRORS);
                break;

            case 'w':
                DefaultStore_setInt(DsStorage_LIBRARY_ID, DsInt_MIB_WARNINGS, 1);
                break;

            case 'W':
                DefaultStore_setInt(DsStorage_LIBRARY_ID, DsInt_MIB_WARNINGS, 2);
                break;

            case 'd':
                DefaultStore_toggleBoolean(DsStorage_LIBRARY_ID, DsBool_SAVE_MIB_DESCRS);
                break;

            case 'R':
                DefaultStore_toggleBoolean(DsStorage_LIBRARY_ID, DsBool_MIB_REPLACE);
                break;

            default:
                /*
                 * return at the unknown option
                 */
                return options;
            }
            options++;
        }
    }
    return NULL;
}

static int Parse_nameHash(const char *name)
{
    int             hash = 0;
    const char     *cp;

    if (!name)
        return 0;
    for (cp = name; *cp; cp++)
        hash += tolower((unsigned char)(*cp));
    return (hash);
}

void Parse_initMibInternals(void)
{
    register struct Parse_Tok_s *tp;
    register int    b, i;
    int             max_modc;

    if (parse_treeHead)
        return;

    /*
     * Set up hash list of pre-defined tokens
     */
    memset(parse_buckets, 0, sizeof(parse_buckets));
    for (tp = parse_tokens; tp->name; tp++) {
        tp->hash = Parse_nameHash(tp->name);
        b = PARSE_BUCKET(tp->hash);
        if (parse_buckets[b])
            tp->next = parse_buckets[b];      /* BUG ??? */
        parse_buckets[b] = tp;
    }

    /*
     * Initialise other internal structures
     */

    max_modc = sizeof(parse_moduleMap) / sizeof(parse_moduleMap[0]) - 1;
    for (i = 0; i < max_modc; ++i)
        parse_moduleMap[i].next = &(parse_moduleMap[i + 1]);
    parse_moduleMap[max_modc].next = NULL;
    parse_moduleMapHead = parse_moduleMap;

    memset(parse_nbuckets, 0, sizeof(parse_nbuckets));
    memset(parse_tbuckets, 0, sizeof(parse_tbuckets));
    memset(parse_tclist, 0, PARSE_MAXTC * sizeof(struct Parse_Tc_s));
    Parse_buildTranslationTable();
    Parse_initTreeRoots();          /* Set up initial roots */
    /*
     * Relies on 'add_mibdir' having set up the modules
     */
}


static void Parse_initNodeHash(struct Parse_Node_s *nodes)
{
    struct Parse_Node_s    *np, *nextp;
    int             hash;

    memset(parse_nbuckets, 0, sizeof(parse_nbuckets));
    for (np = nodes; np;) {
        nextp = np->next;
        hash = PARSE_NBUCKET(Parse_nameHash(np->parent));
        np->next = parse_nbuckets[hash];
        parse_nbuckets[hash] = np;
        np = nextp;
    }
}

static int      parse_erroneousMibs = 0;

int Parse_getMibParseErrorCount(void)
{
    return parse_erroneousMibs;
}


static void Parse_printError(const char *str, const char *token, int type)
{
    parse_erroneousMibs++;
    DEBUG_MSGTL(("parse-mibs", "\n"));
    if (type == PARSE_ENDOFFILE)
        Logger_log(LOGGER_PRIORITY_ERR, "%s (EOF): At line %d in %s\n", str, parse_mibLine,
                  parse_file);
    else if (token && *token)
        Logger_log(LOGGER_PRIORITY_ERR, "%s (%s): At line %d in %s\n", str, token,
                 parse_mibLine, parse_file);
    else
        Logger_log(LOGGER_PRIORITY_ERR, "%s: At line %d in %s\n", str, parse_mibLine, parse_file);
}

static void
Parse_printModuleNotFound(const char *cp)
{
    if (parse_firstErrModule) {
        Logger_log(LOGGER_PRIORITY_ERR, "MIB search path: %s\n",
                           Mib_getMibDirectory());
        parse_firstErrModule = 0;
    }
    if (!parse_lastErrModule || strcmp(cp, parse_lastErrModule))
        Parse_printError("Cannot find module", cp, PARSE_CONTINUE);
    if (parse_lastErrModule)
        free(parse_lastErrModule);
    parse_lastErrModule = strdup(cp);
}

static struct Parse_Node_s *
Parse_allocNode(int modid)
{
    struct Parse_Node_s    *np;
    np = (struct Parse_Node_s *) calloc(1, sizeof(struct Parse_Node_s));
    if (np) {
        np->tc_index = -1;
        np->modid = modid;
    np->filename = strdup(parse_file);
    np->lineno = parse_mibLine;
    }
    return np;
}

static void
Parse_unlinkTbucket(struct Parse_Tree_s *tp)
{
    int hash = PARSE_NBUCKET(Parse_nameHash(tp->label));
    struct Parse_Tree_s    *otp = NULL, *ntp = parse_tbuckets[hash];

    while (ntp && ntp != tp) {
        otp = ntp;
        ntp = ntp->next;
    }
    if (!ntp)
        Logger_log(LOGGER_PRIORITY_EMERG, "Can't find %s in tbuckets\n", tp->label);
    else if (otp)
        otp->next = ntp->next;
    else
        parse_tbuckets[hash] = tp->next;
}

static void
Parse_unlinkTree(struct Parse_Tree_s *tp)
{
    struct Parse_Tree_s    *otp = NULL, *ntp = tp->parent;

    if (!ntp) {                 /* this tree has no parent */
        DEBUG_MSGTL(("unlink_tree", "Tree node %s has no parent\n",
                    tp->label));
    } else {
        ntp = ntp->child_list;

        while (ntp && ntp != tp) {
            otp = ntp;
            ntp = ntp->next_peer;
        }
        if (!ntp)
            Logger_log(LOGGER_PRIORITY_EMERG, "Can't find %s in %s's children\n",
                     tp->label, tp->parent->label);
        else if (otp)
            otp->next_peer = ntp->next_peer;
        else
            tp->parent->child_list = tp->next_peer;
    }

    if (parse_treeHead == tp)
        parse_treeHead = tp->next_peer;
}

static void Parse_freePartialTree(struct Parse_Tree_s *tp, int keep_label)
{
    if (!tp)
        return;

    /*
     * remove the data from this tree node
     */
    Parse_freeEnums(&tp->enums);
    Parse_freeRanges(&tp->ranges);
    Parse_freeIndexes(&tp->indexes);
    Parse_freeVarbinds(&tp->varbinds);
    if (!keep_label)
        TOOLS_FREE(tp->label);
    TOOLS_FREE(tp->hint);
    TOOLS_FREE(tp->units);
    TOOLS_FREE(tp->description);
    TOOLS_FREE(tp->reference);
    TOOLS_FREE(tp->augments);
    TOOLS_FREE(tp->defaultValue);
}

/*
 * free a tree node. Note: the node must already have been unlinked
 * from the tree when calling this routine
 */
static void
Parse_freeTree(struct Parse_Tree_s *Tree)
{
    if (!Tree)
        return;

    Parse_unlinkTbucket(Tree);
    Parse_freePartialTree(Tree, FALSE);
    if (Tree->module_list != &Tree->modid)
        free(Tree->module_list);
    free(Tree);
}

static void Parse_freeNode(struct Parse_Node_s *np)
{
    if (!np)
        return;

    Parse_freeEnums(&np->enums);
    Parse_freeRanges(&np->ranges);
    Parse_freeIndexes(&np->indexes);
    Parse_freeVarbinds(&np->varbinds);
    if (np->label)
        free(np->label);
    if (np->hint)
        free(np->hint);
    if (np->units)
        free(np->units);
    if (np->description)
        free(np->description);
    if (np->reference)
        free(np->reference);
    if (np->defaultValue)
        free(np->defaultValue);
    if (np->parent)
        free(np->parent);
    if (np->augments)
        free(np->augments);
    if (np->filename)
    free(np->filename);
    free((char *) np);
}

static void Parse_printRangeValue(FILE * fp, int type, struct Parse_RangeList_s * rp)
{
    switch (type) {
    case PARSE_TYPE_INTEGER:
    case PARSE_TYPE_INTEGER32:
        if (rp->low == rp->high)
            fprintf(fp, "%d", rp->low);
        else
            fprintf(fp, "%d..%d", rp->low, rp->high);
        break;
    case PARSE_TYPE_UNSIGNED32:
    case PARSE_TYPE_OCTETSTR:
    case PARSE_TYPE_GAUGE:
    case PARSE_TYPE_UINTEGER:
        if (rp->low == rp->high)
            fprintf(fp, "%u", (unsigned)rp->low);
        else
            fprintf(fp, "%u..%u", (unsigned)rp->low, (unsigned)rp->high);
        break;
    default:
        /* No other range types allowed */
        break;
    }
}


void Parse_printSubtree(FILE * f, struct Parse_Tree_s *tree, int count)
{
    struct Parse_Tree_s    *tp;
    int             i;
    char            modbuf[256];

    for (i = 0; i < count; i++)
        fprintf(f, "  ");
    fprintf(f, "Children of %s(%ld):\n", tree->label, tree->subid);
    count++;
    for (tp = tree->child_list; tp; tp = tp->next_peer) {
        for (i = 0; i < count; i++)
            fprintf(f, "  ");
        fprintf(f, "%s:%s(%ld) type=%d",
                Parse_moduleName(tp->module_list[0], modbuf),
                tp->label, tp->subid, tp->type);
        if (tp->tc_index != -1)
            fprintf(f, " tc=%d", tp->tc_index);
        if (tp->hint)
            fprintf(f, " hint=%s", tp->hint);
        if (tp->units)
            fprintf(f, " units=%s", tp->units);
        if (tp->number_modules > 1) {
            fprintf(f, " modules:");
            for (i = 1; i < tp->number_modules; i++)
                fprintf(f, " %s", Parse_moduleName(tp->module_list[i], modbuf));
        }
        fprintf(f, "\n");
    }
    for (tp = tree->child_list; tp; tp = tp->next_peer) {
        if (tp->child_list)
            Parse_printSubtree(f, tp, count);
    }
}

void Parse_printAsciiDumpTree(FILE * f, struct Parse_Tree_s *tree, int count)
{
    struct Parse_Tree_s    *tp;

    count++;
    for (tp = tree->child_list; tp; tp = tp->next_peer) {
        fprintf(f, "%s OBJECT IDENTIFIER .= { %s %ld }\n", tp->label,
                tree->label, tp->subid);
    }
    for (tp = tree->child_list; tp; tp = tp->next_peer) {
        if (tp->child_list)
            Parse_printAsciiDumpTree(f, tp, count);
    }
}

static int      parse_translationTable[256];

static void
Parse_buildTranslationTable(void)
{
    int             count;

    for (count = 0; count < 256; count++) {
        switch (count) {
        case PARSE_OBJID:
            parse_translationTable[count] = PARSE_TYPE_OBJID;
            break;
        case PARSE_OCTETSTR:
            parse_translationTable[count] = PARSE_TYPE_OCTETSTR;
            break;
        case PARSE_INTEGER:
            parse_translationTable[count] = PARSE_TYPE_INTEGER;
            break;
        case PARSE_NETADDR:
            parse_translationTable[count] = PARSE_TYPE_NETADDR;
            break;
        case PARSE_IPADDR:
            parse_translationTable[count] = PARSE_TYPE_IPADDR;
            break;
        case PARSE_COUNTER:
            parse_translationTable[count] = PARSE_TYPE_COUNTER;
            break;
        case PARSE_GAUGE:
            parse_translationTable[count] = PARSE_TYPE_GAUGE;
            break;
        case PARSE_TIMETICKS:
            parse_translationTable[count] = PARSE_TYPE_TIMETICKS;
            break;
        case PARSE_KW_OPAQUE:
            parse_translationTable[count] = PARSE_TYPE_OPAQUE;
            break;
        case PARSE_NUL:
            parse_translationTable[count] = PARSE_TYPE_NULL;
            break;
        case PARSE_COUNTER64:
            parse_translationTable[count] = PARSE_TYPE_COUNTER64;
            break;
        case PARSE_BITSTRING:
            parse_translationTable[count] = PARSE_TYPE_BITSTRING;
            break;
        case PARSE_NSAPADDRESS:
            parse_translationTable[count] = PARSE_TYPE_NSAPADDRESS;
            break;
        case PARSE_INTEGER32:
            parse_translationTable[count] = PARSE_TYPE_INTEGER32;
            break;
        case PARSE_UINTEGER32:
            parse_translationTable[count] = PARSE_TYPE_UINTEGER;
            break;
        case PARSE_UNSIGNED32:
            parse_translationTable[count] = PARSE_TYPE_UNSIGNED32;
            break;
        case PARSE_TRAPTYPE:
            parse_translationTable[count] = PARSE_TYPE_TRAPTYPE;
            break;
        case PARSE_NOTIFTYPE:
            parse_translationTable[count] = PARSE_TYPE_NOTIFTYPE;
            break;
        case PARSE_NOTIFGROUP:
            parse_translationTable[count] = PARSE_TYPE_NOTIFGROUP;
            break;
        case PARSE_OBJGROUP:
            parse_translationTable[count] = PARSE_TYPE_OBJGROUP;
            break;
        case PARSE_MODULEIDENTITY:
            parse_translationTable[count] = PARSE_TYPE_MODID;
            break;
        case PARSE_OBJIDENTITY:
            parse_translationTable[count] = PARSE_TYPE_OBJIDENTITY;
            break;
        case PARSE_AGENTCAP:
            parse_translationTable[count] = PARSE_TYPE_AGENTCAP;
            break;
        case PARSE_COMPLIANCE:
            parse_translationTable[count] = PARSE_TYPE_MODCOMP;
            break;
        default:
            parse_translationTable[count] = PARSE_TYPE_OTHER;
            break;
        }
    }
}

static void
Parse_initTreeRoots(void)
{
    struct Parse_Tree_s    *tp, *lasttp;
    int             base_modid;
    int             hash;

    base_modid = Parse_whichModule("SNMPv2-SMI");
    if (base_modid == -1)
        base_modid = Parse_whichModule("RFC1155-SMI");
    if (base_modid == -1)
        base_modid = Parse_whichModule("RFC1213-MIB");

    /*
     * build root node
     */
    tp = (struct Parse_Tree_s *) calloc(1, sizeof(struct Parse_Tree_s));
    if (tp == NULL)
        return;
    tp->label = strdup("joint-iso-ccitt");
    tp->modid = base_modid;
    tp->number_modules = 1;
    tp->module_list = &(tp->modid);
    tp->subid = 2;
    tp->tc_index = -1;
    Mib_setFunction(tp);           /* from mib.c */
    hash = PARSE_NBUCKET(Parse_nameHash(tp->label));
    tp->next = parse_tbuckets[hash];
    parse_tbuckets[hash] = tp;
    lasttp = tp;
    parse_rootImports[0].label = strdup(tp->label);
    parse_rootImports[0].modid = base_modid;

    /*
     * build root node
     */
    tp = (struct Parse_Tree_s *) calloc(1, sizeof(struct Parse_Tree_s));
    if (tp == NULL)
        return;
    tp->next_peer = lasttp;
    tp->label = strdup("ccitt");
    tp->modid = base_modid;
    tp->number_modules = 1;
    tp->module_list = &(tp->modid);
    tp->subid = 0;
    tp->tc_index = -1;
    Mib_setFunction(tp);           /* from mib.c */
    hash = PARSE_NBUCKET(Parse_nameHash(tp->label));
    tp->next = parse_tbuckets[hash];
    parse_tbuckets[hash] = tp;
    lasttp = tp;
    parse_rootImports[1].label = strdup(tp->label);
    parse_rootImports[1].modid = base_modid;

    /*
     * build root node
     */
    tp = (struct Parse_Tree_s *) calloc(1, sizeof(struct Parse_Tree_s));
    if (tp == NULL)
        return;
    tp->next_peer = lasttp;
    tp->label = strdup("iso");
    tp->modid = base_modid;
    tp->number_modules = 1;
    tp->module_list = &(tp->modid);
    tp->subid = 1;
    tp->tc_index = -1;
    Mib_setFunction(tp);           /* from mib.c */
    hash = PARSE_NBUCKET(Parse_nameHash(tp->label));
    tp->next = parse_tbuckets[hash];
    parse_tbuckets[hash] = tp;
    lasttp = tp;
    parse_rootImports[2].label = strdup(tp->label);
    parse_rootImports[2].modid = base_modid;

    parse_treeHead = tp;
}


#define	PARSE_LABEL_COMPARE strcmp

struct Parse_Tree_s  * Parse_findTreeNode(const char *name, int modid)
{
    struct Parse_Tree_s    *tp, *headtp;
    int             count, *int_p;

    if (!name || !*name)
        return (NULL);

    headtp = parse_tbuckets[PARSE_NBUCKET(Parse_nameHash(name))];
    for (tp = headtp; tp; tp = tp->next) {
        if (tp->label && !PARSE_LABEL_COMPARE(tp->label, name)) {

            if (modid == -1)    /* Any module */
                return (tp);

            for (int_p = tp->module_list, count = 0;
                 count < tp->number_modules; ++count, ++int_p)
                if (*int_p == modid)
                    return (tp);
        }
    }

    return (NULL);
}

/*
 * computes a value which represents how close name1 is to name2.
 * * high scores mean a worse match.
 * * (yes, the algorithm sucks!)
 */
#define PARSE_MAX_BAD 0xffffff

static          u_int
Parse_computeMatch(const char *search_base, const char *key)
{
    int             rc;
    regex_t         parsetree;
    regmatch_t      pmatch;
    rc = regcomp(&parsetree, key, REG_ICASE | REG_EXTENDED);
    if (rc == 0)
        rc = regexec(&parsetree, search_base, 1, &pmatch, 0);
    regfree(&parsetree);
    if (rc == 0) {
        /*
         * found
         */
        return pmatch.rm_so;
    }


    /*
     * not found
     */
    return PARSE_MAX_BAD;
}

/*
 * Find the tree node that best matches the pattern string.
 * Use the "reported" flag such that only one match
 * is attempted for every node.
 *
 * Warning! This function may recurse.
 *
 * Caller _must_ invoke clear_tree_flags before first call
 * to this function.  This function may be called multiple times
 * to ensure that the entire tree is traversed.
 */

struct Parse_Tree_s  * Parse_findBestTreeNode(const char *pattrn, struct Parse_Tree_s *tree_top,
                    u_int * match)
{
    struct Parse_Tree_s    *tp, *best_so_far = NULL, *retptr;
    u_int           old_match = PARSE_MAX_BAD, new_match = PARSE_MAX_BAD;

    if (!pattrn || !*pattrn)
        return (NULL);

    if (!tree_top)
        tree_top = Mib_getTreeHead();

    for (tp = tree_top; tp; tp = tp->next_peer) {
        if (!tp->reported && tp->label)
            new_match = Parse_computeMatch(tp->label, pattrn);
        tp->reported = 1;

        if (new_match < old_match) {
            best_so_far = tp;
            old_match = new_match;
        }
        if (new_match == 0)
            break;              /* this is the best result we can get */
        if (tp->child_list) {
            retptr =
                Parse_findBestTreeNode(pattrn, tp->child_list, &new_match);
            if (new_match < old_match) {
                best_so_far = retptr;
                old_match = new_match;
            }
            if (new_match == 0)
                break;          /* this is the best result we can get */
        }
    }
    if (match)
        *match = old_match;
    return (best_so_far);
}


static void Parse_mergeAnonChildren(struct Parse_Tree_s *tp1, struct Parse_Tree_s *tp2)
                /*
                 * NB: tp1 is the 'anonymous' node
                 */
{
    struct Parse_Tree_s    *child1, *child2, *previous;

    for (child1 = tp1->child_list; child1;) {

        for (child2 = tp2->child_list, previous = NULL;
             child2; previous = child2, child2 = child2->next_peer) {

            if (child1->subid == child2->subid) {
                /*
                 * Found 'matching' children,
                 *  so merge them
                 */
                if (!strncmp(child1->label, PARSE_ANON, PARSE_ANON_LEN)) {
                    Parse_mergeAnonChildren(child1, child2);

                    child1->child_list = NULL;
                    previous = child1;  /* Finished with 'child1' */
                    child1 = child1->next_peer;
                    Parse_freeTree(previous);
                    goto next;
                }

                else if (!strncmp(child2->label, PARSE_ANON, PARSE_ANON_LEN)) {
                    Parse_mergeAnonChildren(child2, child1);

                    if (previous)
                        previous->next_peer = child2->next_peer;
                    else
                        tp2->child_list = child2->next_peer;
                    Parse_freeTree(child2);

                    previous = child1;  /* Move 'child1' to 'tp2' */
                    child1 = child1->next_peer;
                    previous->next_peer = tp2->child_list;
                    tp2->child_list = previous;
                    for (previous = tp2->child_list;
                         previous; previous = previous->next_peer)
                        previous->parent = tp2;
                    goto next;
                } else if (!PARSE_LABEL_COMPARE(child1->label, child2->label)) {
                    if (DefaultStore_getInt(DsStorage_LIBRARY_ID,
                           DsInt_MIB_WARNINGS)) {
                        Logger_log(LOGGER_PRIORITY_WARNING,
                                 "Warning: %s.%ld is both %s and %s (%s)\n",
                                 tp2->label, child1->subid, child1->label,
                                 child2->label, parse_file);
            }
                    continue;
                } else {
                    /*
                     * Two copies of the same node.
                     * 'child2' adopts the children of 'child1'
                     */

                    if (child2->child_list) {
                        for (previous = child2->child_list; previous->next_peer; previous = previous->next_peer);       /* Find the end of the list */
                        previous->next_peer = child1->child_list;
                    } else
                        child2->child_list = child1->child_list;
                    for (previous = child1->child_list;
                         previous; previous = previous->next_peer)
                        previous->parent = child2;
                    child1->child_list = NULL;

                    previous = child1;  /* Finished with 'child1' */
                    child1 = child1->next_peer;
                    Parse_freeTree(previous);
                    goto next;
                }
            }
        }
        /*
         * If no match, move 'child1' to 'tp2' child_list
         */
        if (child1) {
            previous = child1;
            child1 = child1->next_peer;
            previous->parent = tp2;
            previous->next_peer = tp2->child_list;
            tp2->child_list = previous;
        }
      next:;
    }
}


/*
 * Find all the children of root in the list of nodes.  Link them into the
 * tree and out of the nodes list.
 */
static void Parse_doSubtree(struct Parse_Tree_s *root, struct Parse_Node_s **nodes)
{
    struct Parse_Tree_s    *tp, *anon_tp = NULL;
    struct Parse_Tree_s    *xroot = root;
    struct Parse_Node_s    *np, **headp;
    struct Parse_Node_s    *oldnp = NULL, *child_list = NULL, *childp = NULL;
    int             hash;
    int            *int_p;

    while (xroot->next_peer && xroot->next_peer->subid == root->subid) {

        xroot = xroot->next_peer;
    }

    tp = root;
    headp = &parse_nbuckets[PARSE_NBUCKET(Parse_nameHash(tp->label))];
    /*
     * Search each of the nodes for one whose parent is root, and
     * move each into a separate list.
     */
    for (np = *headp; np; np = np->next) {
        if (!PARSE_LABEL_COMPARE(tp->label, np->parent)) {
            /*
             * take this node out of the node list
             */
            if (oldnp == NULL) {
                *headp = np->next;      /* fix root of node list */
            } else {
                oldnp->next = np->next; /* link around this node */
            }
            if (child_list)
                childp->next = np;
            else
                child_list = np;
            childp = np;
        } else {
            oldnp = np;
        }

    }
    if (childp)
        childp->next = NULL;
    /*
     * Take each element in the child list and place it into the tree.
     */
    for (np = child_list; np; np = np->next) {
        struct Parse_Tree_s    *otp = NULL;
        struct Parse_Tree_s    *xxroot = xroot;
        anon_tp = NULL;
        tp = xroot->child_list;

        if (np->subid == -1) {
            /*
             * name .= { parent }
             */
            np->subid = xroot->subid;
            tp = xroot;
            xxroot = xroot->parent;
        }

        while (tp) {
            if (tp->subid == np->subid)
                break;
            else {
                otp = tp;
                tp = tp->next_peer;
            }
        }
        if (tp) {
            if (!PARSE_LABEL_COMPARE(tp->label, np->label)) {
                /*
                 * Update list of modules
                 */
                int_p = (int *)malloc((tp->number_modules + 1) * sizeof(int));
                if (int_p == NULL)
                    return;
                memcpy(int_p, tp->module_list,
                       tp->number_modules * sizeof(int));
                int_p[tp->number_modules] = np->modid;
                if (tp->module_list != &tp->modid)
                    free(tp->module_list);
                ++tp->number_modules;
                tp->module_list = int_p;

                if (DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                       DsBool_MIB_REPLACE)) {
                    /*
                     * Replace from node
                     */
                    Parse_treeFromNode(tp, np);
                }
                /*
                 * Handle children
                 */
                Parse_doSubtree(tp, nodes);
                continue;
            }
            if (!strncmp(np->label, PARSE_ANON, PARSE_ANON_LEN) ||
                !strncmp(tp->label, PARSE_ANON, PARSE_ANON_LEN)) {
                anon_tp = tp;   /* Need to merge these two trees later */
            } else if (DefaultStore_getInt(DsStorage_LIBRARY_ID,
                      DsInt_MIB_WARNINGS)) {
                Logger_log(LOGGER_PRIORITY_WARNING,
                         "Warning: %s.%ld is both %s and %s (%s)\n",
                         root->label, np->subid, tp->label, np->label,
                         parse_file);
        }
        }

        tp = (struct Parse_Tree_s *) calloc(1, sizeof(struct Parse_Tree_s));
        if (tp == NULL)
            return;
        tp->parent = xxroot;
        tp->modid = np->modid;
        tp->number_modules = 1;
        tp->module_list = &(tp->modid);
        Parse_treeFromNode(tp, np);
        tp->next_peer = otp ? otp->next_peer : xxroot->child_list;
        if (otp)
            otp->next_peer = tp;
        else
            xxroot->child_list = tp;
        hash = PARSE_NBUCKET(Parse_nameHash(tp->label));
        tp->next = parse_tbuckets[hash];
        parse_tbuckets[hash] = tp;
        Parse_doSubtree(tp, nodes);

        if (anon_tp) {
            if (!strncmp(tp->label, PARSE_ANON, PARSE_ANON_LEN)) {
                /*
                 * The new node is anonymous,
                 *  so merge it with the existing one.
                 */
                Parse_mergeAnonChildren(tp, anon_tp);

                /*
                 * unlink and destroy tp
                 */
                Parse_unlinkTree(tp);
                Parse_freeTree(tp);
            } else if (!strncmp(anon_tp->label, PARSE_ANON, PARSE_ANON_LEN)) {
                struct Parse_Tree_s    *ntp;
                /*
                 * The old node was anonymous,
                 *  so merge it with the existing one,
                 *  and fill in the full information.
                 */
                Parse_mergeAnonChildren(anon_tp, tp);

                /*
                 * unlink anon_tp from the hash
                 */
                Parse_unlinkTbucket(anon_tp);

                /*
                 * get rid of old contents of anon_tp
                 */
                Parse_freePartialTree(anon_tp, FALSE);

                /*
                 * put in the current information
                 */
                anon_tp->label = tp->label;
                anon_tp->child_list = tp->child_list;
                anon_tp->modid = tp->modid;
                anon_tp->tc_index = tp->tc_index;
                anon_tp->type = tp->type;
                anon_tp->enums = tp->enums;
                anon_tp->indexes = tp->indexes;
                anon_tp->augments = tp->augments;
                anon_tp->varbinds = tp->varbinds;
                anon_tp->ranges = tp->ranges;
                anon_tp->hint = tp->hint;
                anon_tp->units = tp->units;
                anon_tp->description = tp->description;
                anon_tp->reference = tp->reference;
                anon_tp->defaultValue = tp->defaultValue;
                anon_tp->parent = tp->parent;

                Mib_setFunction(anon_tp);

                /*
                 * update parent pointer in moved children
                 */
                ntp = anon_tp->child_list;
                while (ntp) {
                    ntp->parent = anon_tp;
                    ntp = ntp->next_peer;
                }

                /*
                 * hash in anon_tp in its new place
                 */
                hash = PARSE_NBUCKET(Parse_nameHash(anon_tp->label));
                anon_tp->next = parse_tbuckets[hash];
                parse_tbuckets[hash] = anon_tp;

                /*
                 * unlink and destroy tp
                 */
                Parse_unlinkTbucket(tp);
                Parse_unlinkTree(tp);
                free(tp);
            } else {
                /*
                 * Uh?  One of these two should have been anonymous!
                 */
                if (DefaultStore_getInt(DsStorage_LIBRARY_ID,
                       DsInt_MIB_WARNINGS)) {
                    Logger_log(LOGGER_PRIORITY_WARNING,
                             "Warning: expected anonymous node (either %s or %s) in %s\n",
                             tp->label, anon_tp->label, parse_file);
        }
            }
            anon_tp = NULL;
        }
    }
    /*
     * free all nodes that were copied into tree
     */
    oldnp = NULL;
    for (np = child_list; np; np = np->next) {
        if (oldnp)
            Parse_freeNode(oldnp);
        oldnp = np;
    }
    if (oldnp)
        Parse_freeNode(oldnp);
}

static void Parse_doLinkup(struct Parse_Module_s *mp, struct Parse_Node_s *np)
{
    struct Parse_ModuleImport_s *mip;
    struct Parse_Node_s    *onp, *oldp, *newp;
    struct Parse_Tree_s    *tp;
    int             i, more;
    /*
     * All modules implicitly import
     *   the roots of the tree
     */
    if (Debug_getDoDebugging() > 1)
        Parse_dumpModuleList();
    DEBUG_MSGTL(("parse-mibs", "Processing IMPORTS for module %d %s\n",
                mp->modid, mp->name));
    if (mp->no_imports == 0) {
        mp->no_imports = PARSE_NUMBER_OF_ROOT_NODES;
        mp->imports = parse_rootImports;
    }

    /*
     * Build the tree
     */
    Parse_initNodeHash(np);
    for (i = 0, mip = mp->imports; i < mp->no_imports; ++i, ++mip) {
        char            modbuf[256];
        DEBUG_MSGTL(("parse-mibs", "  Processing import: %s\n",
                    mip->label));
        if (Parse_getTcIndex(mip->label, mip->modid) != -1)
            continue;
        tp = Parse_findTreeNode(mip->label, mip->modid);
        if (!tp) {
                Logger_log(LOGGER_PRIORITY_WARNING,
                         "Did not find '%s' in module %s (%s)\n",
                         mip->label, Parse_moduleName(mip->modid, modbuf),
                         parse_file);
            continue;
        }
        Parse_doSubtree(tp, &np);
    }

    /*
     * If any nodes left over,
     *   check that they're not the result of a "fully qualified"
     *   name, and then add them to the list of orphans
     */

    if (!np)
        return;
    for (tp = parse_treeHead; tp; tp = tp->next_peer)
        Parse_doSubtree(tp, &np);
    if (!np)
        return;

    /*
     * quietly move all internal references to the orphan list
     */
    oldp = parse_orphanNodes;
    do {
        for (i = 0; i < PARSE_NHASHSIZE; i++)
            for (onp = parse_nbuckets[i]; onp; onp = onp->next) {
                struct Parse_Node_s    *op = NULL;
                int             hash = PARSE_NBUCKET(Parse_nameHash(onp->label));
                np = parse_nbuckets[hash];
                while (np) {
                    if (PARSE_LABEL_COMPARE(onp->label, np->parent)) {
                        op = np;
                        np = np->next;
                    } else {
                        if (op)
                            op->next = np->next;
                        else
                            parse_nbuckets[hash] = np->next;
                        np->next = parse_orphanNodes;
                        parse_orphanNodes = np;
                        op = NULL;
                        np = parse_nbuckets[hash];
                    }
                }
            }
        newp = parse_orphanNodes;
        more = 0;
        for (onp = parse_orphanNodes; onp != oldp; onp = onp->next) {
            struct Parse_Node_s    *op = NULL;
            int             hash = PARSE_NBUCKET(Parse_nameHash(onp->label));
            np = parse_nbuckets[hash];
            while (np) {
                if (PARSE_LABEL_COMPARE(onp->label, np->parent)) {
                    op = np;
                    np = np->next;
                } else {
                    if (op)
                        op->next = np->next;
                    else
                        parse_nbuckets[hash] = np->next;
                    np->next = parse_orphanNodes;
                    parse_orphanNodes = np;
                    op = NULL;
                    np = parse_nbuckets[hash];
                    more = 1;
                }
            }
        }
        oldp = newp;
    } while (more);

    /*
     * complain about left over nodes
     */
    for (np = parse_orphanNodes; np && np->next; np = np->next);     /* find the end of the orphan list */
    for (i = 0; i < PARSE_NHASHSIZE; i++)
        if (parse_nbuckets[i]) {
            if (parse_orphanNodes)
                onp = np->next = parse_nbuckets[i];
            else
                onp = parse_orphanNodes = parse_nbuckets[i];
            parse_nbuckets[i] = NULL;
            while (onp) {
                Logger_log(LOGGER_PRIORITY_WARNING,
                         "Unlinked OID in %s: %s .= { %s %ld }\n",
                         (mp->name ? mp->name : "<no module>"),
                         (onp->label ? onp->label : "<no label>"),
                         (onp->parent ? onp->parent : "<no parent>"),
                         onp->subid);
         Logger_log(LOGGER_PRIORITY_WARNING,
              "Undefined identifier: %s near line %d of %s\n",
              (onp->parent ? onp->parent : "<no parent>"),
              onp->lineno, onp->filename);
                np = onp;
                onp = onp->next;
            }
        }
    return;
}


/*
 * Takes a list of the form:
 * { iso org(3) dod(6) 1 }
 * and creates several nodes, one for each parent-child pair.
 * Returns 0 on error.
 */
static int Parse_getoid(FILE * fp, struct Parse_Subid_s *id,   /* an array of subids */
       int length)
{                               /* the length of the array */
    register int    count;
    int             type;
    char            token[PARSE_MAXTOKEN];

    if ((type = Parse_getToken(fp, token, PARSE_MAXTOKEN)) != PARSE_LEFTBRACKET) {
        Parse_printError("Expected \"{\"", token, type);
        return 0;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    for (count = 0; count < length; count++, id++) {
        id->label = NULL;
        id->modid = parse_currentModule;
        id->subid = -1;
        if (type == PARSE_RIGHTBRACKET)
            return count;
        if (type == PARSE_LABEL) {
            /*
             * this entry has a label
             */
            id->label = strdup(token);
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type == PARSE_LEFTPAREN) {
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                if (type == PARSE_NUMBER) {
                    id->subid = strtoul(token, NULL, 10);
                    if ((type =
                         Parse_getToken(fp, token, PARSE_MAXTOKEN)) != PARSE_RIGHTPAREN) {
                        Parse_printError("Expected a closing parenthesis",
                                    token, type);
                        return 0;
                    }
                } else {
                    Parse_printError("Expected a number", token, type);
                    return 0;
                }
            } else {
                continue;
            }
        } else if (type == PARSE_NUMBER) {
            /*
             * this entry  has just an integer sub-identifier
             */
            id->subid = strtoul(token, NULL, 10);
        } else {
            Parse_printError("Expected label or number", token, type);
            return 0;
        }
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    }
    Parse_printError("Too long OID", token, type);
    return 0;
}

/*
 * Parse a sequence of object subidentifiers for the given name.
 * The "label OBJECT IDENTIFIER .=" portion has already been parsed.
 *
 * The majority of cases take this form :
 * label OBJECT IDENTIFIER .= { parent 2 }
 * where a parent label and a child subidentifier number are specified.
 *
 * Variations on the theme include cases where a number appears with
 * the parent, or intermediate subidentifiers are specified by label,
 * by number, or both.
 *
 * Here are some representative samples :
 * internet        OBJECT IDENTIFIER .= { iso org(3) dod(6) 1 }
 * mgmt            OBJECT IDENTIFIER .= { internet 2 }
 * rptrInfoHealth  OBJECT IDENTIFIER .= { snmpDot3RptrMgt 0 4 }
 *
 * Here is a very rare form :
 * iso             OBJECT IDENTIFIER .= { 1 }
 *
 * Returns NULL on error.  When this happens, memory may be leaked.
 */
static struct Parse_Node_s *
Parse_objectid(FILE * fp, char *name)
{
    register int    count;
    register struct Parse_Subid_s *op, *nop;
    int             length;
    struct Parse_Subid_s  loid[32];
    struct Parse_Node_s    *np, *root = NULL, *oldnp = NULL;
    struct Parse_Tree_s    *tp;

    if ((length = Parse_getoid(fp, loid, 32)) == 0) {
        Parse_printError("Bad object identifier", NULL, PARSE_CONTINUE);
        return NULL;
    }

    /*
     * Handle numeric-only object identifiers,
     *  by labelling the first sub-identifier
     */
    op = loid;
    if (!op->label) {
        if (length == 1) {
            Parse_printError("Attempt to define a root oid", name, PARSE_OBJECT);
            return NULL;
        }
        for (tp = parse_treeHead; tp; tp = tp->next_peer)
            if ((int) tp->subid == op->subid) {
                op->label = strdup(tp->label);
                break;
            }
    }

    /*
     * Handle  "label OBJECT-IDENTIFIER .= { subid }"
     */
    if (length == 1) {
        op = loid;
        np = Parse_allocNode(op->modid);
        if (np == NULL)
            return (NULL);
        np->subid = op->subid;
        np->label = strdup(name);
        np->parent = op->label;
        return np;
    }

    /*
     * For each parent-child subid pair in the subid array,
     * create a node and link it into the node list.
     */
    for (count = 0, op = loid, nop = loid + 1; count < (length - 1);
         count++, op++, nop++) {
        /*
         * every node must have parent's name and child's name or number
         */
        /*
         * XX the next statement is always true -- does it matter ??
         */
        if (op->label && (nop->label || (nop->subid != -1))) {
            np = Parse_allocNode(nop->modid);
            if (np == NULL)
                return (NULL);
            if (root == NULL)
                root = np;

            np->parent = strdup(op->label);
            if (count == (length - 2)) {
                /*
                 * The name for this node is the label for this entry
                 */
                np->label = strdup(name);
                if (np->label == NULL) {
                    TOOLS_FREE(np->parent);
                    TOOLS_FREE(np);
                    return (NULL);
                }
            } else {
                if (!nop->label) {
                    nop->label = (char *) malloc(20 + PARSE_ANON_LEN);
                    if (nop->label == NULL) {
                        TOOLS_FREE(np->parent);
                        TOOLS_FREE(np);
                        return (NULL);
                    }
                    sprintf(nop->label, "%s%d", PARSE_ANON, parse_anonymous++);
                }
                np->label = strdup(nop->label);
            }
            if (nop->subid != -1)
                np->subid = nop->subid;
            else
                Parse_printError("Warning: This entry is pretty silly",
                            np->label, PARSE_CONTINUE);

            /*
             * set up next entry
             */
            if (oldnp)
                oldnp->next = np;
            oldnp = np;
        }                       /* end if(op->label... */
    }

    /*
     * free the loid array
     */
    for (count = 0, op = loid; count < length; count++, op++) {
        if (op->label)
            free(op->label);
    }

    return root;
}

static int Parse_getTc(const char *descriptor,
       int modid,
       int *tc_index,
       struct Parse_EnumList_s **ep, struct Parse_RangeList_s **rp, char **hint)
{
    int             i;
    struct Parse_Tc_s      *tcp;

    i = Parse_getTcIndex(descriptor, modid);
    if (tc_index)
        *tc_index = i;
    if (i != -1) {
        tcp = &parse_tclist[i];
        if (ep) {
            Parse_freeEnums(ep);
            *ep = Parse_copyEnums(tcp->enums);
        }
        if (rp) {
            Parse_freeRanges(rp);
            *rp = Parse_copyRanges(tcp->ranges);
        }
        if (hint) {
            if (*hint)
                free(*hint);
            *hint = (tcp->hint ? strdup(tcp->hint) : NULL);
        }
        return tcp->type;
    }
    return PARSE_LABEL;
}

/*
 * return index into tclist of given TC descriptor
 * return -1 if not found
 */
static int
Parse_getTcIndex(const char *descriptor, int modid)
{
    int             i;
    struct Parse_Tc_s      *tcp;
    struct Parse_Module_s  *mp;
    struct Parse_ModuleImport_s *mip;

    /*
     * Check that the descriptor isn't imported
     *  by searching the import list
     */

    for (mp = parse_moduleHead; mp; mp = mp->next)
        if (mp->modid == modid)
            break;
    if (mp)
        for (i = 0, mip = mp->imports; i < mp->no_imports; ++i, ++mip) {
            if (!PARSE_LABEL_COMPARE(mip->label, descriptor)) {
                /*
                 * Found it - so amend the module ID
                 */
                modid = mip->modid;
                break;
            }
        }


    for (i = 0, tcp = parse_tclist; i < PARSE_MAXTC; i++, tcp++) {
        if (tcp->type == 0)
            break;
        if (!PARSE_LABEL_COMPARE(descriptor, tcp->descriptor) &&
            ((modid == tcp->modid) || (modid == -1))) {
            return i;
        }
    }
    return -1;
}

/*
 * translate integer tc_index to string identifier from tclist
 * *
 * * Returns pointer to string in table (should not be modified) or NULL
 */
const char     * Parse_getTcDescriptor(int tc_index)
{
    if (tc_index < 0 || tc_index >= PARSE_MAXTC)
        return NULL;
    return (parse_tclist[tc_index].descriptor);
}

/* used in the perl module */
const char     * Parse_getTcDescription(int tc_index)
{
    if (tc_index < 0 || tc_index >= PARSE_MAXTC)
        return NULL;
    return (parse_tclist[tc_index].description);
}

/*
 * Parses an enumeration list of the form:
 *        { label(value) label(value) ... }
 * The initial { has already been parsed.
 * Returns NULL on error.
 */

static struct Parse_EnumList_s *
Parse_parseEnumlist(FILE * fp, struct Parse_EnumList_s **retp)
{
    register int    type;
    char            token[PARSE_MAXTOKEN];
    struct Parse_EnumList_s *ep = NULL, **epp = &ep;

    Parse_freeEnums(retp);

    while ((type = Parse_getToken(fp, token, PARSE_MAXTOKEN)) != PARSE_ENDOFFILE) {
        if (type == PARSE_RIGHTBRACKET)
            break;
        /* some enums use "deprecated" to indicate a no longer value label */
        /* (EG: IP-MIB's IpAddressStatusTC) */
        if (type == PARSE_LABEL || type == PARSE_DEPRECATED) {
            /*
             * this is an enumerated label
             */
            *epp =
                (struct Parse_EnumList_s *) calloc(1, sizeof(struct Parse_EnumList_s));
            if (*epp == NULL)
                return (NULL);
            /*
             * a reasonable approximation for the length
             */
            (*epp)->label = strdup(token);
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type != PARSE_LEFTPAREN) {
                Parse_printError("Expected \"(\"", token, type);
                return NULL;
            }
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type != PARSE_NUMBER) {
                Parse_printError("Expected integer", token, type);
                return NULL;
            }
            (*epp)->value = strtol(token, NULL, 10);
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type != PARSE_RIGHTPAREN) {
                Parse_printError("Expected \")\"", token, type);
                return NULL;
            }
            epp = &(*epp)->next;
        }
    }
    if (type == PARSE_ENDOFFILE) {
        Parse_printError("Expected \"}\"", token, type);
        return NULL;
    }
    *retp = ep;
    return ep;
}

static struct Parse_RangeList_s *
Parse_parseRanges(FILE * fp, struct Parse_RangeList_s **retp)
{
    int             low, high;
    char            nexttoken[PARSE_MAXTOKEN];
    int             nexttype;
    struct Parse_RangeList_s *rp = NULL, **rpp = &rp;
    int             size = 0, taken = 1;

    Parse_freeRanges(retp);

    nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
    if (nexttype == PARSE_SIZE) {
        size = 1;
        taken = 0;
        nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
        if (nexttype != PARSE_LEFTPAREN)
            Parse_printError("Expected \"(\" after SIZE", nexttoken, nexttype);
    }

    do {
        if (!taken)
            nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
        else
            taken = 0;
        high = low = strtoul(nexttoken, NULL, 10);
        nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
        if (nexttype == PARSE_RANGE) {
            nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
            errno = 0;
            high = strtoul(nexttoken, NULL, 10);
            if ( errno == PARSE_RANGE ) {
                if (DefaultStore_getInt(DsStorage_LIBRARY_ID,
                                       DsInt_MIB_WARNINGS))
                    Logger_log(LOGGER_PRIORITY_WARNING,
                             "Warning: Upper bound not handled correctly (%s != %d): At line %d in %s\n",
                                 nexttoken, high, parse_mibLine, parse_file);
            }
            nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
        }
        *rpp = (struct Parse_RangeList_s *) calloc(1, sizeof(struct Parse_RangeList_s));
        if (*rpp == NULL)
            break;
        (*rpp)->low = low;
        (*rpp)->high = high;
        rpp = &(*rpp)->next;

    } while (nexttype == PARSE_BAR);
    if (size) {
        if (nexttype != PARSE_RIGHTPAREN)
            Parse_printError("Expected \")\" after SIZE", nexttoken, nexttype);
        nexttype = Parse_getToken(fp, nexttoken, nexttype);
    }
    if (nexttype != PARSE_RIGHTPAREN)
        Parse_printError("Expected \")\"", nexttoken, nexttype);

    *retp = rp;
    return rp;
}

/*
 * Parses an asn type.  Structures are ignored by this parser.
 * Returns NULL on error.
 */
static struct Parse_Node_s *
Parse_parseAsntype(FILE * fp, char *name, int *ntype, char *ntoken)
{
    int             type, i;
    char            token[PARSE_MAXTOKEN];
    char            quoted_string_buffer[PARSE_MAXQUOTESTR];
    char           *hint = NULL;
    char           *descr = NULL;
    struct Parse_Tc_s      *tcp;
    int             level;

    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type == PARSE_SEQUENCE || type == PARSE_CHOICE) {
        level = 0;
        while ((type = Parse_getToken(fp, token, PARSE_MAXTOKEN)) != PARSE_ENDOFFILE) {
            if (type == PARSE_LEFTBRACKET) {
                level++;
            } else if (type == PARSE_RIGHTBRACKET && --level == 0) {
                *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
                return NULL;
            }
        }
        Parse_printError("Expected \"}\"", token, type);
        return NULL;
    } else if (type == PARSE_LEFTBRACKET) {
        struct Parse_Node_s    *np;
        int             ch_next = '{';
        ungetc(ch_next, fp);
        np = Parse_objectid(fp, name);
        if (np != NULL) {
            *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
            return np;
        }
        return NULL;
    } else if (type == PARSE_LEFTSQBRACK) {
        int             size = 0;
        do {
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        } while (type != PARSE_ENDOFFILE && type != PARSE_RIGHTSQBRACK);
        if (type != PARSE_RIGHTSQBRACK) {
            Parse_printError("Expected \"]\"", token, type);
            return NULL;
        }
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        if (type == PARSE_IMPLICIT)
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
        if (*ntype == PARSE_LEFTPAREN) {
            switch (type) {
            case PARSE_OCTETSTR:
                *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
                if (*ntype != PARSE_SIZE) {
                    Parse_printError("Expected SIZE", ntoken, *ntype);
                    return NULL;
                }
                size = 1;
                *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
                if (*ntype != PARSE_LEFTPAREN) {
                    Parse_printError("Expected \"(\" after SIZE", ntoken,
                                *ntype);
                    return NULL;
                }
                /*
                 * fall through
                 */
            case PARSE_INTEGER:
                *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
                do {
                    if (*ntype != PARSE_NUMBER)
                        Parse_printError("Expected NUMBER", ntoken, *ntype);
                    *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
                    if (*ntype == PARSE_RANGE) {
                        *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
                        if (*ntype != PARSE_NUMBER)
                            Parse_printError("Expected NUMBER", ntoken, *ntype);
                        *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
                    }
                } while (*ntype == PARSE_BAR);
                if (*ntype != PARSE_RIGHTPAREN) {
                    Parse_printError("Expected \")\"", ntoken, *ntype);
                    return NULL;
                }
                *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
                if (size) {
                    if (*ntype != PARSE_RIGHTPAREN) {
                        Parse_printError("Expected \")\" to terminate SIZE",
                                    ntoken, *ntype);
                        return NULL;
                    }
                    *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
                }
            }
        }
        return NULL;
    } else {
        if (type == PARSE_CONVENTION) {
            while (type != PARSE_SYNTAX && type != PARSE_ENDOFFILE) {
                if (type == PARSE_DISPLAYHINT) {
                    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                    if (type != PARSE_QUOTESTRING)
                        Parse_printError("DISPLAY-HINT must be string", token,
                                    type);
                    else
                        hint = strdup(token);
                } else if (type == PARSE_DESCRIPTION &&
                           DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                                                  DsBool_SAVE_MIB_DESCRS)) {
                    type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
                    if (type != PARSE_QUOTESTRING)
                        Parse_printError("DESCRIPTION must be string", token,
                                    type);
                    else
                        descr = strdup(quoted_string_buffer);
                } else
                    type =
                        Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
            }
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type == PARSE_OBJECT) {
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                if (type != PARSE_IDENTIFIER) {
                    Parse_printError("Expected IDENTIFIER", token, type);
                    TOOLS_FREE(hint);
                    return NULL;
                }
                type = PARSE_OBJID;
            }
        } else if (type == PARSE_OBJECT) {
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type != PARSE_IDENTIFIER) {
                Parse_printError("Expected IDENTIFIER", token, type);
                return NULL;
            }
            type = PARSE_OBJID;
        }

        if (type == PARSE_LABEL) {
            type = Parse_getTc(token, parse_currentModule, NULL, NULL, NULL, NULL);
        }

        /*
         * textual convention
         */
        for (i = 0; i < PARSE_MAXTC; i++) {
            if (parse_tclist[i].type == 0)
                break;
        }

        if (i == PARSE_MAXTC) {
            Parse_printError("Too many textual conventions", token, type);
            TOOLS_FREE(hint);
            return NULL;
        }
        if (!(type & PARSE_SYNTAX_MASK)) {
            Parse_printError("Textual convention doesn't map to real type",
                        token, type);
            TOOLS_FREE(hint);
            return NULL;
        }
        tcp = &parse_tclist[i];
        tcp->modid = parse_currentModule;
        tcp->descriptor = strdup(name);
        tcp->hint = hint;
        tcp->description = descr;
        tcp->type = type;
        *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
        if (*ntype == PARSE_LEFTPAREN) {
            tcp->ranges = Parse_parseRanges(fp, &tcp->ranges);
            *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
        } else if (*ntype == PARSE_LEFTBRACKET) {
            /*
             * if there is an enumeration list, parse it
             */
            tcp->enums = Parse_parseEnumlist(fp, &tcp->enums);
            *ntype = Parse_getToken(fp, ntoken, PARSE_MAXTOKEN);
        }
        return NULL;
    }
}


/*
 * Parses an OBJECT TYPE macro.
 * Returns 0 on error.
 */
static struct Parse_Node_s *
Parse_parseObjecttype(FILE * fp, char *name)
{
    register int    type;
    char            token[PARSE_MAXTOKEN];
    char            nexttoken[PARSE_MAXTOKEN];
    char            quoted_string_buffer[PARSE_MAXQUOTESTR];
    int             nexttype, tctype;
    register struct Parse_Node_s *np;

    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_SYNTAX) {
        Parse_printError("Bad format for OBJECT-TYPE", token, type);
        return NULL;
    }
    np = Parse_allocNode(parse_currentModule);
    if (np == NULL)
        return (NULL);
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type == PARSE_OBJECT) {
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        if (type != PARSE_IDENTIFIER) {
            Parse_printError("Expected IDENTIFIER", token, type);
            Parse_freeNode(np);
            return NULL;
        }
        type = PARSE_OBJID;
    }
    if (type == PARSE_LABEL) {
        int             tmp_index;
        tctype = Parse_getTc(token, parse_currentModule, &tmp_index,
                        &np->enums, &np->ranges, &np->hint);
        if (tctype == PARSE_LABEL &&
            DefaultStore_getInt(DsStorage_LIBRARY_ID,
                   DsInt_MIB_WARNINGS) > 1) {
            Parse_printError("Warning: No known translation for type", token,
                        type);
        }
        type = tctype;
        np->tc_index = tmp_index;       /* store TC for later reference */
    }
    np->type = type;
    nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
    switch (type) {
    case PARSE_SEQUENCE:
        if (nexttype == PARSE_OF) {
            nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
            nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);

        }
        break;
    case PARSE_INTEGER:
    case PARSE_INTEGER32:
    case PARSE_UINTEGER32:
    case PARSE_UNSIGNED32:
    case PARSE_COUNTER:
    case PARSE_GAUGE:
    case PARSE_BITSTRING:
    case PARSE_LABEL:
        if (nexttype == PARSE_LEFTBRACKET) {
            /*
             * if there is an enumeration list, parse it
             */
            np->enums = Parse_parseEnumlist(fp, &np->enums);
            nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
        } else if (nexttype == PARSE_LEFTPAREN) {
            /*
             * if there is a range list, parse it
             */
            np->ranges = Parse_parseRanges(fp, &np->ranges);
            nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
        }
        break;
    case PARSE_OCTETSTR:
    case PARSE_KW_OPAQUE:
        /*
         * parse any SIZE specification
         */
        if (nexttype == PARSE_LEFTPAREN) {
            nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
            if (nexttype == PARSE_SIZE) {
                nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
                if (nexttype == PARSE_LEFTPAREN) {
                    np->ranges = Parse_parseRanges(fp, &np->ranges);
                    nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);      /* ) */
                    if (nexttype == PARSE_RIGHTPAREN) {
                        nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
                        break;
                    }
                }
            }
            Parse_printError("Bad SIZE syntax", token, type);
            Parse_freeNode(np);
            return NULL;
        }
        break;
    case PARSE_OBJID:
    case PARSE_NETADDR:
    case PARSE_IPADDR:
    case PARSE_TIMETICKS:
    case PARSE_NUL:
    case PARSE_NSAPADDRESS:
    case PARSE_COUNTER64:
        break;
    default:
        Parse_printError("Bad syntax", token, type);
        Parse_freeNode(np);
        return NULL;
    }
    if (nexttype == PARSE_UNITS) {
        type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
        if (type != PARSE_QUOTESTRING) {
            Parse_printError("Bad UNITS", quoted_string_buffer, type);
            Parse_freeNode(np);
            return NULL;
        }
        np->units = strdup(quoted_string_buffer);
        nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
    }
    if (nexttype != PARSE_ACCESS) {
        Parse_printError("Should be ACCESS", nexttoken, nexttype);
        Parse_freeNode(np);
        return NULL;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_READONLY && type != PARSE_READWRITE && type != PARSE_WRITEONLY
        && type != PARSE_NOACCESS && type != PARSE_READCREATE && type != PARSE_ACCNOTIFY) {
        Parse_printError("Bad ACCESS type", token, type);
        Parse_freeNode(np);
        return NULL;
    }
    np->access = type;
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_STATUS) {
        Parse_printError("Should be STATUS", token, type);
        Parse_freeNode(np);
        return NULL;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_MANDATORY && type != PARSE_CURRENT && type != PARSE_KW_OPTIONAL &&
        type != PARSE_OBSOLETE && type != PARSE_DEPRECATED) {
        Parse_printError("Bad STATUS", token, type);
        Parse_freeNode(np);
        return NULL;
    }
    np->status = type;
    /*
     * Optional parts of the OBJECT-TYPE macro
     */
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    while (type != PARSE_EQUALS && type != PARSE_ENDOFFILE) {
        switch (type) {
        case PARSE_DESCRIPTION:
            type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);

            if (type != PARSE_QUOTESTRING) {
                Parse_printError("Bad DESCRIPTION", quoted_string_buffer, type);
                Parse_freeNode(np);
                return NULL;
            }
            if (DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                       DsBool_SAVE_MIB_DESCRS)) {
                np->description = strdup(quoted_string_buffer);
            }
            break;

        case PARSE_REFERENCE:
            type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
            if (type != PARSE_QUOTESTRING) {
                Parse_printError("Bad REFERENCE", quoted_string_buffer, type);
                Parse_freeNode(np);
                return NULL;
            }
            np->reference = strdup(quoted_string_buffer);
            break;
        case PARSE_INDEX:
            if (np->augments) {
                Parse_printError("Cannot have both INDEX and AUGMENTS", token,
                            type);
                Parse_freeNode(np);
                return NULL;
            }
            np->indexes = Parse_getIndexes(fp, &np->indexes);
            if (np->indexes == NULL) {
                Parse_printError("Bad INDEX list", token, type);
                Parse_freeNode(np);
                return NULL;
            }
            break;
        case PARSE_AUGMENTS:
            if (np->indexes) {
                Parse_printError("Cannot have both INDEX and AUGMENTS", token,
                            type);
                Parse_freeNode(np);
                return NULL;
            }
            np->indexes = Parse_getIndexes(fp, &np->indexes);
            if (np->indexes == NULL) {
                Parse_printError("Bad AUGMENTS list", token, type);
                Parse_freeNode(np);
                return NULL;
            }
            np->augments = strdup(np->indexes->ilabel);
            Parse_freeIndexes(&np->indexes);
            break;
        case PARSE_DEFVAL:
            /*
             * Mark's defVal section
             */
            type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXTOKEN);
            if (type != PARSE_LEFTBRACKET) {
                Parse_printError("Bad DEFAULTVALUE", quoted_string_buffer,
                            type);
                Parse_freeNode(np);
                return NULL;
            }

            {
                int             level = 1;
                char            defbuf[512];

                defbuf[0] = 0;
                while (1) {
                    type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXTOKEN);
                    if ((type == PARSE_RIGHTBRACKET && --level == 0)
                        || type == PARSE_ENDOFFILE)
                        break;
                    else if (type == PARSE_LEFTBRACKET)
                        level++;
                    if (type == PARSE_QUOTESTRING)
                        Strlcat_strlcat(defbuf, "\\\"", sizeof(defbuf));
                    Strlcat_strlcat(defbuf, quoted_string_buffer, sizeof(defbuf));
                    if (type == PARSE_QUOTESTRING)
                        Strlcat_strlcat(defbuf, "\\\"", sizeof(defbuf));
                    Strlcat_strlcat(defbuf, " ", sizeof(defbuf));
                }

                if (type != PARSE_RIGHTBRACKET) {
                    Parse_printError("Bad DEFAULTVALUE", quoted_string_buffer,
                                type);
                    Parse_freeNode(np);
                    return NULL;
                }

                defbuf[strlen(defbuf) - 1] = 0;
                np->defaultValue = strdup(defbuf);
            }

            break;

        case PARSE_NUM_ENTRIES:
            if (Parse_tossObjectIdentifier(fp) != PARSE_OBJID) {
                Parse_printError("Bad Object Identifier", token, type);
                Parse_freeNode(np);
                return NULL;
            }
            break;

        default:
            Parse_printError("Bad format of optional clauses", token, type);
            Parse_freeNode(np);
            return NULL;

        }
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    }
    if (type != PARSE_EQUALS) {
        Parse_printError("Bad format", token, type);
        Parse_freeNode(np);
        return NULL;
    }
    return Parse_mergeParseObjectid(np, fp, name);
}

/*
 * Parses an OBJECT GROUP macro.
 * Returns 0 on error.
 *
 * Also parses object-identity, since they are similar (ignore STATUS).
 *   - WJH 10/96
 */
static struct Parse_Node_s * Parse_parseObjectgroup(FILE * fp, char *name, int what, struct Parse_Objgroup_s **ol)
{
    int             type;
    char            token[PARSE_MAXTOKEN];
    char            quoted_string_buffer[PARSE_MAXQUOTESTR];
    struct Parse_Node_s    *np;

    np = Parse_allocNode(parse_currentModule);
    if (np == NULL)
        return (NULL);
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type == what) {
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        if (type != PARSE_LEFTBRACKET) {
            Parse_printError("Expected \"{\"", token, type);
            goto skip;
        }
        do {
            struct Parse_Objgroup_s *o;
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type != PARSE_LABEL) {
                Parse_printError("Bad identifier", token, type);
                goto skip;
            }
            o = (struct Parse_Objgroup_s *) malloc(sizeof(struct Parse_Objgroup_s));
            if (!o) {
                Parse_printError("Resource failure", token, type);
                goto skip;
            }
            o->line = parse_mibLine;
            o->name = strdup(token);
            o->next = *ol;
            *ol = o;
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        } while (type == PARSE_COMMA);
        if (type != PARSE_RIGHTBRACKET) {
            Parse_printError("Expected \"}\" after list", token, type);
            goto skip;
        }
        type = Parse_getToken(fp, token, type);
    }
    if (type != PARSE_STATUS) {
        Parse_printError("Expected STATUS", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_CURRENT && type != PARSE_DEPRECATED && type != PARSE_OBSOLETE) {
        Parse_printError("Bad STATUS value", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_DESCRIPTION) {
        Parse_printError("Expected DESCRIPTION", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
    if (type != PARSE_QUOTESTRING) {
        Parse_printError("Bad DESCRIPTION", quoted_string_buffer, type);
        Parse_freeNode(np);
        return NULL;
    }
    if (DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                   DsBool_SAVE_MIB_DESCRS)) {
        np->description = strdup(quoted_string_buffer);
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type == PARSE_REFERENCE) {
        type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
        if (type != PARSE_QUOTESTRING) {
            Parse_printError("Bad REFERENCE", quoted_string_buffer, type);
            Parse_freeNode(np);
            return NULL;
        }
        np->reference = strdup(quoted_string_buffer);
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    }
    if (type != PARSE_EQUALS)
        Parse_printError("Expected \".=\"", token, type);
  skip:
    while (type != PARSE_EQUALS && type != PARSE_ENDOFFILE)
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);

    return Parse_mergeParseObjectid(np, fp, name);
}

/*
 * Parses a NOTIFICATION-TYPE macro.
 * Returns 0 on error.
 */
static struct Parse_Node_s *
Parse_parseNotificationDefinition(FILE * fp, char *name)
{
    register int    type;
    char            token[PARSE_MAXTOKEN];
    char            quoted_string_buffer[PARSE_MAXQUOTESTR];
    register struct Parse_Node_s *np;

    np = Parse_allocNode(parse_currentModule);
    if (np == NULL)
        return (NULL);
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    while (type != PARSE_EQUALS && type != PARSE_ENDOFFILE) {
        switch (type) {
        case PARSE_DESCRIPTION:
            type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
            if (type != PARSE_QUOTESTRING) {
                Parse_printError("Bad DESCRIPTION", quoted_string_buffer, type);
                Parse_freeNode(np);
                return NULL;
            }
            if (DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                       DsBool_SAVE_MIB_DESCRS)) {
                np->description = strdup(quoted_string_buffer);
            }
            break;
        case PARSE_REFERENCE:
            type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
            if (type != PARSE_QUOTESTRING) {
                Parse_printError("Bad REFERENCE", quoted_string_buffer, type);
                Parse_freeNode(np);
                return NULL;
            }
            np->reference = strdup(quoted_string_buffer);
            break;
        case PARSE_OBJECTS:
            np->varbinds = Parse_getVarbinds(fp, &np->varbinds);
            if (!np->varbinds) {
                Parse_printError("Bad OBJECTS list", token, type);
                Parse_freeNode(np);
                return NULL;
            }
            break;
        default:
            /*
             * NOTHING
             */
            break;
        }
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    }
    return Parse_mergeParseObjectid(np, fp, name);
}

/*
 * Parses a TRAP-TYPE macro.
 * Returns 0 on error.
 */
static struct Parse_Node_s *
Parse_parseTrapDefinition(FILE * fp, char *name)
{
    register int    type;
    char            token[PARSE_MAXTOKEN];
    char            quoted_string_buffer[PARSE_MAXQUOTESTR];
    register struct Parse_Node_s *np;

    np = Parse_allocNode(parse_currentModule);
    if (np == NULL)
        return (NULL);
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    while (type != PARSE_EQUALS && type != PARSE_ENDOFFILE) {
        switch (type) {
        case PARSE_DESCRIPTION:
            type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
            if (type != PARSE_QUOTESTRING) {
                Parse_printError("Bad DESCRIPTION", quoted_string_buffer, type);
                Parse_freeNode(np);
                return NULL;
            }
            if (DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                       DsBool_SAVE_MIB_DESCRS)) {
                np->description = strdup(quoted_string_buffer);
            }
            break;
        case PARSE_REFERENCE:
            /* I'm not sure REFERENCEs are legal in smiv1 traps??? */
            type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
            if (type != PARSE_QUOTESTRING) {
                Parse_printError("Bad REFERENCE", quoted_string_buffer, type);
                Parse_freeNode(np);
                return NULL;
            }
            np->reference = strdup(quoted_string_buffer);
            break;
        case PARSE_ENTERPRISE:
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type == PARSE_LEFTBRACKET) {
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                if (type != PARSE_LABEL) {
                    Parse_printError("Bad Trap Format", token, type);
                    Parse_freeNode(np);
                    return NULL;
                }
                np->parent = strdup(token);
                /*
                 * Get right bracket
                 */
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            } else if (type == PARSE_LABEL) {
                np->parent = strdup(token);
            } else {
                Parse_freeNode(np);
                return NULL;
            }
            break;
        case PARSE_VARIABLES:
            np->varbinds = Parse_getVarbinds(fp, &np->varbinds);
            if (!np->varbinds) {
                Parse_printError("Bad VARIABLES list", token, type);
                Parse_freeNode(np);
                return NULL;
            }
            break;
        default:
            /*
             * NOTHING
             */
            break;
        }
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);

    np->label = strdup(name);

    if (type != PARSE_NUMBER) {
        Parse_printError("Expected a Number", token, type);
        Parse_freeNode(np);
        return NULL;
    }
    np->subid = strtoul(token, NULL, 10);
    np->next = Parse_allocNode(parse_currentModule);
    if (np->next == NULL) {
        Parse_freeNode(np);
        return (NULL);
    }

    /* Catch the syntax error */
    if (np->parent == NULL) {
        Parse_freeNode(np->next);
        Parse_freeNode(np);
        parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
        return (NULL);
    }

    np->next->parent = np->parent;
    np->parent = (char *) malloc(strlen(np->parent) + 2);
    if (np->parent == NULL) {
        Parse_freeNode(np->next);
        Parse_freeNode(np);
        return (NULL);
    }
    strcpy(np->parent, np->next->parent);
    strcat(np->parent, "#");
    np->next->label = strdup(np->parent);
    return np;
}


/*
 * Parses a compliance macro
 * Returns 0 on error.
 */
static int
Parse_eatSyntax(FILE * fp, char *token, int maxtoken)
{
    int             type, nexttype;
    struct Parse_Node_s    *np = Parse_allocNode(parse_currentModule);
    char            nexttoken[PARSE_MAXTOKEN];

    if (!np)
    return 0;

    type = Parse_getToken(fp, token, maxtoken);
    nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
    switch (type) {
    case PARSE_INTEGER:
    case PARSE_INTEGER32:
    case PARSE_UINTEGER32:
    case PARSE_UNSIGNED32:
    case PARSE_COUNTER:
    case PARSE_GAUGE:
    case PARSE_BITSTRING:
    case PARSE_LABEL:
        if (nexttype == PARSE_LEFTBRACKET) {
            /*
             * if there is an enumeration list, parse it
             */
            np->enums = Parse_parseEnumlist(fp, &np->enums);
            nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
        } else if (nexttype == PARSE_LEFTPAREN) {
            /*
             * if there is a range list, parse it
             */
            np->ranges = Parse_parseRanges(fp, &np->ranges);
            nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
        }
        break;
    case PARSE_OCTETSTR:
    case PARSE_KW_OPAQUE:
        /*
         * parse any SIZE specification
         */
        if (nexttype == PARSE_LEFTPAREN) {
            nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
            if (nexttype == PARSE_SIZE) {
                nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
                if (nexttype == PARSE_LEFTPAREN) {
                    np->ranges = Parse_parseRanges(fp, &np->ranges);
                    nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);      /* ) */
                    if (nexttype == PARSE_RIGHTPAREN) {
                        nexttype = Parse_getToken(fp, nexttoken, PARSE_MAXTOKEN);
                        break;
                    }
                }
            }
            Parse_printError("Bad SIZE syntax", token, type);
            Parse_freeNode(np);
            return nexttype;
        }
        break;
    case PARSE_OBJID:
    case PARSE_NETADDR:
    case PARSE_IPADDR:
    case PARSE_TIMETICKS:
    case PARSE_NUL:
    case PARSE_NSAPADDRESS:
    case PARSE_COUNTER64:
        break;
    default:
        Parse_printError("Bad syntax", token, type);
        Parse_freeNode(np);
        return nexttype;
    }
    Parse_freeNode(np);
    return nexttype;
}

static int
Parse_complianceLookup(const char *name, int modid)
{
    if (modid == -1) {
        struct Parse_Objgroup_s *op =
            (struct Parse_Objgroup_s *) malloc(sizeof(struct Parse_Objgroup_s));
        if (!op)
            return 0;
        op->next = parse_objgroups;
        op->name = strdup(name);
        op->line = parse_mibLine;
        parse_objgroups = op;
        return 1;
    } else
        return Parse_findTreeNode(name, modid) != NULL;
}

static struct Parse_Node_s *
Parse_parseCompliance(FILE * fp, char *name)
{
    int             type;
    char            token[PARSE_MAXTOKEN];
    char            quoted_string_buffer[PARSE_MAXQUOTESTR];
    struct Parse_Node_s    *np;

    np = Parse_allocNode(parse_currentModule);
    if (np == NULL)
        return (NULL);
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_STATUS) {
        Parse_printError("Expected STATUS", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_CURRENT && type != PARSE_DEPRECATED && type != PARSE_OBSOLETE) {
        Parse_printError("Bad STATUS", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_DESCRIPTION) {
        Parse_printError("Expected DESCRIPTION", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
    if (type != PARSE_QUOTESTRING) {
        Parse_printError("Bad DESCRIPTION", quoted_string_buffer, type);
        goto skip;
    }
    if (DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                   DsBool_SAVE_MIB_DESCRS))
        np->description = strdup(quoted_string_buffer);
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type == PARSE_REFERENCE) {
        type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXTOKEN);
        if (type != PARSE_QUOTESTRING) {
            Parse_printError("Bad REFERENCE", quoted_string_buffer, type);
            goto skip;
        }
        np->reference = strdup(quoted_string_buffer);
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    }
    if (type != PARSE_MODULE) {
        Parse_printError("Expected MODULE", token, type);
        goto skip;
    }
    while (type == PARSE_MODULE) {
        int             modid = -1;
        char            modname[PARSE_MAXTOKEN];
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        if (type == PARSE_LABEL
            && strcmp(token, Parse_moduleName(parse_currentModule, modname))) {
            modid = Parse_readModuleInternal(token);
            if (modid != PARSE_MODULE_LOADED_OK
                && modid != PARSE_MODULE_ALREADY_LOADED) {
                Parse_printError("Unknown module", token, type);
                goto skip;
            }
            modid = Parse_whichModule(token);
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        }
        if (type == PARSE_MANDATORYGROUPS) {
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type != PARSE_LEFTBRACKET) {
                Parse_printError("Expected \"{\"", token, type);
                goto skip;
            }
            do {
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                if (type != PARSE_LABEL) {
                    Parse_printError("Bad group name", token, type);
                    goto skip;
                }
                if (!Parse_complianceLookup(token, modid))
                    Parse_printError("Unknown group", token, type);
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            } while (type == PARSE_COMMA);
            if (type != PARSE_RIGHTBRACKET) {
                Parse_printError("Expected \"}\"", token, type);
                goto skip;
            }
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        }
        while (type == PARSE_GROUP || type == PARSE_OBJECT) {
            if (type == PARSE_GROUP) {
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                if (type != PARSE_LABEL) {
                    Parse_printError("Bad group name", token, type);
                    goto skip;
                }
                if (!Parse_complianceLookup(token, modid))
                    Parse_printError("Unknown group", token, type);
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            } else {
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                if (type != PARSE_LABEL) {
                    Parse_printError("Bad object name", token, type);
                    goto skip;
                }
                if (!Parse_complianceLookup(token, modid))
                    Parse_printError("Unknown group", token, type);
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                if (type == PARSE_SYNTAX)
                    type = Parse_eatSyntax(fp, token, PARSE_MAXTOKEN);
                if (type == PARSE_WRSYNTAX)
                    type = Parse_eatSyntax(fp, token, PARSE_MAXTOKEN);
                if (type == PARSE_MINACCESS) {
                    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                    if (type != PARSE_NOACCESS && type != PARSE_ACCNOTIFY
                        && type != PARSE_READONLY && type != PARSE_WRITEONLY
                        && type != PARSE_READCREATE && type != PARSE_READWRITE) {
                        Parse_printError("Bad MIN-ACCESS spec", token, type);
                        goto skip;
                    }
                    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                }
            }
            if (type != PARSE_DESCRIPTION) {
                Parse_printError("Expected DESCRIPTION", token, type);
                goto skip;
            }
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type != PARSE_QUOTESTRING) {
                Parse_printError("Bad DESCRIPTION", token, type);
                goto skip;
            }
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        }
    }
  skip:
    while (type != PARSE_EQUALS && type != PARSE_ENDOFFILE)
        type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);

    return Parse_mergeParseObjectid(np, fp, name);
}


/*
 * Parses a capabilities macro
 * Returns 0 on error.
 */
static struct Parse_Node_s *
Parse_parseCapabilities(FILE * fp, char *name)
{
    int             type;
    char            token[PARSE_MAXTOKEN];
    char            quoted_string_buffer[PARSE_MAXQUOTESTR];
    struct Parse_Node_s    *np;

    np = Parse_allocNode(parse_currentModule);
    if (np == NULL)
        return (NULL);
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_PRODREL) {
        Parse_printError("Expected PRODUCT-RELEASE", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_QUOTESTRING) {
        Parse_printError("Expected STRING after PRODUCT-RELEASE", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_STATUS) {
        Parse_printError("Expected STATUS", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_CURRENT && type != PARSE_OBSOLETE) {
        Parse_printError("STATUS should be current or obsolete", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_DESCRIPTION) {
        Parse_printError("Expected DESCRIPTION", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXTOKEN);
    if (type != PARSE_QUOTESTRING) {
        Parse_printError("Bad DESCRIPTION", quoted_string_buffer, type);
        goto skip;
    }
    if (DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                   DsBool_SAVE_MIB_DESCRS)) {
        np->description = strdup(quoted_string_buffer);
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type == PARSE_REFERENCE) {
        type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXTOKEN);
        if (type != PARSE_QUOTESTRING) {
            Parse_printError("Bad REFERENCE", quoted_string_buffer, type);
            goto skip;
        }
        np->reference = strdup(quoted_string_buffer);
        type = Parse_getToken(fp, token, type);
    }
    while (type == PARSE_SUPPORTS) {
        int             modid;
        struct Parse_Tree_s    *tp;

        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        if (type != PARSE_LABEL) {
            Parse_printError("Bad module name", token, type);
            goto skip;
        }
        modid = Parse_readModuleInternal(token);
        if (modid != PARSE_MODULE_LOADED_OK && modid != PARSE_MODULE_ALREADY_LOADED) {
            Parse_printError("Module not found", token, type);
            goto skip;
        }
        modid = Parse_whichModule(token);
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        if (type != PARSE_INCLUDES) {
            Parse_printError("Expected INCLUDES", token, type);
            goto skip;
        }
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        if (type != PARSE_LEFTBRACKET) {
            Parse_printError("Expected \"{\"", token, type);
            goto skip;
        }
        do {
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type != PARSE_LABEL) {
                Parse_printError("Expected group name", token, type);
                goto skip;
            }
            tp = Parse_findTreeNode(token, modid);
            if (!tp)
                Parse_printError("Group not found in module", token, type);
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        } while (type == PARSE_COMMA);
        if (type != PARSE_RIGHTBRACKET) {
            Parse_printError("Expected \"}\" after group list", token, type);
            goto skip;
        }
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        while (type == PARSE_VARIATION) {
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type != PARSE_LABEL) {
                Parse_printError("Bad object name", token, type);
                goto skip;
            }
            tp = Parse_findTreeNode(token, modid);
            if (!tp)
                Parse_printError("Object not found in module", token, type);
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type == PARSE_SYNTAX) {
                type = Parse_eatSyntax(fp, token, PARSE_MAXTOKEN);
            }
            if (type == PARSE_WRSYNTAX) {
                type = Parse_eatSyntax(fp, token, PARSE_MAXTOKEN);
            }
            if (type == PARSE_ACCESS) {
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                if (type != PARSE_ACCNOTIFY && type != PARSE_READONLY
                    && type != PARSE_READWRITE && type != PARSE_READCREATE
                    && type != PARSE_WRITEONLY && type != PARSE_NOTIMPL) {
                    Parse_printError("Bad ACCESS", token, type);
                    goto skip;
                }
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            }
            if (type == PARSE_CREATEREQ) {
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                if (type != PARSE_LEFTBRACKET) {
                    Parse_printError("Expected \"{\"", token, type);
                    goto skip;
                }
                do {
                    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                    if (type != PARSE_LABEL) {
                        Parse_printError("Bad object name in list", token,
                                    type);
                        goto skip;
                    }
                    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                } while (type == PARSE_COMMA);
                if (type != PARSE_RIGHTBRACKET) {
                    Parse_printError("Expected \"}\" after list", token, type);
                    goto skip;
                }
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            }
            if (type == PARSE_DEFVAL) {
                int             level = 1;
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                if (type != PARSE_LEFTBRACKET) {
                    Parse_printError("Expected \"{\" after DEFVAL", token,
                                type);
                    goto skip;
                }
                do {
                    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                    if (type == PARSE_LEFTBRACKET)
                        level++;
                    else if (type == PARSE_RIGHTBRACKET)
                        level--;
                } while ((type != PARSE_RIGHTBRACKET || level != 0)
                         && type != PARSE_ENDOFFILE);
                if (type != PARSE_RIGHTBRACKET) {
                    Parse_printError("Missing \"}\" after DEFVAL", token, type);
                    goto skip;
                }
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            }
            if (type != PARSE_DESCRIPTION) {
                Parse_printError("Expected DESCRIPTION", token, type);
                goto skip;
            }
            type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXTOKEN);
            if (type != PARSE_QUOTESTRING) {
                Parse_printError("Bad DESCRIPTION", quoted_string_buffer, type);
                goto skip;
            }
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        }
    }
    if (type != PARSE_EQUALS)
        Parse_printError("Expected \".=\"", token, type);
  skip:
    while (type != PARSE_EQUALS && type != PARSE_ENDOFFILE) {
        type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
    }
    return Parse_mergeParseObjectid(np, fp, name);
}

/*
 * Parses a module identity macro
 * Returns 0 on error.
 */
static void
Parse_checkUtc(const char *utc)
{
    int             len, year, month, day, hour, minute;

    len = strlen(utc);
    if (utc[len - 1] != 'Z' && utc[len - 1] != 'z') {
        Parse_printError("Timestamp should end with Z", utc, PARSE_QUOTESTRING);
        return;
    }
    if (len == 11) {
        len =
            sscanf(utc, "%2d%2d%2d%2d%2dZ", &year, &month, &day, &hour,
                   &minute);
        year += 1900;
    } else if (len == 13)
        len =
            sscanf(utc, "%4d%2d%2d%2d%2dZ", &year, &month, &day, &hour,
                   &minute);
    else {
        Parse_printError("Bad timestamp format (11 or 13 characters)",
                    utc, PARSE_QUOTESTRING);
        return;
    }
    if (len != 5) {
        Parse_printError("Bad timestamp format", utc, PARSE_QUOTESTRING);
        return;
    }
    if (month < 1 || month > 12)
        Parse_printError("Bad month in timestamp", utc, PARSE_QUOTESTRING);
    if (day < 1 || day > 31)
        Parse_printError("Bad day in timestamp", utc, PARSE_QUOTESTRING);
    if (hour < 0 || hour > 23)
        Parse_printError("Bad hour in timestamp", utc, PARSE_QUOTESTRING);
    if (minute < 0 || minute > 59)
        Parse_printError("Bad minute in timestamp", utc, PARSE_QUOTESTRING);
}

static struct Parse_Node_s *
Parse_parseModuleIdentity(FILE * fp, char *name)
{
    register int    type;
    char            token[PARSE_MAXTOKEN];
    char            quoted_string_buffer[PARSE_MAXQUOTESTR];
    register struct Parse_Node_s *np;

    np = Parse_allocNode(parse_currentModule);
    if (np == NULL)
        return (NULL);
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_LASTUPDATED) {
        Parse_printError("Expected LAST-UPDATED", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_QUOTESTRING) {
        Parse_printError("Need STRING for LAST-UPDATED", token, type);
        goto skip;
    }
    Parse_checkUtc(token);
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_ORGANIZATION) {
        Parse_printError("Expected ORGANIZATION", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_QUOTESTRING) {
        Parse_printError("Bad ORGANIZATION", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_CONTACTINFO) {
        Parse_printError("Expected CONTACT-INFO", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
    if (type != PARSE_QUOTESTRING) {
        Parse_printError("Bad CONTACT-INFO", quoted_string_buffer, type);
        goto skip;
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    if (type != PARSE_DESCRIPTION) {
        Parse_printError("Expected DESCRIPTION", token, type);
        goto skip;
    }
    type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
    if (type != PARSE_QUOTESTRING) {
        Parse_printError("Bad DESCRIPTION", quoted_string_buffer, type);
        goto skip;
    }
    if (DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                   DsBool_SAVE_MIB_DESCRS)) {
        np->description = strdup(quoted_string_buffer);
    }
    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    while (type == PARSE_REVISION) {
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        if (type != PARSE_QUOTESTRING) {
            Parse_printError("Bad REVISION", token, type);
            goto skip;
        }
        Parse_checkUtc(token);
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        if (type != PARSE_DESCRIPTION) {
            Parse_printError("Expected DESCRIPTION", token, type);
            goto skip;
        }
        type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
        if (type != PARSE_QUOTESTRING) {
            Parse_printError("Bad DESCRIPTION", quoted_string_buffer, type);
            goto skip;
        }
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    }
    if (type != PARSE_EQUALS)
        Parse_printError("Expected \".=\"", token, type);
  skip:
    while (type != PARSE_EQUALS && type != PARSE_ENDOFFILE) {
        type = Parse_getToken(fp, quoted_string_buffer, PARSE_MAXQUOTESTR);
    }
    return Parse_mergeParseObjectid(np, fp, name);
}


/*
 * Parses a MACRO definition
 * Expect BEGIN, discard everything to end.
 * Returns 0 on error.
 */
static struct Parse_Node_s *
Parse_parseMacro(FILE * fp, char *name)
{
    register int    type;
    char            token[PARSE_MAXTOKEN];
    struct Parse_Node_s    *np;
    int             iLine = parse_mibLine;

    np = Parse_allocNode(parse_currentModule);
    if (np == NULL)
        return (NULL);
    type = Parse_getToken(fp, token, sizeof(token));
    while (type != PARSE_EQUALS && type != PARSE_ENDOFFILE) {
        type = Parse_getToken(fp, token, sizeof(token));
    }
    if (type != PARSE_EQUALS) {
        if (np)
            Parse_freeNode(np);
        return NULL;
    }
    while (type != PARSE_BEGIN && type != PARSE_ENDOFFILE) {
        type = Parse_getToken(fp, token, sizeof(token));
    }
    if (type != PARSE_BEGIN) {
        if (np)
            Parse_freeNode(np);
        return NULL;
    }
    while (type != PARSE_END && type != PARSE_ENDOFFILE) {
        type = Parse_getToken(fp, token, sizeof(token));
    }
    if (type != PARSE_END) {
        if (np)
            Parse_freeNode(np);
        return NULL;
    }

    if (DefaultStore_getInt(DsStorage_LIBRARY_ID,
               DsInt_MIB_WARNINGS)) {
        Logger_log(LOGGER_PRIORITY_WARNING,
                 "%s MACRO (lines %d..%d parsed and ignored).\n", name,
                 iLine, parse_mibLine);
    }

    return np;
}

/*
 * Parses a module import clause
 *   loading any modules referenced
 */
static void
Parse_parseImports(FILE * fp)
{
    register int    type;
    char            token[PARSE_MAXTOKEN];
    char            modbuf[256];
#define MAX_IMPORTS	256
    struct Parse_ModuleImport_s import_list[MAX_IMPORTS];
    int             this_module;
    struct Parse_Module_s  *mp;

    int             import_count = 0;   /* Total number of imported descriptors */
    int             i = 0, old_i;       /* index of first import from each module */

    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);

    /*
     * Parse the IMPORTS clause
     */
    while (type != PARSE_SEMI && type != PARSE_ENDOFFILE) {
        if (type == PARSE_LABEL) {
            if (import_count == MAX_IMPORTS) {
                Parse_printError("Too many imported symbols", token, type);
                do {
                    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                } while (type != PARSE_SEMI && type != PARSE_ENDOFFILE);
                return;
            }
            import_list[import_count++].label = strdup(token);
        } else if (type == PARSE_FROM) {
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (import_count == i) {    /* All imports are handled internally */
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
                continue;
            }
            this_module = Parse_whichModule(token);

            for (old_i = i; i < import_count; ++i)
                import_list[i].modid = this_module;

            /*
             * Recursively read any pre-requisite modules
             */
            if (Parse_readModuleInternal(token) == PARSE_MODULE_NOT_FOUND) {
        int found = 0;
                for (; old_i < import_count; ++old_i) {
                    found += Parse_readImportReplacements(token, &import_list[old_i]);
                }
        if (!found)
            Parse_printModuleNotFound(token);
            }
        }
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    }

    /*
     * Save the import information
     *   in the global module table
     */
    for (mp = parse_moduleHead; mp; mp = mp->next)
        if (mp->modid == parse_currentModule) {
            if (import_count == 0)
                return;
            if (mp->imports && (mp->imports != parse_rootImports)) {
                /*
                 * this can happen if all modules are in one source file.
                 */
                for (i = 0; i < mp->no_imports; ++i) {
                    DEBUG_MSGTL(("parse-mibs",
                                "#### freeing Module %d '%s' %d\n",
                                mp->modid, mp->imports[i].label,
                                mp->imports[i].modid));
                    free((char *) mp->imports[i].label);
                }
                free((char *) mp->imports);
            }
            mp->imports = (struct Parse_ModuleImport_s *)
                calloc(import_count, sizeof(struct Parse_ModuleImport_s));
            if (mp->imports == NULL)
                return;
            for (i = 0; i < import_count; ++i) {
                mp->imports[i].label = import_list[i].label;
                mp->imports[i].modid = import_list[i].modid;
                DEBUG_MSGTL(("parse-mibs",
                            "#### adding Module %d '%s' %d\n", mp->modid,
                            mp->imports[i].label, mp->imports[i].modid));
            }
            mp->no_imports = import_count;
            return;
        }

    /*
     * Shouldn't get this far
     */
    Parse_printModuleNotFound(Parse_moduleName(parse_currentModule, modbuf));
    return;
}



/*
 * MIB module handling routines
 */

static void
Parse_dumpModuleList(void)
{
    struct Parse_Module_s  *mp = parse_moduleHead;

    DEBUG_MSGTL(("parse-mibs", "Module list:\n"));
    while (mp) {
        DEBUG_MSGTL(("parse-mibs", "  %s %d %s %d\n", mp->name, mp->modid,
                    mp->file, mp->no_imports));
        mp = mp->next;
    }
}

int
Parse_whichModule(const char *name)
{
    struct Parse_Module_s  *mp;

    for (mp = parse_moduleHead; mp; mp = mp->next)
        if (!PARSE_LABEL_COMPARE(mp->name, name))
            return (mp->modid);

    DEBUG_MSGTL(("parse-mibs", "Module %s not found\n", name));
    return (-1);
}

/*
 * module_name - copy module name to user buffer, return ptr to same.
 */
char           *
Parse_moduleName(int modid, char *cp)
{
    struct Parse_Module_s  *mp;

    for (mp = parse_moduleHead; mp; mp = mp->next)
        if (mp->modid == modid) {
            strcpy(cp, mp->name);
            return (cp);
        }

    if (modid != -1) DEBUG_MSGTL(("parse-mibs", "Module %d not found\n", modid));
    sprintf(cp, "#%d", modid);
    return (cp);
}

/*
 *  Backwards compatability
 *  Read newer modules that replace the one specified:-
 *      either all of them (read_module_replacements),
 *      or those relating to a specified identifier (read_import_replacements)
 *      plus an interface to add new replacement requirements
 */

void Parse_addModuleReplacement(const char *old_module,
                       const char *new_module_name,
                       const char *tag, int len)
{
    struct Parse_ModuleCompatability_s *mcp;

    mcp = (struct Parse_ModuleCompatability_s *)
        calloc(1, sizeof(struct Parse_ModuleCompatability_s));
    if (mcp == NULL)
        return;

    mcp->old_module = strdup(old_module);
    mcp->new_module = strdup(new_module_name);
    if (tag)
        mcp->tag = strdup(tag);
    mcp->tag_len = len;

    mcp->next = parse_moduleMapHead;
    parse_moduleMapHead = mcp;
}

static int
Parse_readModuleReplacements(const char *name)
{
    struct Parse_ModuleCompatability_s *mcp;

    for (mcp = parse_moduleMapHead; mcp; mcp = mcp->next) {
        if (!PARSE_LABEL_COMPARE(mcp->old_module, name)) {
            if (DefaultStore_getInt(DsStorage_LIBRARY_ID,
                   DsInt_MIB_WARNINGS)) {
                Logger_log(LOGGER_PRIORITY_WARNING,
                         "Loading replacement module %s for %s (%s)\n",
                         mcp->new_module, name, parse_file);
        }
            (void) Parse_readModule(mcp->new_module);
            return 1;
        }
    }
    return 0;
}

static int
Parse_readImportReplacements(const char *old_module_name,
                         struct Parse_ModuleImport_s *identifier)
{
    struct Parse_ModuleCompatability_s *mcp;

    /*
     * Look for matches first
     */
    for (mcp = parse_moduleMapHead; mcp; mcp = mcp->next) {
        if (!PARSE_LABEL_COMPARE(mcp->old_module, old_module_name)) {

            if (                /* exact match */
                   (mcp->tag_len == 0 &&
                    (mcp->tag == NULL ||
                     !PARSE_LABEL_COMPARE(mcp->tag, identifier->label))) ||
                   /*
                    * prefix match
                    */
                   (mcp->tag_len != 0 &&
                    !strncmp(mcp->tag, identifier->label, mcp->tag_len))
                ) {

                if (DefaultStore_getInt(DsStorage_LIBRARY_ID,
                       DsInt_MIB_WARNINGS)) {
                    Logger_log(LOGGER_PRIORITY_WARNING,
                             "Importing %s from replacement module %s instead of %s (%s)\n",
                             identifier->label, mcp->new_module,
                             old_module_name, parse_file);
        }
                (void) Parse_readModule(mcp->new_module);
                identifier->modid = Parse_whichModule(mcp->new_module);
                return 1;         /* finished! */
            }
        }
    }

    /*
     * If no exact match, load everything relevant
     */
    return Parse_readModuleReplacements(old_module_name);
}


/*
 *  Read in the named module
 *      Returns the root of the whole tree
 *      (by analogy with 'read_mib')
 */
static int
Parse_readModuleInternal(const char *name)
{
    struct Parse_Module_s  *mp;
    FILE           *fp;
    struct Parse_Node_s    *np;

    Parse_initMibInternals();

    for (mp = parse_moduleHead; mp; mp = mp->next)
        if (!PARSE_LABEL_COMPARE(mp->name, name)) {
            const char     *oldFile = parse_file;
            int             oldLine = parse_mibLine;
            int             oldModule = parse_currentModule;

            if (mp->no_imports != -1) {
                DEBUG_MSGTL(("parse-mibs", "Module %s already loaded\n",
                            name));
                return PARSE_MODULE_ALREADY_LOADED;
            }
            if ((fp = fopen(mp->file, "r")) == NULL) {
                int rval;
                if (errno == ENOTDIR || errno == ENOENT)
                    rval = PARSE_MODULE_NOT_FOUND;
                else
                    rval = PARSE_MODULE_LOAD_FAILED;
                Logger_logPerror(mp->file);
                return rval;
            }
            flockfile(fp);

            mp->no_imports = 0; /* Note that we've read the file */
            parse_file = mp->file;
            parse_mibLine = 1;
            parse_currentModule = mp->modid;
            /*
             * Parse the file
             */
            np = Parse_parse(fp, NULL);
            funlockfile(fp);
            fclose(fp);
            parse_file = oldFile;
            parse_mibLine = oldLine;
            parse_currentModule = oldModule;
            if ((np == NULL) && (parse_gMibError == PARSE_MODULE_SYNTAX_ERROR) )
                return PARSE_MODULE_SYNTAX_ERROR;
            return PARSE_MODULE_LOADED_OK;
        }

    return PARSE_MODULE_NOT_FOUND;
}

void
Parse_adoptOrphans(void)
{
    struct Parse_Node_s    *np, *onp;
    struct Parse_Tree_s    *tp;
    int             i, adopted = 1;

    if (!parse_orphanNodes)
        return;
    Parse_initNodeHash(parse_orphanNodes);
    parse_orphanNodes = NULL;

    while (adopted) {
        adopted = 0;
        for (i = 0; i < PARSE_NHASHSIZE; i++)
            if (parse_nbuckets[i]) {
                for (np = parse_nbuckets[i]; np != NULL; np = np->next) {
                    tp = Parse_findTreeNode(np->parent, -1);
            if (tp) {
            Parse_doSubtree(tp, &np);
            adopted = 1;
                        /*
                         * if do_subtree adopted the entire bucket, stop
                         */
                        if(NULL == parse_nbuckets[i])
                            break;

                        /*
                         * do_subtree may modify nbuckets, and if np
                         * was adopted, np->next probably isn't an orphan
                         * anymore. if np is still in the bucket (do_subtree
                         * didn't adopt it) keep on plugging. otherwise
                         * start over, at the top of the bucket.
                         */
                        for(onp = parse_nbuckets[i]; onp; onp = onp->next)
                            if(onp == np)
                                break;
                        if(NULL == onp) { /* not in the list */
                            np = parse_nbuckets[i]; /* start over */
                        }
            }
        }
            }
    }

    /*
     * Report on outstanding orphans
     *    and link them back into the orphan list
     */
    for (i = 0; i < PARSE_NHASHSIZE; i++)
        if (parse_nbuckets[i]) {
            if (parse_orphanNodes)
                onp = np->next = parse_nbuckets[i];
            else
                onp = parse_orphanNodes = parse_nbuckets[i];
            parse_nbuckets[i] = NULL;
            while (onp) {
                char            modbuf[256];
                Logger_log(LOGGER_PRIORITY_WARNING,
                         "Cannot adopt OID in %s: %s .= { %s %ld }\n",
                         Parse_moduleName(onp->modid, modbuf),
                         (onp->label ? onp->label : "<no label>"),
                         (onp->parent ? onp->parent : "<no parent>"),
                         onp->subid);

                np = onp;
                onp = onp->next;
            }
        }
}


struct Parse_Tree_s    *
Parse_readModule(const char *name)
{
    int status = 0;
    status = Parse_readModuleInternal(name);

    if (status == PARSE_MODULE_NOT_FOUND) {
        if (!Parse_readModuleReplacements(name))
            Parse_printModuleNotFound(name);
    } else if (status == PARSE_MODULE_SYNTAX_ERROR) {
        parse_gMibError = 0;
        parse_gLoop = 1;

        strncat(parse_gMibNames, " ", sizeof(parse_gMibNames) - strlen(parse_gMibNames) - 1);
        strncat(parse_gMibNames, name, sizeof(parse_gMibNames) - strlen(parse_gMibNames) - 1);
    }

    return parse_treeHead;
}

/*
 * Prototype definition
 */

void
Parse_unloadModuleByID(int modID, struct Parse_Tree_s *tree_top)
{
    struct Parse_Tree_s    *tp, *next;
    int             i;

    for (tp = tree_top; tp; tp = next) {
        /*
         * Essentially, this is equivalent to the code fragment:
         *      if (tp->modID == modID)
         *        tp->number_modules--;
         * but handles one tree node being part of several modules,
         * and possible multiple copies of the same module ID.
         */
        int             nmod = tp->number_modules;
        if (nmod > 0) {         /* in some module */
            /*
             * Remove all copies of this module ID
             */
            int             cnt = 0, *pi1, *pi2 = tp->module_list;
            for (i = 0, pi1 = pi2; i < nmod; i++, pi2++) {
                if (*pi2 == modID)
                    continue;
                cnt++;
                *pi1++ = *pi2;
            }
            if (nmod != cnt) {  /* in this module */
                /*
                 * if ( (nmod - cnt) > 1)
                 * printf("Dup modid %d,  %d times, '%s'\n", tp->modid, (nmod-cnt), tp->label); fflush(stdout); ?* XXDEBUG
                 */
                tp->number_modules = cnt;
                switch (cnt) {
                case 0:
                    tp->module_list[0] = -1;    /* Mark unused, and FALL THROUGH */

                case 1:        /* save the remaining module */
                    if (&(tp->modid) != tp->module_list) {
                        tp->modid = tp->module_list[0];
                        free(tp->module_list);
                        tp->module_list = &(tp->modid);
                    }
                    break;

                default:
                    break;
                }
            }                   /* if tree node is in this module */
        }
        /*
         * if tree node is in some module
         */
        next = tp->next_peer;


        /*
         *  OK - that's dealt with *this* node.
         *    Now let's look at the children.
         *    (Isn't recursion wonderful!)
         */
        if (tp->child_list)
            Parse_unloadModuleByID(modID, tp->child_list);


        if (tp->number_modules == 0) {
            /*
             * This node isn't needed any more (except perhaps
             * for the sake of the children)
             */
            if (tp->child_list == NULL) {
                Parse_unlinkTree(tp);
                Parse_freeTree(tp);
            } else {
                Parse_freePartialTree(tp, TRUE);
            }
        }
    }
}

int Parse_unloadModule(const char *name)
{
    return Parse_unloadModule(name);
}

int Parse_netsnmp_unload_module(const char *name)
{
    struct Parse_Module_s  *mp;
    int             modID = -1;

    for (mp = parse_moduleHead; mp; mp = mp->next)
        if (!PARSE_LABEL_COMPARE(mp->name, name)) {
            modID = mp->modid;
            break;
        }

    if (modID == -1) {
        DEBUG_MSGTL(("unload-mib", "Module %s not found to unload\n",
                    name));
        return PARSE_MODULE_NOT_FOUND;
    }
    Parse_unloadModuleByID(modID, parse_treeHead);
    mp->no_imports = -1;        /* mark as unloaded */
    return PARSE_MODULE_LOADED_OK;    /* Well, you know what I mean! */
}

/*
 * Clear module map, tree nodes, textual convention table.
 */
void Parse_unloadAllMibs(void)
{
    struct Parse_Module_s  *mp;
    struct Parse_ModuleCompatability_s *mcp;
    struct Parse_Tc_s      *ptc;
    unsigned int    i;

    for (mcp = parse_moduleMapHead; mcp; mcp = parse_moduleMapHead) {
        if (mcp == parse_moduleMap)
            break;
        parse_moduleMapHead = mcp->next;
        if (mcp->tag) free(TOOLS_REMOVE_CONST(char *, mcp->tag));
        free(TOOLS_REMOVE_CONST(char *, mcp->old_module));
        free(TOOLS_REMOVE_CONST(char *, mcp->new_module));
        free(mcp);
    }

    for (mp = parse_moduleHead; mp; mp = parse_moduleHead) {
        struct Parse_ModuleImport_s *mi = mp->imports;
        if (mi) {
            for (i = 0; i < (unsigned int)mp->no_imports; ++i) {
                TOOLS_FREE((mi + i)->label);
            }
            mp->no_imports = 0;
            if (mi == parse_rootImports)
                memset(mi, 0, sizeof(*mi));
            else
                free(mi);
        }

        Parse_unloadModuleByID(mp->modid, parse_treeHead);
        parse_moduleHead = mp->next;
        free(mp->name);
        free(mp->file);
        free(mp);
    }
    Parse_unloadModuleByID(-1, parse_treeHead);
    /*
     * tree nodes are cleared
     */

    for (i = 0, ptc = parse_tclist; i < PARSE_MAXTC; i++, ptc++) {
        if (ptc->type == 0)
            continue;
        Parse_freeEnums(&ptc->enums);
        Parse_freeRanges(&ptc->ranges);
        free(ptc->descriptor);
        if (ptc->hint)
            free(ptc->hint);
        if (ptc->description)
            free(ptc->description);
    }
    memset(parse_tclist, 0, PARSE_MAXTC * sizeof(struct Parse_Tc_s));

    memset(parse_buckets, 0, sizeof(parse_buckets));
    memset(parse_nbuckets, 0, sizeof(parse_nbuckets));
    memset(parse_tbuckets, 0, sizeof(parse_tbuckets));

    for (i = 0; i < sizeof(parse_rootImports) / sizeof(parse_rootImports[0]); i++) {
        TOOLS_FREE(parse_rootImports[i].label);
    }

    parse_maxModule = 0;
    parse_currentModule = 0;
    parse_moduleMapHead = NULL;
    TOOLS_FREE(parse_lastErrModule);
}

static void
Parse_newModule(const char *name, const char *file)
{
    struct Parse_Module_s  *mp;

    for (mp = parse_moduleHead; mp; mp = mp->next)
        if (!PARSE_LABEL_COMPARE(mp->name, name)) {
            DEBUG_MSGTL(("parse-mibs", "  Module %s already noted\n", name));
            /*
             * Not the same file
             */
            if (PARSE_LABEL_COMPARE(mp->file, file)) {
                DEBUG_MSGTL(("parse-mibs", "    %s is now in %s\n",
                            name, file));
                if (DefaultStore_getInt(DsStorage_LIBRARY_ID,
                       DsInt_MIB_WARNINGS)) {
                    Logger_log(LOGGER_PRIORITY_WARNING,
                             "Warning: Module %s was in %s now is %s\n",
                             name, mp->file, file);
        }

                /*
                 * Use the new one in preference
                 */
                free(mp->file);
                mp->file = strdup(file);
            }
            return;
        }

    /*
     * Add this module to the list
     */
    DEBUG_MSGTL(("parse-mibs", "  Module %d %s is in %s\n", parse_maxModule,
                name, file));
    mp = (struct Parse_Module_s *) calloc(1, sizeof(struct Parse_Module_s));
    if (mp == NULL)
        return;
    mp->name = strdup(name);
    mp->file = strdup(file);
    mp->imports = NULL;
    mp->no_imports = -1;        /* Not yet loaded */
    mp->modid = parse_maxModule;
    ++parse_maxModule;

    mp->next = parse_moduleHead;     /* Or add to the *end* of the list? */
    parse_moduleHead = mp;
}


static void
Parse_scanObjlist(struct Parse_Node_s *root, struct Parse_Module_s *mp, struct Parse_Objgroup_s *list, const char *error)
{
    int             oLine = parse_mibLine;

    while (list) {
        struct Parse_Objgroup_s *gp = list;
        struct Parse_Node_s    *np;
        list = list->next;
        np = root;
        while (np)
            if (PARSE_LABEL_COMPARE(np->label, gp->name))
                np = np->next;
            else
                break;
        if (!np) {
        int i;
        struct Parse_ModuleImport_s *mip;
        /* if not local, check if it was IMPORTed */
        for (i = 0, mip = mp->imports; i < mp->no_imports; i++, mip++)
        if (strcmp(mip->label, gp->name) == 0)
            break;
        if (i == mp->no_imports) {
        parse_mibLine = gp->line;
        Parse_printError(error, gp->name, PARSE_QUOTESTRING);
        }
        }
        free(gp->name);
        free(gp);
    }
    parse_mibLine = oLine;
}

/*
 * Parses a mib file and returns a linked list of nodes found in the file.
 * Returns NULL on error.
 */
static struct Parse_Node_s *
Parse_parse(FILE * fp, struct Parse_Node_s *root)
{

    char            token[PARSE_MAXTOKEN];
    char            name[PARSE_MAXTOKEN+1];
    int             type = PARSE_LABEL;
    int             lasttype = PARSE_LABEL;

#define BETWEEN_MIBS          1
#define IN_MIB                2
    int             state = BETWEEN_MIBS;
    struct Parse_Node_s    *np, *nnp;
    struct Parse_Objgroup_s *oldgroups = NULL, *oldobjects = NULL, *oldnotifs =
        NULL;

    DEBUG_MSGTL(("parse-file", "Parsing file:  %s...\n", parse_file));

    if (parse_lastErrModule)
        free(parse_lastErrModule);
    parse_lastErrModule = NULL;

    np = root;
    if (np != NULL) {
        /*
         * now find end of chain
         */
        while (np->next)
            np = np->next;
    }

    while (type != PARSE_ENDOFFILE) {
        if (lasttype == PARSE_CONTINUE)
            lasttype = type;
        else
            type = lasttype = Parse_getToken(fp, token, PARSE_MAXTOKEN);

        switch (type) {
        case PARSE_END:
            if (state != IN_MIB) {
                Parse_printError("Error, END before start of MIB", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            } else {
                struct Parse_Module_s  *mp;

                for (mp = parse_moduleHead; mp; mp = mp->next)
                    if (mp->modid == parse_currentModule)
                        break;
                Parse_scanObjlist(root, mp, parse_objgroups, "Undefined OBJECT-GROUP");
                Parse_scanObjlist(root, mp, parse_objects, "Undefined OBJECT");
                Parse_scanObjlist(root, mp, parse_notifs, "Undefined NOTIFICATION");
                parse_objgroups = oldgroups;
                parse_objects = oldobjects;
                parse_notifs = oldnotifs;
                Parse_doLinkup(mp, root);
                np = root = NULL;
            }
            state = BETWEEN_MIBS;

            continue;
        case PARSE_IMPORTS:
            Parse_parseImports(fp);
            continue;
        case PARSE_EXPORTS:
            while (type != PARSE_SEMI && type != PARSE_ENDOFFILE)
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            continue;
        case PARSE_LABEL:
        case PARSE_INTEGER:
        case PARSE_INTEGER32:
        case PARSE_UINTEGER32:
        case PARSE_UNSIGNED32:
        case PARSE_COUNTER:
        case PARSE_COUNTER64:
        case PARSE_GAUGE:
        case PARSE_IPADDR:
        case PARSE_NETADDR:
        case PARSE_NSAPADDRESS:
        case PARSE_OBJSYNTAX:
        case PARSE_APPSYNTAX:
        case PARSE_SIMPLESYNTAX:
        case PARSE_OBJNAME:
        case PARSE_NOTIFNAME:
        case PARSE_KW_OPAQUE:
        case PARSE_TIMETICKS:
            break;
        case PARSE_ENDOFFILE:
            continue;
        default:
            Strlcpy_strlcpy(name, token, sizeof(name));
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            nnp = NULL;
            if (type == PARSE_MACRO) {
                nnp = Parse_parseMacro(fp, name);
                if (nnp == NULL) {
                    Parse_printError("Bad parse of MACRO", NULL, type);
                    parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                    /*
                     * return NULL;
                     */
                }
                Parse_freeNode(nnp); /* IGNORE */
                nnp = NULL;
            } else
                Parse_printError(name, "is a reserved word", lasttype);
            continue;           /* see if we can parse the rest of the file */
        }
        Strlcpy_strlcpy(name, token, sizeof(name));
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        nnp = NULL;

        /*
         * Handle obsolete method to assign an object identifier to a
         * module
         */
        if (lasttype == PARSE_LABEL && type == PARSE_LEFTBRACKET) {
            while (type != PARSE_RIGHTBRACKET && type != PARSE_ENDOFFILE)
                type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type == PARSE_ENDOFFILE) {
                Parse_printError("Expected \"}\"", token, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        }

        switch (type) {
        case PARSE_DEFINITIONS:
            if (state != BETWEEN_MIBS) {
                Parse_printError("Error, nested MIBS", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            state = IN_MIB;
            parse_currentModule = Parse_whichModule(name);
            oldgroups = parse_objgroups;
            parse_objgroups = NULL;
            oldobjects = parse_objects;
            parse_objects = NULL;
            oldnotifs = parse_notifs;
            parse_notifs = NULL;
            if (parse_currentModule == -1) {
                Parse_newModule(name, parse_file);
                parse_currentModule = Parse_whichModule(name);
            }
            DEBUG_MSGTL(("parse-mibs", "Parsing MIB: %d %s\n",
                        parse_currentModule, name));
            while ((type = Parse_getToken(fp, token, PARSE_MAXTOKEN)) != PARSE_ENDOFFILE)
                if (type == PARSE_BEGIN)
                    break;
            break;
        case PARSE_OBJTYPE:
            nnp = Parse_parseObjecttype(fp, name);
            if (nnp == NULL) {
                Parse_printError("Bad parse of OBJECT-TYPE", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            break;
        case PARSE_OBJGROUP:
            nnp = Parse_parseObjectgroup(fp, name, PARSE_OBJECTS, &parse_objects);
            if (nnp == NULL) {
                Parse_printError("Bad parse of OBJECT-GROUP", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            break;
        case PARSE_NOTIFGROUP:
            nnp = Parse_parseObjectgroup(fp, name, PARSE_NOTIFICATIONS, &parse_notifs);
            if (nnp == NULL) {
                Parse_printError("Bad parse of NOTIFICATION-GROUP", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            break;
        case PARSE_TRAPTYPE:
            nnp = Parse_parseTrapDefinition(fp, name);
            if (nnp == NULL) {
                Parse_printError("Bad parse of TRAP-TYPE", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            break;
        case PARSE_NOTIFTYPE:
            nnp = Parse_parseNotificationDefinition(fp, name);
            if (nnp == NULL) {
                Parse_printError("Bad parse of NOTIFICATION-TYPE", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            break;
        case PARSE_COMPLIANCE:
            nnp = Parse_parseCompliance(fp, name);
            if (nnp == NULL) {
                Parse_printError("Bad parse of MODULE-COMPLIANCE", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            break;
        case PARSE_AGENTCAP:
            nnp = Parse_parseCapabilities(fp, name);
            if (nnp == NULL) {
                Parse_printError("Bad parse of AGENT-CAPABILITIES", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            break;
        case PARSE_MACRO:
            nnp = Parse_parseMacro(fp, name);
            if (nnp == NULL) {
                Parse_printError("Bad parse of MACRO", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                /*
                 * return NULL;
                 */
            }
            Parse_freeNode(nnp);     /* IGNORE */
            nnp = NULL;
            break;
        case PARSE_MODULEIDENTITY:
            nnp = Parse_parseModuleIdentity(fp, name);
            if (nnp == NULL) {
                Parse_printError("Bad parse of MODULE-IDENTITY", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            break;
        case PARSE_OBJIDENTITY:
            nnp = Parse_parseObjectgroup(fp, name, PARSE_OBJECTS, &parse_objects);
            if (nnp == NULL) {
                Parse_printError("Bad parse of OBJECT-IDENTITY", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            break;
        case PARSE_OBJECT:
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type != PARSE_IDENTIFIER) {
                Parse_printError("Expected IDENTIFIER", token, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
            if (type != PARSE_EQUALS) {
                Parse_printError("Expected \".=\"", token, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            nnp = Parse_objectid(fp, name);
            if (nnp == NULL) {
                Parse_printError("Bad parse of OBJECT IDENTIFIER", NULL, type);
                parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
                return NULL;
            }
            break;
        case PARSE_EQUALS:
            nnp = Parse_parseAsntype(fp, name, &type, token);
            lasttype = PARSE_CONTINUE;
            break;
        case PARSE_ENDOFFILE:
            break;
        default:
            Parse_printError("Bad operator", token, type);
            parse_gMibError = PARSE_MODULE_SYNTAX_ERROR;
            return NULL;
        }
        if (nnp) {
            if (np)
                np->next = nnp;
            else
                np = root = nnp;
            while (np->next)
                np = np->next;
            if (np->type == PARSE_TYPE_OTHER)
                np->type = type;
        }
    }
    DEBUG_MSGTL(("parse-file", "End of file (%s)\n", parse_file));
    return root;
}

/*
 * return zero if character is not a label character.
 */
static int
Parse_isLabelchar(int ich)
{
    if ((isalnum(ich)) || (ich == '-'))
        return 1;
    if (ich == '_' && DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                         DsBool_MIB_PARSE_LABEL)) {
        return 1;
    }

    return 0;
}

/**
 * Read a single character from a file. Assumes that the caller has invoked
 * flockfile(). Uses fgetc_unlocked() instead of getc() since the former is
 * implemented as an inline function in glibc. See also bug 3447196
 * (http://sourceforge.net/tracker/?func=detail&aid=3447196&group_id=12694&atid=112694).
 */
static int Parse_getc(FILE *stream)
{
    return fgetc_unlocked(stream);
}

/*
 * Parses a token from the file.  The type of the token parsed is returned,
 * and the text is placed in the string pointed to by token.
 * Warning: this method may recurse.
 */
int
Parse_getToken(FILE * fp, char *token, int maxtlen)
{
    register int    ch, ch_next;
    register char  *cp = token;
    register int    hash = 0;
    register struct Parse_Tok_s *tp;
    int             too_long = 0;
    enum { bdigits, xdigits, other } seenSymbols;

    /*
     * skip all white space
     */
    do {
        ch = Parse_getc(fp);
        if (ch == '\n')
            parse_mibLine++;
    }
    while (isspace(ch) && ch != EOF);
    *cp++ = ch;
    *cp = '\0';
    switch (ch) {
    case EOF:
        return PARSE_ENDOFFILE;
    case '"':
        return Parse_parseQuoteString(fp, token, maxtlen);
    case '\'':                 /* binary or hex constant */
        seenSymbols = bdigits;
        while ((ch = Parse_getc(fp)) != EOF && ch != '\'') {
            switch (seenSymbols) {
            case bdigits:
                if (ch == '0' || ch == '1')
                    break;
                seenSymbols = xdigits;
            case xdigits:
                if (isxdigit(ch))
                    break;
                seenSymbols = other;
            case other:
                break;
            }
            if (cp - token < maxtlen - 2)
                *cp++ = ch;
        }
        if (ch == '\'') {
            unsigned long   val = 0;
            char           *run = token + 1;
            ch = Parse_getc(fp);
            switch (ch) {
            case EOF:
                return PARSE_ENDOFFILE;
            case 'b':
            case 'B':
                if (seenSymbols > bdigits) {
                    *cp++ = '\'';
                    *cp = 0;
                    return PARSE_LABEL;
                }
                while (run != cp)
                    val = val * 2 + *run++ - '0';
                break;
            case 'h':
            case 'H':
                if (seenSymbols > xdigits) {
                    *cp++ = '\'';
                    *cp = 0;
                    return PARSE_LABEL;
                }
                while (run != cp) {
                    ch = *run++;
                    if ('0' <= ch && ch <= '9')
                        val = val * 16 + ch - '0';
                    else if ('a' <= ch && ch <= 'f')
                        val = val * 16 + ch - 'a' + 10;
                    else if ('A' <= ch && ch <= 'F')
                        val = val * 16 + ch - 'A' + 10;
                }
                break;
            default:
                *cp++ = '\'';
                *cp = 0;
                return PARSE_LABEL;
            }
            sprintf(token, "%ld", val);
            return PARSE_NUMBER;
        } else
            return PARSE_LABEL;
    case '(':
        return PARSE_LEFTPAREN;
    case ')':
        return PARSE_RIGHTPAREN;
    case '{':
        return PARSE_LEFTBRACKET;
    case '}':
        return PARSE_RIGHTBRACKET;
    case '[':
        return PARSE_LEFTSQBRACK;
    case ']':
        return PARSE_RIGHTSQBRACK;
    case ';':
        return PARSE_SEMI;
    case ',':
        return PARSE_COMMA;
    case '|':
        return PARSE_BAR;
    case '.':
        ch_next = Parse_getc(fp);
        if (ch_next == '.')
            return PARSE_RANGE;
        ungetc(ch_next, fp);
        return PARSE_LABEL;
    case ':':
        ch_next = Parse_getc(fp);
        if (ch_next != ':') {
            ungetc(ch_next, fp);
            return PARSE_LABEL;
        }
        ch_next = Parse_getc(fp);
        if (ch_next != '=') {
            ungetc(ch_next, fp);
            return PARSE_LABEL;
        }
        return PARSE_EQUALS;
    case '-':
        ch_next = Parse_getc(fp);
        if (ch_next == '-') {
            if (DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                       DsBool_MIB_COMMENT_TERM)) {
                /*
                 * Treat the rest of this line as a comment.
                 */
                while ((ch_next != EOF) && (ch_next != '\n'))
                    ch_next = Parse_getc(fp);
            } else {
                /*
                 * Treat the rest of the line or until another '--' as a comment
                 */
                /*
                 * (this is the "technically" correct way to parse comments)
                 */
                ch = ' ';
                ch_next = Parse_getc(fp);
                while (ch_next != EOF && ch_next != '\n' &&
                       (ch != '-' || ch_next != '-')) {
                    ch = ch_next;
                    ch_next = Parse_getc(fp);
                }
            }
            if (ch_next == EOF)
                return PARSE_ENDOFFILE;
            if (ch_next == '\n')
                parse_mibLine++;
            return Parse_getToken(fp, token, maxtlen);
        }
        ungetc(ch_next, fp);
    /* fallthrough */
    default:
        /*
         * Accumulate characters until end of token is found.  Then attempt to
         * match this token as a reserved word.  If a match is found, return the
         * type.  Else it is a label.
         */
        if (!Parse_isLabelchar(ch))
            return PARSE_LABEL;
        hash += tolower(ch);
      more:
        while (Parse_isLabelchar(ch_next = Parse_getc(fp))) {
            hash += tolower(ch_next);
            if (cp - token < maxtlen - 1)
                *cp++ = ch_next;
            else
                too_long = 1;
        }
        ungetc(ch_next, fp);
        *cp = '\0';

        if (too_long)
            Parse_printError("Warning: token too long", token, PARSE_CONTINUE);
        for (tp = parse_buckets[PARSE_BUCKET(hash)]; tp; tp = tp->next) {
            if ((tp->hash == hash) && (!PARSE_LABEL_COMPARE(tp->name, token)))
                break;
        }
        if (tp) {
            if (tp->token != PARSE_CONTINUE)
                return (tp->token);
            while (isspace((ch_next = Parse_getc(fp))))
                if (ch_next == '\n')
                    parse_mibLine++;
            if (ch_next == EOF)
                return PARSE_ENDOFFILE;
            if (isalnum(ch_next)) {
                *cp++ = ch_next;
                hash += tolower(ch_next);
                goto more;
            }
        }
        if (token[0] == '-' || isdigit((unsigned char)(token[0]))) {
            for (cp = token + 1; *cp; cp++)
                if (!isdigit((unsigned char)(*cp)))
                    return PARSE_LABEL;
            return PARSE_NUMBER;
        }
        return PARSE_LABEL;
    }
}


int Parse_addMibfile(const char* tmpstr, const char* d_name, FILE *ip )
{
    FILE           *fp;
    char            token[PARSE_MAXTOKEN], token2[PARSE_MAXTOKEN];

    /*
     * which module is this
     */
    if ((fp = fopen(tmpstr, "r")) == NULL) {
        Logger_logPerror(tmpstr);
        return 1;
    }
    DEBUG_MSGTL(("parse-mibs", "Checking file: %s...\n",
                tmpstr));
    parse_mibLine = 1;
    parse_file = tmpstr;
    if (Parse_getToken(fp, token, PARSE_MAXTOKEN) != PARSE_LABEL) {
        fclose(fp);
        return 1;
    }
    /*
     * simple test for this being a MIB
     */
    if (Parse_getToken(fp, token2, PARSE_MAXTOKEN) == PARSE_DEFINITIONS) {
        Parse_newModule(token, tmpstr);
        if (ip)
            fprintf(ip, "%s %s\n", token, d_name);
        fclose(fp);
        return 0;
    } else {
        fclose(fp);
        return 1;
    }
}

/* For Win32 platforms, the directory does not maintain a last modification
 * date that we can compare with the modification date of the .index file.
 * Therefore there is no way to know whether any .index file is valid.
 * This is the reason for the #if !(defined(WIN32) || defined(cygwin))
 * in the add_mibdir function
 */
int Parse_addMibdir(const char *dirname)
{
    FILE           *ip;
    DIR            *dir, *dir2;
    const char     *oldFile = parse_file;
    struct dirent  *file;
    char            tmpstr[300];
    int             count = 0;
    int             fname_len = 0;
    char           *token;
    char space;
    char newline;
    struct stat     dir_stat, idx_stat;
    char            tmpstr1[300];

    DEBUG_MSGTL(("parse-mibs", "Scanning directory %s\n", dirname));

    token = Mib_mibIndexLookup( dirname );
    if (token && stat(token, &idx_stat) == 0 && stat(dirname, &dir_stat) == 0) {
        if (dir_stat.st_mtime < idx_stat.st_mtime) {
            DEBUG_MSGTL(("parse-mibs", "The index is good\n"));
            if ((ip = fopen(token, "r")) != NULL) {
                fgets(tmpstr, sizeof(tmpstr), ip); /* Skip dir line */
                while (fscanf(ip, "%127s%c%299s%c", token, &space, tmpstr,
            &newline) == 4) {

            /*
             * If an overflow of the token or tmpstr buffers has been
             * found log a message and break out of the while loop,
             * thus the rest of the file tokens will be ignored.
             */
            if (space != ' ' || newline != '\n') {
            Logger_log(LOGGER_PRIORITY_ERR,
                "add_mibdir: strings scanned in from %s/%s " \
                "are too large.  count = %d\n ", dirname,
                ".index", count);
                break;
            }

            snprintf(tmpstr1, sizeof(tmpstr1), "%s/%s", dirname, tmpstr);
                    tmpstr1[ sizeof(tmpstr1)-1 ] = 0;
                    Parse_newModule(token, tmpstr1);
                    count++;
                }
                fclose(ip);
                return count;
            } else
                DEBUG_MSGTL(("parse-mibs", "Can't read index\n"));
        } else
            DEBUG_MSGTL(("parse-mibs", "Index outdated\n"));
    } else
        DEBUG_MSGTL(("parse-mibs", "No index\n"));

    if ((dir = opendir(dirname))) {
        ip = Mib_mibIndexNew( dirname );
        while ((file = readdir(dir))) {
            /*
             * Only parse file names that don't begin with a '.'
             * Also skip files ending in '~', or starting/ending
             * with '#' which are typically editor backup files.
             */
            if (file->d_name != NULL) {
              fname_len = strlen( file->d_name );
              if (fname_len > 0 && file->d_name[0] != '.'
                                && file->d_name[0] != '#'
                                && file->d_name[fname_len-1] != '#'
                                && file->d_name[fname_len-1] != '~') {
                snprintf(tmpstr, sizeof(tmpstr), "%s/%s", dirname, file->d_name);
                tmpstr[ sizeof(tmpstr)-1 ] = 0;
                if ((dir2 = opendir(tmpstr))) {
                    /*
                     * file is a directory, don't read it
                     */
                    closedir(dir2);
                } else {
                    if ( !Parse_addMibfile( tmpstr, file->d_name, ip ))
                        count++;
                }
              }
            }
        }
        parse_file = oldFile;
        closedir(dir);
        if (ip)
            fclose(ip);
        return (count);
    }
    else
        DEBUG_MSGTL(("parse-mibs","cannot open MIB directory %s\n", dirname));

    return (-1);
}


/*
 * Returns the root of the whole tree
 *   (for backwards compatability)
 */
struct Parse_Tree_s    *
Parse_readMib(const char *filename)
{
    FILE           *fp;
    char            token[PARSE_MAXTOKEN];

    fp = fopen(filename, "r");
    if (fp == NULL) {
        Logger_logPerror(filename);
        return NULL;
    }
    parse_mibLine = 1;
    parse_file = filename;
    DEBUG_MSGTL(("parse-mibs", "Parsing file: %s...\n", filename));
    if (Parse_getToken(fp, token, PARSE_MAXTOKEN) != PARSE_LABEL) {
        Logger_log(LOGGER_PRIORITY_ERR, "Failed to parse MIB file %s\n", filename);
        fclose(fp);
        return NULL;
    }
    fclose(fp);
    Parse_newModule(token, filename);
    (void) Parse_readModule(token);

    return parse_treeHead;
}


struct Parse_Tree_s * Parse_readAllMibs(void)
{

    struct Parse_Module_s  *mp;

    for (mp = parse_moduleHead; mp; mp = mp->next)
        if (mp->no_imports == -1)
            Parse_readModule(mp->name);
    Parse_adoptOrphans();

    /* If entered the syntax error loop in "read_module()" */
    if (parse_gLoop == 1) {
        parse_gLoop = 0;
        if (parse_gpMibErrorString != NULL) {
            TOOLS_FREE(parse_gpMibErrorString);
        }
        parse_gpMibErrorString = (char *) calloc(1, PARSE_MAXQUOTESTR);
        if (parse_gpMibErrorString == NULL) {
            Logger_log(LOGGER_PRIORITY_CRIT, "failed to allocated memory for gpMibErrorString\n");
        } else {
            snprintf(parse_gpMibErrorString, sizeof(parse_gpMibErrorString)-1, "Error in parsing MIB module(s): %s ! Unable to load corresponding MIB(s)", parse_gMibNames);
        }
    }

    /* Caller's responsibility to free this memory */
    parse_treeHead->parseErrorString = parse_gpMibErrorString;

    return parse_treeHead;
}


static int Parse_parseQuoteString(FILE * fp, char *token, int maxtlen)
{
    register int    ch;
    int             count = 0;
    int             too_long = 0;
    char           *token_start = token;

    for (ch = Parse_getc(fp); ch != EOF; ch = Parse_getc(fp)) {
        if (ch == '\r')
            continue;
        if (ch == '\n') {
            parse_mibLine++;
        } else if (ch == '"') {
            *token = '\0';
            if (too_long && DefaultStore_getInt(DsStorage_LIBRARY_ID,
                       DsInt_MIB_WARNINGS) > 1) {
                /*
                 * show short form for brevity sake
                 */
                char            ch_save = *(token_start + 50);
                *(token_start + 50) = '\0';
                Parse_printError("Warning: string too long",
                            token_start, PARSE_QUOTESTRING);
                *(token_start + 50) = ch_save;
            }
            return PARSE_QUOTESTRING;
        }
        /*
         * maximum description length check.  If greater, keep parsing
         * but truncate the string
         */
        if (++count < maxtlen)
            *token++ = ch;
        else
            too_long = 1;
    }

    return 0;
}

/*
 * struct Parse_IndexList_s *
 * getIndexes(FILE *fp):
 *   This routine parses a string like  { blah blah blah } and returns a
 *   list of the strings enclosed within it.
 *
 */
static struct Parse_IndexList_s *
Parse_getIndexes(FILE * fp, struct Parse_IndexList_s **retp)
{
    int             type;
    char            token[PARSE_MAXTOKEN];
    char            nextIsImplied = 0;

    struct Parse_IndexList_s *mylist = NULL;
    struct Parse_IndexList_s **mypp = &mylist;

    Parse_freeIndexes(retp);

    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);

    if (type != PARSE_LEFTBRACKET) {
        return NULL;
    }

    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    while (type != PARSE_RIGHTBRACKET && type != PARSE_ENDOFFILE) {
        if ((type == PARSE_LABEL) || (type & PARSE_SYNTAX_MASK)) {
            *mypp =
                (struct Parse_IndexList_s *) calloc(1, sizeof(struct Parse_IndexList_s));
            if (*mypp) {
                (*mypp)->ilabel = strdup(token);
                (*mypp)->isimplied = nextIsImplied;
                mypp = &(*mypp)->next;
                nextIsImplied = 0;
            }
        } else if (type == PARSE_IMPLIED) {
            nextIsImplied = 1;
        }
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    }

    *retp = mylist;
    return mylist;
}

static struct Parse_VarbindList_s *
Parse_getVarbinds(FILE * fp, struct Parse_VarbindList_s **retp)
{
    int             type;
    char            token[PARSE_MAXTOKEN];

    struct Parse_VarbindList_s *mylist = NULL;
    struct Parse_VarbindList_s **mypp = &mylist;

    Parse_freeVarbinds(retp);

    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);

    if (type != PARSE_LEFTBRACKET) {
        return NULL;
    }

    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    while (type != PARSE_RIGHTBRACKET && type != PARSE_ENDOFFILE) {
        if ((type == PARSE_LABEL) || (type & PARSE_SYNTAX_MASK)) {
            *mypp =
                (struct Parse_VarbindList_s *) calloc(1,
                                               sizeof(struct
                                                      Parse_VarbindList_s));
            if (*mypp) {
                (*mypp)->vblabel = strdup(token);
                mypp = &(*mypp)->next;
            }
        }
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
    }

    *retp = mylist;
    return mylist;
}

static void
Parse_freeIndexes(struct Parse_IndexList_s **spp)
{
    if (spp && *spp) {
        struct Parse_IndexList_s *pp, *npp;

        pp = *spp;
        *spp = NULL;

        while (pp) {
            npp = pp->next;
            if (pp->ilabel)
                free(pp->ilabel);
            free(pp);
            pp = npp;
        }
    }
}

static void
Parse_freeVarbinds(struct Parse_VarbindList_s **spp)
{
    if (spp && *spp) {
        struct Parse_VarbindList_s *pp, *npp;

        pp = *spp;
        *spp = NULL;

        while (pp) {
            npp = pp->next;
            if (pp->vblabel)
                free(pp->vblabel);
            free(pp);
            pp = npp;
        }
    }
}

static void
Parse_freeRanges(struct Parse_RangeList_s **spp)
{
    if (spp && *spp) {
        struct Parse_RangeList_s *pp, *npp;

        pp = *spp;
        *spp = NULL;

        while (pp) {
            npp = pp->next;
            free(pp);
            pp = npp;
        }
    }
}

static void
Parse_freeEnums(struct Parse_EnumList_s **spp)
{
    if (spp && *spp) {
        struct Parse_EnumList_s *pp, *npp;

        pp = *spp;
        *spp = NULL;

        while (pp) {
            npp = pp->next;
            if (pp->label)
                free(pp->label);
            free(pp);
            pp = npp;
        }
    }
}

static struct Parse_EnumList_s *
Parse_copyEnums(struct Parse_EnumList_s *sp)
{
    struct Parse_EnumList_s *xp = NULL, **spp = &xp;

    while (sp) {
        *spp = (struct Parse_EnumList_s *) calloc(1, sizeof(struct Parse_EnumList_s));
        if (!*spp)
            break;
        (*spp)->label = strdup(sp->label);
        (*spp)->value = sp->value;
        spp = &(*spp)->next;
        sp = sp->next;
    }
    return (xp);
}

static struct Parse_RangeList_s *
Parse_copyRanges(struct Parse_RangeList_s *sp)
{
    struct Parse_RangeList_s *xp = NULL, **spp = &xp;

    while (sp) {
        *spp = (struct Parse_RangeList_s *) calloc(1, sizeof(struct Parse_RangeList_s));
        if (!*spp)
            break;
        (*spp)->low = sp->low;
        (*spp)->high = sp->high;
        spp = &(*spp)->next;
        sp = sp->next;
    }
    return (xp);
}

/*
 * This routine parses a string like  { blah blah blah } and returns OBJID if
 * it is well formed, and NULL if not.
 */
static int Parse_tossObjectIdentifier(FILE * fp)
{
    int             type;
    char            token[PARSE_MAXTOKEN];
    int             bracketcount = 1;

    type = Parse_getToken(fp, token, PARSE_MAXTOKEN);

    if (type != PARSE_LEFTBRACKET)
        return 0;
    while ((type != PARSE_RIGHTBRACKET || bracketcount > 0) && type != PARSE_ENDOFFILE) {
        type = Parse_getToken(fp, token, PARSE_MAXTOKEN);
        if (type == PARSE_LEFTBRACKET)
            bracketcount++;
        else if (type == PARSE_RIGHTBRACKET)
            bracketcount--;
    }

    if (type == PARSE_RIGHTBRACKET)
        return PARSE_OBJID;
    else
        return 0;
}

/* Find node in any MIB module
   Used by Perl modules		*/
struct Parse_Tree_s *
Parse_findNode(const char *name, struct Parse_Tree_s *subtree)
{                               /* Unused */
    return (Parse_findTreeNode(name, -1));
}

struct Parse_Tree_s    *
Parse_findNode2(const char *name, const char *module)
{
  int modid = -1;
  if (module) {
    modid = Parse_whichModule(module);
  }
  if (modid == -1)
  {
    return (NULL);
  }
  return (Parse_findTreeNode(name, modid));
}

/* Used in the perl module */
struct Parse_Module_s  *
Parse_findModule(int mid)
{
    struct Parse_Module_s  *mp;

    for (mp = parse_moduleHead; mp != NULL; mp = mp->next) {
        if (mp->modid == mid)
            break;
    }
    return mp;
}


static char     parse_leaveIndent[256];
static int      parse_leaveWasSimple;

static void
Parse_printMibLeaves(FILE * f, struct Parse_Tree_s *tp, int width)
{
    struct Parse_Tree_s    *ntp;
    char           *ip = parse_leaveIndent + strlen(parse_leaveIndent) - 1;
    char            last_ipch = *ip;

    *ip = '+';
    if (tp->type == PARSE_TYPE_OTHER || tp->type > PARSE_TYPE_SIMPLE_LAST) {
        fprintf(f, "%s--%s(%ld)\n", parse_leaveIndent, tp->label, tp->subid);
        if (tp->indexes) {
            struct Parse_IndexList_s *xp = tp->indexes;
            int             first = 1, cpos = 0, len, cmax =
                width - strlen(parse_leaveIndent) - 12;
            *ip = last_ipch;
            fprintf(f, "%s  |  Index: ", parse_leaveIndent);
            while (xp) {
                if (first)
                    first = 0;
                else
                    fprintf(f, ", ");
                cpos += (len = strlen(xp->ilabel) + 2);
                if (cpos > cmax) {
                    fprintf(f, "\n");
                    fprintf(f, "%s  |         ", parse_leaveIndent);
                    cpos = len;
                }
                fprintf(f, "%s", xp->ilabel);
                xp = xp->next;
            }
            fprintf(f, "\n");
            *ip = '+';
        }
    } else {
        const char     *acc, *typ;
        int             size = 0;
        switch (tp->access) {
        case PARSE_MIB_ACCESS_NOACCESS:
            acc = "----";
            break;
        case PARSE_MIB_ACCESS_READONLY:
            acc = "-R--";
            break;
        case PARSE_MIB_ACCESS_WRITEONLY:
            acc = "--W-";
            break;
        case PARSE_MIB_ACCESS_READWRITE:
            acc = "-RW-";
            break;
        case PARSE_MIB_ACCESS_NOTIFY:
            acc = "---N";
            break;
        case PARSE_MIB_ACCESS_CREATE:
            acc = "CR--";
            break;
        default:
            acc = "    ";
            break;
        }
        switch (tp->type) {
        case PARSE_TYPE_OBJID:
            typ = "ObjID    ";
            break;
        case PARSE_TYPE_OCTETSTR:
            typ = "String   ";
            size = 1;
            break;
        case PARSE_TYPE_INTEGER:
            if (tp->enums)
                typ = "EnumVal  ";
            else
                typ = "INTEGER  ";
            break;
        case PARSE_TYPE_NETADDR:
            typ = "NetAddr  ";
            break;
        case PARSE_TYPE_IPADDR:
            typ = "IpAddr   ";
            break;
        case PARSE_TYPE_COUNTER:
            typ = "Counter  ";
            break;
        case PARSE_TYPE_GAUGE:
            typ = "Gauge    ";
            break;
        case PARSE_TYPE_TIMETICKS:
            typ = "TimeTicks";
            break;
        case PARSE_TYPE_OPAQUE:
            typ = "Opaque   ";
            size = 1;
            break;
        case PARSE_TYPE_NULL:
            typ = "Null     ";
            break;
        case PARSE_TYPE_COUNTER64:
            typ = "Counter64";
            break;
        case PARSE_TYPE_BITSTRING:
            typ = "BitString";
            break;
        case PARSE_TYPE_NSAPADDRESS:
            typ = "NsapAddr ";
            break;
        case PARSE_TYPE_UNSIGNED32:
            typ = "Unsigned ";
            break;
        case PARSE_TYPE_UINTEGER:
            typ = "UInteger ";
            break;
        case PARSE_TYPE_INTEGER32:
            typ = "Integer32";
            break;
        default:
            typ = "         ";
            break;
        }
        fprintf(f, "%s-- %s %s %s(%ld)\n", parse_leaveIndent, acc, typ,
                tp->label, tp->subid);
        *ip = last_ipch;
        if (tp->tc_index >= 0)
            fprintf(f, "%s        Textual Convention: %s\n", parse_leaveIndent,
                    parse_tclist[tp->tc_index].descriptor);
        if (tp->enums) {
            struct Parse_EnumList_s *ep = tp->enums;
            int             cpos = 0, cmax =
                width - strlen(parse_leaveIndent) - 16;
            fprintf(f, "%s        Values: ", parse_leaveIndent);
            while (ep) {
                char            buf[80];
                int             bufw;
                if (ep != tp->enums)
                    fprintf(f, ", ");
                snprintf(buf, sizeof(buf), "%s(%d)", ep->label, ep->value);
                buf[ sizeof(buf)-1 ] = 0;
                cpos += (bufw = strlen(buf) + 2);
                if (cpos >= cmax) {
                    fprintf(f, "\n%s                ", parse_leaveIndent);
                    cpos = bufw;
                }
                fprintf(f, "%s", buf);
                ep = ep->next;
            }
            fprintf(f, "\n");
        }
        if (tp->ranges) {
            struct Parse_RangeList_s *rp = tp->ranges;
            if (size)
                fprintf(f, "%s        Size: ", parse_leaveIndent);
            else
                fprintf(f, "%s        Range: ", parse_leaveIndent);
            while (rp) {
                if (rp != tp->ranges)
                    fprintf(f, " | ");
                Parse_printRangeValue(f, tp->type, rp);
                rp = rp->next;
            }
            fprintf(f, "\n");
        }
    }
    *ip = last_ipch;
    strcat(parse_leaveIndent, "  |");
    parse_leaveWasSimple = tp->type != PARSE_TYPE_OTHER;

    {
        int             i, j, count = 0;
        struct leave {
            oid             id;
            struct Parse_Tree_s    *tp;
        }              *leaves, *lp;

        for (ntp = tp->child_list; ntp; ntp = ntp->next_peer)
            count++;
        if (count) {
            leaves = (struct leave *) calloc(count, sizeof(struct leave));
            if (!leaves)
                return;
            for (ntp = tp->child_list, count = 0; ntp;
                 ntp = ntp->next_peer) {
                for (i = 0, lp = leaves; i < count; i++, lp++)
                    if (lp->id >= ntp->subid)
                        break;
                for (j = count; j > i; j--)
                    leaves[j] = leaves[j - 1];
                lp->id = ntp->subid;
                lp->tp = ntp;
                count++;
            }
            for (i = 1, lp = leaves; i <= count; i++, lp++) {
                if (!parse_leaveWasSimple || lp->tp->type == 0)
                    fprintf(f, "%s\n", parse_leaveIndent);
                if (i == count)
                    ip[3] = ' ';
                Parse_printMibLeaves(f, lp->tp, width);
            }
            free(leaves);
            parse_leaveWasSimple = 0;
        }
    }
    ip[1] = 0;
}

void Mib_printMibTree(FILE * f, struct Parse_Tree_s *tp, int width)
{
    parse_leaveIndent[0] = ' ';
    parse_leaveIndent[1] = 0;
    parse_leaveWasSimple = 1;
    Parse_printMibLeaves(f, tp, width);
}


/*
 * Merge the parsed object identifier with the existing node.
 * If there is a problem with the identifier, release the existing node.
 */
static struct Parse_Node_s *
Parse_mergeParseObjectid(struct Parse_Node_s *np, FILE * fp, char *name)
{
    struct Parse_Node_s    *nnp;
    /*
     * printf("merge defval --> %s\n",np->defaultValue);
     */
    nnp = Parse_objectid(fp, name);
    if (nnp) {

        /*
         * apply last OID sub-identifier data to the information
         */
        /*
         * already collected for this node.
         */
        struct Parse_Node_s    *headp, *nextp;
        int             ncount = 0;
        nextp = headp = nnp;
        while (nnp->next) {
            nextp = nnp;
            ncount++;
            nnp = nnp->next;
        }

        np->label = nnp->label;
        np->subid = nnp->subid;
        np->modid = nnp->modid;
        np->parent = nnp->parent;
    if (nnp->filename != NULL) {
      free(nnp->filename);
    }
        free(nnp);

        if (ncount) {
            nextp->next = np;
            np = headp;
        }
    } else {
        Parse_freeNode(np);
        np = NULL;
    }

    return np;
}

/*
 * transfer data to tree from node
 *
 * move pointers for alloc'd data from np to tp.
 * this prevents them from being freed when np is released.
 * parent member is not moved.
 *
 * CAUTION: nodes may be repeats of existing tree nodes.
 * This can happen especially when resolving IMPORT clauses.
 *
 */
static void
Parse_treeFromNode(struct Parse_Tree_s *tp, struct Parse_Node_s *np)
{
    Parse_freePartialTree(tp, FALSE);

    tp->label = np->label;
    np->label = NULL;
    tp->enums = np->enums;
    np->enums = NULL;
    tp->ranges = np->ranges;
    np->ranges = NULL;
    tp->indexes = np->indexes;
    np->indexes = NULL;
    tp->augments = np->augments;
    np->augments = NULL;
    tp->varbinds = np->varbinds;
    np->varbinds = NULL;
    tp->hint = np->hint;
    np->hint = NULL;
    tp->units = np->units;
    np->units = NULL;
    tp->description = np->description;
    np->description = NULL;
    tp->reference = np->reference;
    np->reference = NULL;
    tp->defaultValue = np->defaultValue;
    np->defaultValue = NULL;
    tp->subid = np->subid;
    tp->tc_index = np->tc_index;
    tp->type = parse_translationTable[np->type];
    tp->access = np->access;
    tp->status = np->status;

    Mib_setFunction(tp);
}
