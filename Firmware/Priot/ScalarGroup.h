#ifndef SCALARGROUP_H
#define SCALARGROUP_H

#include "AgentHandler.h"

/*
 * The scalar group helper is designed to implement a group of
 * scalar objects all in one go, making use of the scalar and
 * instance helpers.
 *
 * GETNEXTs are auto-converted to a GET.  Non-valid GETs are dropped.
 * The client-provided handler just needs to check the OID name to
 * see which object is being requested, but can otherwise assume that
 * things are fine.
 */

typedef struct ScalarGroup_s {
    oid lbound;		/* XXX - or do we need a more flexible arrangement? */
    oid ubound;
} ScalarGroup;

int
ScalarGroup_registerScalarGroup( HandlerRegistration* reginfo,
                                 oid                  first,
                                 oid                  last );

MibHandler*
ScalarGroup_getScalarGroupHandler( oid first,
                                   oid last);

NodeHandlerFT ScalarGroup_helperHandler;

#endif // SCALARGROUP_H
