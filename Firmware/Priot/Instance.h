#ifndef INSTANCE_H
#define INSTANCE_H

#include "PriotSettings.h"
#include "AgentHandler.h"

/*
 * The instance helper is designed to simplify the task of adding simple
 * * instances to the mib tree.
 */

/*
 * GETNEXTs are auto-converted to a GET.
 * * non-valid GETs are dropped.
 * * The client can assume that if you're called for a GET, it shouldn't
 * * have to check the oid at all.  Just answer.
 */

int
Instance_registerInstance( HandlerRegistration* reginfo );

int
Instance_registerReadOnlyInstance( HandlerRegistration* reginfo );

#define INSTANCE_HANDLER_NAME "instance"

MibHandler*
Instance_getInstanceHandler( void );

int
Instance_registerReadOnlyUlongInstance( const char*    name,
                                        const oid *    reg_oid,
                                        size_t         reg_oid_len,
                                        u_long*        it,
                                        NodeHandlerFT* subhandler );
int
Instance_registerUlongInstance( const char * name,
                                const oid * reg_oid,
                                size_t reg_oid_len,
                                u_long * it,
                                NodeHandlerFT * subhandler);
int
Instance_registerReadOnlyCounter32Instance( const char*    name,
                                            const oid*     reg_oid,
                                            size_t         reg_oid_len,
                                            u_long*        it,
                                            NodeHandlerFT* subhandler);
int
Instance_registerReadOnlyLongInstance( const char*    name,
                                       const oid*     reg_oid,
                                       size_t         reg_oid_len,
                                       long*          it,
                                       NodeHandlerFT* subhandler );
int
Instance_registerLongInstance( const char*    name,
                               const oid*     reg_oid,
                               size_t         reg_oid_len,
                               long*          it,
                               NodeHandlerFT* subhandler );

int
Instance_registerReadOnlyIntInstance( const char*    name,
                                      const oid*     reg_oid,
                                      size_t         reg_oid_len,
                                      int *          it,
                                      NodeHandlerFT* subhandler );

int
Instance_registerIntInstance( const char*    name,
                              const oid *    reg_oid,
                              size_t         reg_oid_len,
                              int*           it,
                              NodeHandlerFT* subhandler );

/* identical functions that register a in a particular context */
int
Instance_registerReadOnlyUlongInstanceContext( const char*    name,
                                               const oid*     reg_oid,
                                               size_t         reg_oid_len,
                                               u_long*        it,
                                               NodeHandlerFT* subhandler,
                                               const char*    contextName );
int
Instance_registerUlongInstanceContext( const char*    name,
                                       const oid*     reg_oid,
                                       size_t         reg_oid_len,
                                       u_long*        it,
                                       NodeHandlerFT* subhandler,
                                       const char*    contextName );
int
Instance_registerReadOnlyCounter32InstanceContext( const char*    name,
                                                   const oid*     reg_oid,
                                                   size_t         reg_oid_len,
                                                   u_long*        it,
                                                   NodeHandlerFT* subhandler,
                                                   const char*    contextName );
int
Instance_registerReadOnlyLongInstanceContext( const char*    name,
                                              const oid *    reg_oid,
                                              size_t         reg_oid_len,
                                              long*          it,
                                              NodeHandlerFT* subhandler,
                                              const char*    contextName );
int
Instance_registerLongInstanceContext( const char*    name,
                                      const oid*     reg_oid,
                                      size_t         reg_oid_len,
                                      long*          it,
                                      NodeHandlerFT* subhandler,
                                      const char*    contextName );

int
Instance_registerReadOnlyIntInstanceContext( const char*    name,
                                             const oid*     reg_oid,
                                             size_t         reg_oid_len,
                                             int*           it,
                                             NodeHandlerFT* subhandler,
                                             const char*    contextName );

int
Instance_registerIntInstanceContext( const char*    name,
                                     const oid*     reg_oid,
                                     size_t         reg_oid_len,
                                     int*           it,
                                     NodeHandlerFT* subhandler,
                                     const char*    contextName );

int
Instance_registerNumFileInstance( const char*    name,
                                  const oid*     reg_oid,
                                  size_t         reg_oid_len,
                                  const char*    file_name,
                                  int            asn_type,
                                  int            mode,
                                  NodeHandlerFT* subhandler,
                                  const char*    contextName );

NodeHandlerFT Instance_helperHandler;

NodeHandlerFT Instance_numFileHandler;

#endif // INSTANCE_H
