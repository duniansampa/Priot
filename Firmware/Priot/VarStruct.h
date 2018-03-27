#ifndef VARSTRUCT_H
#define VARSTRUCT_H

#include "Types.h"
#include "AgentHandler.h"
#include "Vars.h"
/*
 * The subtree structure contains a subtree prefix which applies to
 * all variables in the associated variable list.
 *
 * By converting to a tree of subtree structures, entries can
 * now be subtrees of another subtree in the structure. i.e:
 * 1.2
 * 1.2.0
 */

#define UCD_REGISTRY_OID_MAX_LEN	128

/*
 * subtree flags
 */
#define FULLY_QUALIFIED_INSTANCE    0x01
#define SUBTREE_ATTACHED	    	0x02

typedef struct Subtree_s {
    oid*                name_a;	/* objid prefix of registered subtree */
    u_char               namelen;    /* number of subid's in name above */
    oid*                start_a;	/* objid of start of covered range */
    u_char               start_len;  /* number of subid's in start name */
    oid*                end_a;	/* objid of end of covered range   */
    u_char               end_len;    /* number of subid's in end name */
    struct Variable_s*     variables; /* pointer to variables array */
    int                  variables_len;      /* number of entries in above array */
    int                  variables_width;    /* sizeof each variable entry */
    char*                label_a;	/* calling module's label */
    Types_Session*       session;
    u_char               flags;
    u_char               priority;
    int                  timeout;
    struct Subtree_s*    next;       /* List of 'sibling' subtrees */
    struct Subtree_s*    prev;       /* (doubly-linked list) */
    struct Subtree_s*    children;   /* List of 'child' subtrees */
    int                  range_subid;
    oid                 range_ubound;
    HandlerRegistration* reginfo;      /* new API */
    int                  cacheid;
    int                  global_cacheid;
    size_t               oid_off;
} Subtree;

/*
 * This is a new variable structure that doesn't have as much memory
 * tied up in the object identifier.  It's elements have also been re-arranged
 * so that the name field can be variable length.  Any number of these
 * structures can be created with lengths tailor made to a particular
 * application.  The first 5 elements of the structure must remain constant.
 */
struct variable1_s {
    u_char           magic;      /* passed to function as a hint */
    u_char           type;       /* type of variable */
    u_short          acl;        /* access control list for variable */
    FindVarMethodFT* findVar;    /* function that finds variable */
    u_char           namelen;    /* length of name below */
    oid             name[1];    /* object identifier of variable */
};

struct Variable2_s {
    u_char           magic;      /* passed to function as a hint */
    u_char           type;       /* type of variable */
    u_short          acl;        /* access control list for variable */
    FindVarMethodFT* findVar;    /* function that finds variable */
    u_char           namelen;    /* length of name below */
    oid             name[2];    /* object identifier of variable */
};

struct Variable3_s {
    u_char           magic;      /* passed to function as a hint */
    u_char           type;       /* type of variable */
    u_short          acl;        /* access control list for variable */
    FindVarMethodFT* findVar;    /* function that finds variable */
    u_char           namelen;    /* length of name below */
    oid             name[3];    /* object identifier of variable */
};

struct Variable4_s {
    u_char           magic;      /* passed to function as a hint */
    u_char           type;       /* type of variable */
    u_short          acl;        /* access control list for variable */
    FindVarMethodFT* findVar;    /* function that finds variable */
    u_char           namelen;    /* length of name below */
    oid             name[4];    /* object identifier of variable */
};

struct Variable7_s {
    u_char           magic;      /* passed to function as a hint */
    u_char           type;       /* type of variable */
    u_short          acl;        /* access control list for variable */
    FindVarMethodFT* findVar;    /* function that finds variable */
    u_char           namelen;    /* length of name below */
    oid             name[7];    /* object identifier of variable */
};

struct Variable8_s {
    u_char           magic;      /* passed to function as a hint */
    u_char           type;       /* type of variable */
    u_short          acl;        /* access control list for variable */
    FindVarMethodFT* findVar;    /* function that finds variable */
    u_char           namelen;    /* length of name below */
    oid             name[8];    /* object identifier of variable */
};

struct Variable13_s {
    u_char           magic;      /* passed to function as a hint */
    u_char           type;       /* type of variable */
    u_short          acl;        /* access control list for variable */
    FindVarMethodFT* findVar;    /* function that finds variable */
    u_char           namelen;    /* length of name below */
    oid             name[13];   /* object identifier of variable */
};

#endif // VARSTRUCT_H
