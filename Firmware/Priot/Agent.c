#include "Agent.h"
#include "../Plugin/Agentx/Master.h"
#include "AgentHandler.h"
#include "AgentRegistry.h"
#include "System/Util/Alarm.h"
#include "Api.h"
#include "Client.h"
#include "System/Util/DefaultStore.h"
#include "DsAgent.h"
#include "Impl.h"
#include "Mib.h"
#include "PriotSettings.h"
#include "System/Containers/MapList.h"
#include "System/Util/Assert.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"
#include "Trap.h"
#include "Vacm.h"
#include "VarStruct.h"

#include <sys/time.h>

int Agent_handlePdu( AgentSession* asp );
int Agent_handleRequest( AgentSession* asp,
    int status );
int Agent_wrapUpRequest( AgentSession* asp,
    int status );
int Agent_checkDelayedRequest( AgentSession* asp );
int Agent_handleGetnextLoop( AgentSession* asp );
int Agent_handleSetLoop( AgentSession* asp );

int Agent_checkQueuedChainFor( AgentSession* asp );
int Agent_addQueued( AgentSession* asp );
int Agent_removeFromDelegated( AgentSession* asp );

oid agent_versionSysoid[] = { NETSNMP_SYSTEM_MIB };
int agent_versionSysoidLen = ASN01_OID_LENGTH( agent_versionSysoid );

#define SNMP_ADDRCACHE_SIZE 10
#define SNMP_ADDRCACHE_MAXAGE 300 /* in seconds */

void Agent_addListData( AgentRequestInfo* ari,
    Map* node )
{
    if ( ari ) {
        if ( ari->agent_data ) {
            Map_insert( &ari->agent_data, node );
        } else {
            ari->agent_data = node;
        }
    }
}

int Agent_removeListData( AgentRequestInfo* ari,
    const char* name )
{
    if ( ( NULL == ari ) || ( NULL == ari->agent_data ) )
        return 1;

    return Map_erase( &ari->agent_data, name );
}

void* Agent_getListData( AgentRequestInfo* ari,
    const char* name )
{
    if ( ari ) {
        return Map_at( ari->agent_data, name );
    }
    return NULL;
}

void Agent_freeAgentDataSet( AgentRequestInfo* ari )
{
    if ( ari ) {
        Map_free( ari->agent_data );
    }
}

void Agent_freeAgentDataSets( AgentRequestInfo* ari )
{
    if ( ari ) {
        Map_clear( ari->agent_data );
    }
}

void Agent_freeAgentRequestInfo( AgentRequestInfo* ari )
{
    if ( ari ) {
        if ( ari->agent_data ) {
            Map_clear( ari->agent_data );
        }
        MEMORY_FREE( ari );
    }
}

enum {
    SNMP_ADDRCACHE_UNUSED = 0,
    SNMP_ADDRCACHE_USED = 1
};

struct AddrCache_s {
    char* addr;
    int status;
    struct timeval lastHitM;
};

static struct AddrCache_s _agent_addrCache[ SNMP_ADDRCACHE_SIZE ];
int agent_logAddresses = 0;

typedef struct AgentNsap_s {
    int handle;
    Transport_Transport* t;
    void* s; /*  Opaque internal session pointer.  */
    struct AgentNsap_s* next;
} AgentNsap;

static AgentNsap* _agent_agentNsapList = NULL;
static AgentSession* _agent_sessionList = NULL;
AgentSession* agent_processingSet = NULL;
AgentSession* agent_delegated_list = NULL;
AgentSession* agent_agentQueuedList = NULL;

static int _agent_currentGlobalid = 0;

int agent_running = 1;

int Agent_allocateGlobalcacheid( void )
{
    return ++_agent_currentGlobalid;
}

int Agent_getLocalCachid( Cachemap* cache_store, int globalid )
{
    while ( cache_store != NULL ) {
        if ( cache_store->globalid == globalid )
            return cache_store->cacheid;
        cache_store = cache_store->next;
    }
    return -1;
}

Cachemap*
Agent_getOrAddLocalCachid( Cachemap** cache_store,
    int globalid, int localid )
{
    Cachemap* tmpp;

    tmpp = MEMORY_MALLOC_TYPEDEF( Cachemap );
    if ( tmpp != NULL ) {
        if ( *cache_store ) {
            tmpp->next = *cache_store;
            *cache_store = tmpp;
        } else {
            *cache_store = tmpp;
        }

        tmpp->globalid = globalid;
        tmpp->cacheid = localid;
    }
    return tmpp;
}

void Agent_freeCachemap( Cachemap* cache_store )
{
    Cachemap* tmpp;
    while ( cache_store ) {
        tmpp = cache_store;
        cache_store = cache_store->next;
        MEMORY_FREE( tmpp );
    }
}

typedef struct AgentSetCache_s {
    /*
    * match on these 2
    */
    int transID;
    Types_Session* sess;

    /*
    * store this info
    */
    TreeCache* treecache;
    int treecache_len;
    int treecache_num;

    int vbcount;
    RequestInfo* requests;
    VariableList* saved_vars;
    Map* agent_data;

    /*
    * list
    */
    struct AgentSetCache_s* next;
} AgentSetCache;

static AgentSetCache* _agent_sets = NULL;

AgentSetCache*
Agent_saveSetCache( AgentSession* asp )
{
    AgentSetCache* ptr;

    if ( !asp || !asp->reqinfo || !asp->pdu )
        return NULL;

    ptr = MEMORY_MALLOC_TYPEDEF( AgentSetCache );
    if ( ptr == NULL )
        return NULL;

    /*
    * Save the important information
    */
    DEBUG_MSGTL( ( "verbose:asp", "asp %p reqinfo %p saved in cache (mode %d)\n",
        asp, asp->reqinfo, asp->pdu->command ) );
    ptr->transID = asp->pdu->transid;
    ptr->sess = asp->session;
    ptr->treecache = asp->treecache;
    ptr->treecache_len = asp->treecache_len;
    ptr->treecache_num = asp->treecache_num;
    ptr->agent_data = asp->reqinfo->agent_data;
    ptr->requests = asp->requests;
    ptr->saved_vars = asp->pdu->variables; /* requests contains pointers to variables */
    ptr->vbcount = asp->vbcount;

    /*
    * make the agent forget about what we've saved
    */
    asp->treecache = NULL;
    asp->reqinfo->agent_data = NULL;
    asp->pdu->variables = NULL;
    asp->requests = NULL;

    ptr->next = _agent_sets;
    _agent_sets = ptr;

    return ptr;
}

int Agent_getSetCache( AgentSession* asp )
{
    AgentSetCache *ptr, *prev = NULL;

    for ( ptr = _agent_sets; ptr != NULL; ptr = ptr->next ) {
        if ( ptr->sess == asp->session && ptr->transID == asp->pdu->transid ) {
            /*
            * remove this item from list
            */
            if ( prev )
                prev->next = ptr->next;
            else
                _agent_sets = ptr->next;

            /*
            * found it.  Get the needed data
            */
            asp->treecache = ptr->treecache;
            asp->treecache_len = ptr->treecache_len;
            asp->treecache_num = ptr->treecache_num;

            /*
            * Free previously allocated requests before overwriting by
            * cached ones, otherwise memory leaks!
            */
            if ( asp->requests ) {
                /*
                * I don't think this case should ever happen. Please email
                * the net-snmp-coders@lists.sourceforge.net if you have
                * a test case that hits this condition. -- rstory
                */
                int i;
                Assert_assert( NULL == asp->requests ); /* see note above */
                for ( i = 0; i < asp->vbcount; i++ ) {
                    AgentHandler_freeRequestDataSets( &asp->requests[ i ] );
                }
                free( asp->requests );
            }
            /*
        * If we replace asp->requests with the info from the set cache,
        * we should replace asp->pdu->variables also with the cached
        * info, as asp->requests contains pointers to them.  And we
        * should also free the current asp->pdu->variables list...
        */
            if ( ptr->saved_vars ) {
                if ( asp->pdu->variables )
                    Api_freeVarbind( asp->pdu->variables );
                asp->pdu->variables = ptr->saved_vars;
                asp->vbcount = ptr->vbcount;
            } else {
                /*
                * when would we not have saved variables? someone
                * let me know if they hit this condition. -- rstory
                */
                Assert_assert( NULL != ptr->saved_vars );
            }
            asp->requests = ptr->requests;

            Assert_assert( NULL != asp->reqinfo );
            asp->reqinfo->asp = asp;
            asp->reqinfo->agent_data = ptr->agent_data;

            /*
            * update request reqinfo, if it's out of date.
            * yyy-rks: investigate when/why sometimes they match,
            * sometimes they don't.
            */
            if ( asp->requests->agent_req_info != asp->reqinfo ) {
                /*
                * - one don't match case: agentx subagents. prev asp & reqinfo
                *   freed, request reqinfo ptrs not cleared.
                */
                RequestInfo* tmp = asp->requests;
                DEBUG_MSGTL( ( "verbose:asp",
                    "  reqinfo %p doesn't match cached reqinfo %p\n",
                    asp->reqinfo, asp->requests->agent_req_info ) );
                for ( ; tmp; tmp = tmp->next )
                    tmp->agent_req_info = asp->reqinfo;
            } else {
                /*
                * - match case: ?
                */
                DEBUG_MSGTL( ( "verbose:asp",
                    "  reqinfo %p matches cached reqinfo %p\n",
                    asp->reqinfo, asp->requests->agent_req_info ) );
            }

            MEMORY_FREE( ptr );
            return PRIOT_ERR_NOERROR;
        }
        prev = ptr;
    }
    return PRIOT_ERR_GENERR;
}

/* Bulkcache holds the values for the *repeating* varbinds (only),
*   but ordered "by column" - i.e. the repetitions for each
*   repeating varbind follow on immediately from one another,
*   rather than being interleaved, as required by the protocol.
*
* So we need to rearrange the varbind list so it's ordered "by row".
*
* In the following code chunk:
*     n            = # non-repeating varbinds
*     r            = # repeating varbinds
*     asp->vbcount = # varbinds in the incoming PDU
*         (So asp->vbcount = n+r)
*
*     repeats = Desired # of repetitions (of 'r' varbinds)
*/
static inline void
_Agent_reorderGetbulk( AgentSession* asp )
{
    int i, n = 0, r = 0;
    int repeats = asp->pdu->errindex;
    int j, k;
    int all_eoMib;
    VariableList *prev = NULL, *curr;

    if ( asp->vbcount == 0 ) /* Nothing to do! */
        return;

    if ( asp->pdu->errstat < asp->vbcount ) {
        n = asp->pdu->errstat;
    } else {
        n = asp->vbcount;
    }
    if ( ( r = asp->vbcount - n ) < 0 ) {
        r = 0;
    }

    /* we do nothing if there is nothing repeated */
    if ( r == 0 )
        return;

    /* Fix endOfMibView entries. */
    for ( i = 0; i < r; i++ ) {
        prev = NULL;
        for ( j = 0; j < repeats; j++ ) {
            curr = asp->bulkcache[ i * repeats + j ];
            /*
            *  If we don't have a valid name for a given repetition
            *   (and probably for all the ones that follow as well),
            *   extend the previous result to indicate 'endOfMibView'.
            *  Or if the repetition already has type endOfMibView make
            *   sure it has the correct objid (i.e. that of the previous
            *   entry or that of the original request).
            */
            if ( curr->nameLength == 0 || curr->type == PRIOT_ENDOFMIBVIEW ) {
                if ( prev == NULL ) {
                    /* Use objid from original pdu. */
                    prev = asp->orig_pdu->variables;
                    for ( k = i; prev && k > 0; k-- )
                        prev = prev->next;
                }
                if ( prev ) {
                    Client_setVarObjid( curr, prev->name, prev->nameLength );
                    Client_setVarTypedValue( curr, PRIOT_ENDOFMIBVIEW, NULL, 0 );
                }
            }
            prev = curr;
        }
    }

    /*
    * For each of the original repeating varbinds (except the last),
    *  go through the block of results for that varbind,
    *  and link each instance to the corresponding instance
    *  in the next block.
    */
    for ( i = 0; i < r - 1; i++ ) {
        for ( j = 0; j < repeats; j++ ) {
            asp->bulkcache[ i * repeats + j ]->next = asp->bulkcache[ ( i + 1 ) * repeats + j ];
        }
    }

    /*
    * For the last of the original repeating varbinds,
    *  go through that block of results, and link each
    *  instance to the *next* instance in the *first* block.
    *
    * The very last instance of this block is left untouched
    *  since it (correctly) points to the end of the list.
    */
    for ( j = 0; j < repeats - 1; j++ ) {
        asp->bulkcache[ ( r - 1 ) * repeats + j ]->next = asp->bulkcache[ j + 1 ];
    }

    /*
    * If we've got a full row of endOfMibViews, then we
    *  can truncate the result varbind list after that.
    *
    * Look for endOfMibView exception values in the list of
    *  repetitions for the first varbind, and check the
    *  corresponding instances for the other varbinds
    *  (following the next_variable links).
    *
    * If they're all endOfMibView too, then we can terminate
    *  the linked list there, and free any redundant varbinds.
    */
    all_eoMib = 0;
    for ( i = 0; i < repeats; i++ ) {
        if ( asp->bulkcache[ i ]->type == PRIOT_ENDOFMIBVIEW ) {
            all_eoMib = 1;
            for ( j = 1, prev = asp->bulkcache[ i ];
                  j < r;
                  j++, prev = prev->next ) {
                if ( prev->type != PRIOT_ENDOFMIBVIEW ) {
                    all_eoMib = 0;
                    break; /* Found a real value */
                }
            }
            if ( all_eoMib ) {
                /*
                * This is indeed a full endOfMibView row.
                * Terminate the list here & free the rest.
                */
                Api_freeVarbind( prev->next );
                prev->next = NULL;
                break;
            }
        }
    }
}

/* EndOfMibView replies to a GETNEXT request should according to RFC3416
*  have the object ID set to that of the request. Our tree search
*  algorithm will sometimes break that requirement. This function will
*  fix that.
*/
static inline void
_Agent_fixEndofmibview( AgentSession* asp )
{
    VariableList *vb, *ovb;

    if ( asp->vbcount == 0 ) /* Nothing to do! */
        return;

    for ( vb = asp->pdu->variables, ovb = asp->orig_pdu->variables;
          vb && ovb; vb = vb->next, ovb = ovb->next ) {
        if ( vb->type == PRIOT_ENDOFMIBVIEW )
            Client_setVarObjid( vb, ovb->name, ovb->nameLength );
    }
}

/**
* This function checks for packets arriving on the SNMP port and
* processes them(Api_read) if some are found, using the select(). If block
* is non zero, the function call blocks until a packet arrives
*
* @param block used to control blocking in the select() function, 1 = block
*        forever, and 0 = don't block
*
* @return  Returns a positive integer if packets were processed, and -1 if an
* error was found.
*
*/
int Agent_checkAndProcess( int block )
{
    int numfds;
    fd_set fdset;
    struct timeval timeout = { LONG_MAX, 0 }, *tvp = &timeout;
    int count;
    int fakeblock = 0;

    numfds = 0;
    FD_ZERO( &fdset );
    Api_selectInfo( &numfds, &fdset, tvp, &fakeblock );
    if ( block != 0 && fakeblock != 0 ) {
        /*
        * There are no alarms registered, and the caller asked for blocking, so
        * let select() block forever.
        */

        tvp = NULL;
    } else if ( block != 0 && fakeblock == 0 ) {
        /*
        * The caller asked for blocking, but there is an alarm due sooner than
        * LONG_MAX seconds from now, so use the modified timeout returned by
        * Api_selectInfo as the timeout for select().
        */

    } else if ( block == 0 ) {
        /*
        * The caller does not want us to block at all.
        */

        timerclear( tvp );
    }

    count = select( numfds, &fdset, NULL, NULL, tvp );

    if ( count > 0 ) {
        /*
        * packets found, process them
        */
        Api_read( &fdset );
    } else
        switch ( count ) {
        case 0:
            Api_timeout();
            break;
        case -1:
            if ( errno != EINTR ) {
                Logger_logPerror( "select" );
            }
            return -1;
        default:
            Logger_log( LOGGER_PRIORITY_ERR, "select returned %d\n", count );
            return -1;
        } /* endif -- count>0 */

    /*
    * see if persistent store needs to be saved
    */
    Api_storeIfNeeded();

    /*
    * Run requested alarms.
    */
    Alarm_runAlarms();

    Agent_checkOutstandingAgentRequests();

    return count;
}

/*
* Set up the address cache.
*/
void Agent_addrcacheInitialise( void )
{
    int i = 0;

    for ( i = 0; i < SNMP_ADDRCACHE_SIZE; i++ ) {
        _agent_addrCache[ i ].addr = NULL;
        _agent_addrCache[ i ].status = SNMP_ADDRCACHE_UNUSED;
    }
}

void Agent_addrcacheDestroy( void )
{
    int i = 0;

    for ( i = 0; i < SNMP_ADDRCACHE_SIZE; i++ ) {
        if ( _agent_addrCache[ i ].status == SNMP_ADDRCACHE_USED ) {
            free( _agent_addrCache[ i ].addr );
            _agent_addrCache[ i ].status = SNMP_ADDRCACHE_UNUSED;
        }
    }
}

/*
* Adds a new entry to the cache of addresses that
* have recently made connections to the agent.
* Returns 0 if the entry already exists (but updates
* the entry with a new timestamp) and 1 if the
* entry did not previously exist.
*
* Implements a simple LRU cache replacement
* policy. Uses a linear search, which should be
* okay, as long as SNMP_ADDRCACHE_SIZE remains
* relatively small.
*
* @retval 0 : updated existing entry
* @retval 1 : added new entry
*/
int Agent_addrcacheAdd( const char* addr )
{
    int oldest = -1; /* Index of the oldest cache entry */
    int unused = -1; /* Index of the first free cache entry */
    int i; /* Looping variable */
    int rc = -1;
    struct timeval now; /* What time is it now? */
    struct timeval aged; /* Oldest allowable cache entry */

    /*
    * First get the current and oldest allowable timestamps
    */
    Time_getMonotonicClock( &now );
    aged.tv_sec = now.tv_sec - SNMP_ADDRCACHE_MAXAGE;
    aged.tv_usec = now.tv_usec;

    /*
    * Now look for a place to put this thing
    */
    for ( i = 0; i < SNMP_ADDRCACHE_SIZE; i++ ) {
        if ( _agent_addrCache[ i ].status == SNMP_ADDRCACHE_UNUSED ) { /* If unused */
            /*
            * remember this location, in case addr isn't in the cache
            */
            if ( unused < 0 )
                unused = i;
        } else { /* If used */
            if ( ( NULL != addr ) && ( strcmp( _agent_addrCache[ i ].addr, addr ) == 0 ) ) {
                /*
                * found a match
                */
                _agent_addrCache[ i ].lastHitM = now;
                if ( timercmp( &_agent_addrCache[ i ].lastHitM, &aged, < ) )
                    rc = 1; /* should have expired, so is new */
                else
                    rc = 0; /* not expired, so is existing entry */
                break;
            } else {
                /*
                * Used, but not this address. check if it's stale.
                */
                if ( timercmp( &_agent_addrCache[ i ].lastHitM, &aged, < ) ) {
                    /*
                    * Stale, reuse
                    */
                    MEMORY_FREE( _agent_addrCache[ i ].addr );
                    _agent_addrCache[ i ].status = SNMP_ADDRCACHE_UNUSED;
                    /*
                    * remember this location, in case addr isn't in the cache
                    */
                    if ( unused < 0 )
                        unused = i;
                } else {
                    /*
                    * Still fresh, but a candidate for LRU replacement
                    */
                    if ( oldest < 0 )
                        oldest = i;
                    else if ( timercmp( &_agent_addrCache[ i ].lastHitM,
                                  &_agent_addrCache[ oldest ].lastHitM, < ) )
                        oldest = i;
                } /* fresh */
            } /* used, no match */
        } /* used */
    } /* for loop */

    if ( ( -1 == rc ) && ( NULL != addr ) ) {
        /*
        * We didn't find the entry in the cache
        */
        if ( unused >= 0 ) {
            /*
            * If we have a slot free anyway, use it
            */
            _agent_addrCache[ unused ].addr = strdup( addr );
            _agent_addrCache[ unused ].status = SNMP_ADDRCACHE_USED;
            _agent_addrCache[ unused ].lastHitM = now;
        } else { /* Otherwise, replace oldest entry */
            if ( DefaultStore_getBoolean( DsStore_APPLICATION_ID,
                     DsAgentBoolean_VERBOSE ) )
                Logger_log( LOGGER_PRIORITY_INFO, "Purging address from address cache: %s",
                    _agent_addrCache[ oldest ].addr );

            free( _agent_addrCache[ oldest ].addr );
            _agent_addrCache[ oldest ].addr = strdup( addr );
            _agent_addrCache[ oldest ].lastHitM = now;
        }
        rc = 1;
    }
    if ( ( agent_logAddresses && ( 1 == rc ) ) || DefaultStore_getBoolean( DsStore_APPLICATION_ID,
                                                      DsAgentBoolean_VERBOSE ) ) {
        Logger_log( LOGGER_PRIORITY_INFO, "Received SNMP packet(s) from %s\n", addr );
    }

    return rc;
}

/*
* Age the entries in the address cache.
*
* backwards compatability; not used anywhere
*/
void Agent_addrcacheAge( void )
{
    ( void )Agent_addrcacheAdd( NULL );
}

/*******************************************************************-o-******
* Agent_checkPacket
*
* Parameters:
*	session, transport, transport_data, transport_data_length
*
* Returns:
*	1	On success.
*	0	On error.
*
* Handler for all incoming messages (a.k.a. packets) for the agent.  If using
* the libwrap utility, log the connection and deny/allow the access. Print
* output when appropriate, and increment the incoming counter.
*
*/

int Agent_checkPacket( Types_Session* session,
    Transport_Transport* transport,
    void* transport_data,
    int transport_data_length )
{
    char* addr_string = NULL;

    /*
    * Log the message and/or dump the message.
    * Optionally cache the network address of the sender.
    */

    if ( transport != NULL && transport->f_fmtaddr != NULL ) {
        /*
        * Okay I do know how to format this address for logging.
        */
        addr_string = transport->f_fmtaddr( transport, transport_data,
            transport_data_length );
        /*
        * Don't forget to free() it.
        */
    }

    Api_incrementStatistic( API_STAT_SNMPINPKTS );

    if ( addr_string != NULL ) {
        Agent_addrcacheAdd( addr_string );
        MEMORY_FREE( addr_string );
    }
    return 1;
}

int Agent_checkParse( Types_Session* session, Types_Pdu* pdu,
    int result )
{
    if ( result == 0 ) {
        if ( Logger_isDoLogging() && DefaultStore_getBoolean( DsStore_APPLICATION_ID,
                                         DsAgentBoolean_VERBOSE ) ) {
            VariableList* var_ptr;

            switch ( pdu->command ) {
            case PRIOT_MSG_GET:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  GET message\n" );
                break;
            case PRIOT_MSG_GETNEXT:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  GETNEXT message\n" );
                break;
            case PRIOT_MSG_RESPONSE:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  RESPONSE message\n" );
                break;
            case PRIOT_MSG_SET:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  SET message\n" );
                break;
            case PRIOT_MSG_TRAP:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  TRAP message\n" );
                break;
            case PRIOT_MSG_GETBULK:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  GETBULK message, non-rep=%ld, max_rep=%ld\n",
                    pdu->errstat, pdu->errindex );
                break;
            case PRIOT_MSG_INFORM:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  INFORM message\n" );
                break;
            case PRIOT_MSG_TRAP2:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  TRAP2 message\n" );
                break;
            case PRIOT_MSG_REPORT:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  REPORT message\n" );
                break;

            case PRIOT_MSG_INTERNAL_SET_RESERVE1:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  INTERNAL IMPL_RESERVE1 message\n" );
                break;

            case PRIOT_MSG_INTERNAL_SET_RESERVE2:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  INTERNAL IMPL_RESERVE2 message\n" );
                break;

            case PRIOT_MSG_INTERNAL_SET_ACTION:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  INTERNAL IMPL_ACTION message\n" );
                break;

            case PRIOT_MSG_INTERNAL_SET_COMMIT:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  INTERNAL IMPL_COMMIT message\n" );
                break;

            case PRIOT_MSG_INTERNAL_SET_FREE:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  INTERNAL IMPL_FREE message\n" );
                break;

            case PRIOT_MSG_INTERNAL_SET_UNDO:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  INTERNAL IMPL_UNDO message\n" );
                break;

            default:
                Logger_log( LOGGER_PRIORITY_DEBUG, "  UNKNOWN message, type=%02X\n",
                    pdu->command );
                Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
                return 0;
            }

            for ( var_ptr = pdu->variables; var_ptr != NULL;
                  var_ptr = var_ptr->next ) {
                size_t c_oidlen = 256, c_outlen = 0;
                u_char* c_oid = ( u_char* )malloc( c_oidlen );

                if ( c_oid ) {
                    if ( !Mib_sprintReallocObjid2( &c_oid, &c_oidlen, &c_outlen, 1, var_ptr->name,
                             var_ptr->nameLength ) ) {
                        Logger_log( LOGGER_PRIORITY_DEBUG, "    -- %s [TRUNCATED]\n",
                            c_oid );
                    } else {
                        Logger_log( LOGGER_PRIORITY_DEBUG, "    -- %s\n", c_oid );
                    }
                    MEMORY_FREE( c_oid );
                }
            }
        }
        return 1;
    }
    return 0; /* XXX: does it matter what the return value
                                * is?  Yes: if we return 0, then the PDU is
                                * dumped.  */
}

/*
* Global access to the primary session structure for this agent.
* for Index Allocation use initially.
*/

/*
* I don't understand what this is for at the moment.  AFAICS as long as it
* gets set and points at a session, that's fine.  ???
*/

Types_Session* agent_mainSession = NULL;

/*
* Set up an agent session on the given transport.  Return a handle
* which may later be used to de-register this transport.  A return
* value of -1 indicates an error.
*/

int Agent_registerAgentNsap( Transport_Transport* t )
{
    Types_Session *s, *sp = NULL;
    AgentNsap *a = NULL, *n = NULL, **prevNext = &_agent_agentNsapList;
    int handle = 0;
    void* isp = NULL;

    if ( t == NULL ) {
        return -1;
    }

    DEBUG_MSGTL( ( "Agent_registerAgentNsap", "fd %d\n", t->sock ) );

    n = ( AgentNsap* )malloc( sizeof( AgentNsap ) );
    if ( n == NULL ) {
        return -1;
    }
    s = ( Types_Session* )malloc( sizeof( Types_Session ) );
    if ( s == NULL ) {
        MEMORY_FREE( n );
        return -1;
    }
    memset( s, 0, sizeof( Types_Session ) );
    Api_sessInit( s );

    /*
    * Set up the session appropriately for an agent.
    */

    s->version = API_DEFAULT_VERSION;
    s->callback = Agent_handlePriotPacket;
    s->authenticator = NULL;
    s->flags = DefaultStore_getInt( DsStore_APPLICATION_ID,
        DsAgentInterger_FLAGS );
    s->isAuthoritative = API_SESS_AUTHORITATIVE;

    /* Optional supplimental transport configuration information and
      final call to actually open the transport */
    if ( Api_sessConfigTransport( s->transportConfiguration, t )
        != ErrorCode_SUCCESS ) {
        MEMORY_FREE( s );
        MEMORY_FREE( n );
        return -1;
    }

    if ( t->f_open )
        t = t->f_open( t );

    if ( NULL == t ) {
        MEMORY_FREE( s );
        MEMORY_FREE( n );
        return -1;
    }

    t->flags |= TRANSPORT_FLAG_OPENED;

    sp = Api_add( s, t, Agent_checkPacket,
        Agent_checkParse );
    if ( sp == NULL ) {
        MEMORY_FREE( s );
        MEMORY_FREE( n );
        return -1;
    }

    isp = Api_sessPointer( sp );
    if ( isp == NULL ) { /*  over-cautious  */
        MEMORY_FREE( s );
        MEMORY_FREE( n );
        return -1;
    }

    n->s = isp;
    n->t = t;

    if ( agent_mainSession == NULL ) {
        agent_mainSession = Api_sessSession( isp );
    }

    for ( a = _agent_agentNsapList; a != NULL && handle + 1 >= a->handle;
          a = a->next ) {
        handle = a->handle;
        prevNext = &( a->next );
    }

    if ( handle < INT_MAX ) {
        n->handle = handle + 1;
        n->next = a;
        *prevNext = n;
        MEMORY_FREE( s );
        return n->handle;
    } else {
        MEMORY_FREE( s );
        MEMORY_FREE( n );
        return -1;
    }
}

void Agent_deregisterAgentNsap( int handle )
{
    AgentNsap *a = NULL, **prevNext = &_agent_agentNsapList;
    int main_session_deregistered = 0;

    DEBUG_MSGTL( ( "Agent_deregisterAgentNsap", "handle %d\n", handle ) );

    for ( a = _agent_agentNsapList; a != NULL && a->handle < handle; a = a->next ) {
        prevNext = &( a->next );
    }

    if ( a != NULL && a->handle == handle ) {
        *prevNext = a->next;
        if ( Api_sessSessionLookup( a->s ) ) {
            if ( agent_mainSession == Api_sessSession( a->s ) ) {
                main_session_deregistered = 1;
            }
            Api_close( Api_sessSession( a->s ) );
            /*
            * The above free()s the transport and session pointers.
            */
        }
        MEMORY_FREE( a );
    }

    /*
    * If we've deregistered the session that agent_mainSession used to point to,
    * then make it point to another one, or in the last resort, make it equal
    * to NULL.  Basically this shouldn't ever happen in normal operation
    * because agent_mainSession starts off pointing at the first session added by
    * Agent_initMasterAgent(), which then discards the handle.
    */

    if ( main_session_deregistered ) {
        if ( _agent_agentNsapList != NULL ) {
            DEBUG_MSGTL( ( "snmp_agent",
                "WARNING: agent_mainSession ptr changed from %p to %p\n",
                agent_mainSession, Api_sessSession( _agent_agentNsapList->s ) ) );
            agent_mainSession = Api_sessSession( _agent_agentNsapList->s );
        } else {
            DEBUG_MSGTL( ( "snmp_agent",
                "WARNING: agent_mainSession ptr changed from %p to NULL\n",
                agent_mainSession ) );
            agent_mainSession = NULL;
        }
    }
}

/*
*
* This function has been modified to use the experimental
* Agent_registerAgentNsap interface.  The major responsibility of this
* function now is to interpret a string specified to the agent (via -p on the
* command line, or from a configuration file) as a list of agent NSAPs on
* which to listen for SNMP packets.  Typically, when you add a new transport
* domain "foo", you add code here such that if the "foo" code is compiled
* into the agent (SNMP_TRANSPORT_FOO_DOMAIN is defined), then a token of the
* form "foo:bletch-3a0054ef%wob&wob" gets turned into the appropriate
* transport descriptor.  Agent_registerAgentNsap is then called with that
* transport descriptor and sets up a listening agent session on it.
*
* Everything then works much as normal: the agent runs in an infinite loop
* (in the snmpd.c/receive()routine), which calls snmp_read() when a request
* is readable on any of the given transports.  This routine then traverses
* the library 'Sessions' list to identify the relevant session and eventually
* invokes '_sess_read'.  This then processes the incoming packet, calling the
* pre_parse, parse, post_parse and callback routines in turn.
*
* JBPN 20001117
*/

int Agent_initMasterAgent( void )
{
    Transport_Transport* transport;
    char* cptr;
    char* buf = NULL;
    char* st;

    /* default to a default cache size */
    AgentRegistry_setLookupCacheSize( -1 );

    if ( DefaultStore_getBoolean( DsStore_APPLICATION_ID,
             DsAgentBoolean_ROLE )
        != MASTER_AGENT ) {
        DEBUG_MSGTL( ( "snmp_agent",
            "Agent_initMasterAgent; not master agent\n" ) );

        Assert_assert( "agent role !master && !sub_agent" );

        return 0; /*  No error if ! MASTER_AGENT  */
    }

    /*
    * Have specific agent ports been specified?
    */
    cptr = DefaultStore_getString( DsStore_APPLICATION_ID,
        DsAgentString_PORTS );

    if ( cptr ) {
        buf = strdup( cptr );
        if ( !buf ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "Error processing transport \"%s\"\n", cptr );
            return 1;
        }
    } else {
        /*
        * No, so just specify the default port.
        */
        buf = strdup( "" );
    }

    DEBUG_MSGTL( ( "snmp_agent", "final port spec: \"%s\"\n", buf ) );
    st = buf;
    do {
        /*
        * Specification format:
        *
        * NONE:                      (a pseudo-transport)
        * UDP:[address:]port        (also default if no transport is specified)
        * TCP:[address:]port         (if supported)
        * Unix:pathname              (if supported)
        * AAL5PVC:itf.vpi.vci        (if supported)
        * IPX:[network]:node[/port] (if supported)
        *
        */

        cptr = st;
        st = strchr( st, ',' );
        if ( st )
            *st++ = '\0';

        DEBUG_MSGTL( ( "snmp_agent", "installing master agent on port %s\n",
            cptr ) );

        if ( strncasecmp( cptr, "none", 4 ) == 0 ) {
            DEBUG_MSGTL( ( "snmp_agent",
                "Agent_initMasterAgent; pseudo-transport \"none\" "
                "requested\n" ) );
            break;
        }
        transport = Transport_openServer( "priot", cptr );

        if ( transport == NULL ) {
            Logger_log( LOGGER_PRIORITY_ERR, "Error opening specified endpoint \"%s\"\n",
                cptr );
            return 1;
        }

        if ( Agent_registerAgentNsap( transport ) == 0 ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "Error registering specified transport \"%s\" as an "
                "agent NSAP\n",
                cptr );
            return 1;
        } else {
            DEBUG_MSGTL( ( "snmp_agent",
                "Agent_initMasterAgent; \"%s\" registered as an agent "
                "NSAP\n",
                cptr ) );
        }
    } while ( st && *st != '\0' );
    MEMORY_FREE( buf );

    if ( DefaultStore_getBoolean( DsStore_APPLICATION_ID,
             DsAgentBoolean_AGENTX_MASTER )
        == 1 )
        Master_realInitMaster();

    return 0;
}

void Agent_clearNsapList( void )
{
    DEBUG_MSGTL( ( "Agent_clearNsapList", "clear the nsap list\n" ) );

    while ( _agent_agentNsapList != NULL )
        Agent_deregisterAgentNsap( _agent_agentNsapList->handle );
}

void Agent_shutdownMasterAgent( void )
{
    Agent_clearNsapList();
}

AgentSession*
Agent_initAgentPriotSession( Types_Session* session, Types_Pdu* pdu )
{
    AgentSession* asp = ( AgentSession* )
        calloc( 1, sizeof( AgentSession ) );

    if ( asp == NULL ) {
        return NULL;
    }

    DEBUG_MSGTL( ( "snmp_agent", "agent_sesion %8p created\n", asp ) );
    asp->session = session;
    asp->pdu = Client_clonePdu( pdu );
    asp->orig_pdu = Client_clonePdu( pdu );
    asp->rw = IMPL_READ;
    asp->exact = TRUE;
    asp->next = NULL;
    asp->mode = IMPL_RESERVE1;
    asp->status = PRIOT_ERR_NOERROR;
    asp->index = 0;
    asp->oldmode = 0;
    asp->treecache_num = -1;
    asp->treecache_len = 0;
    asp->reqinfo = MEMORY_MALLOC_TYPEDEF( AgentRequestInfo );
    DEBUG_MSGTL( ( "verbose:asp", "asp %p reqinfo %p created\n",
        asp, asp->reqinfo ) );

    return asp;
}

void Agent_freeAgentPriotSession( AgentSession* asp )
{
    if ( !asp )
        return;

    DEBUG_MSGTL( ( "snmp_agent", "agent_session %8p released\n", asp ) );

    Agent_removeFromDelegated( asp );

    DEBUG_MSGTL( ( "verbose:asp", "asp %p reqinfo %p freed\n",
        asp, asp->reqinfo ) );
    if ( asp->orig_pdu )
        Api_freePdu( asp->orig_pdu );
    if ( asp->pdu )
        Api_freePdu( asp->pdu );
    if ( asp->reqinfo )
        Agent_freeAgentRequestInfo( asp->reqinfo );
    MEMORY_FREE( asp->treecache );
    MEMORY_FREE( asp->bulkcache );
    if ( asp->requests ) {
        int i;
        for ( i = 0; i < asp->vbcount; i++ ) {
            AgentHandler_freeRequestDataSets( &asp->requests[ i ] );
        }
        MEMORY_FREE( asp->requests );
    }
    if ( asp->cache_store ) {
        Agent_freeCachemap( asp->cache_store );
        asp->cache_store = NULL;
    }
    MEMORY_FREE( asp );
}

int Agent_checkForDelegated( AgentSession* asp )
{
    int i;
    RequestInfo* request;

    if ( NULL == asp->treecache )
        return 0;

    for ( i = 0; i <= asp->treecache_num; i++ ) {
        for ( request = asp->treecache[ i ].requests_begin; request;
              request = request->next ) {
            if ( request->delegated )
                return 1;
        }
    }
    return 0;
}

int Agent_checkDelegatedChainFor( AgentSession* asp )
{
    AgentSession* asptmp;
    for ( asptmp = agent_delegated_list; asptmp; asptmp = asptmp->next ) {
        if ( asptmp == asp )
            return 1;
    }
    return 0;
}

int Agent_checkForDelegatedAndAdd( AgentSession* asp )
{
    if ( Agent_checkForDelegated( asp ) ) {
        if ( !Agent_checkDelegatedChainFor( asp ) ) {
            /*
            * add to delegated request chain
            */
            asp->next = agent_delegated_list;
            agent_delegated_list = asp;
            DEBUG_MSGTL( ( "snmp_agent", "delegate session == %8p\n", asp ) );
        }
        return 1;
    }
    return 0;
}

int Agent_removeFromDelegated( AgentSession* asp )
{
    AgentSession *curr, *prev = NULL;

    for ( curr = agent_delegated_list; curr; prev = curr, curr = curr->next ) {
        /*
        * is this us?
        */
        if ( curr != asp )
            continue;

        /*
        * remove from queue
        */
        if ( prev != NULL )
            prev->next = asp->next;
        else
            agent_delegated_list = asp->next;

        DEBUG_MSGTL( ( "snmp_agent", "remove delegated session == %8p\n", asp ) );

        return 1;
    }

    return 0;
}

/*
* Agent_removeDelegatedRequestsForSession
*
* called when a session is being closed. Check all delegated requests to
* see if the are waiting on this session, and if set, set the status for
* that request to GENERR.
*/
int Agent_removeDelegatedRequestsForSession( Types_Session* sess )
{
    AgentSession* asp;
    int count = 0;

    for ( asp = agent_delegated_list; asp; asp = asp->next ) {
        /*
        * check each request
        */
        RequestInfo* request;
        for ( request = asp->requests; request; request = request->next ) {
            /*
            * check session
            */
            Assert_assert( NULL != request->subtree );
            if ( request->subtree->session != sess )
                continue;

            /*
            * matched! mark request as done
            */
            Agent_requestSetError( request, PRIOT_ERR_GENERR );
            ++count;
        }
    }

    /*
    * if we found any, that request may be finished now
    */
    if ( count ) {
        DEBUG_MSGTL( ( "snmp_agent", "removed %d delegated request(s) for session "
                                     "%8p\n",
            count, sess ) );
        Agent_checkOutstandingAgentRequests();
    }

    return count;
}

int Agent_checkQueuedChainFor( AgentSession* asp )
{
    AgentSession* asptmp;
    for ( asptmp = agent_agentQueuedList; asptmp; asptmp = asptmp->next ) {
        if ( asptmp == asp )
            return 1;
    }
    return 0;
}

int Agent_addQueued( AgentSession* asp )
{
    AgentSession* asp_tmp;

    /*
    * first item?
    */
    if ( NULL == agent_agentQueuedList ) {
        agent_agentQueuedList = asp;
        return 1;
    }

    /*
    * add to end of queued request chain
    */
    asp_tmp = agent_agentQueuedList;
    for ( ; asp_tmp; asp_tmp = asp_tmp->next ) {
        /*
        * already in queue?
        */
        if ( asp_tmp == asp )
            break;

        /*
        * end of queue?
        */
        if ( NULL == asp_tmp->next )
            asp_tmp->next = asp;
    }
    return 1;
}

int Agent_wrapUpRequest( AgentSession* asp, int status )
{
    /*
    * if this request was a set, clear the global now that we are
    * done.
    */
    if ( asp == agent_processingSet ) {
        DEBUG_MSGTL( ( "snmp_agent", "SET request complete, asp = %8p\n",
            asp ) );
        agent_processingSet = NULL;
    }

    if ( asp->pdu ) {
        /*
        * If we've got an error status, then this needs to be
        *  passed back up to the higher levels....
        */
        if ( status != 0 && asp->status == 0 )
            asp->status = status;

        switch ( asp->pdu->command ) {
        case PRIOT_MSG_INTERNAL_SET_BEGIN:
        case PRIOT_MSG_INTERNAL_SET_RESERVE1:
        case PRIOT_MSG_INTERNAL_SET_RESERVE2:
        case PRIOT_MSG_INTERNAL_SET_ACTION:
            /*
                * some stuff needs to be saved in special subagent cases
                */
            Agent_saveSetCache( asp );
            break;

        case PRIOT_MSG_GETNEXT:
            _Agent_fixEndofmibview( asp );
            break;

        case PRIOT_MSG_GETBULK:
            /*
                * for a GETBULK response we need to rearrange the varbinds
                */
            _Agent_reorderGetbulk( asp );
            break;
        }
    } /** if asp->pdu */

/*
    * Update the snmp error-count statistics
    *   XXX - should we include the V2 errors in this or not?
    */
#define INCLUDE_V2ERRORS_IN_V1STATS

    switch ( status ) {
    case PRIOT_ERR_WRONGVALUE:
    case PRIOT_ERR_WRONGENCODING:
    case PRIOT_ERR_WRONGTYPE:
    case PRIOT_ERR_WRONGLENGTH:
    case PRIOT_ERR_INCONSISTENTVALUE:
    case PRIOT_ERR_BADVALUE:
        Api_incrementStatistic( API_STAT_SNMPOUTBADVALUES );
        break;
    case PRIOT_ERR_NOACCESS:
    case PRIOT_ERR_NOTWRITABLE:
    case PRIOT_ERR_NOCREATION:
    case PRIOT_ERR_INCONSISTENTNAME:
    case PRIOT_ERR_AUTHORIZATIONERROR:
    case PRIOT_ERR_NOSUCHNAME:
        Api_incrementStatistic( API_STAT_SNMPOUTNOSUCHNAMES );
        break;
    case PRIOT_ERR_RESOURCEUNAVAILABLE:
    case PRIOT_ERR_COMMITFAILED:
    case PRIOT_ERR_UNDOFAILED:
    case PRIOT_ERR_GENERR:
        Api_incrementStatistic( API_STAT_SNMPOUTGENERRS );
        break;

    case PRIOT_ERR_TOOBIG:
        Api_incrementStatistic( API_STAT_SNMPOUTTOOBIGS );
        break;
    }

    if ( ( status == PRIOT_ERR_NOERROR ) && ( asp->pdu ) ) {
        Api_incrementStatisticBy( ( asp->pdu->command == PRIOT_MSG_SET ? API_STAT_SNMPINTOTALSETVARS : API_STAT_SNMPINTOTALREQVARS ),
            Client_countVarbinds( asp->pdu->variables ) );

    } else {
        /*
        * Use a copy of the original request
        *   to report failures.
        */
        Api_freePdu( asp->pdu );
        asp->pdu = asp->orig_pdu;
        asp->orig_pdu = NULL;
    }
    if ( asp->pdu ) {
        asp->pdu->command = PRIOT_MSG_RESPONSE;
        asp->pdu->errstat = asp->status;
        asp->pdu->errindex = asp->index;
        if ( !Api_send( asp->session, asp->pdu ) && asp->session->s_snmp_errno != ErrorCode_SUCCESS ) {
            VariableList* var_ptr;
            Api_perror( "send response" );
            for ( var_ptr = asp->pdu->variables; var_ptr != NULL;
                  var_ptr = var_ptr->next ) {
                size_t c_oidlen = 256, c_outlen = 0;
                u_char* c_oid = ( u_char* )malloc( c_oidlen );

                if ( c_oid ) {
                    if ( !Mib_sprintReallocObjid2( &c_oid, &c_oidlen, &c_outlen, 1,
                             var_ptr->name,
                             var_ptr->nameLength ) ) {
                        Logger_log( LOGGER_PRIORITY_ERR, "    -- %s [TRUNCATED]\n", c_oid );
                    } else {
                        Logger_log( LOGGER_PRIORITY_ERR, "    -- %s\n", c_oid );
                    }
                    MEMORY_FREE( c_oid );
                }
            }
            Api_freePdu( asp->pdu );
            asp->pdu = NULL;
        }
        Api_incrementStatistic( API_STAT_SNMPOUTPKTS );
        Api_incrementStatistic( API_STAT_SNMPOUTGETRESPONSES );
        asp->pdu = NULL; /* yyy-rks: redundant, no? */
        Agent_removeAndFreeAgentPriotSession( asp );
    }
    return 1;
}

void Agent_dumpSessList( void )
{
    AgentSession* a;

    DEBUG_MSGTL( ( "snmp_agent", "DUMP agent_sess_list -> " ) );
    for ( a = _agent_sessionList; a != NULL; a = a->next ) {
        DEBUG_MSG( ( "snmp_agent", "%8p[session %8p] -> ", a, a->session ) );
    }
    DEBUG_MSG( ( "snmp_agent", "[NIL]\n" ) );
}

void Agent_removeAndFreeAgentPriotSession( AgentSession* asp )
{
    AgentSession *a, **prevNext = &_agent_sessionList;

    DEBUG_MSGTL( ( "snmp_agent", "REMOVE session == %8p\n", asp ) );

    for ( a = _agent_sessionList; a != NULL; a = *prevNext ) {
        if ( a == asp ) {
            *prevNext = a->next;
            a->next = NULL;
            Agent_freeAgentPriotSession( a );
            asp = NULL;
            break;
        } else {
            prevNext = &( a->next );
        }
    }

    if ( a == NULL && asp != NULL ) {
        /*
        * We coulnd't find it on the list, so free it anyway.
        */
        Agent_freeAgentPriotSession( asp );
    }
}

void Agent_freeAgentPriotSessionBySession( Types_Session* sess,
    void ( *free_request )( Api_RequestList* ) )
{
    AgentSession *a, *next, **prevNext = &_agent_sessionList;

    DEBUG_MSGTL( ( "snmp_agent", "REMOVE session == %8p\n", sess ) );

    for ( a = _agent_sessionList; a != NULL; a = next ) {
        if ( a->session == sess ) {
            *prevNext = a->next;
            next = a->next;
            Agent_freeAgentPriotSession( a );
        } else {
            prevNext = &( a->next );
            next = a->next;
        }
    }
}

/** handles an incoming SNMP packet into the agent */
int Agent_handlePriotPacket( int op, Types_Session* session, int reqid,
    Types_Pdu* pdu, void* magic )
{
    AgentSession* asp;
    int status, access_ret, rc;

    /*
    * We only support receiving here.
    */
    if ( op != API_CALLBACK_OP_RECEIVED_MESSAGE ) {
        return 1;
    }

    /*
    * RESPONSE messages won't get this far, but TRAP-like messages
    * might.
    */
    if ( pdu->command == PRIOT_MSG_TRAP || pdu->command == PRIOT_MSG_INFORM || pdu->command == PRIOT_MSG_TRAP2 ) {
        DEBUG_MSGTL( ( "snmp_agent", "received trap-like PDU (%02x)\n",
            pdu->command ) );
        pdu->command = PRIOT_MSG_TRAP2;
        Api_incrementStatistic( API_STAT_SNMPUNKNOWNPDUHANDLERS );
        return 1;
    }

    /*
    * send snmpv3 authfail trap.
    */
    if ( pdu->version == PRIOT_VERSION_3 && session->s_snmp_errno == ErrorCode_USM_AUTHENTICATIONFAILURE ) {
        Trap_sendEasyTrap( PRIOT_TRAP_AUTHFAIL, 0 );
        return 1;
    }

    if ( magic == NULL ) {
        asp = Agent_initAgentPriotSession( session, pdu );
        status = PRIOT_ERR_NOERROR;
    } else {
        asp = ( AgentSession* )magic;
        status = asp->status;
    }

    if ( ( access_ret = AgentRegistry_checkAccess( asp->pdu ) ) != 0 ) {
        if ( access_ret == VACM_NOSUCHCONTEXT ) {
            /*
            * rfc3413 section 3.2, step 5 says that we increment the
            * counter but don't return a response of any kind
            */

            /*
            * we currently don't support unavailable contexts, as
            * there is no reason to that I currently know of
            */
            Api_incrementStatistic( API_STAT_SNMPUNKNOWNCONTEXTS );

            /*
            * drop the request
            */
            Agent_removeAndFreeAgentPriotSession( asp );
            return 0;
        } else {
            /*
            * access control setup is incorrect
            */
            Trap_sendEasyTrap( PRIOT_TRAP_AUTHFAIL, 0 );

            /*
            * drop the request
            */
            Agent_removeAndFreeAgentPriotSession( asp );
            return 0;
        }
    }

    rc = Agent_handleRequest( asp, status );

    /*
    * done
    */
    DEBUG_MSGTL( ( "snmp_agent", "end of Agent_handlePriotPacket, asp = %8p\n",
        asp ) );
    return rc;
}

RequestInfo*
Agent_addVarbindToCache( AgentSession* asp, int vbcount,
    VariableList* varbind_ptr,
    Subtree* tp )
{
    RequestInfo* request = NULL;

    DEBUG_MSGTL( ( "snmp_agent", "add_vb_to_cache(%8p, %d, ", asp, vbcount ) );
    DEBUG_MSGOID( ( "snmp_agent", varbind_ptr->name,
        varbind_ptr->nameLength ) );
    DEBUG_MSG( ( "snmp_agent", ", %8p)\n", tp ) );

    if ( tp && ( asp->pdu->command == PRIOT_MSG_GETNEXT || asp->pdu->command == PRIOT_MSG_GETBULK ) ) {
        int result;
        int prefix_len;

        prefix_len = Api_oidFindPrefix( tp->start_a,
            tp->start_len,
            tp->end_a, tp->end_len );
        if ( prefix_len < 1 ) {
            result = VACM_NOTINVIEW; /* ack...  bad bad thing happened */
        } else {
            result = AgentRegistry_acmCheckSubtree( asp->pdu, tp->start_a, prefix_len );
        }

        while ( result == VACM_NOTINVIEW ) {
            /* the entire subtree is not in view. Skip it. */
            /** @todo make this be more intelligent about ranges.
               Right now we merely take the highest level
               commonality of a registration range and use that.
               At times we might be able to be smarter about
               checking the range itself as opposed to the node
               above where the range exists, but I doubt this will
               come up all that frequently. */
            tp = tp->next;
            if ( tp ) {
                prefix_len = Api_oidFindPrefix( tp->start_a,
                    tp->start_len,
                    tp->end_a,
                    tp->end_len );
                if ( prefix_len < 1 ) {
                    /* ack...  bad bad thing happened */
                    result = VACM_NOTINVIEW;
                } else {
                    result = AgentRegistry_acmCheckSubtree( asp->pdu,
                        tp->start_a, prefix_len );
                }
            } else
                break;
        }
    }
    if ( tp == NULL ) {
        /*
        * no appropriate registration found
        */
        /*
        * make up the response ourselves
        */
        switch ( asp->pdu->command ) {
        case PRIOT_MSG_GETNEXT:
        case PRIOT_MSG_GETBULK:
            varbind_ptr->type = PRIOT_ENDOFMIBVIEW;
            break;

        case PRIOT_MSG_SET:
        case PRIOT_MSG_GET:
            varbind_ptr->type = PRIOT_NOSUCHOBJECT;
            break;

        default:
            return NULL; /* shouldn't get here */
        }
    } else {
        int cacheid;

        DEBUG_MSGTL( ( "snmp_agent", "tp->start " ) );
        DEBUG_MSGOID( ( "snmp_agent", tp->start_a, tp->start_len ) );
        DEBUG_MSG( ( "snmp_agent", ", tp->end " ) );
        DEBUG_MSGOID( ( "snmp_agent", tp->end_a, tp->end_len ) );
        DEBUG_MSG( ( "snmp_agent", ", \n" ) );

        /*
        * malloc the request structure
        */
        request = &( asp->requests[ vbcount - 1 ] );
        request->index = vbcount;
        request->delegated = 0;
        request->processed = 0;
        request->status = 0;
        request->subtree = tp;
        request->agent_req_info = asp->reqinfo;
        if ( request->parent_data ) {
            AgentHandler_freeRequestDataSets( request );
        }
        DEBUG_MSGTL( ( "verbose:asp", "asp %p reqinfo %p assigned to request\n",
            asp, asp->reqinfo ) );

        /*
        * for non-SET modes, set the type to NULL
        */
        if ( !MODE_IS_SET( asp->pdu->command ) ) {
            DEBUG_MSGTL( ( "verbose:asp", "asp %p reqinfo %p assigned to request\n",
                asp, asp->reqinfo ) );
            if ( varbind_ptr->type == ASN01_PRIV_INCL_RANGE ) {
                DEBUG_MSGTL( ( "snmp_agent", "varbind %d is inclusive\n",
                    request->index ) );
                request->inclusive = 1;
            }
            varbind_ptr->type = ASN01_NULL;
        }

        /*
        * place them in a cache
        */
        if ( tp->global_cacheid ) {
            /*
            * we need to merge all marked subtrees together
            */
            if ( asp->cache_store && -1 != ( cacheid = Agent_getLocalCachid( asp->cache_store, tp->global_cacheid ) ) ) {
            } else {
                cacheid = ++( asp->treecache_num );
                Agent_getOrAddLocalCachid( &asp->cache_store,
                    tp->global_cacheid,
                    cacheid );
                goto goto_mallocslot; /* XXX: ick */
            }
        } else if ( tp->cacheid > -1 && tp->cacheid <= asp->treecache_num && asp->treecache[ tp->cacheid ].subtree == tp ) {
            /*
            * we have already added a request to this tree
            * pointer before
            */
            cacheid = tp->cacheid;
        } else {
            cacheid = ++( asp->treecache_num );
        goto_mallocslot:
            /*
            * new slot needed
            */
            if ( asp->treecache_num >= asp->treecache_len ) {
/*
                * exapand cache array
                */
/*
                * WWW: non-linear expansion needed (with cap)
                */
#define CACHE_GROW_SIZE 16
                asp->treecache_len = ( asp->treecache_len + CACHE_GROW_SIZE );
                asp->treecache = ( TreeCache* )realloc( asp->treecache,
                    sizeof( TreeCache ) * asp->treecache_len );
                if ( asp->treecache == NULL )
                    return NULL;
                memset( &( asp->treecache[ cacheid ] ), 0x00,
                    sizeof( TreeCache ) * ( CACHE_GROW_SIZE ) );
            }
            asp->treecache[ cacheid ].subtree = tp;
            asp->treecache[ cacheid ].requests_begin = request;
            tp->cacheid = cacheid;
        }

        /*
        * if this is a search type, get the ending range oid as well
        */
        if ( asp->pdu->command == PRIOT_MSG_GETNEXT || asp->pdu->command == PRIOT_MSG_GETBULK ) {
            request->range_end = tp->end_a;
            request->range_end_len = tp->end_len;
        } else {
            request->range_end = NULL;
            request->range_end_len = 0;
        }

        /*
        * link into chain
        */
        if ( asp->treecache[ cacheid ].requests_end )
            asp->treecache[ cacheid ].requests_end->next = request;
        request->next = NULL;
        request->prev = asp->treecache[ cacheid ].requests_end;
        asp->treecache[ cacheid ].requests_end = request;

        /*
        * add the given request to the list of requests they need
        * to handle results for
        */
        request->requestvb = request->requestvb_start = varbind_ptr;
    }
    return request;
}

/*
* check the ACM(s) for the results on each of the varbinds.
* If ACM disallows it, replace the value with type
*
* Returns number of varbinds with ACM errors
*/
int Agent_checkAcm( AgentSession* asp, u_char type )
{
    int view;
    int i, j, k;
    RequestInfo* request;
    int ret = 0;
    VariableList *vb, *vb2, *vbc;
    int earliest = 0;

    for ( i = 0; i <= asp->treecache_num; i++ ) {
        for ( request = asp->treecache[ i ].requests_begin;
              request; request = request->next ) {
            /*
            * for each request, run it through AgentRegistry_inAView()
            */
            earliest = 0;
            for ( j = request->repeat, vb = request->requestvb_start;
                  vb && j > -1;
                  j--, vb = vb->next ) {
                if ( vb->type != ASN01_NULL && vb->type != ASN01_PRIV_RETRY ) { /* not yet processed */
                    view = AgentRegistry_inAView( vb->name, &vb->nameLength,
                        asp->pdu, vb->type );

                    /*
                    * if a ACM error occurs, mark it as type passed in
                    */
                    if ( view != VACM_SUCCESS ) {
                        ret++;
                        if ( request->repeat < request->orig_repeat ) {
                            /* basically this means a GETBULK */
                            request->repeat++;
                            if ( !earliest ) {
                                request->requestvb = vb;
                                earliest = 1;
                            }

                            /* ugh.  if a whole now exists, we need to
                              move the contents up the chain and fill
                              in at the end else we won't end up
                              lexographically sorted properly */
                            if ( j > -1 && vb->next && vb->next->type != ASN01_NULL && vb->next->type != ASN01_PRIV_RETRY ) {
                                for ( k = j, vbc = vb, vb2 = vb->next;
                                      k > -2 && vbc && vb2;
                                      k--, vbc = vb2, vb2 = vb2->next ) {
                                    /* clone next into the current */
                                    Client_cloneVar( vb2, vbc );
                                    vbc->next = vb2;
                                }
                            }
                        }
                        Client_setVarTypedValue( vb, type, NULL, 0 );
                    }
                }
            }
        }
    }
    return ret;
}

int Agent_createSubtreeCache( AgentSession* asp )
{
    Subtree* tp;
    VariableList *varbind_ptr, *vbsave, *vbptr, **prevNext;
    int view;
    int vbcount = 0;
    int bulkcount = 0, bulkrep = 0;
    int i = 0, n = 0, r = 0;
    RequestInfo* request;

    if ( asp->treecache == NULL && asp->treecache_len == 0 ) {
        asp->treecache_len = UTILITIES_MAX_VALUE( 1 + asp->vbcount / 4, 16 );
        asp->treecache = ( TreeCache* )calloc( asp->treecache_len, sizeof( TreeCache ) );
        if ( asp->treecache == NULL )
            return PRIOT_ERR_GENERR;
    }
    asp->treecache_num = -1;

    if ( asp->pdu->command == PRIOT_MSG_GETBULK ) {
        /*
        * getbulk prep
        */
        int count = Client_countVarbinds( asp->pdu->variables );
        if ( asp->pdu->errstat < 0 ) {
            asp->pdu->errstat = 0;
        }
        if ( asp->pdu->errindex < 0 ) {
            asp->pdu->errindex = 0;
        }

        if ( asp->pdu->errstat < count ) {
            n = asp->pdu->errstat;
        } else {
            n = count;
        }
        if ( ( r = count - n ) <= 0 ) {
            r = 0;
            asp->bulkcache = NULL;
        } else {
            int maxbulk = DefaultStore_getInt( DsStore_APPLICATION_ID,
                DsAgentInterger_MAX_GETBULKREPEATS );
            int maxresponses = DefaultStore_getInt( DsStore_APPLICATION_ID,
                DsAgentInterger_MAX_GETBULKRESPONSES );

            if ( maxresponses == 0 )
                maxresponses = 100; /* more than reasonable default */

            /* ensure that the total number of responses fits in a mallocable
            * result vector
            */
            if ( maxresponses < 0 || maxresponses > ( int )( INT_MAX / sizeof( struct varbind_list* ) ) )
                maxresponses = ( int )( INT_MAX / sizeof( struct varbind_list* ) );

            /* ensure that the maximum number of repetitions will fit in the
            * result vector
            */
            if ( maxbulk <= 0 || maxbulk > maxresponses / r )
                maxbulk = maxresponses / r;

            /* limit getbulk number of repeats to a configured size */
            if ( asp->pdu->errindex > maxbulk ) {
                asp->pdu->errindex = maxbulk;
                DEBUG_MSGTL( ( "snmp_agent",
                    "truncating number of getbulk repeats to %ld\n",
                    asp->pdu->errindex ) );
            }

            asp->bulkcache = ( VariableList** )malloc(
                ( n + asp->pdu->errindex * r ) * sizeof( struct varbind_list* ) );

            if ( !asp->bulkcache ) {
                DEBUG_MSGTL( ( "snmp_agent", "Bulkcache malloc failed\n" ) );
                return PRIOT_ERR_GENERR;
            }
        }
        DEBUG_MSGTL( ( "snmp_agent", "GETBULK N = %d, M = %ld, R = %d\n",
            n, asp->pdu->errindex, r ) );
    }

    /*
    * collect varbinds into their registered trees
    */
    prevNext = &( asp->pdu->variables );
    for ( varbind_ptr = asp->pdu->variables; varbind_ptr;
          varbind_ptr = vbsave ) {

        /*
        * getbulk mess with this pointer, so save it
        */
        vbsave = varbind_ptr->next;

        if ( asp->pdu->command == PRIOT_MSG_GETBULK ) {
            if ( n > 0 ) {
                n--;
            } else {
                /*
                * repeate request varbinds on GETBULK.  These will
                * have to be properly rearranged later though as
                * responses are supposed to actually be interlaced
                * with each other.  This is done with the asp->bulkcache.
                */
                bulkrep = asp->pdu->errindex - 1;
                if ( asp->pdu->errindex > 0 ) {
                    vbptr = varbind_ptr;
                    asp->bulkcache[ bulkcount++ ] = vbptr;

                    for ( i = 1; i < asp->pdu->errindex; i++ ) {
                        vbptr->next = MEMORY_MALLOC_STRUCT( VariableList_s );
                        /*
                        * don't clone the oid as it's got to be
                        * overwritten anyway
                        */
                        if ( !vbptr->next ) {
                            /*
                            * XXXWWW: ack!!!
                            */
                            DEBUG_MSGTL( ( "snmp_agent", "NextVar malloc failed\n" ) );
                        } else {
                            vbptr = vbptr->next;
                            vbptr->nameLength = 0;
                            vbptr->type = ASN01_NULL;
                            asp->bulkcache[ bulkcount++ ] = vbptr;
                        }
                    }
                    vbptr->next = vbsave;
                } else {
                    /*
                    * 0 repeats requested for this varbind, so take it off
                    * the list.
                    */
                    vbptr = varbind_ptr;
                    *prevNext = vbptr->next;
                    vbptr->next = NULL;
                    Api_freeVarbind( vbptr );
                    asp->vbcount--;
                    continue;
                }
            }
        }

        /*
        * count the varbinds
        */
        ++vbcount;

        /*
        * find the owning tree
        */
        tp = AgentRegistry_subtreeFind( varbind_ptr->name, varbind_ptr->nameLength,
            NULL, asp->pdu->contextName );

        /*
        * check access control
        */
        switch ( asp->pdu->command ) {
        case PRIOT_MSG_GET:
            view = AgentRegistry_inAView( varbind_ptr->name, &varbind_ptr->nameLength,
                asp->pdu, varbind_ptr->type );
            if ( view != VACM_SUCCESS )
                Client_setVarTypedValue( varbind_ptr, PRIOT_NOSUCHOBJECT,
                    NULL, 0 );
            break;

        case PRIOT_MSG_SET:
            view = AgentRegistry_inAView( varbind_ptr->name, &varbind_ptr->nameLength,
                asp->pdu, varbind_ptr->type );
            if ( view != VACM_SUCCESS ) {
                asp->index = vbcount;
                return PRIOT_ERR_NOACCESS;
            }
            break;

        case PRIOT_MSG_GETNEXT:
        case PRIOT_MSG_GETBULK:
        default:
            view = VACM_SUCCESS;
            /*
            * XXXWWW: check VACM here to see if "tp" is even worthwhile
            */
        }
        if ( view == VACM_SUCCESS ) {
            request = Agent_addVarbindToCache( asp, vbcount, varbind_ptr,
                tp );
            if ( request && asp->pdu->command == PRIOT_MSG_GETBULK ) {
                request->repeat = request->orig_repeat = bulkrep;
            }
        }

        prevNext = &( varbind_ptr->next );
    }

    return ErrorCode_SUCCESS;
}

/*
* this function is only applicable in getnext like contexts
*/
int Agent_reassignRequests( AgentSession* asp )
{
    /*
    * assume all the requests have been filled or rejected by the
    * subtrees, so reassign the rejected ones to the next subtree in
    * the chain
    */

    int i;

    /*
    * get old info
    */
    TreeCache* old_treecache = asp->treecache;

    /*
    * malloc new space
    */
    asp->treecache = ( TreeCache* )calloc( asp->treecache_len,
        sizeof( TreeCache ) );

    if ( asp->treecache == NULL )
        return PRIOT_ERR_GENERR;

    asp->treecache_num = -1;
    if ( asp->cache_store ) {
        Agent_freeCachemap( asp->cache_store );
        asp->cache_store = NULL;
    }

    for ( i = 0; i < asp->vbcount; i++ ) {
        if ( asp->requests[ i ].requestvb == NULL ) {
            /*
            * Occurs when there's a mixture of still active
            *   and "endOfMibView" repetitions
            */
            continue;
        }
        if ( asp->requests[ i ].requestvb->type == ASN01_NULL ) {
            if ( !Agent_addVarbindToCache( asp, asp->requests[ i ].index,
                     asp->requests[ i ].requestvb,
                     asp->requests[ i ].subtree->next ) ) {
                MEMORY_FREE( old_treecache );
            }
        } else if ( asp->requests[ i ].requestvb->type == ASN01_PRIV_RETRY ) {
            /*
            * re-add the same subtree
            */
            asp->requests[ i ].requestvb->type = ASN01_NULL;
            if ( !Agent_addVarbindToCache( asp, asp->requests[ i ].index,
                     asp->requests[ i ].requestvb,
                     asp->requests[ i ].subtree ) ) {
                MEMORY_FREE( old_treecache );
            }
        }
    }

    MEMORY_FREE( old_treecache );
    return PRIOT_ERR_NOERROR;
}

void Agent_deleteRequestInfos( RequestInfo* reqlist )
{
    while ( reqlist ) {
        AgentHandler_freeRequestDataSets( reqlist );
        reqlist = reqlist->next;
    }
}

void Agent_deleteSubtreeCache( AgentSession* asp )
{
    while ( asp->treecache_num >= 0 ) {
        /*
        * don't delete subtrees
        */
        Agent_deleteRequestInfos( asp->treecache[ asp->treecache_num ].requests_begin );
        asp->treecache_num--;
    }
}

/*
* check all requests for errors
*
* @Note:
* This function is a little different from the others in that
* it does not use any linked lists, instead using the original
* asp requests array. This is of particular importance for
* cases where the linked lists are unreliable. One known instance
* of this scenario occurs when the row_merge helper is used, which
* may temporarily disrupts linked lists during its (and its childrens)
* handling of requests.
*/
int Agent_checkAllRequestsError( AgentSession* asp,
    int look_for_specific )
{
    int i;

    /*
    * find any errors marked in the requests
    */
    for ( i = 0; i < asp->vbcount; ++i ) {
        if ( ( PRIOT_ERR_NOERROR != asp->requests[ i ].status ) && ( !look_for_specific || asp->requests[ i ].status == look_for_specific ) )
            return asp->requests[ i ].status;
    }

    return PRIOT_ERR_NOERROR;
}

int Agent_checkRequestsError( RequestInfo* requests )
{
    /*
    * find any errors marked in the requests
    */
    for ( ; requests; requests = requests->next ) {
        if ( requests->status != PRIOT_ERR_NOERROR )
            return requests->status;
    }
    return PRIOT_ERR_NOERROR;
}

int Agent_checkRequestsStatus( AgentSession* asp,
    RequestInfo* requests,
    int look_for_specific )
{
    /*
    * find any errors marked in the requests
    */
    while ( requests ) {
        if ( requests->agent_req_info != asp->reqinfo ) {
            DEBUG_MSGTL( ( "verbose:asp",
                "**reqinfo %p doesn't match cached reqinfo %p\n",
                asp->reqinfo, requests->agent_req_info ) );
        }
        if ( requests->status != PRIOT_ERR_NOERROR && ( !look_for_specific || requests->status == look_for_specific )
            && ( look_for_specific || asp->index == 0
                   || requests->index < asp->index ) ) {
            asp->index = requests->index;
            asp->status = requests->status;
        }
        requests = requests->next;
    }
    return asp->status;
}

int Agent_checkAllRequestsStatus( AgentSession* asp,
    int look_for_specific )
{
    int i;
    for ( i = 0; i <= asp->treecache_num; i++ ) {
        Agent_checkRequestsStatus( asp,
            asp->treecache[ i ].requests_begin,
            look_for_specific );
    }
    return asp->status;
}

int Agent_handleVarRequests( AgentSession* asp )
{
    int i, retstatus = PRIOT_ERR_NOERROR,
           status = PRIOT_ERR_NOERROR, final_status = PRIOT_ERR_NOERROR;
    HandlerRegistration* reginfo;

    asp->reqinfo->asp = asp;
    asp->reqinfo->mode = asp->mode;

    /*
    * now, have the subtrees in the cache go search for their results
    */
    for ( i = 0; i <= asp->treecache_num; i++ ) {
        /*
        * don't call handlers w/null reginfo.
        * - when is this? sub agent disconnected while request processing?
        * - should this case encompass more of this subroutine?
        *   - does check_request_status make send if handlers weren't called?
        */
        if ( NULL != asp->treecache[ i ].subtree->reginfo ) {
            reginfo = asp->treecache[ i ].subtree->reginfo;
            status = AgentHandler_callHandlers( reginfo, asp->reqinfo,
                asp->treecache[ i ].requests_begin );
        } else
            status = PRIOT_ERR_GENERR;

        /*
        * find any errors marked in the requests.  For later parts of
        * SET processing, only check for new errors specific to that
        * set processing directive (which must superceed the previous
        * errors).
        */
        switch ( asp->mode ) {
        case MODE_SET_COMMIT:
            retstatus = Agent_checkRequestsStatus( asp,
                asp->treecache[ i ].requests_begin,
                PRIOT_ERR_COMMITFAILED );
            break;

        case MODE_SET_UNDO:
            retstatus = Agent_checkRequestsStatus( asp,
                asp->treecache[ i ].requests_begin,
                PRIOT_ERR_UNDOFAILED );
            break;

        default:
            retstatus = Agent_checkRequestsStatus( asp,
                asp->treecache[ i ].requests_begin, 0 );
            break;
        }

        /*
        * always take lowest varbind if possible
        */
        if ( retstatus != PRIOT_ERR_NOERROR ) {
            status = retstatus;
        }

        /*
        * other things we know less about (no index)
        */
        /*
        * WWW: drop support for this?
        */
        if ( final_status == PRIOT_ERR_NOERROR && status != PRIOT_ERR_NOERROR ) {
            /*
            * we can't break here, since some processing needs to be
            * done for all requests anyway (IE, SET handling for IMPL_UNDO
            * needs to be called regardless of previous status
            * results.
            * WWW:  This should be predictable though and
            * breaking should be possible in some cases (eg GET,
            * GETNEXT, ...)
            */
            final_status = status;
        }
    }

    return final_status;
}

/*
* loop through our sessions known delegated sessions and check to see
* if they've completed yet. If there are no more delegated sessions,
* check for and process any queued requests
*/
void Agent_checkOutstandingAgentRequests( void )
{
    AgentSession *asp, *prev_asp = NULL, *next_asp = NULL;

    /*
    * deal with delegated requests
    */
    for ( asp = agent_delegated_list; asp; asp = next_asp ) {
        next_asp = asp->next; /* save in case we clean up asp */
        if ( !Agent_checkForDelegated( asp ) ) {

            /*
            * we're done with this one, remove from queue
            */
            if ( prev_asp != NULL )
                prev_asp->next = asp->next;
            else
                agent_delegated_list = asp->next;
            asp->next = NULL;

            /*
            * check request status
            */
            Agent_checkAllRequestsStatus( asp, 0 );

            /*
            * continue processing or finish up
            */
            Agent_checkDelayedRequest( asp );

            /*
            * if head was removed, don't drop it if it
            * was it re-queued
            */
            if ( ( prev_asp == NULL ) && ( agent_delegated_list == asp ) ) {
                prev_asp = asp;
            }
        } else {

            /*
            * asp is still on the queue
            */
            prev_asp = asp;
        }
    }

    /*
    * if we are processing a set and there are more delegated
    * requests, keep waiting before getting to queued requests.
    */
    if ( agent_processingSet && ( NULL != agent_delegated_list ) )
        return;

    while ( agent_agentQueuedList ) {

        /*
        * if we are processing a set, the first item better be
        * the set being (or waiting to be) processed.
        */
        Assert_assert( ( !agent_processingSet ) || ( agent_processingSet == agent_agentQueuedList ) );

        /*
        * if the top request is a set, don't pop it
        * off if there are delegated requests
        */
        if ( ( agent_agentQueuedList->pdu->command == PRIOT_MSG_SET ) && ( agent_delegated_list ) ) {

            Assert_assert( agent_processingSet == NULL );

            agent_processingSet = agent_agentQueuedList;
            DEBUG_MSGTL( ( "snmp_agent", "SET request remains queued while "
                                         "delegated requests finish, asp = %8p\n",
                asp ) );
            break;
        }

        /*
        * pop the first request and process it
        */
        asp = agent_agentQueuedList;
        agent_agentQueuedList = asp->next;
        DEBUG_MSGTL( ( "snmp_agent",
            "processing queued request, asp = %8p\n", asp ) );

        Agent_handleRequest( asp, asp->status );

        /*
        * if we hit a set, stop
        */
        if ( NULL != agent_processingSet )
            break;
    }
}

/** Decide if the requested transaction_id is still being processed
  within the agent.  This is used to validate whether a delayed cache
  (containing possibly freed pointers) is still usable.

  returns ErrorCode_SUCCESS if it's still valid, or ErrorCode_GENERR if not. */
int Agent_checkTransactionId( int transaction_id )
{
    AgentSession* asp;

    for ( asp = agent_delegated_list; asp; asp = asp->next ) {
        if ( asp->pdu->transid == transaction_id )
            return ErrorCode_SUCCESS;
    }
    return ErrorCode_GENERR;
}

/*
* Agent_checkDelayedRequest(asp)
*
* Called to rexamine a set of requests and continue processing them
* once all the previous (delayed) requests have been handled one way
* or another.
*/

int Agent_checkDelayedRequest( AgentSession* asp )
{
    int status = PRIOT_ERR_NOERROR;

    DEBUG_MSGTL( ( "snmp_agent", "processing delegated request, asp = %8p\n",
        asp ) );

    switch ( asp->mode ) {
    case PRIOT_MSG_GETBULK:
    case PRIOT_MSG_GETNEXT:
        Agent_checkAllRequestsStatus( asp, 0 );
        Agent_handleGetnextLoop( asp );
        if ( Agent_checkForDelegated( asp ) && Agent_checkTransactionId( asp->pdu->transid ) != ErrorCode_SUCCESS ) {
            /*
            * add to delegated request chain
            */
            if ( !Agent_checkDelegatedChainFor( asp ) ) {
                asp->next = agent_delegated_list;
                agent_delegated_list = asp;
            }
        }
        break;

    case MODE_SET_COMMIT:
        Agent_checkAllRequestsStatus( asp, PRIOT_ERR_COMMITFAILED );
        goto goto_settop;

    case MODE_SET_UNDO:
        Agent_checkAllRequestsStatus( asp, PRIOT_ERR_UNDOFAILED );
        goto goto_settop;

    case MODE_SET_BEGIN:
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_FREE:
    goto_settop:
        /* If we should do only one pass, this mean we */
        /* should not reenter this function */
        if ( ( asp->pdu->flags & PRIOT_UCD_MSG_FLAG_ONE_PASS_ONLY ) ) {
            /* We should have finished the processing after the first */
            /* Agent_handleSetLoop, so just wrap up */
            break;
        }
        Agent_handleSetLoop( asp );
        if ( asp->mode != IMPL_FINISHED_SUCCESS && asp->mode != IMPL_FINISHED_FAILURE ) {

            if ( Agent_checkForDelegatedAndAdd( asp ) ) {
                /*
                * add to delegated request chain
                */
                if ( !asp->status )
                    asp->status = status;
            }

            return PRIOT_ERR_NOERROR;
        }
        break;

    default:
        break;
    }

    /*
    * if we don't have anything outstanding (delegated), wrap up
    */
    if ( !Agent_checkForDelegated( asp ) )
        return Agent_wrapUpRequest( asp, status );

    return 1;
}

/** returns 1 if there are valid GETNEXT requests left.  Returns 0 if not. */
int Agent_checkGetnextResults( AgentSession* asp )
{
    /*
    * get old info
    */
    TreeCache* old_treecache = asp->treecache;
    int old_treecache_num = asp->treecache_num;
    int count = 0;
    int i, special = 0;
    RequestInfo* request;

    if ( asp->mode == PRIOT_MSG_GET ) {
        /*
        * Special case for doing INCLUSIVE getNext operations in
        * AgentX subagents.
        */
        DEBUG_MSGTL( ( "snmp_agent",
            "asp->mode == PRIOT_MSG_GET in ch_getnext\n" ) );
        asp->mode = asp->oldmode;
        special = 1;
    }

    for ( i = 0; i <= old_treecache_num; i++ ) {
        for ( request = old_treecache[ i ].requests_begin; request;
              request = request->next ) {

            /*
            * If we have just done the special case AgentX GET, then any
            * requests which were not INCLUSIVE will now have a wrong
            * response, so junk them and retry from the same place (except
            * that this time the handler will be called in "inexact"
            * mode).
            */

            if ( special ) {
                if ( !request->inclusive ) {
                    DEBUG_MSGTL( ( "snmp_agent",
                        "request %d wasn't inclusive\n",
                        request->index ) );
                    Client_setVarTypedValue( request->requestvb,
                        ASN01_PRIV_RETRY, NULL, 0 );
                } else if ( request->requestvb->type == ASN01_NULL || request->requestvb->type == PRIOT_NOSUCHINSTANCE || request->requestvb->type == PRIOT_NOSUCHOBJECT ) {
                    /*
                    * it was inclusive, but no results.  Still retry this
                    * search.
                    */
                    Client_setVarTypedValue( request->requestvb,
                        ASN01_PRIV_RETRY, NULL, 0 );
                }
            }

            /*
            * out of range?
            */
            if ( Api_oidCompare( request->requestvb->name,
                     request->requestvb->nameLength,
                     request->range_end,
                     request->range_end_len )
                >= 0 ) {
                /*
                * ack, it's beyond the accepted end of range.
                */
                /*
                * fix it by setting the oid to the end of range oid instead
                */
                DEBUG_MSGTL( ( "Agent_checkGetnextResults",
                    "request response %d out of range\n",
                    request->index ) );
                /*
                * I'm not sure why inclusive is set unconditionally here (see
                * comments for revision 1.161), but it causes a problem for
                * GETBULK over an overridden variable. The bulk-to-next
                * handler re-uses the same request for multiple varbinds,
                * and once inclusive was set, it was never cleared. So, a
                * hack. Instead of setting it to 1, set it to 2, so bulk-to
                * next can clear it later. As of the time of this hack, all
                * checks of this var are boolean checks (not == 1), so this
                * should be safe. Cross your fingers.
                */
                request->inclusive = 2;
                /*
                * XXX: should set this to the original OID?
                */
                Client_setVarObjid( request->requestvb,
                    request->range_end,
                    request->range_end_len );
                Client_setVarTypedValue( request->requestvb, ASN01_NULL,
                    NULL, 0 );
            }

            /*
            * mark any existent requests with illegal results as NULL
            */
            if ( request->requestvb->type == PRIOT_ENDOFMIBVIEW ) {
                /*
                * illegal response from a subagent.  Change it back to NULL
                *  xxx-rks: err, how do we know this is a subagent?
                */
                request->requestvb->type = ASN01_NULL;
                request->inclusive = 1;
            }

            if ( request->requestvb->type == ASN01_NULL || request->requestvb->type == ASN01_PRIV_RETRY || ( asp->reqinfo->mode == MODE_GETBULK
                                                                                                               && request->repeat > 0 ) )
                count++;
        }
    }
    return count;
}

/** repeatedly calls getnext handlers looking for an answer till all
  requests are satisified.  It's expected that one pass has been made
  before entering this function */
int Agent_handleGetnextLoop( AgentSession* asp )
{
    int status;
    VariableList* var_ptr;

    /*
    * loop
    */
    while ( agent_running ) {

        /*
        * bail for now if anything is delegated.
        */
        if ( Agent_checkForDelegated( asp ) ) {
            return PRIOT_ERR_NOERROR;
        }

        /*
        * check vacm against results
        */
        Agent_checkAcm( asp, ASN01_PRIV_RETRY );

        /*
        * need to keep going we're not done yet.
        */
        if ( !Agent_checkGetnextResults( asp ) )
            /*
            * nothing left, quit now
            */
            break;

        /*
        * never had a request (empty pdu), quit now
        */
        /*
        * XXXWWW: huh?  this would be too late, no?  shouldn't we
        * catch this earlier?
        */
        /*
        * if (count == 0)
        * break;
        */

        DEBUG_IF( "results" )
        {
            DEBUG_MSGTL( ( "results",
                "getnext results, before next pass:\n" ) );
            for ( var_ptr = asp->pdu->variables; var_ptr;
                  var_ptr = var_ptr->next ) {
                DEBUG_MSGTL( ( "results", "\t" ) );
                DEBUG_MSGVAR( ( "results", var_ptr ) );
                DEBUG_MSG( ( "results", "\n" ) );
            }
        }

        Agent_reassignRequests( asp );
        status = Agent_handleVarRequests( asp );
        if ( status != PRIOT_ERR_NOERROR ) {
            return status; /* should never really happen */
        }
    }
    if ( !agent_running ) {
        return PRIOT_ERR_GENERR;
    }
    return PRIOT_ERR_NOERROR;
}

int Agent_handleSet( AgentSession* asp )
{
    int status;
    /*
    * SETS require 3-4 passes through the var_op_list.
    * The first two
    * passes verify that all types, lengths, and values are valid
    * and may reserve resources and the third does the set and a
    * fourth executes any actions.  Then the identical GET RESPONSE
    * packet is returned.
    * If either of the first two passes returns an error, another
    * pass is made so that any reserved resources can be freed.
    * If the third pass returns an error, another pass is
    * made so that
    * any changes can be reversed.
    * If the fourth pass (or any of the error handling passes)
    * return an error, we'd rather not know about it!
    */
    if ( !( asp->pdu->flags & PRIOT_UCD_MSG_FLAG_ONE_PASS_ONLY ) ) {
        switch ( asp->mode ) {
        case MODE_SET_BEGIN:
            Api_incrementStatistic( API_STAT_SNMPINSETREQUESTS );
            asp->rw = IMPL_WRITE; /* WWW: still needed? */
            asp->mode = MODE_SET_RESERVE1;
            asp->status = PRIOT_ERR_NOERROR;
            break;

        case MODE_SET_RESERVE1:

            if ( asp->status != PRIOT_ERR_NOERROR )
                asp->mode = MODE_SET_FREE;
            else
                asp->mode = MODE_SET_RESERVE2;
            break;

        case MODE_SET_RESERVE2:
            if ( asp->status != PRIOT_ERR_NOERROR )
                asp->mode = MODE_SET_FREE;
            else
                asp->mode = MODE_SET_ACTION;
            break;

        case MODE_SET_ACTION:
            if ( asp->status != PRIOT_ERR_NOERROR )
                asp->mode = MODE_SET_UNDO;
            else
                asp->mode = MODE_SET_COMMIT;
            break;

        case MODE_SET_COMMIT:
            if ( asp->status != PRIOT_ERR_NOERROR ) {
                asp->mode = IMPL_FINISHED_FAILURE;
            } else {
                asp->mode = IMPL_FINISHED_SUCCESS;
            }
            break;

        case MODE_SET_UNDO:
            asp->mode = IMPL_FINISHED_FAILURE;
            break;

        case MODE_SET_FREE:
            asp->mode = IMPL_FINISHED_FAILURE;
            break;
        }
    }

    if ( asp->mode != IMPL_FINISHED_SUCCESS && asp->mode != IMPL_FINISHED_FAILURE ) {
        DEBUG_MSGTL( ( "agent_set", "doing set mode = %d (%s)\n", asp->mode,
            MapList_findLabel( "agentMode", asp->mode ) ) );
        status = Agent_handleVarRequests( asp );
        DEBUG_MSGTL( ( "agent_set", "did set mode = %d, status = %d\n",
            asp->mode, status ) );
        if ( ( status != PRIOT_ERR_NOERROR && asp->status == PRIOT_ERR_NOERROR ) || status == PRIOT_ERR_COMMITFAILED || status == PRIOT_ERR_UNDOFAILED ) {
            asp->status = status;
        }
    }
    return asp->status;
}

int Agent_handleSetLoop( AgentSession* asp )
{
    while ( asp->mode != IMPL_FINISHED_FAILURE && asp->mode != IMPL_FINISHED_SUCCESS ) {
        Agent_handleSet( asp );
        if ( Agent_checkForDelegated( asp ) ) {
            return PRIOT_ERR_NOERROR;
        }
        if ( asp->pdu->flags & PRIOT_UCD_MSG_FLAG_ONE_PASS_ONLY ) {
            return asp->status;
        }
    }
    return asp->status;
}

int Agent_handleRequest( AgentSession* asp, int status )
{
    /*
    * if this isn't a delegated request trying to finish,
    * processing of a set request should not start until all
    * delegated requests have completed, and no other new requests
    * should be processed until the set request completes.
    */
    if ( ( 0 == Agent_checkDelegatedChainFor( asp ) ) && ( asp != agent_processingSet ) ) {
        /*
        * if we are processing a set and this is not a delegated
        * request, queue the request
        */
        if ( agent_processingSet ) {
            Agent_addQueued( asp );
            DEBUG_MSGTL( ( "snmp_agent",
                "request queued while processing set, "
                "asp = %8p\n",
                asp ) );
            return 1;
        }

        /*
        * check for set request
        */
        if ( asp->pdu->command == PRIOT_MSG_SET ) {
            agent_processingSet = asp;

            /*
            * if there are delegated requests, we must wait for them
            * to finish.
            */
            if ( agent_delegated_list ) {
                DEBUG_MSGTL( ( "snmp_agent", "SET request queued while "
                                             "delegated requests finish, asp = %8p\n",
                    asp ) );
                Agent_addQueued( asp );
                return 1;
            }
        }
    }

    /*
    * process the request
    */
    status = Agent_handlePdu( asp );

    /*
    * print the results in appropriate debugging mode
    */
    DEBUG_IF( "results" )
    {
        VariableList* var_ptr;
        DEBUG_MSGTL( ( "results", "request results (status = %d):\n",
            status ) );
        for ( var_ptr = asp->pdu->variables; var_ptr;
              var_ptr = var_ptr->next ) {
            DEBUG_MSGTL( ( "results", "\t" ) );
            DEBUG_MSGVAR( ( "results", var_ptr ) );
            DEBUG_MSG( ( "results", "\n" ) );
        }
    }

    /*
    * check for uncompleted requests
    */
    if ( Agent_checkForDelegatedAndAdd( asp ) ) {
        /*
        * add to delegated request chain
        */
        asp->status = status;
    } else {
        /*
        * if we don't have anything outstanding (delegated), wrap up
        */
        return Agent_wrapUpRequest( asp, status );
    }

    return 1;
}

int Agent_handlePdu( AgentSession* asp )
{
    int status, inclusives = 0;
    VariableList* v = NULL;

    /*
    * for illegal requests, mark all nodes as ASN01_NULL
    */
    switch ( asp->pdu->command ) {

    case PRIOT_MSG_INTERNAL_SET_RESERVE2:
    case PRIOT_MSG_INTERNAL_SET_ACTION:
    case PRIOT_MSG_INTERNAL_SET_COMMIT:
    case PRIOT_MSG_INTERNAL_SET_FREE:
    case PRIOT_MSG_INTERNAL_SET_UNDO:
        status = Agent_getSetCache( asp );
        if ( status != PRIOT_ERR_NOERROR )
            return status;
        break;

    case PRIOT_MSG_GET:
    case PRIOT_MSG_GETNEXT:
    case PRIOT_MSG_GETBULK:
        for ( v = asp->pdu->variables; v != NULL; v = v->next ) {
            if ( v->type == ASN01_PRIV_INCL_RANGE ) {
                /*
                * Leave the type for now (it gets set to
                * ASN01_NULL in Agent_addVarbindToCache,
                * called by create_subnetsnmp_tree_cache below).
                * If we set it to ASN01_NULL now, we wouldn't be
                * able to distinguish INCLUSIVE search
                * ranges.
                */
                inclusives++;
            } else {
                Client_setVarTypedValue( v, ASN01_NULL, NULL, 0 );
            }
        }
    /*
        * fall through
        */

    case PRIOT_MSG_INTERNAL_SET_BEGIN:
    case PRIOT_MSG_INTERNAL_SET_RESERVE1:
    default:
        asp->vbcount = Client_countVarbinds( asp->pdu->variables );
        if ( asp->vbcount ) /* efence doesn't like 0 size allocs */
            asp->requests = ( RequestInfo* )
                calloc( asp->vbcount, sizeof( RequestInfo ) );
        /*
        * collect varbinds
        */
        status = Agent_createSubtreeCache( asp );
        if ( status != PRIOT_ERR_NOERROR )
            return status;
    }

    asp->mode = asp->pdu->command;
    switch ( asp->mode ) {
    case PRIOT_MSG_GET:
        /*
        * increment the message type counter
        */
        Api_incrementStatistic( API_STAT_SNMPINGETREQUESTS );

        /*
        * check vacm ahead of time
        */
        Agent_checkAcm( asp, PRIOT_NOSUCHOBJECT );

        /*
        * get the results
        */
        status = Agent_handleVarRequests( asp );

        /*
        * Deal with unhandled results -> noSuchInstance (rather
        * than noSuchObject -- in that case, the type will
        * already have been set to noSuchObject when we realised
        * we couldn't find an appropriate tree).
        */
        if ( status == PRIOT_ERR_NOERROR )
            Client_replaceVarTypes( asp->pdu->variables, ASN01_NULL,
                PRIOT_NOSUCHINSTANCE );
        break;

    case PRIOT_MSG_GETNEXT:
        Api_incrementStatistic( API_STAT_SNMPINGETNEXTS );
    /*
        * fall through
        */

    case PRIOT_MSG_GETBULK: /* note: there is no getbulk stat */
        /*
        * loop through our mib tree till we find an
        * appropriate response to return to the caller.
        */

        if ( inclusives ) {
            /*
            * This is a special case for AgentX INCLUSIVE getNext
            * requests where a result lexi-equal to the request is okay
            * but if such a result does not exist, we still want the
            * lexi-next one.  So basically we do a GET first, and if any
            * of the INCLUSIVE requests are satisfied, we use that
            * value.  Then, unsatisfied INCLUSIVE requests, and
            * non-INCLUSIVE requests get done as normal.
            */

            DEBUG_MSGTL( ( "snmp_agent", "inclusive range(s) in getNext\n" ) );
            asp->oldmode = asp->mode;
            asp->mode = PRIOT_MSG_GET;
        }

        /*
        * first pass
        */
        status = Agent_handleVarRequests( asp );
        if ( status != PRIOT_ERR_NOERROR ) {
            if ( !inclusives )
                return status; /* should never really happen */
            else
                asp->status = PRIOT_ERR_NOERROR;
        }

        /*
        * loop through our mib tree till we find an
        * appropriate response to return to the caller.
        */

        status = Agent_handleGetnextLoop( asp );
        break;

    case PRIOT_MSG_SET:
        /*
        * check access permissions first
        */
        if ( Agent_checkAcm( asp, PRIOT_NOSUCHOBJECT ) )
            return PRIOT_ERR_NOTWRITABLE;

        asp->mode = MODE_SET_BEGIN;
        status = Agent_handleSetLoop( asp );
        break;

    case PRIOT_MSG_INTERNAL_SET_BEGIN:
    case PRIOT_MSG_INTERNAL_SET_RESERVE1:
    case PRIOT_MSG_INTERNAL_SET_RESERVE2:
    case PRIOT_MSG_INTERNAL_SET_ACTION:
    case PRIOT_MSG_INTERNAL_SET_COMMIT:
    case PRIOT_MSG_INTERNAL_SET_FREE:
    case PRIOT_MSG_INTERNAL_SET_UNDO:
        asp->pdu->flags |= PRIOT_UCD_MSG_FLAG_ONE_PASS_ONLY;
        status = Agent_handleSetLoop( asp );
        /*
        * asp related cache is saved in cleanup
        */
        break;

    case PRIOT_MSG_RESPONSE:
        Api_incrementStatistic( API_STAT_SNMPINGETRESPONSES );
        return PRIOT_ERR_NOERROR;

    case PRIOT_MSG_TRAP:
    case PRIOT_MSG_TRAP2:
        Api_incrementStatistic( API_STAT_SNMPINTRAPS );
        return PRIOT_ERR_NOERROR;

    default:
        /*
        * WWW: are reports counted somewhere ?
        */
        Api_incrementStatistic( API_STAT_SNMPINASNPARSEERRS );
        return ErrorCode_GENERR; /* shouldn't get here */
        /*
        * WWW
        */
    }
    return status;
}

/** set error for a request
* \internal external interface: Agent_requestSetError
*/
static inline int
_Agent_requestSetError( RequestInfo* request, int mode, int error_value )
{
    if ( !request )
        return ErrorCode_NO_VARS;

    request->processed = 1;
    request->delegated = REQUEST_IS_NOT_DELEGATED;

    switch ( error_value ) {
    case PRIOT_NOSUCHOBJECT:
    case PRIOT_NOSUCHINSTANCE:
    case PRIOT_ENDOFMIBVIEW:
        /*
        * these are exceptions that should be put in the varbind
        * in the case of a GET but should be translated for a SET
        * into a real error status code and put in the request
        */
        switch ( mode ) {
        case MODE_GET:
        case MODE_GETNEXT:
        case MODE_GETBULK:
            request->requestvb->type = error_value;
            break;

        /*
            * These are technically illegal to set by the
            * client APIs for these modes.  But accepting
            * them here allows the 'sparse_table' helper to
            * provide some common table handling processing
            *
           Logger_log(LOGGER_PRIORITY_ERR, "Illegal error_value %d for mode %d ignored\n",
                    error_value, mode);
           return ErrorCode_VALUE;
            */

        case PRIOT_MSG_INTERNAL_SET_RESERVE1:
            request->status = PRIOT_ERR_NOCREATION;
            break;

        default:
            request->status = PRIOT_ERR_NOSUCHNAME; /* WWW: correct? */
            break;
        }
        break; /* never get here */

    default:
        if ( error_value < 0 ) {
            /*
            * illegal local error code.  translate to generr
            */
            /*
            * WWW: full translation map?
            */
            Logger_log( LOGGER_PRIORITY_ERR, "Illegal error_value %d translated to %d\n",
                error_value, PRIOT_ERR_GENERR );
            request->status = PRIOT_ERR_GENERR;
        } else {
            /*
            * WWW: translations and mode checking?
            */
            request->status = error_value;
        }
        break;
    }
    return ErrorCode_SUCCESS;
}

/** set error for a request
* @param request request which has error
* @param error_value error value for request
*/
int Agent_requestSetError( RequestInfo* request, int error_value )
{
    if ( !request || !request->agent_req_info )
        return ErrorCode_NO_VARS;

    return _Agent_requestSetError( request, request->agent_req_info->mode,
        error_value );
}

/** set error for a request within a request list
* @param request head of the request list
* @param error_value error value for request
* @param idx index of the request which has the error
*/
int Agent_requestSetErrorIdx( RequestInfo* request,
    int error_value, int idx )
{
    int i;
    RequestInfo* req = request;

    if ( !request || !request->agent_req_info )
        return ErrorCode_NO_VARS;

    /*
    * Skip to the indicated varbind
    */
    for ( i = 2; i < idx; i++ ) {
        req = req->next;
        if ( !req )
            return ErrorCode_NO_VARS;
    }

    return _Agent_requestSetError( req, request->agent_req_info->mode,
        error_value );
}

/** set error for all requests
* @param requests request list
* @param error error value for requests
* @return SNMPERR.SUCCESS, or an error code
*/
int Agent_requestSetErrorAll( RequestInfo* requests, int error )
{
    int mode, rc, result = ErrorCode_SUCCESS;

    if ( ( NULL == requests ) || ( NULL == requests->agent_req_info ) )
        return ErrorCode_NO_VARS;

    mode = requests->agent_req_info->mode; /* every req has same mode */

    for ( ; requests; requests = requests->next ) {

        /** paranoid sanity checks */
        Assert_assert( NULL != requests->agent_req_info );
        Assert_assert( mode == requests->agent_req_info->mode );

        /*
        * set error for this request. Log any errors, save the last
        * to return to the user.
        */
        if ( ( rc = _Agent_requestSetError( requests, mode, error ) ) ) {
            Logger_log( LOGGER_PRIORITY_WARNING, "got %d while setting request error\n", rc );
            result = rc;
        }
    }
    return result;
}

/**
* Return the difference between pm and the agent start time in hundredths of
* a second.
* \deprecated Don't use in new code.
*
* @param[in] pm An absolute time as e.g. reported by gettimeofday().
*/
u_long
Agent_markerUptime( timeMarker pm )
{
    u_long res;
    timeMarkerConst start = Agent_getAgentStarttime();

    res = Time_uatimeHdiff( start, pm );
    return res;
}

/**
* Return the difference between tv and the agent start time in hundredths of
* a second.
*
* \deprecated Use Agent_getAgentUptime() instead.
*
* @param[in] tv An absolute time as e.g. reported by gettimeofday().
*/
u_long
Agent_timevalUptime( struct timeval* tv )
{
    return Agent_markerUptime( ( timeMarker )tv );
}

struct timeval agent_starttime;
static struct timeval _agent_starttimeM;

/**
* Return a pointer to the variable in which the Net-SNMP start time has
* been stored.
*
* @note Use Agent_getAgentRuntime() instead of this function if you need
*   to know how much time elapsed since Agent_setAgentStarttime() has been
*   called.
*/
timeMarkerConst
Agent_getAgentStarttime( void )
{
    return &agent_starttime;
}

/**
* Report the time that elapsed since the agent start time in hundredths of a
* second.
*
* @see See also Agent_setAgentStarttime().
*/
uint64_t
Agent_getAgentRuntime( void )
{
    struct timeval now, delta;

    Time_getMonotonicClock( &now );
    TIME_SUB_TIME( &now, &_agent_starttimeM, &delta );
    return delta.tv_sec * ( uint64_t )100 + delta.tv_usec / 10000;
}

/**
* Set the time at which Net-SNMP started either to the current time
* (if s == NULL) or to *s (if s is not NULL).
*
* @see See also Agent_setAgentUptime().
*/
void Agent_setAgentStarttime( timeMarker s )
{
    if ( s ) {
        struct timeval nowA, nowM;

        agent_starttime = *( struct timeval* )s;
        gettimeofday( &nowA, NULL );
        Time_getMonotonicClock( &nowM );
        TIME_SUB_TIME( &agent_starttime, &nowA, &_agent_starttimeM );
        TIME_ADD_TIME( &_agent_starttimeM, &nowM, &_agent_starttimeM );
    } else {
        gettimeofday( &agent_starttime, NULL );
        Time_getMonotonicClock( &_agent_starttimeM );
    }
}

/**
* Return the current value of 'sysUpTime'
*/
u_long
Agent_getAgentUptime( void )
{
    struct timeval now, delta;

    Time_getMonotonicClock( &now );
    TIME_SUB_TIME( &now, &_agent_starttimeM, &delta );
    return delta.tv_sec * 100UL + delta.tv_usec / 10000;
}

/**
* Set the start time from which 'sysUpTime' is computed.
*
* @param[in] hsec New sysUpTime in hundredths of a second.
*
* @see See also Agent_setAgentStarttime().
*/
void Agent_setAgentUptime( u_long hsec )
{
    struct timeval nowA, nowM;
    struct timeval new_uptime;

    gettimeofday( &nowA, NULL );
    Time_getMonotonicClock( &nowM );
    new_uptime.tv_sec = hsec / 100;
    new_uptime.tv_usec = ( uint32_t )( hsec - new_uptime.tv_sec * 100 ) * 10000L;
    TIME_SUB_TIME( &nowA, &new_uptime, &agent_starttime );
    TIME_SUB_TIME( &nowM, &new_uptime, &_agent_starttimeM );
}

/*************************************************************************
*
* deprecated functions
*
*/

/** set error for a request
* \deprecated, use Agent_requestSetError instead
* @param reqinfo agent_request_info pointer for request
* @param request request_info pointer
* @param error_value error value for requests
* @return error_value
*/
int Agent_setRequestError( AgentRequestInfo* reqinfo,
    RequestInfo* request, int error_value )
{
    if ( !request || !reqinfo )
        return error_value;

    _Agent_requestSetError( request, reqinfo->mode, error_value );

    return error_value;
}
