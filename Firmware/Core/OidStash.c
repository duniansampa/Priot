#include "OidStash.h"
#include "Priot.h"
#include "ReadConfig.h"
#include "System/Util/DefaultStore.h"
#include "System/Util/Trace.h"
#include "System/Util/Utilities.h"
#include "Types.h"

#define oidstashCHILDREN_SIZE 31

OidStashNode_t* OidStash_createSizedNode( size_t childrenSize )
{
    OidStashNode_t* ret;
    ret = MEMORY_MALLOC_TYPEDEF( OidStashNode_t );
    if ( !ret )
        return NULL;
    ret->children = ( OidStashNode_t** )calloc( childrenSize, sizeof( OidStashNode_t* ) );
    if ( !ret->children ) {
        free( ret );
        return NULL;
    }
    ret->childrenSize = childrenSize;
    return ret;
}

OidStashNode_t*
OidStash_createNode( void )
{
    return OidStash_createSizedNode( oidstashCHILDREN_SIZE );
}

int OidStash_addData( OidStashNode_t** root, const oid* lookup, size_t lookupLength, void* data )
{
    OidStashNode_t *curnode, *tmpp, *loopp;
    unsigned int i;

    if ( !root || !lookup || lookupLength == 0 )
        return ErrorCode_GENERR;

    if ( !*root ) {
        *root = OidStash_createNode();
        if ( !*root )
            return ErrorCode_MALLOC;
    }
    DEBUG_MSGTL( ( "oidStash", "stashAddData " ) );
    DEBUG_MSGOID( ( "oidStash", lookup, lookupLength ) );
    DEBUG_MSG( ( "oidStash", "\n" ) );
    tmpp = NULL;
    for ( curnode = *root, i = 0; i < lookupLength; i++ ) {
        tmpp = curnode->children[ lookup[ i ] % curnode->childrenSize ];
        if ( !tmpp ) {
            /*
             * no child in array at all
             */
            tmpp = curnode->children[ lookup[ i ] % curnode->childrenSize ] = OidStash_createNode();
            tmpp->value = lookup[ i ];
            tmpp->parent = curnode;
        } else {
            for ( loopp = tmpp; loopp; loopp = loopp->nextSibling ) {
                if ( loopp->value == lookup[ i ] )
                    break;
            }
            if ( loopp ) {
                tmpp = loopp;
            } else {
                /*
                 * none exists.  Create it
                 */
                loopp = OidStash_createNode();
                loopp->value = lookup[ i ];
                loopp->nextSibling = tmpp;
                loopp->parent = curnode;
                tmpp->prevSibling = loopp;
                curnode->children[ lookup[ i ] % curnode->childrenSize ] = loopp;
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
    if ( curnode->data )
        return ErrorCode_GENERR;
    if ( NULL == tmpp )
        return ErrorCode_GENERR;
    tmpp->data = data;
    return ErrorCode_SUCCESS;
}

OidStashNode_t* OidStash_getNode( OidStashNode_t* root, const oid* lookup, size_t lookupLength )
{
    OidStashNode_t *curnode, *tmpp, *loopp;
    unsigned int i;

    if ( !root )
        return NULL;
    tmpp = NULL;
    for ( curnode = root, i = 0; i < lookupLength; i++ ) {
        tmpp = curnode->children[ lookup[ i ] % curnode->childrenSize ];
        if ( !tmpp ) {
            return NULL;
        } else {
            for ( loopp = tmpp; loopp; loopp = loopp->nextSibling ) {
                if ( loopp->value == lookup[ i ] )
                    break;
            }
            if ( loopp ) {
                tmpp = loopp;
            } else {
                return NULL;
            }
        }
        curnode = tmpp;
    }
    return tmpp;
}

OidStashNode_t* OidStash_getNextNode( OidStashNode_t* root, oid* lookup, size_t lookupLength )
{
    OidStashNode_t *curnode, *tmpp, *loopp;
    unsigned int i, j, bigger_than = 0, do_bigger = 0;

    if ( !root )
        return NULL;
    tmpp = NULL;

    /* get closest matching node */
    for ( curnode = root, i = 0; i < lookupLength; i++ ) {
        tmpp = curnode->children[ lookup[ i ] % curnode->childrenSize ];
        if ( !tmpp ) {
            break;
        } else {
            for ( loopp = tmpp; loopp; loopp = loopp->nextSibling ) {
                if ( loopp->value == lookup[ i ] )
                    break;
            }
            if ( loopp ) {
                tmpp = loopp;
            } else {
                break;
            }
        }
        curnode = tmpp;
    }

    /* find the *next* node lexographically greater */
    if ( !curnode )
        return NULL; /* ack! */

    if ( i + 1 < lookupLength ) {
        bigger_than = lookup[ i + 1 ];
        do_bigger = 1;
    }

    do {
        /* check the children first */
        tmpp = NULL;
        /* next child must be (next) greater than our next search node */
        /* XXX: should start this loop at best_nums[i]%... and wrap */
        for ( j = 0; j < curnode->childrenSize; j++ ) {
            for ( loopp = curnode->children[ j ];
                  loopp; loopp = loopp->nextSibling ) {
                if ( ( !do_bigger || loopp->value > bigger_than ) && ( !tmpp || tmpp->value > loopp->value ) ) {
                    tmpp = loopp;
                    /* XXX: can do better and include min_nums[i] */
                    if ( tmpp->value <= curnode->childrenSize - 1 ) {
                        /* best we can do. */
                        goto goto_doneThisLoop;
                    }
                }
            }
        }

    goto_doneThisLoop:
        if ( tmpp && tmpp->data )
            /* found a node with data.  Go with it. */
            return tmpp;

        if ( tmpp ) {
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
    } while ( curnode );

    /* fell off the top */
    return NULL;
}

void* OidStash_getData( OidStashNode_t* root, const oid* lookup, size_t lookupLength )
{
    OidStashNode_t* ret;
    ret = OidStash_getNode( root, lookup, lookupLength );
    if ( ret )
        return ret->data;
    return NULL;
}

int OidStash_storeAll( int majorID, int minorID,
    void* extraArgument, void* callbackFuncArg )
{
    oid oidbase[ TYPES_MAX_OID_LEN ];
    OidStashSaveInfo_t* sinfo;

    if ( !callbackFuncArg )
        return PRIOT_ERR_NOERROR;

    sinfo = ( OidStashSaveInfo_t* )callbackFuncArg;
    OidStash_store( *( sinfo->root ), sinfo->token, sinfo->dumpFunction,
        oidbase, 0 );
    return PRIOT_ERR_NOERROR;
}

void OidStash_store( OidStashNode_t* root,
    const char* tokenName, OidStashDump_f* dumpFunction,
    oid* curOid, size_t curOidLength )
{

    char buf[ UTILITIES_MAX_BUFFER ];
    OidStashNode_t* tmpp;
    char* cp;
    char* appname = DefaultStore_getString( DsStore_LIBRARY_ID,
        DsStr_APPTYPE );
    int i;

    if ( !tokenName || !root || !curOid || !dumpFunction )
        return;

    for ( i = 0; i < ( int )root->childrenSize; i++ ) {
        if ( root->children[ i ] ) {
            for ( tmpp = root->children[ i ]; tmpp; tmpp = tmpp->nextSibling ) {
                curOid[ curOidLength ] = tmpp->value;
                if ( tmpp->data ) {
                    snprintf( buf, sizeof( buf ), "%s ", tokenName );
                    cp = ReadConfig_saveObjid( buf + strlen( buf ), curOid,
                        curOidLength + 1 );
                    *cp++ = ' ';
                    *cp = '\0';
                    if ( ( *dumpFunction )( cp, sizeof( buf ) - strlen( buf ),
                             tmpp->data, tmpp ) )
                        ReadConfig_store( appname, buf );
                }
                OidStash_store( tmpp, tokenName, dumpFunction,
                    curOid, curOidLength + 1 );
            }
        }
    }
}

void OidStash_dump( OidStashNode_t* root, char* prefix )
{
    char myprefix[ TYPES_MAX_OID_LEN * 4 ];
    OidStashNode_t* tmpp;
    int prefix_len = strlen( prefix ) + 1; /* actually it's +2 */
    unsigned int i;

    memset( myprefix, ' ', TYPES_MAX_OID_LEN * 4 );
    myprefix[ prefix_len ] = '\0';

    for ( i = 0; i < root->childrenSize; i++ ) {
        if ( root->children[ i ] ) {
            for ( tmpp = root->children[ i ]; tmpp; tmpp = tmpp->nextSibling ) {
                printf( "%s%"
                        "l"
                        "d@%d: %s\n",
                    prefix, tmpp->value, i,
                    ( tmpp->data ) ? "DATA" : "" );
                OidStash_dump( tmpp, myprefix );
            }
        }
    }
}

void OidStash_clear( OidStashNode_t** root, OidStashFreeNode_f* freeFunction )
{

    OidStashNode_t *curnode, *tmpp;
    unsigned int i;

    if ( !root || !*root )
        return;

    /* loop through all our children and free each node */
    for ( i = 0; i < ( *root )->childrenSize; i++ ) {
        if ( ( *root )->children[ i ] ) {
            for ( tmpp = ( *root )->children[ i ]; tmpp; tmpp = curnode ) {
                if ( tmpp->data ) {
                    if ( freeFunction )
                        ( *freeFunction )( tmpp->data );
                    else
                        free( tmpp->data );
                }
                curnode = tmpp->nextSibling;
                OidStash_clear( &tmpp, freeFunction );
            }
        }
    }
    free( ( *root )->children );
    free( *root );
    *root = NULL;
}
