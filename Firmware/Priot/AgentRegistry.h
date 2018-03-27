#ifndef AGENTREGISTRY_H
#define AGENTREGISTRY_H

#include "Generals.h"
#include "Types.h"
#include "VarStruct.h"
#include "Debug.h"

#define MIB_REGISTERED_OK		     0
#define MIB_DUPLICATE_REGISTRATION	-1
#define MIB_REGISTRATION_FAILED		-2
#define MIB_UNREGISTERED_OK		     0
#define MIB_NO_SUCH_REGISTRATION	-1
#define MIB_UNREGISTRATION_FAILED	-2
#define DEFAULT_MIB_PRIORITY		127


/*
 * REGISTER_MIB(): This macro simply loads register_mib with less pain:
 *
 * descr:   A short description of the mib group being loaded.
 * var:     The variable structure to load.
 * vartype: The variable structure used to define it (variable[2, 4, ...])
 * theoid:  An *initialized* *exact length* oid pointer.
 *          (sizeof(theoid) *must* return the number of elements!)
 */

#define REGISTER_MIB(descr, var, vartype, theoid)                       \
    if (AgentRegistry_registerMib(descr, (const struct variable *) var, \
                     sizeof(struct vartype),                            \
                     sizeof(var)/sizeof(struct vartype),                \
                     theoid, sizeof(theoid)/sizeof(oid)) !=             \
        MIB_REGISTERED_OK)                                              \
    DEBUG_MSGTL(("register_mib", "%s registration failed\n", descr));


#define NUM_EXTERNAL_SIGS       32
#define SIG_REGISTERED_OK		 0
#define SIG_REGISTRATION_FAILED -2
#define SIG_UNREGISTERED_OK		 0

/***********************************************************************/
    /*
     * requests api definitions
     */
/***********************************************************************/

    /*
     * the structure of parameters passed to registered ACM modules
     */
struct ViewParameters_s {
    Types_Pdu* pdu;
    oid*       name;
    size_t     namelen;
    int        test;
    int        errorcode;		/*  Do not change unless you're
                        specifying an error, as it starts
                        in a success state.  */
    int        check_subtree;
};

struct RegisterParameters_s {
    oid*                name;
    size_t               namelen;
    int                  priority;
    int                  range_subid;
    oid                 range_ubound;
    int                  timeout;
    u_char               flags;
    const char*          contextName;
    Types_Session*       session;
    HandlerRegistration* reginfo;
};

typedef struct SubtreeContextCache_s {
    const char*                   context_name;
    struct Subtree_s*             first_subtree;
    struct SubtreeContextCache_s* next;
} SubtreeContextCache;

void
AgentRegistry_setupTree( void );

void
AgentRegistry_shutdownTree( void );

void AgentRegistry_dumpRegistry(void);

Subtree *
AgentRegistry_subtreeFind( const oid *, size_t,
                           Subtree *,
                           const char *context_name );

Subtree *
AgentRegistry_subtreeFindNext( const oid *,
                               size_t,
                               Subtree* ,
                               const char* context_name );

Subtree *
AgentRegistry_subtreeFindPrev( const oid *,
                               size_t,
                               Subtree *,
                               const char* context_name );

Subtree *
AgentRegistry_subtreeFindFirst( const char *context_name );

Types_Session *
AgentRegistry_getSessionForOid( const oid *,
                                size_t,
                                const char *context_name );

SubtreeContextCache *
AgentRegistry_getTopContextCache( void );

void
AgentRegistry_setLookupCacheSize( int newsize );

int
AgentRegistry_getLookupCacheSize( void );

int
AgentRegistry_registerMib( const char *,
                           const struct Variable_s *,
                           size_t,
                           size_t,
                           const oid *,
                           size_t );

int
AgentRegistry_registerMibPriority ( const char *,
                                    const struct Variable_s *,
                                    size_t,
                                    size_t,
                                    const oid *,
                                    size_t,
                                    int );

int
AgentRegistry_registerMibRange( const char *,
                                const struct Variable_s *,
                                size_t,
                                size_t, const oid *,
                                size_t,
                                int,
                                int, oid,
                                Types_Session *);

int
AgentRegistry_registerMibContext( const char *,
                                  const struct Variable_s *,
                                  size_t,
                                  size_t,
                                  const oid *,
                                  size_t,
                                  int,
                                  int,
                                  oid,
                                  Types_Session *,
                                  const char *,
                                  int,
                                  int );

int
AgentRegistry_unregisterMib( oid *,
                             size_t );

int
AgentRegistry_unregisterMibPriority( oid *,
                                     size_t,
                                     int );

int
AgentRegistry_unregisterMibRange( oid *,
                                  size_t,
                                  int,
                                  int,
                                  oid );

int
AgentRegistry_unregisterMibContext( oid *,
                                    size_t,
                                    int,
                                    int,
                                    oid,
                                    const char * );

void
AgentRegistry_clearContext( void );

void
AgentRegistry_unregisterMibsBySession( Types_Session * );

int
AgentRegistry_unregisterMibTableRow( oid *mibloc,
                                     size_t mibloclen,
                                     int priority,
                                     int var_subid,
                                     oid range_ubound,
                                     const char *context );

int
AgentRegistry_inAView( oid *,
                       size_t *,
                       Types_Pdu *, int );

int
AgentRegistry_checkAccess( Types_Pdu * pdu );

int
AgentRegistry_acmCheckSubtree( Types_Pdu *,
                               oid *,
                               size_t );

void
AgentRegistry_registerMibReattach( void );

void
AgentRegistry_registerMibDetach( void );


extern int  agentRegistry_externalSignalScheduled[NUM_EXTERNAL_SIGS];
extern void (*agentRegistry_externalSignalHandler[NUM_EXTERNAL_SIGS]) (int);

int
AgentRegistry_registerSignal(int, void (*func)(int));

int
AgentRegistry_unregisterSignal(int);



/*
 * internal API.  Don't use this.  Use netsnmp_register_handler instead
 */

struct AgentRegistry_handlerRegistration;

int
AgentRegistry_registerMib2( const char *,
                           struct Variable_s *,
                           size_t,
                           size_t,
                           oid *,
                           size_t,
                           int,
                           int,
                           oid,
                           Types_Session *,
                           const char *,
                           int, int,
                           HandlerRegistration *,
                           int );

#endif // AGENTREGISTRY_H
