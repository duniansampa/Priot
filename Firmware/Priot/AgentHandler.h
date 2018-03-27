#ifndef AGENTHANDLER_H
#define AGENTHANDLER_H

#include "Agent.h"

/** @file AgentHandler.h
 *
 *  @addtogroup handler
 *
 * @{
 */

struct HandlerRegistration_s;

#define REQUEST_IS_DELEGATED     1
#define REQUEST_IS_NOT_DELEGATED 0


/*
 * per mib handler flags.
 * NOTE: Lower bits are reserved for the agent handler's use.
 *       The high 4 bits (31-28) are reserved for use by the handler.
 */
#define MIB_HANDLER_AUTO_NEXT                   0x00000001
#define MIB_HANDLER_AUTO_NEXT_OVERRIDE_ONCE     0x00000002
#define MIB_HANDLER_INSTANCE                    0x00000004

#define MIB_HANDLER_CUSTOM4                     0x10000000
#define MIB_HANDLER_CUSTOM3                     0x20000000
#define MIB_HANDLER_CUSTOM2                     0x40000000
#define MIB_HANDLER_CUSTOM1                     0x80000000


/** @typedef struct MibHandler_s MibHandler
 * Typedefs the MibHandler_s struct into  MibHandler */

/** @struct MibHandler_s
 *  the mib handler structure to be registered
 */
typedef struct MibHandler_s {
        char* handler_name;
    /** for handler's internal use */
        void* myvoid;
        /** for agent_handler's internal use */
        int   flags;

        /** if you add more members, you probably also want to update */
        /** _clone_handler in agent_handler.c. */

        int (*access_method) (struct MibHandler_s*,
                              struct HandlerRegistration_s*,
                              struct AgentRequestInfo_s*,
                              struct RequestInfo_s*);
        /** data clone hook for myvoid
         *  deep copy the myvoid member - default is to copy the pointer
         *  This method is only called if myvoid != NULL
         *  myvoid is the current myvoid pointer.
         *  returns NULL on failure
         */
        void* ( *data_clone )( void* myvoid );
        /** data free hook for myvoid
         *  delete the myvoid member - default is to do nothing
         *  This method is only called if myvoid != NULL
         */
        void ( *data_free )( void* myvoid ); /**< data free hook for myvoid */

        struct MibHandler_s *next;
        struct MibHandler_s *prev;
} MibHandler;

/*
 * per registration flags
 */
#define HANDLER_CAN_GETANDGETNEXT     0x01       /* must be able to do both */
#define HANDLER_CAN_SET               0x02           /* implies create, too */
#define HANDLER_CAN_GETBULK           0x04
#define HANDLER_CAN_NOT_CREATE        0x08         /* auto set if ! CAN_SET */
#define HANDLER_CAN_BABY_STEP         0x10
#define HANDLER_CAN_STASH             0x20


#define HANDLER_CAN_RONLY   (HANDLER_CAN_GETANDGETNEXT)
#define HANDLER_CAN_RWRITE  (HANDLER_CAN_GETANDGETNEXT | HANDLER_CAN_SET)
#define HANDLER_CAN_SET_ONLY (HANDLER_CAN_SET | HANDLER_CAN_NOT_CREATE)
#define HANDLER_CAN_DEFAULT (HANDLER_CAN_RONLY | HANDLER_CAN_NOT_CREATE)

/** @typedef struct HandlerRegistration_s HandlerRegistration
 * Typedefs the HandlerRegistration_s struct into HandlerRegistration  */

/** @struct HandlerRegistration_s
 *  Root registration info.
 *  The variables handlerName, contextName, and rootoid need to be allocated
 *  on the heap, when the registration structure is unregistered using
 *  unregister_mib_context() the code attempts to free them.
 */
typedef struct HandlerRegistration_s {

    /** for mrTable listings, and other uses */
        char* handlerName;
    /** NULL = default context */
        char* contextName;

        /**
         * where are we registered at?
         */
        oid*       rootoid;
        size_t      rootoid_len;

        /**
         * handler details
         */
        MibHandler* handler;
        int         modes;

        /**
         * more optional stuff
         */
        int         priority;
        int         range_subid;
        oid        range_ubound;
        int         timeout;
        int         global_cacheid;

        /**
         * void ptr for registeree
         */
        void*       my_reg_void;
} HandlerRegistration;

/*
 * function handler definitions
 */

typedef int (NodeHandlerFT) ( MibHandler*          handler,
                              HandlerRegistration* reginfo,  /** pointer to registration struct */
                              AgentRequestInfo*    reqinfo,  /** pointer to current transaction */
                              RequestInfo*         requests );

typedef struct HandlerArgs_s {
    MibHandler*          handler;
    HandlerRegistration* reginfo;
    AgentRequestInfo*    reqinfo;
    RequestInfo*         requests;
} HandlerArgs;

typedef struct DelegatedCache_s {
    int                  transaction_id;
    MibHandler*          handler;
    HandlerRegistration* reginfo;
    AgentRequestInfo*    reqinfo;
    RequestInfo*         requests;
    void*                localinfo;
} DelegatedCache;

/*
 * handler API functions
 */

void
AgentHandler_initHandlerConf( void );

int
AgentHandler_registerHandler( HandlerRegistration* reginfo);

int
AgentHandler_unregisterHandler( HandlerRegistration* reginfo );

int
AgentHandler_registerHandlerNocallback( HandlerRegistration* reginfo );

int
AgentHandler_injectHandler( HandlerRegistration* reginfo,
                            MibHandler*          handler );
int
AgentHandler_injectHandlerBefore( HandlerRegistration* reginfo,
                                  MibHandler*          handler,
                                  const char*          before_what );
MibHandler*
AgentHandler_findHandlerByName( HandlerRegistration* reginfo,
                                const char*          name ) ;

void*
AgentHandler_findHandlerDataByName( HandlerRegistration* reginfo,
                                    const char*          name );

int
AgentHandler_callHandlers( HandlerRegistration* reginfo,
                           AgentRequestInfo*    reqinfo,
                           RequestInfo*         requests );

int
AgentHandler_callHandler( MibHandler*           next_handler,
                          HandlerRegistration*  reginfo,
                          AgentRequestInfo*     reqinfo,
                          RequestInfo*          requests );

int
AgentHandler_callNextHandler( MibHandler*           current,
                              HandlerRegistration*  reginfo,
                              AgentRequestInfo*     reqinfo,
                              RequestInfo*          requests );
int
AgentHandler_callNextHandlerOneRequest( MibHandler*          current,
                                        HandlerRegistration* reginfo,
                                        AgentRequestInfo*    reqinfo,
                                        RequestInfo*         requests );

MibHandler*
AgentHandler_createHandler( const char*     name,
                            NodeHandlerFT*  handler_access_method );

HandlerRegistration*
AgentHandler_registrationCreate( const char* name,
                                        MibHandler* handler,
                                        const oid* reg_oid,
                                        size_t      reg_oid_len,
                                        int         modes );

HandlerRegistration*
AgentHandler_createHandlerRegistration( const char*    name,
                                        NodeHandlerFT* handler_access_method,
                                        const oid*    reg_oid,
                                        size_t         reg_oid_len,
                                        int            modes );

DelegatedCache*
AgentHandler_createDelegatedCache( MibHandler*,
                                   HandlerRegistration*,
                                   AgentRequestInfo*,
                                   RequestInfo*,
                                   void* );

void
AgentHandler_freeDelegatedCache( DelegatedCache* dcache );

DelegatedCache*
AgentHandler_handlerCheckCache( DelegatedCache* dcache );

void
AgentHandler_registerHandlerByName( const char*,
                                    MibHandler* );

void
AgentHandler_clearHandlerList( void );

void
AgentHandler_requestAddListData( RequestInfo*       request,
                                 DataList_DataList* node );

int
AgentHandler_requestRemoveListData( RequestInfo* request,
                                    const char*  name );

int
AgentHandler_requestRemoveListData( RequestInfo* request,
                                    const char*  name);

void*
AgentHandler_requestGetListData( RequestInfo* request,
                                 const char*  name );

void
AgentHandler_freeRequestDataSet( RequestInfo* request );

void
AgentHandler_freeRequestDataSets( RequestInfo* request );

void
AgentHandler_handlerFree( MibHandler* );

MibHandler*
AgentHandler_handlerDup( MibHandler* );

HandlerRegistration*
AgentHandler_handlerRegistrationDup( HandlerRegistration* );

void
AgentHandler_handlerRegistrationFree( HandlerRegistration* );

void
AgentHandler_handlerMarkRequestsAsDelegated( RequestInfo*,
                                             int );

#endif // AGENTHANDLER_H
