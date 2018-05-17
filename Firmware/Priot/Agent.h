#ifndef AGENT_H
#define AGENT_H

#include "Api.h"
#include "System/Containers/Map.h"
#include "Priot.h"
#include "System/Util/Utilities.h"
#include "Transport.h"
#include <stdint.h>

#define SNMP_MAX_PDU_SIZE                               \
    64000 /* local constraint on PDU size sent by agent \
           * (see also SNMP_MAX_MSG_SIZE in snmp_api.h) */

extern oid agent_versionSysoid[];
extern int agent_versionSysoidLen;

/*
 * If non-zero, causes the addresses of peers to be logged when receptions
 * occur.
 */

extern int agent_logAddresses;

/*
 * How many ticks since we last aged the address cache entries.
 */

// extern int      lastAddrAge;

/** @typedef struct RequestInfo_s RequestInfo
 * Typedefs the RequestInfo_s struct into
 * RequestInfo*/
/** @struct RequestInfo_s
 * The netsnmp request info structure.
*/

typedef struct RequestInfo_s {
    /**
   * variable bindings
   */
    Types_VariableList* requestvb;

    /**
   * can be used to pass information on a per-request basis from a
   * helper to the later handlers
   */
    Map* parent_data;

    /*
   * pointer to the agent_request_info for this request
   */
    struct AgentRequestInfo_s* agent_req_info;

    /** don't free, reference to (struct tree)->end */
    oid* range_end;
    size_t range_end_len;

    /*
   * flags
   */
    int delegated;
    int processed;
    int inclusive;

    int status;
    /** index in original pdu */
    int index;

    /** get-bulk */
    int repeat;
    int orig_repeat;
    Types_VariableList* requestvb_start;

    /* internal use */
    struct RequestInfo_s* next;
    struct RequestInfo_s* prev;
    struct Subtree_s* subtree;
} RequestInfo;

typedef struct SetInfo_s {
    int action;
    void* stateRef;

    /*
   * don't use yet:
   */
    void** oldData;
    int setCleanupFlags;
#define AUTO_FREE_STATEREF 0x01 /* calls free(stateRef) */
#define AUTO_FREE_OLDDATA 0x02 /* calls free(*oldData) */
#define AUTO_UNDO 0x03 /* ... */
} SetInfo;

typedef struct TreeCache_s {
    struct Subtree_s* subtree;
    RequestInfo* requests_begin;
    RequestInfo* requests_end;
} TreeCache;

#define MODE_GET PRIOT_MSG_GET
#define MODE_GETNEXT PRIOT_MSG_GETNEXT
#define MODE_GETBULK PRIOT_MSG_GETBULK
#define MODE_GET_STASH PRIOT_MSG_INTERNAL_GET_STASH
#define MODE_IS_GET( x ) \
    ( ( x >= 128 ) && ( x != -1 ) && ( x != PRIOT_MSG_SET ) )

/* #define MODE_IS_GET(x)        ((x == PRIOT_MSG_GET) || (x ==
 * PRIOT_MSG_GETNEXT) || (x == PRIOT_MSG_GETBULK) || (x ==
 * PRIOT_MSG_INTERNAL_GET_STASH)) */

#define MODE_SET_BEGIN PRIOT_MSG_INTERNAL_SET_BEGIN
#define MODE_SET_RESERVE1 PRIOT_MSG_INTERNAL_SET_RESERVE1
#define MODE_SET_RESERVE2 PRIOT_MSG_INTERNAL_SET_RESERVE2
#define MODE_SET_ACTION PRIOT_MSG_INTERNAL_SET_ACTION
#define MODE_SET_COMMIT PRIOT_MSG_INTERNAL_SET_COMMIT
#define MODE_SET_FREE PRIOT_MSG_INTERNAL_SET_FREE
#define MODE_SET_UNDO PRIOT_MSG_INTERNAL_SET_UNDO
#define MODE_IS_SET( x ) \
    ( ( x < 128 ) || ( x == -1 ) || ( x == PRIOT_MSG_SET ) )
/* #define MODE_IS_SET(x)         (!MODE_IS_GET(x)) */

#define MODE_BSTEP_PRE_REQUEST PRIOT_MSG_INTERNAL_PRE_REQUEST
#define MODE_BSTEP_POST_REQUEST PRIOT_MSG_INTERNAL_POST_REQUEST

#define MODE_BSTEP_OBJECT_LOOKUP PRIOT_MSG_INTERNAL_OBJECT_LOOKUP

#define MODE_BSTEP_CHECK_VALUE PRIOT_MSG_INTERNAL_CHECK_VALUE
#define MODE_BSTEP_ROW_CREATE PRIOT_MSG_INTERNAL_ROW_CREATE
#define MODE_BSTEP_UNDO_SETUP PRIOT_MSG_INTERNAL_UNDO_SETUP
#define MODE_BSTEP_SET_VALUE PRIOT_MSG_INTERNAL_SET_VALUE
#define MODE_BSTEP_CHECK_CONSISTENCY PRIOT_MSG_INTERNAL_CHECK_CONSISTENCY
#define MODE_BSTEP_UNDO_SET PRIOT_MSG_INTERNAL_UNDO_SET
#define MODE_BSTEP_COMMIT PRIOT_MSG_INTERNAL_COMMIT
#define MODE_BSTEP_UNDO_COMMIT PRIOT_MSG_INTERNAL_UNDO_COMMIT
#define MODE_BSTEP_IRREVERSIBLE_COMMIT PRIOT_MSG_INTERNAL_IRREVERSIBLE_COMMIT
#define MODE_BSTEP_UNDO_CLEANUP PRIOT_MSG_INTERNAL_UNDO_CLEANUP

/** @typedef struct AgentRequestInfo_s AgentRequestInfo
* Typedefs the AgentRequestInfo_s struct into
* AgentRequestInfo
*/

/** @struct AgentRequestInfo_s
* The agent transaction request structure
*/
typedef struct AgentRequestInfo_s {
    int mode;
    /** pdu contains authinfo, eg */
    /*        Types_Pdu    *pdu;    */
    struct AgentSession_s* asp; /* may not be needed */
    /*
                               * can be used to pass information on a per-pdu basis from a
                               * helper to the later handlers
                               */
    Map* agent_data;
} AgentRequestInfo;

typedef struct Cachemap_s {
    int globalid;
    int cacheid;
    struct Cachemap_s* next;
} Cachemap;

typedef struct AgentSession_s {
    int mode;
    Types_Session* session;
    Types_Pdu* pdu;
    Types_Pdu* orig_pdu;
    int rw;
    int exact;
    int status;
    int index;
    int oldmode;

    struct AgentSession_s* next;

    /*
   * new API pointers
   */
    AgentRequestInfo* reqinfo;
    RequestInfo* requests;
    TreeCache* treecache;
    Types_VariableList** bulkcache;
    int treecache_len; /* length of cache array */
    int treecache_num; /* number of current cache entries */
    Cachemap* cache_store;
    int vbcount;
} AgentSession;

/*
 * Address cache handling functions.
 */

void Agent_addrcacheInitialise( void );
void Agent_addrcacheDestroy( void );
void Agent_addrcacheAge( void );

/*
 * config file parsing routines
 */
int Agent_handlePriotPacket( int, Types_Session*, int, Types_Pdu*, void* );

AgentSession* Agent_initAgentPriotSession( Types_Session*, Types_Pdu* );

void Agent_freeAgentPriotSession( AgentSession* );

void Agent_removeAndFreeAgentPriotSession( AgentSession* asp );

void Agent_dumpSessList( void );
int Agent_initMasterAgent( void );
void Agent_shutdownMasterAgent( void );
int Agent_checkAndProcess( int block );
void Agent_checkOutstandingAgentRequests( void );

int Agent_requestSetError( RequestInfo* request, int error_value );
int Agent_checkRequestsError( RequestInfo* reqs );

int Agent_checkAllRequestsError( AgentSession* asp, int look_for_specific );
int Agent_requestSetErrorIdx( RequestInfo* requests, int error_value, int idx );
int Agent_requestSetErrorAll( RequestInfo* requests, int error_value );

/////** deprecated, use Agent_requestSetError instead */
int Agent_setRequestError( AgentRequestInfo* reqinfo, RequestInfo* request,
    int error_value );

u_long Agent_markerUptime( timeMarker pm );
u_long Agent_timevalUptime( struct timeval* tv );
timeMarkerConst Agent_getAgentStarttime( void );
uint64_t Agent_getAgentRuntime( void );
void Agent_setAgentStarttime( timeMarker s );
u_long Agent_getAgentUptime( void );
void Agent_setAgentUptime( u_long hsec );
int Agent_checkTransactionId( int transaction_id );
int Agent_checkPacket( Types_Session*, Transport_Transport*, void*, int );

int Agent_checkParse( Types_Session*, Types_Pdu*, int );
int Agent_allocateGlobalcacheid( void );

int Agent_removeDelegatedRequestsForSession( Types_Session* sess );

/*
 * Register and de-register agent NSAPs.
 */

int Agent_registerAgentNsap( Transport_Transport* t );

void Agent_deregisterAgentNsap( int handle );

void Agent_addListData( AgentRequestInfo* agent, Map* node );

int Agent_removeListData( AgentRequestInfo* ari, const char* name );

void* Agent_getListData( AgentRequestInfo* agent, const char* name );

void Agent_freeAgentDataSet( AgentRequestInfo* agent );

void Agent_freeAgentDataSets( AgentRequestInfo* agent );

void Agent_freeAgentRequestInfo( AgentRequestInfo* ari );

void Agent_freeAgentPriotSessionBySession(
    Types_Session* sess, void ( *free_request )( Api_RequestList* ) );

#endif // AGENT_H
