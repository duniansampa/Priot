#ifndef TABLEROW_H
#define TABLEROW_H

#include "AgentHandler.h"
#include "Table.h"

MibHandler *
TableRow_handlerGet( void *row );

int
TableRow_register( HandlerRegistration *reginfo,
                   TableRegistrationInfo *tabreg,
                   void *row, VariableList *index);

void *
TableRow_extract( RequestInfo *request );

#endif // TABLEROW_H
