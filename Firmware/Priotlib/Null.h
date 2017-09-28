#ifndef NULL_H
#define NULL_H

#include "AgentHandler.h"
/*
 * literally does nothing and is used as a final handler for
 * "do-nothing" nodes that must exist solely for mib tree storage
 * usage..
 */

int
Null_registerNull(oid *, size_t);

int
Null_registerNullContext( oid *,
                          size_t,
                          const char *contextName);

NodeHandlerFT Null_handler;

#endif // NULL_H
