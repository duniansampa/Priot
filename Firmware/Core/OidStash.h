#ifndef IOT_OIDSTASH_H
#define IOT_OIDSTASH_H

#include "Generals.h"
#include "System/Util/Callback.h"

/**
 * @brief Store and retrieve data referenced by an OID.
 *        This is essentially a way of storing data associated with a given
 *        OID.  It stores a bunch of data pointers within a memory tree that
 *        allows fairly efficient lookups with a heavily populated tree.
 */

struct OidStashNode_s;

/**
 * function type: a function which can dump the data stored at a particular node into a char buffer
 */
typedef int( OidStashDump_f )( char* buffer, size_t bufferSize, void* data, struct OidStashNode_s* node );

/**
 * function type: called to free the OidStashNode_s->data
 */
typedef void( OidStashFreeNode_f )( void* data );

typedef struct OidStashNode_s {

    /** the oid of the node */
    oid value;

    /** pointer to the stored data */
    void* data;

    /* array of children */
    struct OidStashNode_s** children;

    /** the size of the children array */
    size_t childrenSize;

    /** cache too small links */
    struct OidStashNode_s* nextSibling;
    struct OidStashNode_s* prevSibling;
    struct OidStashNode_s* parent;

} OidStashNode_t;

typedef struct OidStashSaveInfo_s {
    const char* token;
    OidStashNode_t** root;
    OidStashDump_f* dumpFunction;
} OidStashSaveInfo_t;

/**
 * @brief OidStash_addData
 *        adds data to the stash at a given oid.
 *
 * @param root - the top of the stash tree
 * @param lookup - the oid index to store the data at.
 * @param lookupLength - the length of the lookup oid.
 * @param data - the data to store
 *
 * @returns ErrorCode_SUCCESS : on success
 *          ErrorCode_GENERR : if data is already there
 *          ErrorCode_MALLOC : on malloc failures or if arguments passed in with NULL values.
 */
int OidStash_addData( OidStashNode_t** root, const oid* lookup, size_t lookupLength, void* data );

/**
 * @brief OidStash_getNode
 *        returns a node associated with a given OID.
 *
 * @param root - the top of the stash tree
 * @param lookup - the oid to look up a node for.
 * @param lookupLength - the length of the lookup oid
 *
 * @return on success, returns a node. Otherwise, returns NULL.
 */
OidStashNode_t* OidStash_getNode( OidStashNode_t* root, const oid* lookup, size_t lookupLength );

/**
 * @brief OidStash_getData
 *        returns a data pointer associated with a given OID.
 *
 * @note This is equivelent to OidStashNode_t, but returns only
 *       the data not the entire node.
 *
 * @param root - the top of the stash
 * @param lookup - the oid to search for
 * @param lookupLength - the length of the search oid.
 *
 * @return on success, returns a data pointer. Otherwise, returns NULL.
 */
void* OidStash_getData( OidStashNode_t* root, const oid* lookup, size_t lookupLength );

/**
 * @brief OidStash_getNextNode
 *        returns the next node associated with a given OID.
 *        This is equivelent to a GETNEXT operation.
 *
 * @param root - the top of the stash tree
 * @param lookup - the oid to look up a node for.
 * @param lookupLength - the length of the lookup oid
 *
 * @return on success, returns the next node. Otherwise, returns NULL.
 */
OidStashNode_t* OidStash_getNextNode( OidStashNode_t* root, oid* lookup, size_t lookupLength );

/**
 * @brief OidStash_createSizedNode
 *        creates an OidStashNode_t node
 *
 * @param childrenSize - the size of the child pointer array
 *
 * @return NULL on error, otherwise the newly allocated node
 */
OidStashNode_t* OidStash_createSizedNode( size_t childrenSize );

/**
 * @brief OidStash_createNode
 *        creates a OidStash_Node.
 *
 * @note Assumes you want the default oidstashCHILDREN_SIZE hash size for the node.
 *
 * @return NULL on error, otherwise the newly allocated node
 */
OidStashNode_t* OidStash_createNode( void );

/**
 * @brief OidStash_store
 *        stores data in a stash tree to peristent storage.
 *
 *
 * @details This function can be called to save all data in a stash tree to
 *          PRIOT's percent storage.  Make sure you register a parsing
 *          function with the ReadConfig_* system to re-incorperate your saved
 *          data into future trees.
 *
 * @param root - the top of the stash to store.
 * @param tokenName - the file token name to save in (passing "priotd" will
 *                    save things into priotd.conf).
 * @param dumpFunction - a function which can dump the data stored at a particular
 *                       node into a char buffer.
 * @param curOid - must be a pointer to a OID array of length ASN01_MAX_OID_LEN.
 * @param curOidLength - must be 0 for the top level call.
 */
void OidStash_store( OidStashNode_t* root, const char* tokenName,
    OidStashDump_f* dumpFunction, oid* curOid, size_t curOidLength );

/**
 * @brief OidStash_storeAll
 *        a wrapper around OidStash_store for use with a Alarm.
 *        when calling Alarm, you can list this as a callback.  The
 *        callbackFuncArg should be a pointer to a OidStashSaveInfo_s
 *        pointer.  It can also be called directly, of course.  The last
 *        argument (callbackFuncArg) is the only one that is used.  The rest are
 *        ignored by the function.
 *
 * @param majorID - is the callback major type
 * @param minorID - is the callback minor type
 * @param extraArgument - the extra argument
 * @param callbackFuncArg - a pointer to a OidStashSaveInfo_s structure.
 */
Callback_f OidStash_storeAll;

/**
 * @brief OidStash_clear
 *        frees all data in the stash and cleans it out.  Sets root = NULL
 *
 * @param root - the top of the tree (or branch to be freed)
 *
 * @param freeFunction - the function to be called on each data (void *)
 *                       pointer.  If left NULL the system free() function will be called
 */
void OidStash_clear( OidStashNode_t** root, OidStashFreeNode_f* freeFunction );

/**
 * @brief OidStash_dump
 *        for debugging: dump the OidStashNode_t tree to stdout
 *
 *  @param root - the top of the tree
 *  @param prefix - a character string prefix printed to the beginning of each line.
 */
void OidStash_dump( OidStashNode_t* root, char* prefix );

#endif // IOT_OIDSTASH_H
