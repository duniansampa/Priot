#ifndef SERIALIZE_H
#define SERIALIZE_H

#include "AgentHandler.h"

/*
 * The serialized helper merely calls its clients multiple times for a
 * * given request set, so they don't have to loop through the requests
 * * themselves.
 */


MibHandler*
Serialize_getSerializeHandler(void);

int
Serialize_registerSerialize( HandlerRegistration* reginfo);

void
Serialize_initSerialize(void);

NodeHandlerFT
Serialize_helperHandler;

#endif // SERIALIZE_H
