#include "AgentIndex.h"

#include "System/Util/DefaultStore.h"
#include "System/Util/Utilities.h"
#include "Client.h"
#include "Priot.h"
#include "Api.h"
#include "Mib.h"
#include "PriotSettings.h"
#include "DsAgent.h"
#include "System/Util/Utilities.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "Agentx/XClient.h"

/*
* agent_index.c
*
* Maintain a registry of index allocations
*      (Primarily required for AgentX support,
*       but it could be more widely useable).
*/

/*
 * Initial support for index allocation
 */

struct Index_s {
    VariableList* varbind;     /* or pointer to var_list ? */
    int                 allocated;
    Types_Session*      session;
    struct Index_s*     next_oid;
    struct Index_s*     prev_oid;
    struct Index_s*     next_idx;
} *agentIndex_indexHead = NULL;

extern Types_Session *agent_mainSession;

/*
 * The caller is responsible for free()ing the memory returned by
 * this function.
 */

char*
AgentIndex_registerStringIndex( oid*  name,
                                size_t name_len,
                                char*  cp )
{
    VariableList varbind, *res;

    memset(&varbind, 0, sizeof(VariableList));
    varbind.type = asnOCTET_STR;
    Client_setVarObjid(&varbind, name, name_len);
    if (cp != ANY_STRING_INDEX) {
        Client_setVarValue(&varbind, (u_char *) cp, strlen(cp));
        res = AgentIndex_registerIndex(&varbind, ALLOCATE_THIS_INDEX, agent_mainSession);
    } else {
        res = AgentIndex_registerIndex(&varbind, ALLOCATE_ANY_INDEX, agent_mainSession);
    }

    if (res == NULL) {
        return NULL;
    } else {
        char *rv = (char *)malloc(res->valueLength + 1);
        if (rv) {
            memcpy(rv, res->value.string, res->valueLength);
            rv[res->valueLength] = 0;
        }
        free(res);
        return rv;
    }
}

int
AgentIndex_registerIntIndex( oid*  name,
                             size_t name_len,
                             int    val )
{
    VariableList varbind, *res;

    memset(&varbind, 0, sizeof(VariableList));
    varbind.type = asnINTEGER;
    Client_setVarObjid(&varbind, name, name_len);
    varbind.value.string = varbind.buffer;
    if (val != ANY_INTEGER_INDEX) {
        varbind.valueLength = sizeof(long);
        *varbind.value.integer = val;
        res = AgentIndex_registerIndex(&varbind, ALLOCATE_THIS_INDEX, agent_mainSession);
    } else {
        res = AgentIndex_registerIndex(&varbind, ALLOCATE_ANY_INDEX, agent_mainSession);
    }

    if (res == NULL) {
        return -1;
    } else {
        int             rv = *(res->value.integer);
        free(res);
        return rv;
    }
}

/*
 * The caller is responsible for free()ing the memory returned by
 * this function.
 */

VariableList*
AgentIndex_registerOidIndex( oid*  name,
                             size_t name_len,
                             oid*  value,
                             size_t value_len )
{
    VariableList varbind;

    memset(&varbind, 0, sizeof(VariableList));
    varbind.type = asnOBJECT_ID;
    Client_setVarObjid(&varbind, name, name_len);
    if (value != ANY_OID_INDEX) {
        Client_setVarValue(&varbind, (u_char *) value,
                           value_len * sizeof(oid));
        return AgentIndex_registerIndex(&varbind, ALLOCATE_THIS_INDEX, agent_mainSession);
    } else {
        return AgentIndex_registerIndex(&varbind, ALLOCATE_ANY_INDEX, agent_mainSession);
    }
}

/*
 * The caller is responsible for free()ing the memory returned by
 * this function.
 */

VariableList*
AgentIndex_registerIndex( VariableList* varbind,
                          int                 flags,
                          Types_Session*      ss )
{
    VariableList *rv = NULL;
    struct Index_s *new_index, *idxptr, *idxptr2;
    struct Index_s *prev_oid_ptr, *prev_idx_ptr;
    int             res, res2, i;

    DEBUG_MSGTL(("AgentIndex_registerIndex", "register "));
    DEBUG_MSGVAR(("AgentIndex_registerIndex", varbind));
    DEBUG_MSG(("AgentIndex_registerIndex", "for session %8p\n", ss));

    if (DefaultStore_getBoolean(DsStore_APPLICATION_ID,
                   DsAgentBoolean_ROLE) == SUB_AGENT) {
        return (XClient_registerIndex(ss, varbind, flags));
    }
    /*
     * Look for the requested OID entry
     */
    prev_oid_ptr = NULL;
    prev_idx_ptr = NULL;
    res = 1;
    res2 = 1;
    for (idxptr = agentIndex_indexHead; idxptr != NULL;
         prev_oid_ptr = idxptr, idxptr = idxptr->next_oid) {
        if ((res = Api_oidCompare(varbind->name, varbind->nameLength,
                                    idxptr->varbind->name,
                                    idxptr->varbind->nameLength)) <= 0)
            break;
    }

    /*
     * Found the OID - now look at the registered indices
     */
    if (res == 0 && idxptr) {
        if (varbind->type != idxptr->varbind->type)
            return NULL;        /* wrong type */

        /*
         * If we've been asked for an arbitrary new value,
         *      then find the end of the list.
         * If we've been asked for any arbitrary value,
         *      then look for an unused entry, and use that.
         *      If there aren't any, continue as for new.
         * Otherwise, locate the given value in the (sorted)
         *      list of already allocated values
         */
        if (flags & ALLOCATE_ANY_INDEX) {
            for (idxptr2 = idxptr; idxptr2 != NULL;
                 prev_idx_ptr = idxptr2, idxptr2 = idxptr2->next_idx) {

                if (flags == ALLOCATE_ANY_INDEX && !(idxptr2->allocated)) {
                    if ((rv =
                         Client_cloneVarbind(idxptr2->varbind)) != NULL) {
                        idxptr2->session = ss;
                        idxptr2->allocated = 1;
                    }
                    return rv;
                }
            }
        } else {
            for (idxptr2 = idxptr; idxptr2 != NULL;
                 prev_idx_ptr = idxptr2, idxptr2 = idxptr2->next_idx) {
                switch (varbind->type) {
                case asnINTEGER:
                    res2 =
                        (*varbind->value.integer -
                         *idxptr2->varbind->value.integer);
                    break;
                case asnOCTET_STR:
                    i = UTILITIES_MIN_VALUE(varbind->valueLength,
                                 idxptr2->varbind->valueLength);
                    res2 =
                        memcmp(varbind->value.string,
                               idxptr2->varbind->value.string, i);
                    break;
                case asnOBJECT_ID:
                    res2 =
                        Api_oidCompare(varbind->value.objectId,
                                         varbind->valueLength / sizeof(oid),
                                         idxptr2->varbind->value.objectId,
                                         idxptr2->varbind->valueLength /
                                         sizeof(oid));
                    break;
                default:
                    return NULL;        /* wrong type */
                }
                if (res2 <= 0)
                    break;
            }
            if (res2 == 0) {
                if (idxptr2->allocated) {
                    /*
                     * No good: the index is in use.
                     */
                    return NULL;
                } else {
                    /*
                     * Okay, it's unallocated, we can just claim ownership
                     * here.
                     */
                    if ((rv =
                         Client_cloneVarbind(idxptr2->varbind)) != NULL) {
                        idxptr2->session = ss;
                        idxptr2->allocated = 1;
                    }
                    return rv;
                }
            }
        }
    }

    /*
     * OK - we've now located where the new entry needs to
     *      be fitted into the index registry tree
     * To recap:
     *      'prev_oid_ptr' points to the head of the OID index
     *          list prior to this one.  If this is null, then
     *          it means that this is the first OID in the list.
     *      'idxptr' points either to the head of this OID list,
     *          or the next OID (if this is a new OID request)
     *          These can be distinguished by the value of 'res'.
     *
     *      'prev_idx_ptr' points to the index entry that sorts
     *          immediately prior to the requested value (if any).
     *          If an arbitrary value is required, then this will
     *          point to the last allocated index.
     *          If this pointer is null, then either this is a new
     *          OID request, or the requested value is the first
     *          in the list.
     *      'idxptr2' points to the next sorted index (if any)
     *          but is not actually needed any more.
     *
     *  Clear?  Good!
     *      I hope you've been paying attention.
     *          There'll be a test later :-)
     */

    /*
     *      We proceed by creating the new entry
     *         (by copying the entry provided)
     */
    new_index = (struct Index_s *) calloc(1, sizeof(struct Index_s));
    if (new_index == NULL)
        return NULL;

    if (NULL == Api_varlistAddVariable(&new_index->varbind,
                                          varbind->name,
                                          varbind->nameLength,
                                          varbind->type,
                                          varbind->value.string,
                                          varbind->valueLength)) {
        /*
         * if (Client_cloneVar( varbind, new_index->varbind ) != 0 )
         */
        free(new_index);
        return NULL;
    }
    new_index->session = ss;
    new_index->allocated = 1;

    if (varbind->type == asnOCTET_STR && flags == ALLOCATE_THIS_INDEX)
        new_index->varbind->value.string[new_index->varbind->valueLength] = 0;

    /*
     * If we've been given a value, then we can use that, but
     *    otherwise, we need to create a new value for this entry.
     * Note that ANY_INDEX and NEW_INDEX are both covered by this
     *   test (since NEW_INDEX & ANY_INDEX = ANY_INDEX, remember?)
     */
    if (flags & ALLOCATE_ANY_INDEX) {
        if (prev_idx_ptr) {
            if (Client_cloneVar(prev_idx_ptr->varbind, new_index->varbind)
                != 0) {
                free(new_index);
                return NULL;
            }
        } else
            new_index->varbind->value.string = new_index->varbind->buffer;

        switch (varbind->type) {
        case asnINTEGER:
            if (prev_idx_ptr) {
                (*new_index->varbind->value.integer)++;
            } else
                *(new_index->varbind->value.integer) = 1;
            new_index->varbind->valueLength = sizeof(long);
            break;
        case asnOCTET_STR:
            if (prev_idx_ptr) {
                i = new_index->varbind->valueLength - 1;
                while (new_index->varbind->buffer[i] == 'z') {
                    new_index->varbind->buffer[i] = 'a';
                    i--;
                    if (i < 0) {
                        i = new_index->varbind->valueLength;
                        new_index->varbind->buffer[i] = 'a';
                        new_index->varbind->buffer[i + 1] = 0;
                    }
                }
                new_index->varbind->buffer[i]++;
            } else
                strcpy((char *) new_index->varbind->buffer, "aaaa");
            new_index->varbind->valueLength =
                strlen((char *) new_index->varbind->buffer);
            break;
        case asnOBJECT_ID:
            if (prev_idx_ptr) {
                i = prev_idx_ptr->varbind->valueLength / sizeof(oid) - 1;
                while (new_index->varbind->value.objectId[i] == 255) {
                    new_index->varbind->value.objectId[i] = 1;
                    i--;
                    if (i == 0 && new_index->varbind->value.objectId[0] == 2) {
                        new_index->varbind->value.objectId[0] = 1;
                        i = new_index->varbind->valueLength / sizeof(oid);
                        new_index->varbind->value.objectId[i] = 0;
                        new_index->varbind->valueLength += sizeof(oid);
                    }
                }
                new_index->varbind->value.objectId[i]++;
            } else {
                /*
                 * If the requested OID name is small enough,
                 * *   append another OID (1) and use this as the
                 * *   default starting value for new indexes.
                 */
                if ((varbind->nameLength + 1) * sizeof(oid) <= 40) {
                    for (i = 0; i < (int) varbind->nameLength; i++)
                        new_index->varbind->value.objectId[i] =
                            varbind->name[i];
                    new_index->varbind->value.objectId[varbind->nameLength] =
                        1;
                    new_index->varbind->valueLength =
                        (varbind->nameLength + 1) * sizeof(oid);
                } else {
                    /*
                     * Otherwise use '.1.1.1.1...'
                     */
                    i = 40 / sizeof(oid);
                    if (i > 4)
                        i = 4;
                    new_index->varbind->valueLength = i * (sizeof(oid));
                    for (i--; i >= 0; i--)
                        new_index->varbind->value.objectId[i] = 1;
                }
            }
            break;
        default:
            Api_freeVar(new_index->varbind);
            free(new_index);
            return NULL;        /* Index type not supported */
        }
    }

    /*
     * Try to duplicate the new varbind for return.
     */

    if ((rv = Client_cloneVarbind(new_index->varbind)) == NULL) {
        Api_freeVar(new_index->varbind);
        free(new_index);
        return NULL;
    }

    /*
     * Right - we've set up the new entry.
     * All that remains is to link it into the tree.
     * There are a number of possible cases here,
     *   so watch carefully.
     */
    if (prev_idx_ptr) {
        new_index->next_idx = prev_idx_ptr->next_idx;
        new_index->next_oid = prev_idx_ptr->next_oid;
        prev_idx_ptr->next_idx = new_index;
    } else {
        if (res == 0 && idxptr) {
            new_index->next_idx = idxptr;
            new_index->next_oid = idxptr->next_oid;
        } else {
            new_index->next_idx = NULL;
            new_index->next_oid = idxptr;
        }

        if (prev_oid_ptr) {
            while (prev_oid_ptr) {
                prev_oid_ptr->next_oid = new_index;
                prev_oid_ptr = prev_oid_ptr->next_idx;
            }
        } else
            agentIndex_indexHead = new_index;
    }
    return rv;
}

/*
 * Release an allocated index,
 *   to allow it to be used elsewhere
 */
int
AgentIndex_releaseIndex( VariableList* varbind )
{
    return (AgentIndex_unregisterIndex(varbind, TRUE, NULL));
}

/*
 * Completely remove an allocated index,
 *   due to errors in the registration process.
 */
int
AgentIndex_removeIndex( VariableList* varbind,
                        Types_Session*      ss )
{
    return (AgentIndex_unregisterIndex(varbind, FALSE, ss));
}

void
AgentIndex_unregisterIndexBySession( Types_Session * ss )
{
    struct Index_s *idxptr, *idxptr2;
    for (idxptr = agentIndex_indexHead; idxptr != NULL;
         idxptr = idxptr->next_oid)
        for (idxptr2 = idxptr; idxptr2 != NULL;
             idxptr2 = idxptr2->next_idx)
            if (idxptr2->session == ss) {
                idxptr2->allocated = 0;
                idxptr2->session = NULL;
            }
}


int
AgentIndex_unregisterIndex( VariableList* varbind,
                            int                 remember,
                            Types_Session*      ss )
{
    struct Index_s *idxptr, *idxptr2;
    struct Index_s *prev_oid_ptr, *prev_idx_ptr;
    int             res, res2, i;

    if (DefaultStore_getBoolean(DsStore_APPLICATION_ID,
                   DsAgentBoolean_ROLE) == SUB_AGENT) {
        return (XClient_unregisterIndex(ss, varbind));
    }
    /*
     * Look for the requested OID entry
     */
    prev_oid_ptr = NULL;
    prev_idx_ptr = NULL;
    res = 1;
    res2 = 1;
    for (idxptr = agentIndex_indexHead; idxptr != NULL;
         prev_oid_ptr = idxptr, idxptr = idxptr->next_oid) {
        if ((res = Api_oidCompare(varbind->name, varbind->nameLength,
                                    idxptr->varbind->name,
                                    idxptr->varbind->nameLength)) <= 0)
            break;
    }

    if (res != 0)
        return INDEX_ERR_NOT_ALLOCATED;
    if (varbind->type != idxptr->varbind->type)
        return INDEX_ERR_WRONG_TYPE;

    for (idxptr2 = idxptr; idxptr2 != NULL;
         prev_idx_ptr = idxptr2, idxptr2 = idxptr2->next_idx) {
        switch (varbind->type) {
        case asnINTEGER:
            res2 =
                (*varbind->value.integer -
                 *idxptr2->varbind->value.integer);
            break;
        case asnOCTET_STR:
            i = UTILITIES_MIN_VALUE(varbind->valueLength,
                         idxptr2->varbind->valueLength);
            res2 =
                memcmp(varbind->value.string,
                       idxptr2->varbind->value.string, i);
            break;
        case asnOBJECT_ID:
            res2 =
                Api_oidCompare(varbind->value.objectId,
                                 varbind->valueLength / sizeof(oid),
                                 idxptr2->varbind->value.objectId,
                                 idxptr2->varbind->valueLength /
                                 sizeof(oid));
            break;
        default:
            return INDEX_ERR_WRONG_TYPE;        /* wrong type */
        }
        if (res2 <= 0)
            break;
    }
    if (res2 != 0 || (res2 == 0 && !idxptr2->allocated)) {
        return INDEX_ERR_NOT_ALLOCATED;
    }
    if (ss != idxptr2->session)
        return INDEX_ERR_WRONG_SESSION;

    /*
     *  If this is a "normal" index unregistration,
     *      mark the index entry as unused, but leave
     *      it in situ.  This allows differentiation
     *      between ANY_INDEX and NEW_INDEX
     */
    if (remember) {
        idxptr2->allocated = 0; /* Unused index */
        idxptr2->session = NULL;
        return PRIOT_ERR_NOERROR;
    }
    /*
     *  If this is a failed attempt to register a
     *      number of indexes, the successful ones
     *      must be removed completely.
     */
    if (prev_idx_ptr) {
        prev_idx_ptr->next_idx = idxptr2->next_idx;
    } else if (prev_oid_ptr) {
        if (idxptr2->next_idx)  /* Use p_idx_ptr as a temp variable */
            prev_idx_ptr = idxptr2->next_idx;
        else
            prev_idx_ptr = idxptr2->next_oid;
        while (prev_oid_ptr) {
            prev_oid_ptr->next_oid = prev_idx_ptr;
            prev_oid_ptr = prev_oid_ptr->next_idx;
        }
    } else {
        if (idxptr2->next_idx)
            agentIndex_indexHead = idxptr2->next_idx;
        else
            agentIndex_indexHead = idxptr2->next_oid;
    }
    Api_freeVar(idxptr2->varbind);
    free(idxptr2);
    return PRIOT_ERR_NOERROR;
}

int
AgentIndex_unregisterStringIndex( oid*  name,
                                  size_t name_len,
                                  char*  cp )
{
    VariableList varbind;

    memset(&varbind, 0, sizeof(VariableList));
    varbind.type = asnOCTET_STR;
    Client_setVarObjid(&varbind, name, name_len);
    Client_setVarValue(&varbind, (u_char *) cp, strlen(cp));
    return (AgentIndex_unregisterIndex(&varbind, FALSE, agent_mainSession));
}

int
AgentIndex_unregisterIntIndex( oid*  name,
                               size_t name_len,
                               int    val )
{
    VariableList varbind;

    memset(&varbind, 0, sizeof(VariableList));
    varbind.type = asnINTEGER;
    Client_setVarObjid(&varbind, name, name_len);
    varbind.value.string = varbind.buffer;
    varbind.valueLength = sizeof(long);
    *varbind.value.integer = val;
    return (AgentIndex_unregisterIndex(&varbind, FALSE, agent_mainSession));
}

int
AgentIndex_unregisterOidIndex( oid*  name,
                               size_t name_len,
                               oid*  value,
                               size_t value_len )
{
    VariableList varbind;

    memset(&varbind, 0, sizeof(VariableList));
    varbind.type = asnOBJECT_ID;
    Client_setVarObjid(&varbind, name, name_len);
    Client_setVarValue(&varbind, (u_char *) value,
                       value_len * sizeof(oid));
    return (AgentIndex_unregisterIndex(&varbind, FALSE, agent_mainSession));
}

void
AgentIndex_dumpIdxRegistry( void )
{
    struct Index_s *idxptr, *idxptr2;
    u_char         *sbuf = NULL, *ebuf = NULL;
    size_t          sbuf_len = 0, sout_len = 0, ebuf_len = 0, eout_len = 0;

    if (agentIndex_indexHead != NULL) {
        printf("\nIndex Allocations:\n");
    }

    for (idxptr = agentIndex_indexHead; idxptr != NULL;
         idxptr = idxptr->next_oid) {
        sout_len = 0;
        if (Mib_sprintReallocObjid2(&sbuf, &sbuf_len, &sout_len, 1,
                                 idxptr->varbind->name,
                                 idxptr->varbind->nameLength)) {
            printf("%s indexes:\n", sbuf);
        } else {
            printf("%s [TRUNCATED] indexes:\n", sbuf);
        }

        for (idxptr2 = idxptr; idxptr2 != NULL;
             idxptr2 = idxptr2->next_idx) {
            switch (idxptr2->varbind->type) {
            case asnINTEGER:
                printf("    %ld for session %8p, allocated %d\n",
                       *idxptr2->varbind->value.integer, idxptr2->session,
                       idxptr2->allocated);
                break;
            case asnOCTET_STR:
                printf("    \"%s\" for session %8p, allocated %d\n",
                       idxptr2->varbind->value.string, idxptr2->session,
                       idxptr2->allocated);
                break;
            case asnOBJECT_ID:
                eout_len = 0;
                if (Mib_sprintReallocObjid2(&ebuf, &ebuf_len, &eout_len, 1,
                                         idxptr2->varbind->value.objectId,
                                         idxptr2->varbind->valueLength /
                                         sizeof(oid))) {
                    printf("    %s for session %8p, allocated %d\n", ebuf,
                           idxptr2->session, idxptr2->allocated);
                } else {
                    printf
                        ("    %s [TRUNCATED] for sess %8p, allocated %d\n",
                         ebuf, idxptr2->session, idxptr2->allocated);
                }
                break;
            default:
                printf("unsupported type (%d/0x%02x)\n",
                       idxptr2->varbind->type, idxptr2->varbind->type);
            }
        }
    }

    if (sbuf != NULL) {
        free(sbuf);
    }
    if (ebuf != NULL) {
        free(ebuf);
    }
}

unsigned long
AgentIndex_countIndexes( oid*  name,
                         size_t namelen,
                         int    include_unallocated )
{
    struct Index_s *i = NULL, *j = NULL;
    unsigned long   n = 0;

    for (i = agentIndex_indexHead; i != NULL; i = i->next_oid) {
        if (Api_oidEquals(name, namelen,
                             i->varbind->name,
                             i->varbind->nameLength) == 0) {
            for (j = i; j != NULL; j = j->next_idx) {
                if (j->allocated || include_unallocated) {
                    n++;
                }
            }
        }
    }
    return n;
}
