#ifndef READONLY_H
#define READONLY_H

#include "AgentHandler.h"

/*
 * The helper merely intercepts SET requests and handles them early on
 * making everything read-only (no SETs are actually permitted).
 * Useful as a helper to handlers that are implementing MIBs with no
 * SET support.
 */


MibHandler*
ReadOnly_getReadOnlyHandler(void);

void
ReadOnly_initReadOnlyHelper(void);

NodeHandlerFT ReadOnly_helper;


#endif // READONLY_H
