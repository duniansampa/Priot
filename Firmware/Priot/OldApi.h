#ifndef OLDAPI_H
#define OLDAPI_H

#include "AgentHandler.h"
#include "Vars.h"


#define OLD_API_NAME "oldApi"

typedef struct OldApiInfo_s {
    struct Variable_s *var;
    size_t          varsize;
    size_t          numvars;

    /*
     * old stuff
     */
    Types_Session* ss;
    int            flags;
} OldApiInfo;

typedef struct OldOpiCache {
    u_char* data;
    WriteMethodFT* write_method;
} OldApiCache;

int
OldApi_registerOlAapi( const char*              moduleName,
                       const struct Variable_s* var,
                       size_t                   varsize,
                       size_t                   numvars,
                       const oid*               mibloc,
                       size_t                   mibloclen,
                       int                      priority,
                       int                      range_subid,
                       oid                      range_ubound,
                       Types_Session*           ss,
                       const char*              context,
                       int                      timeout,
                       int                      flags );

NodeHandlerFT OldApi_helper;

/*
 * really shouldn't be used
 */
AgentSession* OldApi_getCurrentAgentSession(void);


#endif // OLDAPI_H
