#ifndef STASHTONEXT_H
#define STASHTONEXT_H

#include "AgentHandler.h"

/*
 * The helper merely intercepts GETSTASH requests and converts them to
 * GETNEXT reequests.
 */

MibHandler *  StashToNext_getStashToNextHandler(void);
NodeHandlerFT StashToNext_helper;

#endif // STASHTONEXT_H
