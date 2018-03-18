#ifndef XCLIENT_H
#define XCLIENT_H

#include "Types.h"

/*
 *  Utility functions for Agent Extensibility Protocol (RFC 2257)
 *
 */

#define AGENTX_CLOSE_OTHER    1
#define AGENTX_CLOSE_PARSE    2
#define AGENTX_CLOSE_PROTOCOL 3
#define AGENTX_CLOSE_TIMEOUT  4
#define AGENTX_CLOSE_SHUTDOWN 5
#define AGENTX_CLOSE_MANAGER  6

int
XClient_openSession( Types_Session * );

int
XClient_closeSession( Types_Session *,
                      int );

int
XClient_register( Types_Session * ss,
                  oid             start[],
                  size_t          startlen,
                  int             priority,
                  int             range_subid,
                  oid             range_ubound,
                  int             timeout,
                  u_char          flags,
                  const char    * contextName );
int
XClient_unregister( Types_Session * ss,
                    oid             start[],
                    size_t          startlen,
                    int             priority,
                    int             range_subid,
                    oid             range_ubound,
                    const char *    contextName );

Types_VariableList *
XClient_registerIndex( Types_Session *,
                       Types_VariableList *,
                       int );

int
XClient_unregisterIndex( Types_Session *,
                         Types_VariableList * );

int
XClient_addAgentcaps( Types_Session *,
                      const oid *,
                      size_t,
                      const char * );

int
XClient_removeAgentcaps( Types_Session *,
                         const oid *,
                         size_t );

int
XClient_sendPing( Types_Session * );


#endif // XCLIENT_H
