#include "AgentRegistry.h"
#include "AgentCallbacks.h"
#include "DsAgent.h"
#include "Mib.h"
#include "Null.h"
#include "OldApi.h"
#include "PriotSettings.h"
#include "System/AccessControl/Vacm.h"
#include "System/Util/Assert.h"
#include "System/Util/Callback.h"
#include "System/Util/DefaultStore.h"
#include "System/Util/Trace.h"

/** @defgroup agent_lookup_cache Lookup cache, storing the registered OIDs.
 *     Maintain the cache used for locating sub-trees and OIDs.
 *   @ingroup agent_registry
 *
 * @{
 */

/**  Lookup cache - default size.*/
#define SUBTREE_DEFAULT_CACHE_SIZE 8
/**  Lookup cache - max acceptable size.*/
#define SUBTREE_MAX_CACHE_SIZE 32
int agentRegistry_lookupCacheSize = 0; /*enabled later after registrations are loaded */

typedef struct LookupCache_s {
    Subtree* next;
    Subtree* previous;
} LookupCache;

typedef struct LookupCacheContext_s {
    char* context;
    struct LookupCacheContext_s* next;
    int thecachecount;
    int currentpos;
    LookupCache cache[ SUBTREE_MAX_CACHE_SIZE ];
} LookupCacheContext;

static LookupCacheContext* _agentRegistry_thecontextcache = NULL;

/** Set the lookup cache size for optimized agent registration performance.
 * Note that it is only used by master agent - sub-agent doesn't need the cache.
 * The rough guide is that the cache size should be equal to the maximum
 * number of simultaneous managers you expect to talk to the agent (M) times 80%
 * (or so, he says randomly) the average number (N) of varbinds you
 * expect to receive in a given request for a manager.  ie, M times N.
 * Bigger does NOT necessarily mean better.  Certainly 16 should be an
 * upper limit.  32 is the hard coded limit.
 *
 * @param newsize set to the maximum size of a cache for a given
 * context.  Set to 0 to completely disable caching, or to -1 to set
 * to the default cache size (8), or to a number of your chosing.  The
 */
void AgentRegistry_setLookupCacheSize( int newsize )
{
    if ( newsize < 0 )
        agentRegistry_lookupCacheSize = SUBTREE_DEFAULT_CACHE_SIZE;
    else if ( newsize < SUBTREE_MAX_CACHE_SIZE )
        agentRegistry_lookupCacheSize = newsize;
    else
        agentRegistry_lookupCacheSize = SUBTREE_MAX_CACHE_SIZE;
}

/** Retrieves the current value of the lookup cache size
 *  Should be called from master agent only - sub-agent doesn't need the cache.
 *
 *  @return the current lookup cache size
 */
int AgentRegistry_getLookupCacheSize( void )
{
    return agentRegistry_lookupCacheSize;
}

/** Returns lookup cache entry for the context of given name.
 *
 *  @param context Name of the context. Name is case sensitive.
 *
 *  @return the lookup cache context
 */
static inline LookupCacheContext*
_AgentRegistry_getContextLookupCache( const char* context )
{
    LookupCacheContext* ptr;
    if ( !context )
        context = "";

    for ( ptr = _agentRegistry_thecontextcache; ptr; ptr = ptr->next ) {
        if ( strcmp( ptr->context, context ) == 0 )
            break;
    }
    if ( !ptr ) {
        if ( AgentRegistry_subtreeFindFirst( context ) ) {
            ptr = MEMORY_MALLOC_TYPEDEF( LookupCacheContext );
            ptr->next = _agentRegistry_thecontextcache;
            ptr->context = strdup( context );
            _agentRegistry_thecontextcache = ptr;
        } else {
            return NULL;
        }
    }
    return ptr;
}

/** Adds an entry to the Lookup Cache under specified context name.
 *
 *  @param context  Name of the context. Name is case sensitive.
 *
 *  @param next     Next subtree item.
 *
 *  @param previous Previous subtree item.
 */
static inline void
_AgentRegistry_lookupCacheAdd( const char* context,
    Subtree* next, Subtree* previous )
{
    LookupCacheContext* cptr;

    if ( ( cptr = _AgentRegistry_getContextLookupCache( context ) ) == NULL )
        return;

    if ( cptr->thecachecount < agentRegistry_lookupCacheSize )
        cptr->thecachecount++;

    cptr->cache[ cptr->currentpos ].next = next;
    cptr->cache[ cptr->currentpos ].previous = previous;

    if ( ++cptr->currentpos >= agentRegistry_lookupCacheSize )
        cptr->currentpos = 0;
}

/** @private
 *  Replaces next and previous pointer in given Lookup Cache.
 *
 *  @param ptr      Lookup Cache pointer.
 *
 *  @param next     Next subtree item.
 *
 *  @param previous Previous subtree item.
 */
static inline void
_AgentRegistry_lookupCacheReplace( LookupCache* ptr,
    Subtree* next, Subtree* previous )
{

    ptr->next = next;
    ptr->previous = previous;
}

/** Finds an entry in the Lookup Cache.
 *
 *  @param context  Case sensitive name of the context.
 *
 *  @param name     The OID we're searching for.
 *
 *  @param name_len Number of sub-ids (single integers) in the OID.
 *
 *  @param retcmp   Value set to Api_oidCompare() call result.
 *                  The value, if set, is always nonnegative.
 *
 *  @return gives Lookup Cache entry, or NULL if not found.
 *
 *  @see Api_oidCompare()
 */
static inline LookupCache*
_AgentRegistry_lookupCacheFind( const char* context, const oid* name, size_t name_len,
    int* retcmp )
{
    LookupCacheContext* cptr;
    LookupCache* ret = NULL;
    int cmp;
    int i;

    if ( ( cptr = _AgentRegistry_getContextLookupCache( context ) ) == NULL )
        return NULL;

    for ( i = 0; i < cptr->thecachecount && i < agentRegistry_lookupCacheSize; i++ ) {
        if ( cptr->cache[ i ].previous->start_a )
            cmp = Api_oidCompare( name, name_len,
                cptr->cache[ i ].previous->start_a,
                cptr->cache[ i ].previous->start_len );
        else
            cmp = 1;
        if ( cmp >= 0 ) {
            *retcmp = cmp;
            ret = &( cptr->cache[ i ] );
        }
    }
    return ret;
}

/** @private
 *  Clears cache count and position in Lookup Cache.
 */
static inline void
_AgentRegistry_invalidateLookupCache( const char* context )
{
    LookupCacheContext* cptr;
    if ( ( cptr = _AgentRegistry_getContextLookupCache( context ) ) != NULL ) {
        cptr->thecachecount = 0;
        cptr->currentpos = 0;
    }
}

void AgentRegistry_clearLookupCache( void )
{

    LookupCacheContext *ptr = NULL, *next = NULL;

    ptr = _agentRegistry_thecontextcache;
    while ( ptr ) {
        next = ptr->next;
        MEMORY_FREE( ptr->context );
        MEMORY_FREE( ptr );
        ptr = next;
    }
    _agentRegistry_thecontextcache = NULL; /* !!! */
}

/**  @} */
/* End of Lookup cache code */

/** @defgroup agent_context_cache Context cache, storing the OIDs under their contexts.
 *     Maintain the cache used for locating sub-trees registered under different contexts.
 *   @ingroup agent_registry
 *
 * @{
 */
SubtreeContextCache* agentRegistry_contextSubtrees = NULL;

/** Returns the top element of context subtrees cache.
 *  Use it if you wish to sweep through the cache elements.
 *  Note that the return may be NULL (cache may be empty).
 *
 *  @return pointer to topmost context subtree cache element.
 */
SubtreeContextCache*
AgentRegistry_getTopContextCache( void )
{
    return agentRegistry_contextSubtrees;
}

/** Finds the first subtree registered under given context.
 *
 *  @param context_name Text name of the context we're searching for.
 *
 *  @return pointer to the first subtree element, or NULL if not found.
 */
Subtree*
AgentRegistry_subtreeFindFirst( const char* context_name )
{
    SubtreeContextCache* ptr;

    if ( !context_name ) {
        context_name = "";
    }

    DEBUG_MSGTL( ( "subtree", "looking for subtree for context: \"%s\"\n",
        context_name ) );
    for ( ptr = agentRegistry_contextSubtrees; ptr != NULL; ptr = ptr->next ) {
        if ( ptr->context_name != NULL && strcmp( ptr->context_name, context_name ) == 0 ) {
            DEBUG_MSGTL( ( "subtree", "found one for: \"%s\"\n", context_name ) );
            return ptr->first_subtree;
        }
    }
    DEBUG_MSGTL( ( "subtree", "didn't find a subtree for context: \"%s\"\n",
        context_name ) );
    return NULL;
}

/** Adds the subtree to Context Cache under given context name.
 *
 *  @param context_name Text name of the context we're adding.
 *
 *  @param new_tree The subtree to be added.
 *
 *  @return copy of the new_tree pointer, or NULL if cannot add.
 */
Subtree*
AgentRegistry_addSubtree( Subtree* new_tree, const char* context_name )
{
    SubtreeContextCache* ptr = MEMORY_MALLOC_TYPEDEF( SubtreeContextCache );

    if ( !context_name ) {
        context_name = "";
    }

    if ( !ptr ) {
        return NULL;
    }

    DEBUG_MSGTL( ( "subtree", "adding subtree for context: \"%s\"\n",
        context_name ) );

    ptr->next = agentRegistry_contextSubtrees;
    ptr->first_subtree = new_tree;
    ptr->context_name = strdup( context_name );
    agentRegistry_contextSubtrees = ptr;

    return ptr->first_subtree;
}

void AgentRegistry_removeSubtree( Subtree* tree )
{
    SubtreeContextCache* ptr;

    if ( !tree->prev ) {
        for ( ptr = agentRegistry_contextSubtrees; ptr; ptr = ptr->next )
            if ( ptr->first_subtree == tree )
                break;
        Assert_assert( ptr );
        if ( ptr )
            ptr->first_subtree = tree->next;
    } else
        tree->prev->next = tree->next;

    if ( tree->next )
        tree->next->prev = tree->prev;
}

/** Replaces first subtree registered under given context name.
 *  Overwrites a subtree pointer in Context Cache for the context name.
 *  The previous subtree pointer is lost. If there's no subtree
 *  under the supplied name, then a new cache item is created.
 *
 *  @param new_tree     The new subtree to be set.
 *
 *  @param context_name Text name of the context we're replacing.
 *                      It is case sensitive.
 *
 * @return copy of the new_tree pointer, or NULL on error.
 */
Subtree*
AgentRegistry_subtreeReplaceFirst( Subtree* new_tree,
    const char* context_name )
{
    SubtreeContextCache* ptr;
    if ( !context_name ) {
        context_name = "";
    }
    for ( ptr = agentRegistry_contextSubtrees; ptr != NULL; ptr = ptr->next ) {
        if ( ptr->context_name != NULL && strcmp( ptr->context_name, context_name ) == 0 ) {
            ptr->first_subtree = new_tree;
            return ptr->first_subtree;
        }
    }
    return AgentRegistry_addSubtree( new_tree, context_name );
}

void AgentRegistry_clearSubtree( Subtree* sub );

/** Completely clears both the Context cache and the Lookup cache.
 */
void AgentRegistry_clearContext( void )
{

    SubtreeContextCache *ptr = NULL, *next = NULL;
    Subtree *t, *u;

    DEBUG_MSGTL( ( "agent_registry", "clear context\n" ) );

    ptr = AgentRegistry_getTopContextCache();
    while ( ptr ) {
        next = ptr->next;

        for ( t = ptr->first_subtree; t; t = u ) {
            u = t->next;
            AgentRegistry_clearSubtree( t );
        }

        free( UTILITIES_REMOVE_CONST( char*, ptr->context_name ) );
        MEMORY_FREE( ptr );

        ptr = next;
    }
    agentRegistry_contextSubtrees = NULL; /* !!! */
    AgentRegistry_clearLookupCache();
}

/**  @} */
/* End of Context cache code */

/** @defgroup agent_mib_subtree Maintaining MIB subtrees.
 *     Maintaining MIB nodes and subtrees.
 *   @ingroup agent_registry
 *
 * @{
 */

static void _AgentRegistry_registerMibDetachNode( Subtree* s );

/** Frees single subtree item.
 *  Deallocated memory for given Subtree item, including
 *  Handle Registration structure stored inside this item.
 *  After calling this function, the pointer is invalid
 *  and should be set to NULL.
 *
 *  @param a The subtree item to dispose.
 */
void AgentRegistry_subtreeFree( Subtree* a )
{
    if ( a != NULL ) {
        if ( a->variables != NULL && Api_oidEquals( a->name_a, a->namelen, a->start_a, a->start_len ) == 0 ) {
            MEMORY_FREE( a->variables );
        }
        MEMORY_FREE( a->name_a );
        a->namelen = 0;
        MEMORY_FREE( a->start_a );
        a->start_len = 0;
        MEMORY_FREE( a->end_a );
        a->end_len = 0;
        MEMORY_FREE( a->label_a );
        AgentHandler_handlerRegistrationFree( a->reginfo );
        a->reginfo = NULL;
        MEMORY_FREE( a );
    }
}

/** Creates deep copy of a subtree item.
 *  Duplicates all properties stored in the structure, including
 *  Handle Registration structure stored inside the item.
 *
 *  @param a The subtree item to copy.
 *
 *  @return deep copy of the subtree item, or NULL on error.
 */
Subtree*
AgentRegistry_subtreeDeepcopy( Subtree* a )
{
    Subtree* b = ( Subtree* )calloc( 1, sizeof( Subtree ) );

    if ( b != NULL ) {
        memcpy( b, a, sizeof( Subtree ) );
        b->name_a = Api_duplicateObjid( a->name_a, a->namelen );
        b->start_a = Api_duplicateObjid( a->start_a, a->start_len );
        b->end_a = Api_duplicateObjid( a->end_a, a->end_len );
        b->label_a = strdup( a->label_a );

        if ( b->name_a == NULL || b->start_a == NULL || b->end_a == NULL || b->label_a == NULL ) {
            AgentRegistry_subtreeFree( b );
            return NULL;
        }

        if ( a->variables != NULL ) {
            b->variables = ( struct Variable_s* )malloc( a->variables_len * a->variables_width );
            if ( b->variables != NULL ) {
                memcpy( b->variables, a->variables, a->variables_len * a->variables_width );
            } else {
                AgentRegistry_subtreeFree( b );
                return NULL;
            }
        }

        if ( a->reginfo != NULL ) {
            b->reginfo = AgentHandler_handlerRegistrationDup( a->reginfo );
            if ( b->reginfo == NULL ) {
                AgentRegistry_subtreeFree( b );
                return NULL;
            }
        }
    }
    return b;
}

/** @private
 *  Replaces next subtree pointer in given subtree.
 */
static inline void
_AgentRegistry_subtreeChangeNext( Subtree* ptr, Subtree* thenext )
{
    ptr->next = thenext;
    if ( thenext )
        Api_oidCompareLl( ptr->start_a,
            ptr->start_len,
            thenext->start_a,
            thenext->start_len,
            &thenext->oid_off );
}

/** @private
 *  Replaces previous subtree pointer in given subtree.
 */
static inline void
_AgentRegistry_subtreeChangePrev( Subtree* ptr, Subtree* theprev )
{
    ptr->prev = theprev;
    if ( theprev )
        Api_oidCompareLl( theprev->start_a,
            theprev->start_len,
            ptr->start_a,
            ptr->start_len,
            &ptr->oid_off );
}

/** Compares OIDs of given subtrees.
 *
 *  @param ap,bp Pointers to the subtrees to be compared.
 *
 *  @return OIDs lexicographical comparison result.
 *
 *  @see Api_oidCompare()
 */
int AgentRegistry_subtreeCompare( const Subtree* ap, const Subtree* bp )
{
    return Api_oidCompare( ap->name_a, ap->namelen, bp->name_a, bp->namelen );
}

/** Joins the given subtree with the current tree.
 *  Trees are joined and the one supplied as parameter is freed.
 *
 *  @param root The subtree to be merged with current subtree.
 *              Do not use the pointer after joining - it may be invalid.
 */
void AgentRegistry_subtreeJoin( Subtree* root )
{
    Subtree *s, *tmp, *c, *d;

    while ( root != NULL ) {
        s = root->next;
        while ( s != NULL && root->reginfo == s->reginfo ) {
            tmp = s->next;
            DEBUG_MSGTL( ( "subtree", "root start " ) );
            DEBUG_MSGOID( ( "subtree", root->start_a, root->start_len ) );
            DEBUG_MSG( ( "subtree", " (original end " ) );
            DEBUG_MSGOID( ( "subtree", root->end_a, root->end_len ) );
            DEBUG_MSG( ( "subtree", ")\n" ) );
            DEBUG_MSGTL( ( "subtree", "  JOINING to " ) );
            DEBUG_MSGOID( ( "subtree", s->start_a, s->start_len ) );

            MEMORY_FREE( root->end_a );
            root->end_a = s->end_a;
            root->end_len = s->end_len;
            s->end_a = NULL;

            for ( c = root; c != NULL; c = c->children ) {
                _AgentRegistry_subtreeChangeNext( c, s->next );
            }
            for ( c = s; c != NULL; c = c->children ) {
                _AgentRegistry_subtreeChangePrev( c, root );
            }
            DEBUG_MSG( ( "subtree", " so new end " ) );
            DEBUG_MSGOID( ( "subtree", root->end_a, root->end_len ) );
            DEBUG_MSG( ( "subtree", "\n" ) );
            /*
             * Probably need to free children too?
             */
            for ( c = s->children; c != NULL; c = d ) {
                d = c->children;
                AgentRegistry_subtreeFree( c );
            }
            AgentRegistry_subtreeFree( s );
            s = tmp;
        }
        root = root->next;
    }
}

/** Split the subtree into two at the specified point.
 *  Subtrees of the given OID and separated and formed into the
 *  returned subtree.
 *
 *  @param current The element at which splitting is started.
 *
 *  @param name The OID we'd like to split.
 *
 *  @param name_len Length of the OID.
 *
 *  @return head of the new (second) subtree.
 */
Subtree*
AgentRegistry_subtreeSplit( Subtree* current, oid name[], int name_len )
{
    struct Variable_s* vp = NULL;
    Subtree *new_sub, *ptr;
    int i = 0, rc = 0, rc2 = 0;
    size_t common_len = 0;
    char* cp;
    oid *tmp_a, *tmp_b;

    if ( Api_oidCompare( name, name_len, current->end_a, current->end_len ) > 0 ) {
        /* Split comes after the end of this subtree */
        return NULL;
    }

    new_sub = AgentRegistry_subtreeDeepcopy( current );
    if ( new_sub == NULL ) {
        return NULL;
    }

    /*  Set up the point of division.  */
    tmp_a = Api_duplicateObjid( name, name_len );
    if ( tmp_a == NULL ) {
        AgentRegistry_subtreeFree( new_sub );
        return NULL;
    }
    tmp_b = Api_duplicateObjid( name, name_len );
    if ( tmp_b == NULL ) {
        AgentRegistry_subtreeFree( new_sub );
        MEMORY_FREE( tmp_a );
        return NULL;
    }

    MEMORY_FREE( current->end_a );
    current->end_a = tmp_a;
    current->end_len = name_len;
    if ( new_sub->start_a != NULL ) {
        MEMORY_FREE( new_sub->start_a );
    }
    new_sub->start_a = tmp_b;
    new_sub->start_len = name_len;

    /*  Split the variables between the two new subtrees.  */
    i = current->variables_len;
    current->variables_len = 0;

    for ( vp = current->variables; i > 0; i-- ) {
        /*  Note that the variable "name" field omits the prefix common to the
        whole registration, hence the strange comparison here.  */

        rc = Api_oidCompare( vp->name, vp->namelen,
            name + current->namelen,
            name_len - current->namelen );

        if ( name_len - current->namelen > vp->namelen ) {
            common_len = vp->namelen;
        } else {
            common_len = name_len - current->namelen;
        }

        rc2 = Api_oidCompare( vp->name, common_len,
            name + current->namelen, common_len );

        if ( rc >= 0 ) {
            break; /* All following variables belong to the second subtree */
        }

        current->variables_len++;
        if ( rc2 < 0 ) {
            new_sub->variables_len--;
            cp = ( char* )new_sub->variables;
            new_sub->variables = ( struct Variable_s* )( cp + new_sub->variables_width );
        }
        vp = ( struct Variable_s* )( ( char* )vp + current->variables_width );
    }

    /* Delegated trees should retain their variables regardless */
    if ( current->variables_len > 0 && ASN01_IS_DELEGATED( ( u_char )current->variables[ 0 ].type ) ) {
        new_sub->variables_len = 1;
        new_sub->variables = current->variables;
    }

    /* Propogate this split down through any children */
    if ( current->children ) {
        new_sub->children = AgentRegistry_subtreeSplit( current->children,
            name, name_len );
    }

    /* Retain the correct linking of the list */
    for ( ptr = current; ptr != NULL; ptr = ptr->children ) {
        _AgentRegistry_subtreeChangeNext( ptr, new_sub );
    }
    for ( ptr = new_sub; ptr != NULL; ptr = ptr->children ) {
        _AgentRegistry_subtreeChangePrev( ptr, current );
    }
    for ( ptr = new_sub->next; ptr != NULL; ptr = ptr->children ) {
        _AgentRegistry_subtreeChangePrev( ptr, new_sub );
    }

    return new_sub;
}

/** Loads the subtree under given context name.
 *
 *  @param new_sub The subtree to be loaded into current subtree.
 *
 *  @param context_name Text name of the context we're searching for.
 *
 *  @return gives MIB_REGISTERED_OK on success, error code otherwise.
 */
int AgentRegistry_subtreeLoad( Subtree* new_sub, const char* context_name )
{
    Subtree *tree1, *tree2;
    Subtree *prev, *next;

    if ( new_sub == NULL ) {
        return MIB_REGISTERED_OK; /* Degenerate case */
    }

    if ( !AgentRegistry_subtreeFindFirst( context_name ) ) {
        static int inloop = 0;
        if ( !inloop ) {
            oid ccitt[ 1 ] = { 0 };
            oid iso[ 1 ] = { 1 };
            oid joint_ccitt_iso[ 1 ] = { 2 };
            inloop = 1;
            Null_registerNullContext( Api_duplicateObjid( ccitt, 1 ), 1,
                context_name );
            Null_registerNullContext( Api_duplicateObjid( iso, 1 ), 1,
                context_name );
            Null_registerNullContext( Api_duplicateObjid( joint_ccitt_iso, 1 ),
                1, context_name );
            inloop = 0;
        }
    }

    /*  Find the subtree that contains the start of the new subtree (if
    any)...*/

    tree1 = AgentRegistry_subtreeFind( new_sub->start_a, new_sub->start_len,
        NULL, context_name );

    /*  ... and the subtree that follows the new one (NULL implies this is the
    final region covered).  */

    if ( tree1 == NULL ) {
        tree2 = AgentRegistry_subtreeFindNext( new_sub->start_a, new_sub->start_len,
            NULL, context_name );
    } else {
        tree2 = tree1->next;
    }

    /*  Handle new subtrees that start in virgin territory.  */

    if ( tree1 == NULL ) {
        Subtree* new2 = NULL;
        /*  Is there any overlap with later subtrees?  */
        if ( tree2 && Api_oidCompare( new_sub->end_a, new_sub->end_len, tree2->start_a, tree2->start_len ) > 0 ) {
            new2 = AgentRegistry_subtreeSplit( new_sub,
                tree2->start_a, tree2->start_len );
        }

        /*  Link the new subtree (less any overlapping region) with the list of
        existing registrations.  */

        if ( tree2 ) {
            _AgentRegistry_subtreeChangePrev( new_sub, tree2->prev );
            _AgentRegistry_subtreeChangePrev( tree2, new_sub );
        } else {
            _AgentRegistry_subtreeChangePrev( new_sub,
                AgentRegistry_subtreeFindPrev( new_sub->start_a,
                                                  new_sub->start_len, NULL, context_name ) );

            if ( new_sub->prev ) {
                _AgentRegistry_subtreeChangeNext( new_sub->prev, new_sub );
            } else {
                AgentRegistry_subtreeReplaceFirst( new_sub, context_name );
            }

            _AgentRegistry_subtreeChangeNext( new_sub, tree2 );

            /* If there was any overlap, recurse to merge in the overlapping
           region (including anything that may follow the overlap).  */
            if ( new2 ) {
                return AgentRegistry_subtreeLoad( new2, context_name );
            }
        }
    } else {
        /*  If the new subtree starts *within* an existing registration
        (rather than at the same point as it), then split the existing
        subtree at this point.  */

        if ( Api_oidEquals( new_sub->start_a, new_sub->start_len,
                 tree1->start_a, tree1->start_len )
            != 0 ) {
            tree1 = AgentRegistry_subtreeSplit( tree1, new_sub->start_a,
                new_sub->start_len );
        }

        if ( tree1 == NULL ) {
            return MIB_REGISTRATION_FAILED;
        }

        /*  Now consider the end of this existing subtree:

        If it matches the new subtree precisely,
                simply merge the new one into the list of children

        If it includes the whole of the new subtree,
            split it at the appropriate point, and merge again

        If the new subtree extends beyond this existing region,
                split it, and recurse to merge the two parts.  */

        switch ( Api_oidCompare( new_sub->end_a, new_sub->end_len,
            tree1->end_a, tree1->end_len ) ) {

        case -1:
            /*  Existing subtree contains new one.  */
            AgentRegistry_subtreeSplit( tree1, new_sub->end_a, new_sub->end_len );
        /* Fall Through */

        case 0:
            /*  The two trees match precisely.  */

            /*  Note: This is the only point where the original registration
            OID ("name") is used.  */

            prev = NULL;
            next = tree1;

            while ( next && next->namelen > new_sub->namelen ) {
                prev = next;
                next = next->children;
            }

            while ( next && next->namelen == new_sub->namelen && next->priority < new_sub->priority ) {
                prev = next;
                next = next->children;
            }

            if ( next && ( next->namelen == new_sub->namelen ) && ( next->priority == new_sub->priority ) ) {
                if ( new_sub->namelen != 1 ) { /* ignore root OID dups */
                    size_t out_len = 0;
                    size_t buf_len = 0;
                    char* buf = NULL;
                    int buf_overflow = 0;

                    Mib_sprintReallocObjid( ( u_char** )&buf, &buf_len, &out_len,
                        1, &buf_overflow,
                        new_sub->start_a,
                        new_sub->start_len );
                    Logger_log( LOGGER_PRIORITY_ERR,
                        "duplicate registration: MIB modules %s and %s (oid %s%s).\n",
                        next->label_a, new_sub->label_a,
                        buf ? buf : "",
                        buf_overflow ? " [TRUNCATED]" : "" );
                    free( buf );
                }
                return MIB_DUPLICATE_REGISTRATION;
            }

            if ( prev ) {
                prev->children = new_sub;
                new_sub->children = next;
                _AgentRegistry_subtreeChangePrev( new_sub, prev->prev );
                _AgentRegistry_subtreeChangeNext( new_sub, prev->next );
            } else {
                new_sub->children = next;
                _AgentRegistry_subtreeChangePrev( new_sub, next->prev );
                _AgentRegistry_subtreeChangeNext( new_sub, next->next );

                for ( next = new_sub->next; next != NULL; next = next->children ) {
                    _AgentRegistry_subtreeChangePrev( next, new_sub );
                }

                for ( prev = new_sub->prev; prev != NULL; prev = prev->children ) {
                    _AgentRegistry_subtreeChangeNext( prev, new_sub );
                }
            }
            break;

        case 1:
            /*  New subtree contains the existing one.  */
            {
                Subtree* new2 = AgentRegistry_subtreeSplit( new_sub, tree1->end_a, tree1->end_len );
                int res = AgentRegistry_subtreeLoad( new_sub, context_name );
                if ( res != MIB_REGISTERED_OK ) {
                    AgentRegistry_removeSubtree( new2 );
                    AgentRegistry_subtreeFree( new2 );
                    return res;
                }
                return AgentRegistry_subtreeLoad( new2, context_name );
            }
        }
    }
    return 0;
}

/** Free the given subtree and all its children.
 *
 *  @param sub Subtree branch to be cleared and freed.
 *             After the call, this pointer is invalid
 *             and should be set to NULL.
 */
void AgentRegistry_clearSubtree( Subtree* sub )
{

    Subtree* c;

    if ( sub == NULL )
        return;

    for ( c = sub; c; ) {
        sub = c;
        c = c->children;
        AgentRegistry_subtreeFree( sub );
    }
}

Subtree*
AgentRegistry_subtreeFindPrev( const oid* name, size_t len, Subtree* subtree,
    const char* context_name )
{
    LookupCache* lookup_cache = NULL;
    Subtree *myptr = NULL, *previous = NULL;
    int cmp = 1;
    size_t ll_off = 0;

    if ( subtree ) {
        myptr = subtree;
    } else {
        /* look through everything */
        if ( agentRegistry_lookupCacheSize ) {
            lookup_cache = _AgentRegistry_lookupCacheFind( context_name, name, len, &cmp );
            if ( lookup_cache ) {
                myptr = lookup_cache->next;
                previous = lookup_cache->previous;
            }
            if ( !myptr )
                myptr = AgentRegistry_subtreeFindFirst( context_name );
        } else {
            myptr = AgentRegistry_subtreeFindFirst( context_name );
        }
    }

/*
     * this optimization causes a segfault on sf cf alpha-linux1.
     * ifdef out until someone figures out why and fixes it. xxx-rks 20051117
     */
#define WTEST_OPTIMIZATION 1
    DEBUG_MSGTL( ( "wtest", "oid in: " ) );
    DEBUG_MSGOID( ( "wtest", name, len ) );
    DEBUG_MSG( ( "wtest", "\n" ) );
    for ( ; myptr != NULL; previous = myptr, myptr = myptr->next ) {
        /* Compare the incoming oid with the linked list.  If we have
           results of previous compares, its faster to make sure the
           length we differed in the last check is greater than the
           length between this pointer and the last then we don't need
           to actually perform a comparison */
        DEBUG_MSGTL( ( "wtest", "oid cmp: " ) );
        DEBUG_MSGOID( ( "wtest", myptr->start_a, myptr->start_len ) );
        DEBUG_MSG( ( "wtest", "  --- off = %lu, in off = %lu test = %d\n",
            ( unsigned long )myptr->oid_off, ( unsigned long )ll_off,
            !( ll_off && myptr->oid_off && myptr->oid_off > ll_off ) ) );
        if ( !( ll_off && myptr->oid_off && myptr->oid_off > ll_off ) && Api_oidCompareLl( name, len, myptr->start_a, myptr->start_len, &ll_off ) < 0 ) {

            if ( agentRegistry_lookupCacheSize && previous && cmp ) {
                if ( lookup_cache ) {
                    _AgentRegistry_lookupCacheReplace( lookup_cache, myptr, previous );
                } else {
                    _AgentRegistry_lookupCacheAdd( context_name, myptr, previous );
                }
            }
            return previous;
        }
    }
    return previous;
}

Subtree*
AgentRegistry_subtreeFindNext( const oid* name, size_t len,
    Subtree* subtree, const char* context_name )
{
    Subtree* myptr = NULL;

    myptr = AgentRegistry_subtreeFindPrev( name, len, subtree, context_name );

    if ( myptr != NULL ) {
        myptr = myptr->next;
        while ( myptr != NULL && ( myptr->variables == NULL || myptr->variables_len == 0 ) ) {
            myptr = myptr->next;
        }
        return myptr;
    } else if ( subtree != NULL && Api_oidCompare( name, len, subtree->start_a, subtree->start_len ) < 0 ) {
        return subtree;
    } else {
        return NULL;
    }
}

Subtree*
AgentRegistry_subtreeFind( const oid* name, size_t len, Subtree* subtree,
    const char* context_name )
{
    Subtree* myptr;

    myptr = AgentRegistry_subtreeFindPrev( name, len, subtree, context_name );
    if ( myptr && myptr->end_a && Api_oidCompare( name, len, myptr->end_a, myptr->end_len ) < 0 ) {
        return myptr;
    }

    return NULL;
}

/**  @} */
/* End of Subtrees maintaining code */

/** @defgroup agent_mib_registering Registering and unregistering MIB subtrees.
 *     Adding and removing MIB nodes to the database under their contexts.
 *   @ingroup agent_registry
 *
 * @{
 */

/** Registers a MIB handler.
 *
 *  @param moduleName
 *  @param var
 *  @param varsize
 *  @param numvars
 *  @param  mibloc
 *  @param mibloclen
 *  @param priority
 *  @param range_subid
 *  @param range_ubound
 *  @param  ss
 *  @param context
 *  @param timeout
 *  @param flags
 *  @param reginfo Registration handler structure.
 *                 In a case of failure, it will be freed.
 *  @param perform_callback
 *
 *  @return gives MIB_REGISTERED_OK or MIB_* error code.
 *
 *  @see netsnmp_register_handler()
 *  @see register_agentx_list()
 *  @see AgentHandler_handlerRegistrationFree()
 */
int AgentRegistry_registerMib2( const char* moduleName,
    struct Variable_s* var,
    size_t varsize,
    size_t numvars,
    oid* mibloc,
    size_t mibloclen,
    int priority,
    int range_subid,
    oid range_ubound,
    Types_Session* ss,
    const char* context,
    int timeout,
    int flags,
    HandlerRegistration* reginfo,
    int perform_callback )
{
    Subtree *subtree, *sub2;
    int res;
    struct RegisterParameters_s reg_parms;
    int old_lookup_cache_val = AgentRegistry_getLookupCacheSize();

    if ( moduleName == NULL || mibloc == NULL ) {
        /* Shouldn't happen ??? */
        AgentHandler_handlerRegistrationFree( reginfo );
        return MIB_REGISTRATION_FAILED;
    }
    subtree = ( Subtree* )calloc( 1, sizeof( Subtree ) );
    if ( subtree == NULL ) {
        AgentHandler_handlerRegistrationFree( reginfo );
        return MIB_REGISTRATION_FAILED;
    }

    DEBUG_MSGTL( ( "AgentRegistry_registerMib", "registering \"%s\" at ", moduleName ) );
    DEBUG_MSGOIDRANGE( ( "AgentRegistry_registerMib", mibloc, mibloclen, range_subid,
        range_ubound ) );
    DEBUG_MSG( ( "AgentRegistry_registerMib", " with context \"%s\"\n",
        UTILITIES_STRING_OR_NULL( context ) ) );

    /*
     * verify that the passed context is equal to the context
     * in the reginfo.
     * (which begs the question, why do we have both? It appears that the
     *  reginfo item didn't appear til 5.2)
     */
    if ( ( ( NULL == context ) && ( NULL != reginfo->contextName ) ) || ( ( NULL != context ) && ( NULL == reginfo->contextName ) ) || ( ( ( NULL != context ) && ( NULL != reginfo->contextName ) ) && ( 0 != strcmp( context, reginfo->contextName ) ) ) ) {
        Logger_log( LOGGER_PRIORITY_WARNING, "context passed during registration does not "
                                             "equal the reginfo contextName! ('%s' != '%s')\n",
            context, reginfo->contextName );
        Assert_assert( !"register context == reginfo->contextName" ); /* always false */
    }

    /*  Create the new subtree node being registered.  */

    subtree->reginfo = reginfo;
    subtree->name_a = Api_duplicateObjid( mibloc, mibloclen );
    subtree->start_a = Api_duplicateObjid( mibloc, mibloclen );
    subtree->end_a = Api_duplicateObjid( mibloc, mibloclen );
    subtree->label_a = strdup( moduleName );
    if ( subtree->name_a == NULL || subtree->start_a == NULL || subtree->end_a == NULL || subtree->label_a == NULL ) {
        AgentRegistry_subtreeFree( subtree ); /* also frees reginfo */
        return MIB_REGISTRATION_FAILED;
    }
    subtree->namelen = ( u_char )mibloclen;
    subtree->start_len = ( u_char )mibloclen;
    subtree->end_len = ( u_char )mibloclen;
    subtree->end_a[ mibloclen - 1 ]++;

    if ( var != NULL ) {
        subtree->variables = ( struct Variable_s* )malloc( varsize * numvars );
        if ( subtree->variables == NULL ) {
            AgentRegistry_subtreeFree( subtree ); /* also frees reginfo */
            return MIB_REGISTRATION_FAILED;
        }
        memcpy( subtree->variables, var, numvars * varsize );
        subtree->variables_len = numvars;
        subtree->variables_width = varsize;
    }
    subtree->priority = priority;
    subtree->timeout = timeout;
    subtree->range_subid = range_subid;
    subtree->range_ubound = range_ubound;
    subtree->session = ss;
    subtree->flags = ( u_char )flags; /*  used to identify instance oids  */
    subtree->flags |= SUBTREE_ATTACHED;
    subtree->global_cacheid = reginfo->global_cacheid;

    AgentRegistry_setLookupCacheSize( 0 );
    res = AgentRegistry_subtreeLoad( subtree, context );

    /*  If registering a range, use the first subtree as a template for the
    rest of the range.  */

    if ( res == MIB_REGISTERED_OK && range_subid != 0 ) {
        int i;
        for ( i = mibloc[ range_subid - 1 ] + 1; i <= ( int )range_ubound; i++ ) {
            sub2 = AgentRegistry_subtreeDeepcopy( subtree );

            if ( sub2 == NULL ) {
                AgentRegistry_unregisterMibContext( mibloc, mibloclen, priority,
                    range_subid, range_ubound, context );
                AgentRegistry_setLookupCacheSize( old_lookup_cache_val );
                _AgentRegistry_invalidateLookupCache( context );
                return MIB_REGISTRATION_FAILED;
            }

            sub2->name_a[ range_subid - 1 ] = i;
            sub2->start_a[ range_subid - 1 ] = i;
            sub2->end_a[ range_subid - 1 ] = i; /* XXX - ???? */
            if ( range_subid == ( int )mibloclen ) {
                ++sub2->end_a[ range_subid - 1 ];
            }
            sub2->flags |= SUBTREE_ATTACHED;
            sub2->global_cacheid = reginfo->global_cacheid;
            /* FRQ This is essential for requests to succeed! */
            sub2->reginfo->rootoid[ range_subid - 1 ] = i;

            res = AgentRegistry_subtreeLoad( sub2, context );
            if ( res != MIB_REGISTERED_OK ) {
                AgentRegistry_unregisterMibContext( mibloc, mibloclen, priority,
                    range_subid, range_ubound, context );
                AgentRegistry_removeSubtree( sub2 );
                AgentRegistry_subtreeFree( sub2 );
                AgentRegistry_setLookupCacheSize( old_lookup_cache_val );
                _AgentRegistry_invalidateLookupCache( context );
                return res;
            }
        }
    } else if ( res == MIB_DUPLICATE_REGISTRATION || res == MIB_REGISTRATION_FAILED ) {
        AgentRegistry_setLookupCacheSize( old_lookup_cache_val );
        _AgentRegistry_invalidateLookupCache( context );
        AgentRegistry_subtreeFree( subtree );
        return res;
    }

    /*
     * mark the MIB as detached, if there's no master agent present as of now
     */
    if ( DefaultStore_getBoolean( DsStore_APPLICATION_ID,
             DsAgentBoolean_ROLE )
        != MASTER_AGENT ) {
        extern struct Types_Session_s* agent_mainSession;
        if ( agent_mainSession == NULL ) {
            _AgentRegistry_registerMibDetachNode( subtree );
        }
    }

    if ( res == MIB_REGISTERED_OK && perform_callback ) {
        memset( &reg_parms, 0x0, sizeof( reg_parms ) );
        reg_parms.name = mibloc;
        reg_parms.namelen = mibloclen;
        reg_parms.priority = priority;
        reg_parms.range_subid = range_subid;
        reg_parms.range_ubound = range_ubound;
        reg_parms.timeout = timeout;
        reg_parms.flags = ( u_char )flags;
        reg_parms.contextName = context;
        reg_parms.session = ss;
        reg_parms.reginfo = reginfo;
        reg_parms.contextName = context;
        Callback_call( CallbackMajor_APPLICATION,
            PriotdCallback_REGISTER_OID, &reg_parms );
    }

    AgentRegistry_setLookupCacheSize( old_lookup_cache_val );
    _AgentRegistry_invalidateLookupCache( context );
    return res;
}

/** @private
 *  Reattach a particular node.
 */
static void
_AgentRegistry_registerMibReattachNode( Subtree* s )
{
    if ( ( s != NULL ) && ( s->namelen > 1 ) && !( s->flags & SUBTREE_ATTACHED ) ) {
        struct RegisterParameters_s reg_parms;
        /*
         * only do registrations that are not the top level nodes
         */
        memset( &reg_parms, 0x0, sizeof( reg_parms ) );

        /*
         * XXX: do this better
         */
        reg_parms.name = s->name_a;
        reg_parms.namelen = s->namelen;
        reg_parms.priority = s->priority;
        reg_parms.range_subid = s->range_subid;
        reg_parms.range_ubound = s->range_ubound;
        reg_parms.timeout = s->timeout;
        reg_parms.flags = s->flags;
        reg_parms.session = s->session;
        reg_parms.reginfo = s->reginfo;
        /* XXX: missing in subtree: reg_parms.contextName = s->context; */
        if ( ( NULL != s->reginfo ) && ( NULL != s->reginfo->contextName ) )
            reg_parms.contextName = s->reginfo->contextName;
        Callback_call( CallbackMajor_APPLICATION,
            PriotdCallback_REGISTER_OID, &reg_parms );
        s->flags |= SUBTREE_ATTACHED;
    }
}

/** Call callbacks to reattach all our nodes.
 */
void AgentRegistry_registerMibReattach( void )
{
    Subtree *s, *t;
    SubtreeContextCache* ptr;

    for ( ptr = agentRegistry_contextSubtrees; ptr; ptr = ptr->next ) {
        for ( s = ptr->first_subtree; s != NULL; s = s->next ) {
            _AgentRegistry_registerMibReattachNode( s );
            for ( t = s->children; t != NULL; t = t->children ) {
                _AgentRegistry_registerMibReattachNode( t );
            }
        }
    }
}

/** @private
 *  Mark a node as detached.
 *
 *  @param s The note to be marked
 */
static void
_AgentRegistry_registerMibDetachNode( Subtree* s )
{
    if ( s != NULL ) {
        s->flags = s->flags & ~SUBTREE_ATTACHED;
    }
}

/** Mark all our registered OIDs as detached.
 *  This is only really useful for subagent protocols, when
 *  a connection is lost or the subagent is being shut down.
 */
void AgentRegistry_registerMibDetach( void )
{
    Subtree *s, *t;
    SubtreeContextCache* ptr;
    for ( ptr = agentRegistry_contextSubtrees; ptr; ptr = ptr->next ) {
        for ( s = ptr->first_subtree; s != NULL; s = s->next ) {
            _AgentRegistry_registerMibDetachNode( s );
            for ( t = s->children; t != NULL; t = t->children ) {
                _AgentRegistry_registerMibDetachNode( t );
            }
        }
    }
}

/** Register a new module into the MIB database, with all possible custom options
 *
 *  @param  moduleName Text name of the module.
 *                     The given name will be used to identify the module
 *                     inside the agent.
 *
 *  @param  var        Array of variables to be registered in the module.
 *
 *  @param  varsize    Size of a single variable in var array.
 *                     The size is normally equal to sizeof(struct Variable_s),
 *                     but if we wish to use shorter (or longer) OIDs, then we
 *                     could use different variant of the variable structure.
 *
 *  @param  numvars    Number of variables in the var array.
 *                     This is how many variables the function will try to register.
 *
 *  @param  mibloc     Base OID of the module.
 *                     All OIDs in var array should be sub-oids of the base OID.
 *
 *  @param  mibloclen  Length of the base OID.
 *                     Number of integers making up the base OID.
 *
 *  @param  priority   Registration priority.
 *                     Used to achieve a desired configuration when different
 *                     sessions register identical or overlapping regions.
 *                     Primarily used with AgentX subagent registrations.
 *
 *  @param range_subid If non-zero, the module is registered against a range
 *                     of OIDs, with this parameter identifying the relevant
 *                     subidentifier - see RFC 2741 for details.
 *                     Typically used to register a single row of a table.
 *                     If zero, then register the module against the full OID subtree.
 *
 *  @param range_ubound The end of the range being registered (see RFC 2741)
 *                     If range_subid is zero, then this parameter is ignored.
 *
 *  @param ss
 *  @param context
 *  @param timeout
 *  @param flags
 *
 *  @return gives ErrorCode_SUCCESS or ErrorCode_* error code.
 *
 *  @see AgentRegistry_registerMib()
 *  @see AgentRegistry_registerMibPriority()
 *  @see AgentRegistry_registerMibRange()
 *  @see AgentRegistry_unregisterMib()
 */
int AgentRegistry_registerMibContext( const char* moduleName,
    const struct Variable_s* var,
    size_t varsize,
    size_t numvars,
    const oid* mibloc,
    size_t mibloclen,
    int priority,
    int range_subid,
    oid range_ubound,
    Types_Session* ss,
    const char* context, int timeout, int flags )
{
    return OldApi_registerOldApi( moduleName, var, varsize, numvars,
        mibloc, mibloclen, priority,
        range_subid, range_ubound, ss, context,
        timeout, flags );
}

/** Register a new module into the MIB database, as being responsible
 *   for a range of OIDs (typically a single row of a table).
 *
 *  @param  moduleName Text name of the module.
 *                     The given name will be used to identify the module
 *                     inside the agent.
 *
 *  @param  var        Array of variables to be registered in the module.
 *
 *  @param  varsize    Size of a single variable in var array.
 *                     The size is normally equal to sizeof(struct Variable_s),
 *                     but if we wish to use shorter (or longer) OIDs, then we
 *                     could use different variant of the variable structure.
 *
 *  @param  numvars    Number of variables in the var array.
 *                     This is how many variables the function will try to register.
 *
 *  @param  mibloc     Base OID of the module.
 *                     All OIDs in var array should be sub-oids of the base OID.
 *
 *  @param  mibloclen  Length of the base OID.
 *                     Number of integers making up the base OID.
 *
 *  @param  priority   Registration priority.
 *                     Used to achieve a desired configuration when different
 *                     sessions register identical or overlapping regions.
 *                     Primarily used with AgentX subagent registrations.
 *
 *  @param range_subid If non-zero, the module is registered against a range
 *                     of OIDs, with this parameter identifying the relevant
 *                     subidentifier - see RFC 2741 for details.
 *                     Typically used to register a single row of a table.
 *                     If zero, then register the module against the full OID subtree.
 *
 *  @param range_ubound The end of the range being registered (see RFC 2741)
 *                     If range_subid is zero, then this parameter is ignored.
 *
 *  @param ss
 *
 *  @return gives ErrorCode_SUCCESS or ErrorCode_* error code.
 *
 *  @see AgentRegistry_registerMib()
 *  @see AgentRegistry_registerMibPriority()
 *  @see AgentRegistry_registerMibContext()
 *  @see AgentRegistry_unregisterMib()
 */
int AgentRegistry_registerMibRange( const char* moduleName,
    const struct Variable_s* var,
    size_t varsize,
    size_t numvars,
    const oid* mibloc,
    size_t mibloclen,
    int priority,
    int range_subid,
    oid range_ubound,
    Types_Session* ss )
{
    return AgentRegistry_registerMibContext( moduleName, var, varsize, numvars,
        mibloc, mibloclen, priority,
        range_subid, range_ubound, ss, "", -1, 0 );
}

/** Register a new module into the MIB database, with a non-default priority
 *
 *  @param  moduleName Text name of the module.
 *                     The given name will be used to identify the module
 *                     inside the agent.
 *
 *  @param  var        Array of variables to be registered in the module.
 *
 *  @param  varsize    Size of a single variable in var array.
 *                     The size is normally equal to sizeof(struct Variable_s),
 *                     but if we wish to use shorter (or longer) OIDs, then we
 *                     could use different variant of the variable structure.
 *
 *  @param  numvars    Number of variables in the var array.
 *                     This is how many variables the function will try to register.
 *
 *  @param  mibloc     Base OID of the module.
 *                     All OIDs in var array should be sub-oids of the base OID.
 *
 *  @param  mibloclen  Length of the base OID.
 *                     Number of integers making up the base OID.
 *
 *  @param  priority   Registration priority.
 *                     Used to achieve a desired configuration when different
 *                     sessions register identical or overlapping regions.
 *                     Primarily used with AgentX subagent registrations.
 *
 *  @return gives ErrorCode_SUCCESS or ErrorCode_* error code.
 *
 *  @see AgentRegistry_registerMib()
 *  @see AgentRegistry_registerMibRange()
 *  @see AgentRegistry_registerMibContext()
 *  @see AgentRegistry_unregisterMib()
 */
int AgentRegistry_registerMibPriority( const char* moduleName,
    const struct Variable_s* var,
    size_t varsize,
    size_t numvars,
    const oid* mibloc,
    size_t mibloclen,
    int priority )
{
    return AgentRegistry_registerMibRange( moduleName, var, varsize, numvars,
        mibloc, mibloclen, priority, 0, 0, NULL );
}

/** Register a new module into the MIB database, using default priority and context
 *
 *  @param  moduleName Text name of the module.
 *                     The given name will be used to identify the module
 *                     inside the agent.
 *
 *  @param  var        Array of variables to be registered in the module.
 *
 *  @param  varsize    Size of a single variable in var array.
 *                     The size is normally equal to sizeof(struct Variable_s),
 *                     but if we wish to use shorter (or longer) OIDs, then we
 *                     could use different variant of the variable structure.
 *
 *  @param  numvars    Number of variables in the var array.
 *                     This is how many variables the function will try to register.
 *
 *  @param  mibloc     Base OID of the module.
 *                     All OIDs in var array should be sub-oids of the base OID.
 *
 *  @param  mibloclen  Length of the base OID.
 *                     Number of integers making up the base OID.
 *
 *  @return gives ErrorCode_SUCCESS or ErrorCode_* error code.
 *
 *  @see AgentRegistry_registerMibPriority()
 *  @see AgentRegistry_registerMibRange()
 *  @see AgentRegistry_registerMibContext()
 *  @see AgentRegistry_unregisterMib()
 */
int AgentRegistry_registerMib( const char* moduleName,
    const struct Variable_s* var,
    size_t varsize,
    size_t numvars,
    const oid* mibloc,
    size_t mibloclen )
{
    return AgentRegistry_registerMibPriority( moduleName, var, varsize, numvars,
        mibloc, mibloclen, DEFAULT_MIB_PRIORITY );
}

/** @private
 *  Unloads a subtree from MIB tree.
 *
 *  @param  sub     The sub-tree which is being removed.
 *
 *  @param  prev    Previous entry, before the unloaded one.
 *
 *  @param  context Name of the context which is being removed.
 *
 *  @see AgentRegistry_unregisterMibContext()
 */
void AgentRegistry_subtreeUnload( Subtree* sub, Subtree* prev, const char* context )
{
    Subtree* ptr;

    DEBUG_MSGTL( ( "AgentRegistry_registerMib", "unload(" ) );
    if ( sub != NULL ) {
        DEBUG_MSGOID( ( "AgentRegistry_registerMib", sub->start_a, sub->start_len ) );
    } else {
        DEBUG_MSG( ( "AgentRegistry_registerMib", "[NIL]" ) );
        return;
    }
    DEBUG_MSG( ( "AgentRegistry_registerMib", ", " ) );
    if ( prev != NULL ) {
        DEBUG_MSGOID( ( "AgentRegistry_registerMib", prev->start_a, prev->start_len ) );
    } else {
        DEBUG_MSG( ( "AgentRegistry_registerMib", "[NIL]" ) );
    }
    DEBUG_MSG( ( "AgentRegistry_registerMib", ")\n" ) );

    if ( prev != NULL ) { /* non-leading entries are easy */
        prev->children = sub->children;
        _AgentRegistry_invalidateLookupCache( context );
        return;
    }
    /*
     * otherwise, we need to amend our neighbours as well
     */

    if ( sub->children == NULL ) { /* just remove this node completely */
        for ( ptr = sub->prev; ptr; ptr = ptr->children ) {
            _AgentRegistry_subtreeChangeNext( ptr, sub->next );
        }
        for ( ptr = sub->next; ptr; ptr = ptr->children ) {
            _AgentRegistry_subtreeChangePrev( ptr, sub->prev );
        }

        if ( sub->prev == NULL ) {
            AgentRegistry_subtreeReplaceFirst( sub->next, context );
        }

    } else {
        for ( ptr = sub->prev; ptr; ptr = ptr->children )
            _AgentRegistry_subtreeChangeNext( ptr, sub->children );
        for ( ptr = sub->next; ptr; ptr = ptr->children )
            _AgentRegistry_subtreeChangePrev( ptr, sub->children );

        if ( sub->prev == NULL ) {
            AgentRegistry_subtreeReplaceFirst( sub->children, context );
        }
    }
    _AgentRegistry_invalidateLookupCache( context );
}

/**
 * Unregisters a module registered against a given OID (or range) in a specified context.
 * Typically used when a module has multiple contexts defined.
 * The parameters priority, range_subid, range_ubound and context
 * should match those used to register the module originally.
 *
 * @param name  the specific OID to unregister if it conatins the associated
 *              context.
 *
 * @param len   the length of the OID, use  ASN01_OID_LENGTH macro.
 *
 * @param priority  a value between 1 and 255, used to achieve a desired
 *                  configuration when different sessions register identical or
 *                  overlapping regions.  Subagents with no particular
 *                  knowledge of priority should register with the default
 *                  value of 127.
 *
 * @param range_subid  permits specifying a range in place of one of a subtree
 *                     sub-identifiers.  When this value is zero, no range is
 *                     being specified.
 *
 * @param range_ubound  the upper bound of a sub-identifier's range.
 *                      This field is present only if range_subid is not 0.
 *
 * @param context  a context name that has been created
 *
 * @return gives MIB_UNREGISTERED_OK or MIB_* error code.
 *
 * @see AgentRegistry_unregisterMib()
 * @see AgentRegistry_unregisterMibPriority()
 * @see AgentRegistry_unregisterMibRange()
 */
int AgentRegistry_unregisterMibContext( oid* name, size_t len, int priority,
    int range_subid, oid range_ubound,
    const char* context )
{
    Subtree *list, *myptr = NULL;
    Subtree *prev, *child, *next; /* loop through children */
    struct RegisterParameters_s reg_parms;
    int old_lookup_cache_val = AgentRegistry_getLookupCacheSize();
    int unregistering = 1;
    int orig_subid_val = -1;

    AgentRegistry_setLookupCacheSize( 0 );

    if ( ( range_subid > 0 ) && ( ( size_t )range_subid <= len ) )
        orig_subid_val = name[ range_subid - 1 ];

    while ( unregistering ) {
        DEBUG_MSGTL( ( "AgentRegistry_registerMib", "unregistering " ) );
        DEBUG_MSGOIDRANGE( ( "AgentRegistry_registerMib", name, len, range_subid, range_ubound ) );
        DEBUG_MSG( ( "AgentRegistry_registerMib", "\n" ) );

        list = AgentRegistry_subtreeFind( name, len, AgentRegistry_subtreeFindFirst( context ),
            context );
        if ( list == NULL ) {
            return MIB_NO_SUCH_REGISTRATION;
        }

        for ( child = list, prev = NULL; child != NULL;
              prev = child, child = child->children ) {
            if ( Api_oidEquals( child->name_a, child->namelen, name, len ) == 0 && child->priority == priority ) {
                break; /* found it */
            }
        }

        if ( child == NULL ) {
            return MIB_NO_SUCH_REGISTRATION;
        }

        AgentRegistry_subtreeUnload( child, prev, context );
        myptr = child; /* remember this for later */

        /*
        *  Now handle any occurances in the following subtrees,
        *      as a result of splitting this range.  Due to the
        *      nature of the way such splits work, the first
        *      subtree 'slice' that doesn't refer to the given
        *      name marks the end of the original region.
        *
        *  This should also serve to register ranges.
        */

        for ( list = myptr->next; list != NULL; list = next ) {
            next = list->next; /* list gets freed sometimes; cache next */
            for ( child = list, prev = NULL; child != NULL;
                  prev = child, child = child->children ) {
                if ( ( Api_oidEquals( child->name_a, child->namelen,
                           name, len )
                         == 0 )
                    && ( child->priority == priority ) ) {
                    AgentRegistry_subtreeUnload( child, prev, context );
                    AgentRegistry_subtreeFree( child );
                    break;
                }
            }
            if ( child == NULL ) /* Didn't find the given name */
                break;
        }

        /* Maybe we are in a range... */
        if ( orig_subid_val != -1 ) {
            if ( ++name[ range_subid - 1 ] >= orig_subid_val + range_ubound ) {
                unregistering = 0;
                name[ range_subid - 1 ] = orig_subid_val;
            }
        } else {
            unregistering = 0;
        }
    }

    memset( &reg_parms, 0x0, sizeof( reg_parms ) );
    reg_parms.name = name;
    reg_parms.namelen = len;
    reg_parms.priority = priority;
    reg_parms.range_subid = range_subid;
    reg_parms.range_ubound = range_ubound;
    reg_parms.flags = 0x00; /*  this is okay I think  */
    reg_parms.contextName = context;
    Callback_call( CallbackMajor_APPLICATION,
        PriotdCallback_UNREGISTER_OID, &reg_parms );

    AgentRegistry_subtreeFree( myptr );
    AgentRegistry_setLookupCacheSize( old_lookup_cache_val );
    _AgentRegistry_invalidateLookupCache( context );
    return MIB_UNREGISTERED_OK;
}

int AgentRegistry_unregisterMibTableRow( oid* name, size_t len, int priority,
    int var_subid, oid range_ubound,
    const char* context )
{
    Subtree *list, *myptr, *futureptr;
    Subtree *prev, *child; /* loop through children */
    struct RegisterParameters_s reg_parms;
    oid range_lbound = name[ var_subid - 1 ];

    DEBUG_MSGTL( ( "AgentRegistry_registerMib", "unregistering " ) );
    DEBUG_MSGOIDRANGE( ( "AgentRegistry_registerMib", name, len, var_subid, range_ubound ) );
    DEBUG_MSG( ( "AgentRegistry_registerMib", "\n" ) );

    for ( ; name[ var_subid - 1 ] <= range_ubound; name[ var_subid - 1 ]++ ) {
        list = AgentRegistry_subtreeFind( name, len,
            AgentRegistry_subtreeFindFirst( context ), context );

        if ( list == NULL ) {
            continue;
        }

        for ( child = list, prev = NULL; child != NULL;
              prev = child, child = child->children ) {

            if ( Api_oidEquals( child->name_a, child->namelen,
                     name, len )
                    == 0
                && ( child->priority == priority ) ) {
                break; /* found it */
            }
        }

        if ( child == NULL ) {
            continue;
        }

        AgentRegistry_subtreeUnload( child, prev, context );
        myptr = child; /* remember this for later */

        for ( list = myptr->next; list != NULL; list = futureptr ) {
            /* remember the next spot in the list in case we free this node */
            futureptr = list->next;

            /* check each child */
            for ( child = list, prev = NULL; child != NULL;
                  prev = child, child = child->children ) {

                if ( Api_oidEquals( child->name_a, child->namelen,
                         name, len )
                        == 0
                    && ( child->priority == priority ) ) {
                    AgentRegistry_subtreeUnload( child, prev, context );
                    AgentRegistry_subtreeFree( child );
                    break;
                }
            }

            /* XXX: wjh: not sure why we're bailing here */
            if ( child == NULL ) { /* Didn't find the given name */
                break;
            }
        }
        AgentRegistry_subtreeFree( myptr );
    }

    name[ var_subid - 1 ] = range_lbound;
    memset( &reg_parms, 0x0, sizeof( reg_parms ) );
    reg_parms.name = name;
    reg_parms.namelen = len;
    reg_parms.priority = priority;
    reg_parms.range_subid = var_subid;
    reg_parms.range_ubound = range_ubound;
    reg_parms.flags = 0x00; /*  this is okay I think  */
    reg_parms.contextName = context;
    Callback_call( CallbackMajor_APPLICATION,
        PriotdCallback_UNREGISTER_OID, &reg_parms );

    return 0;
}

/**
 * Unregisters a module registered against a given OID (or range) in the default context.
 * Typically used when a module has multiple contexts defined.
 * The parameters priority, range_subid, and range_ubound should
 * match those used to register the module originally.
 *
 * @param name  the specific OID to unregister if it conatins the associated
 *              context.
 *
 * @param len   the length of the OID, use  ASN01_OID_LENGTH macro.
 *
 * @param priority  a value between 1 and 255, used to achieve a desired
 *                  configuration when different sessions register identical or
 *                  overlapping regions.  Subagents with no particular
 *                  knowledge of priority should register with the default
 *                  value of 127.
 *
 * @param range_subid  permits specifying a range in place of one of a subtree
 *                     sub-identifiers.  When this value is zero, no range is
 *                     being specified.
 *
 * @param range_ubound  the upper bound of a sub-identifier's range.
 *                      This field is present only if range_subid is not 0.
 *
 * @return gives MIB_UNREGISTERED_OK or MIB_* error code.
 *
 * @see AgentRegistry_unregisterMib()
 * @see AgentRegistry_unregisterMibPriority()
 * @see AgentRegistry_unregisterMibContext()
 */
int AgentRegistry_unregisterMibRange( oid* name, size_t len, int priority,
    int range_subid, oid range_ubound )
{
    return AgentRegistry_unregisterMibContext( name, len, priority, range_subid,
        range_ubound, "" );
}

/**
 * Unregisters a module registered against a given OID at the specified priority.
 * The priority parameter should match that used to register the module originally.
 *
 * @param name  the specific OID to unregister if it conatins the associated
 *              context.
 *
 * @param len   the length of the OID, use  ASN01_OID_LENGTH macro.
 *
 * @param priority  a value between 1 and 255, used to achieve a desired
 *                  configuration when different sessions register identical or
 *                  overlapping regions.  Subagents with no particular
 *                  knowledge of priority should register with the default
 *                  value of 127.
 *
 * @return gives MIB_UNREGISTERED_OK or MIB_* error code.
 *
 * @see AgentRegistry_unregisterMib()
 * @see AgentRegistry_unregisterMibRange()
 * @see AgentRegistry_unregisterMibContext()
 */
int AgentRegistry_unregisterMibPriority( oid* name, size_t len, int priority )
{
    return AgentRegistry_unregisterMibRange( name, len, priority, 0, 0 );
}

/**
 * Unregisters a module registered against a given OID at the default priority.
 *
 * @param name  the specific OID to unregister if it conatins the associated
 *              context.
 *
 * @param len   the length of the OID, use  ASN01_OID_LENGTH macro.
 *
 * @return gives MIB_UNREGISTERED_OK or MIB_* error code.
 *
 * @see AgentRegistry_unregisterMibPriority()
 * @see AgentRegistry_unregisterMibContext()
 * @see AgentRegistry_unregisterMibRange()
 * @see unregister_agentx_list()
 */
int AgentRegistry_unregisterMib( oid* name, size_t len )
{
    return AgentRegistry_unregisterMibPriority( name, len, DEFAULT_MIB_PRIORITY );
}

/** Unregisters subtree of OIDs bounded to given session.
 *
 *  @param ss Session which OIDs will be removed from tree.
 *
 *  @see AgentRegistry_unregisterMib()
 *  @see unregister_agentx_list()
 */
void AgentRegistry_unregisterMibsBySession( Types_Session* ss )
{
    Subtree *list, *list2;
    Subtree *child, *prev, *next_child;
    struct RegisterParameters_s rp;
    SubtreeContextCache* contextptr;

    DEBUG_MSGTL( ( "AgentRegistry_registerMib", "AgentRegistry_unregisterMibsBySession(%p) ctxt \"%s\"\n",
        ss, ( ss && ss->contextName ) ? ss->contextName : "[NIL]" ) );

    for ( contextptr = AgentRegistry_getTopContextCache(); contextptr != NULL;
          contextptr = contextptr->next ) {
        for ( list = contextptr->first_subtree; list != NULL; list = list2 ) {
            list2 = list->next;

            for ( child = list, prev = NULL; child != NULL; child = next_child ) {
                next_child = child->children;

                if ( ( ( !ss || ss->flags & API_FLAGS_SUBSESSION ) && child->session == ss ) || ( !( !ss || ss->flags & API_FLAGS_SUBSESSION ) && child->session && child->session->subsession == ss ) ) {

                    memset( &rp, 0x0, sizeof( rp ) );
                    rp.name = child->name_a;
                    child->name_a = NULL;
                    rp.namelen = child->namelen;
                    rp.priority = child->priority;
                    rp.range_subid = child->range_subid;
                    rp.range_ubound = child->range_ubound;
                    rp.timeout = child->timeout;
                    rp.flags = child->flags;
                    if ( ( NULL != child->reginfo ) && ( NULL != child->reginfo->contextName ) )
                        rp.contextName = child->reginfo->contextName;

                    if ( child->reginfo != NULL ) {
                        /*
                         * Don't let's free the session pointer just yet!
                         */
                        child->reginfo->handler->myvoid = NULL;
                        AgentHandler_handlerRegistrationFree( child->reginfo );
                        child->reginfo = NULL;
                    }

                    AgentRegistry_subtreeUnload( child, prev, contextptr->context_name );
                    AgentRegistry_subtreeFree( child );

                    Callback_call( CallbackMajor_APPLICATION,
                        PriotdCallback_UNREGISTER_OID, &rp );
                    MEMORY_FREE( rp.name );
                } else {
                    prev = child;
                }
            }
        }
        AgentRegistry_subtreeJoin( contextptr->first_subtree );
    }
}

/** Determines if given PDU is allowed to see (or update) a given OID.
 *
 * @param name    The OID to check access for.
 *                On return, this parameter holds the OID actually matched
 *
 * @param namelen Number of sub-identifiers in the OID.
 *                On return, this parameter holds the length of the matched OID
 *
 * @param pdu     PDU requesting access to the OID.
 *
 * @param type    ANS.1 type of the value at given OID.
 *                (Used for catching SNMPv1 requests for SMIv2-only objects)
 *
 * @return gives VACM_SUCCESS if the OID is in the PDU, otherwise error code.
 */
int AgentRegistry_inAView( oid* name, size_t* namelen, Types_Pdu* pdu, int type )
{
    struct ViewParameters_s view_parms;

    if ( pdu->flags & PRIOT_UCD_MSG_FLAG_ALWAYS_IN_VIEW ) {
        /* Enable bypassing of view-based access control */
        return VACM_SUCCESS;
    }

    view_parms.pdu = pdu;
    view_parms.name = name;
    if ( namelen != NULL ) {
        view_parms.namelen = *namelen;
    } else {
        view_parms.namelen = 0;
    }
    view_parms.errorcode = 0;
    view_parms.check_subtree = 0;

    switch ( pdu->version ) {
    case PRIOT_VERSION_3:
        Callback_call( CallbackMajor_APPLICATION,
            PriotdCallback_ACM_CHECK, &view_parms );
        return view_parms.errorcode;
    }
    return VACM_NOSECNAME;
}

/** Determines if the given PDU request could potentially succeed.
 *  (Preliminary, OID-independent validation)
 *
 * @param pdu     PDU requesting access
 *
 * @return gives VACM_SUCCESS   if the entire MIB tree is accessible
 *               VACM_NOTINVIEW if the entire MIB tree is inaccessible
 *               VACM_SUBTREE_UNKNOWN if some portions are accessible
 *               other codes may returned on error
 */
int AgentRegistry_checkAccess( Types_Pdu* pdu )
{ /* IN - pdu being checked */
    struct ViewParameters_s view_parms;
    view_parms.pdu = pdu;
    view_parms.name = NULL;
    view_parms.namelen = 0;
    view_parms.errorcode = 0;
    view_parms.check_subtree = 0;

    if ( pdu->flags & PRIOT_UCD_MSG_FLAG_ALWAYS_IN_VIEW ) {
        /* Enable bypassing of view-based access control */
        return 0;
    }

    switch ( pdu->version ) {
    case PRIOT_VERSION_3:
        Callback_call( CallbackMajor_APPLICATION,
            PriotdCallback_ACM_CHECK_INITIAL, &view_parms );
        return view_parms.errorcode;
    }
    return 1;
}

/** Determines if the given PDU request could potentially access
 *   the specified MIB subtree
 *
 * @param pdu     PDU requesting access
 *
 * @param name    The OID to check access for.
 *
 * @param namelen Number of sub-identifiers in the OID.
 *
 * @return gives VACM_SUCCESS   if the entire MIB tree is accessible
 *               VACM_NOTINVIEW if the entire MIB tree is inaccessible
 *               VACM_SUBTREE_UNKNOWN if some portions are accessible
 *               other codes may returned on error
 */
int AgentRegistry_acmCheckSubtree( Types_Pdu* pdu, oid* name, size_t namelen )
{ /* IN - pdu being checked */
    struct ViewParameters_s view_parms;
    view_parms.pdu = pdu;
    view_parms.name = name;
    view_parms.namelen = namelen;
    view_parms.errorcode = 0;
    view_parms.check_subtree = 1;

    if ( pdu->flags & PRIOT_UCD_MSG_FLAG_ALWAYS_IN_VIEW ) {
        /* Enable bypassing of view-based access control */
        return 0;
    }

    switch ( pdu->version ) {
    case PRIOT_VERSION_3:
        Callback_call( CallbackMajor_APPLICATION,
            PriotdCallback_ACM_CHECK_SUBTREE, &view_parms );
        return view_parms.errorcode;
    }
    return 1;
}

Types_Session*
AgentRegistry_getSessionForOid( const oid* name, size_t len, const char* context_name )
{
    Subtree* myptr;

    myptr = AgentRegistry_subtreeFindPrev( name, len,
        AgentRegistry_subtreeFindFirst( context_name ),
        context_name );

    while ( myptr && myptr->variables == NULL ) {
        myptr = myptr->next;
    }

    if ( myptr == NULL ) {
        return NULL;
    } else {
        return myptr->session;
    }
}

void AgentRegistry_setupTree( void )
{
    oid ccitt[ 1 ] = { 0 };
    oid iso[ 1 ] = { 1 };
    oid joint_ccitt_iso[ 1 ] = { 2 };

    int role = DefaultStore_getBoolean( DsStore_APPLICATION_ID,
        DsAgentBoolean_ROLE );

    DefaultStore_setBoolean( DsStore_APPLICATION_ID, DsAgentBoolean_ROLE,
        MASTER_AGENT );

    /*
     * we need to have the oid's in the heap, that we can *free* it for every case,
     * thats the purpose of the duplicate_objid's
     */
    Null_registerNull( Api_duplicateObjid( ccitt, 1 ), 1 );
    Null_registerNull( Api_duplicateObjid( iso, 1 ), 1 );
    Null_registerNull( Api_duplicateObjid( joint_ccitt_iso, 1 ), 1 );

    DefaultStore_setBoolean( DsStore_APPLICATION_ID, DsAgentBoolean_ROLE,
        role );
}

int AgentRegistry_removeTreeEntry( oid* name, size_t len )
{

    Subtree* sub = NULL;

    if ( ( sub = AgentRegistry_subtreeFind( name, len, NULL, "" ) ) == NULL ) {
        return MIB_NO_SUCH_REGISTRATION;
    }

    return AgentRegistry_unregisterMibContext( name, len, sub->priority,
        sub->range_subid, sub->range_ubound, "" );
}

void AgentRegistry_shutdownTree( void )
{
    oid ccitt[ 1 ] = { 0 };
    oid iso[ 1 ] = { 1 };
    oid joint_ccitt_iso[ 1 ] = { 2 };

    DEBUG_MSGTL( ( "agent_registry", "shut down tree\n" ) );

    AgentRegistry_removeTreeEntry( joint_ccitt_iso, 1 );
    AgentRegistry_removeTreeEntry( iso, 1 );
    AgentRegistry_removeTreeEntry( ccitt, 1 );
}

extern void AgentIndex_dumpIdxRegistry( void );

void AgentRegistry_dumpRegistry( void )
{
    struct Variable_s* vp = NULL;
    Subtree *myptr, *myptr2;
    u_char *s = NULL, *e = NULL, *v = NULL;
    size_t sl = 256, el = 256, vl = 256, sl_o = 0, el_o = 0, vl_o = 0;
    int i = 0;

    if ( ( s = ( u_char* )calloc( sl, 1 ) ) != NULL && ( e = ( u_char* )calloc( sl, 1 ) ) != NULL && ( v = ( u_char* )calloc( sl, 1 ) ) != NULL ) {

        SubtreeContextCache* ptr;
        for ( ptr = agentRegistry_contextSubtrees; ptr; ptr = ptr->next ) {
            printf( "Subtrees for Context: %s\n", ptr->context_name );
            for ( myptr = ptr->first_subtree; myptr != NULL;
                  myptr = myptr->next ) {
                sl_o = el_o = vl_o = 0;

                if ( !Mib_sprintReallocObjid2( &s, &sl, &sl_o, 1,
                         myptr->start_a,
                         myptr->start_len ) ) {
                    break;
                }
                if ( !Mib_sprintReallocObjid2( &e, &el, &el_o, 1,
                         myptr->end_a,
                         myptr->end_len ) ) {
                    break;
                }

                if ( myptr->variables ) {
                    printf( "%02x ( %s - %s ) [", myptr->flags, s, e );
                    for ( i = 0, vp = myptr->variables;
                          i < myptr->variables_len; i++ ) {
                        vl_o = 0;
                        if ( !Mib_sprintReallocObjid2( &v, &vl, &vl_o, 1, vp->name, vp->namelen ) ) {
                            break;
                        }
                        printf( "%s, ", v );
                        vp = ( struct Variable_s* )( ( char* )vp + myptr->variables_width );
                    }
                    printf( "]\n" );
                } else {
                    printf( "%02x   %s - %s  \n", myptr->flags, s, e );
                }
                for ( myptr2 = myptr; myptr2 != NULL;
                      myptr2 = myptr2->children ) {
                    if ( myptr2->label_a && myptr2->label_a[ 0 ] ) {
                        if ( strcmp( myptr2->label_a, "oldApi" ) == 0 ) {
                            struct Variable_s* vp = ( struct Variable_s* )myptr2->reginfo->handler->myvoid;

                            if ( !Mib_sprintReallocObjid2( &s, &sl, &sl_o, 1,
                                     vp->name, vp->namelen ) ) {
                                continue;
                            }
                            printf( "\t%s[%s] %p var %s\n", myptr2->label_a,
                                myptr2->reginfo->handlerName ? myptr2->reginfo->handlerName : "no-name",
                                myptr2->reginfo, s );
                        } else {
                            printf( "\t%s %s %p\n", myptr2->label_a,
                                myptr2->reginfo->handlerName ? myptr2->reginfo->handlerName : "no-handler-name",
                                myptr2->reginfo );
                        }
                    }
                }
            }
        }
    }

    MEMORY_FREE( s );
    MEMORY_FREE( e );
    MEMORY_FREE( v );

    AgentIndex_dumpIdxRegistry();
}

/**  @} */
/* End of MIB registration code */

/** @defgroup agent_signals POSIX signals support for agents.
 *     Registering and unregistering signal handlers.
 *   @ingroup agent_registry
 *
 * @{
 */

int agentRegistry_externalSignalScheduled[ NUM_EXTERNAL_SIGS ];
void ( *agentRegistry_externalSignalHandler[ NUM_EXTERNAL_SIGS ] )( int );

/*
 * TODO: add agent_SIGXXX_handler functions and `case SIGXXX: ...' lines
 *       below for every single that might be handled by AgentRegistry_registerSignal().
 */

void AgentRegistry_agentSIGCHLDHandler( int sig )
{
    agentRegistry_externalSignalScheduled[ SIGCHLD ]++;
}

/** Registers a POSIX Signal handler.
 *  Implements the signal registering process for POSIX and non-POSIX
 *  systems. Also, unifies the way signals work.
 *  Note that the signal handler should register itself again with
 *  signal() call before end of execution to prevent possible problems.
 *
 *  @param sig POSIX Signal ID number, as defined in signal.h.
 *
 *  @param func New signal handler function.
 *
 *  @return value is SIG_REGISTERED_OK for success and
 *        SIG_REGISTRATION_FAILED if the registration can't
 *        be handled.
 */
int AgentRegistry_registerSignal( int sig, void ( *func )( int ) )
{

    switch ( sig ) {
    case SIGCHLD: {
        static struct sigaction act;
        act.sa_handler = AgentRegistry_agentSIGCHLDHandler;
        sigemptyset( &act.sa_mask );
        act.sa_flags = 0;
        sigaction( SIGCHLD, &act, NULL );
    }

    break;
    default:
        Logger_log( LOGGER_PRIORITY_CRIT,
            "AgentRegistry_registerSignal: signal %d cannot be handled\n", sig );
        return SIG_REGISTRATION_FAILED;
    }

    agentRegistry_externalSignalHandler[ sig ] = func;
    agentRegistry_externalSignalScheduled[ sig ] = 0;

    DEBUG_MSGTL( ( "AgentRegistry_registerSignal", "registered signal %d\n", sig ) );
    return SIG_REGISTERED_OK;
}

/** Unregisters a POSIX Signal handler.
 *
 *  @param sig POSIX Signal ID number, as defined in signal.h.
 *
 *  @return value is SIG_UNREGISTERED_OK for success, or error code.
 */
int AgentRegistry_unregisterSignal( int sig )
{
    signal( sig, SIG_DFL );
    DEBUG_MSGTL( ( "AgentRegistry_unregisterSignal", "unregistered signal %d\n", sig ) );
    return SIG_UNREGISTERED_OK;
}
