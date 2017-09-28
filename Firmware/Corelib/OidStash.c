#include "OidStash.h"
#include "Tools.h"
#include "Debug.h"
#include "DefaultStore.h"
#include "ReadConfig.h"
#include "Types.h"
#include "Priot.h"

/** @defgroup oid_stash Store and retrieve data referenced by an OID.
    This is essentially a way of storing data associated with a given
    OID.  It stores a bunch of data pointers within a memory tree that
    allows fairly efficient lookups with a heavily populated tree.
    @ingroup library
    @{
*/

/*
 * xxx-rks: when you have some spare time:
 *
 * b) basically, everything currently creates one node per sub-oid,
 *    which is less than optimal. add code to create nodes with the
 *    longest possible OID per node, and split nodes when necessary
 *    during adds.
 *
 * c) If you are feeling really ambitious, also merge split nodes if
 *    possible on a delete.
 *
 * xxx-wes: uh, right, like I *ever* have that much time.
 *
 */


/***************************************************************************
 *
 *
 ***************************************************************************/

/**
 * Create an netsnmp_oid_stash node
 *
 * @param mysize  the size of the child pointer array
 *
 * @return NULL on error, otherwise the newly allocated node
 */

OidStash_Node *
OidStash_createSizedNode(size_t mysize)
{
    OidStash_Node *ret;
    ret = TOOLS_MALLOC_TYPEDEF(OidStash_Node);
    if (!ret)
        return NULL;
    ret->children = (OidStash_Node**) calloc(mysize, sizeof(OidStash_Node *));
    if (!ret->children) {
        free(ret);
        return NULL;
    }
    ret->childrenSize = mysize;
    return ret;
}

/** Creates a OidStash_Node.
 * Assumes you want the default OID_STASH_CHILDREN_SIZE hash size for the node.
 * @return NULL on error, otherwise the newly allocated node
 */
 OidStash_Node *
OidStash_createNode(void)
{
    return OidStash_createSizedNode(OIDSTASH_CHILDREN_SIZE);
}


/** adds data to the stash at a given oid.

 * @param root the top of the stash tree
 * @param lookup the oid index to store the data at.
 * @param lookup_len the length of the lookup oid.
 * @param mydata the data to store

 * @return ErrorCode_SUCCESS on success, ErrorCode_GENERR if data is
   already there, SNMPERR_MALLOC on malloc failures or if arguments
   passed in with NULL values.
 */
int
OidStash_addData(OidStash_Node **root,
                           const oid * lookup, size_t lookup_len, void *mydata)
{
    OidStash_Node *curnode, *tmpp, *loopp;
    unsigned int    i;

    if (!root || !lookup || lookup_len == 0)
        return ErrorCode_GENERR;

    if (!*root) {
        *root = OidStash_createNode();
        if (!*root)
            return ErrorCode_MALLOC;
    }
    DEBUG_MSGTL(( "oid_stash", "stash_add_data "));
    DEBUG_MSGOID(("oid_stash", lookup, lookup_len));
    DEBUG_MSG((   "oid_stash", "\n"));
    tmpp = NULL;
    for (curnode = *root, i = 0; i < lookup_len; i++) {
        tmpp = curnode->children[lookup[i] % curnode->childrenSize];
        if (!tmpp) {
            /*
             * no child in array at all
             */
            tmpp = curnode->children[lookup[i] % curnode->childrenSize] =
                OidStash_createNode();
            tmpp->value = lookup[i];
            tmpp->parent = curnode;
        } else {
            for (loopp = tmpp; loopp; loopp = loopp->nextSibling) {
                if (loopp->value == lookup[i])
                    break;
            }
            if (loopp) {
                tmpp = loopp;
            } else {
                /*
                 * none exists.  Create it
                 */
                loopp = OidStash_createNode();
                loopp->value = lookup[i];
                loopp->nextSibling = tmpp;
                loopp->parent = curnode;
                tmpp->prevSibling = loopp;
                curnode->children[lookup[i] % curnode->childrenSize] =
                    loopp;
                tmpp = loopp;
            }
            /*
             * tmpp now points to the proper node
             */
        }
        curnode = tmpp;
    }
    /*
     * tmpp now points to the exact match
     */
    if (curnode->thedata)
        return ErrorCode_GENERR;
    if (NULL == tmpp)
        return ErrorCode_GENERR;
    tmpp->thedata = mydata;
    return ErrorCode_SUCCESS;
}

/** returns a node associated with a given OID.
 * @param root the top of the stash tree
 * @param lookup the oid to look up a node for.
 * @param lookup_len the length of the lookup oid
 */
OidStash_Node *
OidStash_stashGetNode(OidStash_Node *root,
                           const oid * lookup, size_t lookup_len)
{
    OidStash_Node *curnode, *tmpp, *loopp;
    unsigned int    i;

    if (!root)
        return NULL;
    tmpp = NULL;
    for (curnode = root, i = 0; i < lookup_len; i++) {
        tmpp = curnode->children[lookup[i] % curnode->childrenSize];
        if (!tmpp) {
            return NULL;
        } else {
            for (loopp = tmpp; loopp; loopp = loopp->nextSibling) {
                if (loopp->value == lookup[i])
                    break;
            }
            if (loopp) {
                tmpp = loopp;
            } else {
                return NULL;
            }
        }
        curnode = tmpp;
    }
    return tmpp;
}

/** returns the next node associated with a given OID. INCOMPLETE.
    This is equivelent to a GETNEXT operation.
 * @internal
 * @param root the top of the stash tree
 * @param lookup the oid to look up a node for.
 * @param lookup_len the length of the lookup oid
 */
OidStash_Node *
OidStash_stashGetnextNode(OidStash_Node *root,
                               oid * lookup, size_t lookup_len)
{
    OidStash_Node *curnode, *tmpp, *loopp;
    unsigned int    i, j, bigger_than = 0, do_bigger = 0;

    if (!root)
        return NULL;
    tmpp = NULL;

    /* get closest matching node */
    for (curnode = root, i = 0; i < lookup_len; i++) {
        tmpp = curnode->children[lookup[i] % curnode->childrenSize];
        if (!tmpp) {
            break;
        } else {
            for (loopp = tmpp; loopp; loopp = loopp->nextSibling) {
                if (loopp->value == lookup[i])
                    break;
            }
            if (loopp) {
                tmpp = loopp;
            } else {
                break;
            }
        }
        curnode = tmpp;
    }

    /* find the *next* node lexographically greater */
    if (!curnode)
        return NULL; /* ack! */

    if (i+1 < lookup_len) {
        bigger_than = lookup[i+1];
        do_bigger = 1;
    }

    do {
        /* check the children first */
        tmpp = NULL;
        /* next child must be (next) greater than our next search node */
        /* XXX: should start this loop at best_nums[i]%... and wrap */
        for(j = 0; j < curnode->childrenSize; j++) {
            for (loopp = curnode->children[j];
                 loopp; loopp = loopp->nextSibling) {
                if ((!do_bigger || loopp->value > bigger_than) &&
                    (!tmpp || tmpp->value > loopp->value)) {
                    tmpp = loopp;
                    /* XXX: can do better and include min_nums[i] */
                    if (tmpp->value <= curnode->childrenSize-1) {
                        /* best we can do. */
                        goto done_this_loop;
                    }
                }
            }
        }

      done_this_loop:
        if (tmpp && tmpp->thedata)
            /* found a node with data.  Go with it. */
            return tmpp;

        if (tmpp) {
            /* found a child node without data, maybe find a grandchild? */
            do_bigger = 0;
            curnode = tmpp;
        } else {
            /* no respectable children (the bums), we'll have to go up.
               But to do so, they must be better than our current best_num + 1.
            */
            do_bigger = 1;
            bigger_than = curnode->value;
            curnode = curnode->parent;
        }
    } while (curnode);

    /* fell off the top */
    return NULL;
}

/** returns a data pointer associated with a given OID.

    This is equivelent to netsnmp_oid_stash_get_node, but returns only
    the data not the entire node.

 * @param root the top of the stash
 * @param lookup the oid to search for
 * @param lookup_len the length of the search oid.
 */
void *
OidStash_stashGetData(OidStash_Node *root,
                           const oid * lookup, size_t lookup_len)
{
    OidStash_Node *ret;
    ret = OidStash_stashGetNode(root, lookup, lookup_len);
    if (ret)
        return ret->thedata;
    return NULL;
}


/** a wrapper around netsnmp_oid_stash_store for use with a snmp_alarm.
 * when calling snmp_alarm, you can list this as a callback.  The
 * clientarg should be a pointer to a netsnmp_oid_stash_save_info
 * pointer.  It can also be called directly, of course.  The last
 * argument (clientarg) is the only one that is used.  The rest are
 * ignored by the function.
 * @param majorID
 * @param minorID
 * @param serverarg
 * @param clientarg A pointer to a netsnmp_oid_stash_save_info structure.
 */
int
OidStash_storeAll(int majorID, int minorID,
                            void *serverarg, void *clientarg) {
    oid oidbase[TYPES_MAX_OID_LEN];
    OidStash_SaveInfo *sinfo;

    if (!clientarg)
        return PRIOT_ERR_NOERROR;

    sinfo = (OidStash_SaveInfo *) clientarg;
    OidStash_store(*(sinfo->root), sinfo->token, sinfo->dumpfn,
                            oidbase,0);
    return PRIOT_ERR_NOERROR;
}

/** stores data in a starsh tree to peristent storage.

    This function can be called to save all data in a stash tree to
    Net-SNMP's percent storage.  Make sure you register a parsing
    function with the read_config system to re-incorperate your saved
    data into future trees.

    @param root the top of the stash to store.
    @param tokenname the file token name to save in (passing "snmpd" will
    save things into snmpd.conf).
    @param dumpfn A function which can dump the data stored at a particular
    node into a char buffer.
    @param curoid must be a pointer to a OID array of length MAX_OID_LEN.
    @param curoid_len must be 0 for the top level call.
*/
void
OidStash_store(OidStash_Node *root,
                        const char *tokenname, OidStash_FuncStashDump *dumpfn,
                        oid *curoid, size_t curoid_len) {

    char buf[TOOLS_MAXBUF];
    OidStash_Node *tmpp;
    char *cp;
    char *appname = DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                                          DSLIB_STRING.APPTYPE);
    int i;

    if (!tokenname || !root || !curoid || !dumpfn)
        return;

    for (i = 0; i < (int)root->childrenSize; i++) {
        if (root->children[i]) {
            for (tmpp = root->children[i]; tmpp; tmpp = tmpp->nextSibling) {
                curoid[curoid_len] = tmpp->value;
                if (tmpp->thedata) {
                    snprintf(buf, sizeof(buf), "%s ", tokenname);
                    cp = ReadConfig_saveObjid(buf+strlen(buf), curoid,
                                                curoid_len+1);
                    *cp++ = ' ';
                    *cp = '\0';
                    if ((*dumpfn)(cp, sizeof(buf) - strlen(buf),
                                  tmpp->thedata, tmpp))
                        ReadConfig_store(appname, buf);
                }
                OidStash_store(tmpp, tokenname, dumpfn,
                                        curoid, curoid_len+1);
            }
        }
    }
}

/** For debugging: dump the netsnmp_oid_stash tree to stdout
    @param root The top of the tree
    @param prefix a character string prefix printed to the beginning of each line.
*/
void OidStash_dump(OidStash_Node *root, char *prefix)
{
    char            myprefix[TYPES_MAX_OID_LEN * 4];
    OidStash_Node *tmpp;
    int             prefix_len = strlen(prefix) + 1;    /* actually it's +2 */
    unsigned int    i;

    memset(myprefix, ' ', TYPES_MAX_OID_LEN * 4);
    myprefix[prefix_len] = '\0';

    for (i = 0; i < root->childrenSize; i++) {
        if (root->children[i]) {
            for (tmpp = root->children[i]; tmpp; tmpp = tmpp->nextSibling) {
                printf("%s%" "l" "d@%d: %s\n", prefix, tmpp->value, i,
                       (tmpp->thedata) ? "DATA" : "");
                OidStash_dump(tmpp, myprefix);
            }
        }
    }
}

/** Frees the contents of a netsnmp_oid_stash tree.
    @param root the top of the tree (or branch to be freed)
    @param freefn The function to be called on each data (void *)
    pointer.  If left NULL the system free() function will be called
*/
void
OidStash_free(OidStash_Node **root,
                       OidStash_FuncStashFreeNode *freefn) {

    OidStash_Node *curnode, *tmpp;
    unsigned int    i;

    if (!root || !*root)
        return;

    /* loop through all our children and free each node */
    for (i = 0; i < (*root)->childrenSize; i++) {
        if ((*root)->children[i]) {
            for(tmpp = (*root)->children[i]; tmpp; tmpp = curnode) {
                if (tmpp->thedata) {
                    if (freefn)
                        (*freefn)(tmpp->thedata);
                    else
                        free(tmpp->thedata);
                }
                curnode = tmpp->nextSibling;
                OidStash_free(&tmpp, freefn);
            }
        }
    }
    free((*root)->children);
    free (*root);
    *root = NULL;
}


void
OidStash_noFree(void *bogus)
{
    /* noop */
}
