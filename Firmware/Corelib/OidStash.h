#ifndef OIDSTASH_H
#define OIDSTASH_H

#include "Generals.h"
#include "Callback.h"

/*
 * designed to store/retrieve information associated with a given oid.
 * Storage is done in an efficient tree manner for fast lookups.
 */

#define OIDSTASH_CHILDREN_SIZE 31


/* args: buffer, sizeof(buffer), yourdata, stashnode */
typedef int     (OidStash_FuncStashDump) (char *, size_t,
                                    void *,
                                    struct OidStash_Node_s *);

typedef void    (OidStash_FuncStashFreeNode) (void *);

typedef struct OidStash_Node_s {
    _oid             value;
    struct OidStash_Node_s **children;     /* array of children */
    size_t childrenSize;
    struct OidStash_Node_s *nextSibling;  /* cache too small links */
    struct OidStash_Node_s *prevSibling;
    struct OidStash_Node_s *parent;

    void *thedata;
} OidStash_Node;

typedef struct OidStash_SaveInfo_s {
   const char *token;
   OidStash_Node **root;
   OidStash_FuncStashDump *dumpfn;
} OidStash_SaveInfo;


int  OidStash_addData(OidStash_Node **root, const _oid * lookup,
                                 size_t lookup_len,
                                 void *mydata);

Callback_CallbackFT OidStash_storeAll;


OidStash_Node
    *OidStash_getNode(OidStash_Node *root,
                      const _oid * lookup,
                      size_t lookup_len);

void           *OidStash_getData(OidStash_Node *root,
                                 const _oid * lookup,
                                 size_t lookup_len);

OidStash_Node *
OidStash_getnextNode(OidStash_Node *root,
                               _oid * lookup, size_t lookup_len);

OidStash_Node *OidStash_createSizedNode(size_t
                                                            mysize);
OidStash_Node *OidStash_createNode(void);        /* returns a malloced node */

void OidStash_store(OidStash_Node *root,
                             const char *tokenname,
                             OidStash_FuncStashDump *dumpfn,
                             _oid *curoid, size_t curoid_len);

/* frees all data in the stash and cleans it out.  Sets root = NULL */

void OidStash_free(OidStash_Node **root,
                            OidStash_FuncStashFreeNode *freefn);


/* a noop function that can be passed to OidStash_Node to
   NOT free the data */
OidStash_FuncStashFreeNode OidStash_noFree;


#endif // OIDSTASH_H
