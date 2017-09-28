#ifndef SCALAR_H
#define SCALAR_H

#include "AgentHandler.h"

/*
 * The scalar helper is designed to simplify the task of adding simple
 * scalar objects to the mib tree.
 */

/*
 * GETNEXTs are auto-converted to a GET.
 * * non-valid GETs are dropped.
 * * The client can assume that if you're called for a GET, it shouldn't
 * * have to check the oid at all.  Just answer.
 */

int
Scalar_registerScalar(HandlerRegistration *reginfo);

int
Scalar_registerReadOnlyScalar( HandlerRegistration *reginfo);

#define SCALAR_HANDLER_NAME "scalar"

MibHandler*
Scalar_getScalarHandler(void);

NodeHandlerFT Scalar_helperHandler;


#endif // SCALAR_H
