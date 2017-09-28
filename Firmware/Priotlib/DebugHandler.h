#ifndef DEBUGHANDLER_H
#define DEBUGHANDLER_H

#include "AgentHandler.h"

MibHandler*
DebugHandler_getDebugHandler(void);

void
DebugHandler_initDebugHelper(void);

NodeHandlerFT DebugHandler_debugHelper;


#endif // DEBUGHANDLER_H
