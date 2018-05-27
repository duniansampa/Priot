#ifndef PARSE_H
#define PARSE_H

#include "Generals.h"

#include "Types.h"

#define PARSE_NETSNMP_MAXLABEL 64      /* maximum characters in a label */
#define PARSE_MAXTOKEN        128     /* maximum characters in a token */
#define PARSE_MAXQUOTESTR     4096    /* maximum characters in a quoted string */


struct Parse_VariableList_s;

/*
 * A linked list of tag-value pairs for enumerated integers.
 */
struct Parse_EnumList_s {
    struct Parse_EnumList_s *next;
    int             value;
    char           *label;
};

/*
 * A linked list of ranges
 */
struct Parse_RangeList_s {
    struct Parse_RangeList_s *next;
    int low, high;
};

/*
 * A linked list of indexes
 */
struct Parse_IndexList_s {
    struct Parse_IndexList_s *next;
    char           *ilabel;
    char            isimplied;
};

/*
 * A linked list of varbinds
 */
struct Parse_VarbindList_s {
    struct Parse_VarbindList_s *next;
    char           *vblabel;
};

/*
 * A tree in the format of the tree structure of the MIB.
 */
struct Parse_Tree_s {
    struct Parse_Tree_s    *child_list;     /* list of children of this node */
    struct Parse_Tree_s    *next_peer;      /* Next node in list of peers */
    struct Parse_Tree_s    *next;   /* Next node in hashed list of names */
    struct Parse_Tree_s    *parent;
    char           *label;  /* This node's textual name */
    u_long          subid;  /* This node's integer subidentifier */
    int             modid;  /* The module containing this node */
    int             number_modules;
    int            *module_list;    /* To handle multiple modules */
    int             tc_index;       /* index into tclist (-1 if NA) */
    int             type;   /* This node's object type */
    int             access; /* This nodes access */
    int             status; /* This nodes status */
    struct Parse_EnumList_s *enums;        /* (optional) list of enumerated integers */
    struct Parse_RangeList_s *ranges;
    struct Parse_IndexList_s *indexes;
    char           *augments;
    struct Parse_VarbindList_s *varbinds;
    char           *hint;
    char           *units;
    int             (*printomat) (u_char **, size_t *, size_t *, int,
                                  const VariableList *,
                                  const struct Parse_EnumList_s *, const char *,
                                  const char *);
    void            (*printer) (char *, const VariableList *, const struct Parse_EnumList_s *, const char *, const char *);   /* Value printing function */
    char           *description;    /* description (a quoted string) */
    char           *reference;    /* references (a quoted string) */
    int             reported;       /* 1=report started in print_subtree... */
    char           *defaultValue;
   char	           *parseErrorString; /* Contains the error string if there are errors in parsing MIBs */
};

/*
 * Information held about each MIB module
 */
struct Parse_ModuleImport_s {
    char           *label;  /* The descriptor being imported */
    int             modid;  /* The module imported from */
};

struct Parse_Module_s {
    char           *name;   /* This module's name */
    char           *file;   /* The file containing the module */
    struct Parse_ModuleImport_s *imports;  /* List of descriptors being imported */
    int             no_imports;     /* The number of such import descriptors */
    /*
     * -1 implies the module hasn't been read in yet
     */
    int             modid;  /* The index number of this module */
    struct Parse_Module_s  *next;   /* Linked list pointer */
};

struct Parse_ModuleCompatability_s {
    const char     *old_module;
    const char     *new_module;
    const char     *tag;    /* NULL implies unconditional replacement,
                             * otherwise node identifier or prefix */
    size_t          tag_len;        /* 0 implies exact match (or unconditional) */
    struct Parse_ModuleCompatability_s *next;      /* linked list */
};


/*
 * non-aggregate types for tree end nodes
 */
#define PARSE_TYPE_OTHER          0
#define PARSE_TYPE_OBJID          1
#define PARSE_TYPE_OCTETSTR       2
#define PARSE_TYPE_INTEGER        3
#define PARSE_TYPE_NETADDR        4
#define PARSE_TYPE_IPADDR         5
#define PARSE_TYPE_COUNTER        6
#define PARSE_TYPE_GAUGE          7
#define PARSE_TYPE_TIMETICKS      8
#define PARSE_TYPE_OPAQUE         9
#define PARSE_TYPE_NULL           10
#define PARSE_TYPE_COUNTER64      11
#define PARSE_TYPE_BITSTRING      12
#define PARSE_TYPE_NSAPADDRESS    13
#define PARSE_TYPE_UINTEGER       14
#define PARSE_TYPE_UNSIGNED32     15
#define PARSE_TYPE_INTEGER32      16

#define PARSE_TYPE_SIMPLE_LAST    16

#define PARSE_TYPE_TRAPTYPE	      20
#define PARSE_TYPE_NOTIFTYPE      21
#define PARSE_TYPE_OBJGROUP	      22
#define PARSE_TYPE_NOTIFGROUP	  23
#define PARSE_TYPE_MODID	      24
#define PARSE_TYPE_AGENTCAP       25
#define PARSE_TYPE_MODCOMP        26
#define PARSE_TYPE_OBJIDENTITY    27

#define PARSE_MIB_ACCESS_READONLY    18
#define PARSE_MIB_ACCESS_READWRITE   19
#define	PARSE_MIB_ACCESS_WRITEONLY   20
#define PARSE_MIB_ACCESS_NOACCESS    21
#define PARSE_MIB_ACCESS_NOTIFY      67
#define PARSE_MIB_ACCESS_CREATE      48

#define PARSE_MIB_STATUS_MANDATORY   23
#define PARSE_MIB_STATUS_OPTIONAL    24
#define PARSE_MIB_STATUS_OBSOLETE    25
#define PARSE_MIB_STATUS_DEPRECATED  39
#define PARSE_MIB_STATUS_CURRENT     57

#define	PARSE_ANON	"anonymous#"
#define	PARSE_ANON_LEN  strlen(PARSE_ANON)

int             Parse_unloadModule(const char *name);

void            Parse_initMibInternals(void);
void            Parse_unloadAllMibs(void);
int             Parse_addMibfile(const char*, const char*, FILE *);
int             Parse_whichModule(const char *);

char *          Parse_moduleName(int, char *);

void            Parse_printSubtree(FILE *, struct Parse_Tree_s *, int);

void            Parse_printAsciiDumpTree(FILE *, struct Parse_Tree_s *, int);

struct Parse_Tree_s * Parse_findTreeNode(const char *, int);

const char *    Parse_getTcDescriptor(int);

const char *    Parse_getTcDescription(int);

struct Parse_Tree_s * Parse_findBestTreeNode(const char *, struct Parse_Tree_s *, u_int *);
/*
 * backwards compatability
 */

struct Parse_Tree_s * Parse_findNode(const char *, struct Parse_Tree_s *);
struct Parse_Tree_s * Parse_findNode2(const char *, const char *);

struct Parse_Module_s * Parse_findModule(int);
void            Parse_adoptOrphans(void);

char *          Parse_mibToggleOptions(char *options);

void            Parse_mibToggleOptionsUsage(const char *lead,
                                              FILE * outf);

void            Parse_printMib(FILE *);

void            Parse_printMibTree(FILE *, struct Parse_Tree_s *, int);
int             Parse_getMibParseErrorCount(void);

int             Parse_getToken(FILE * fp, char *token, int maxtlen);

struct Parse_Tree_s * Parse_findBestTreeNode(const char *name,
                                    struct Parse_Tree_s *tree_top,
                                    u_int * match);

struct Parse_Tree_s * Parse_readModule(const char *name);
struct Parse_Tree_s * Parse_readAllMibs(void);

int Parse_addMibdir(const char *dirname);

struct Parse_Tree_s    * Parse_readMib(const char *filename);

#endif // PARSE_H
