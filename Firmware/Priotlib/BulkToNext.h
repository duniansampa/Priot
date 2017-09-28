#ifndef BULKTONEXT_H
#define BULKTONEXT_H

#include "AgentHandler.h"

/*
 * The helper merely intercepts GETBULK requests and converts them to
 * * GETNEXT reequests.
 */

MibHandler*
BulkToNext_getBulkToNextHandler( void );

void
BulkToNext_initBulkToNextHelper( void );

void
BulkToNext_fixRequests( RequestInfo* requests );

NodeHandlerFT BulkToNext_helper;

#endif // BULKTONEXT_H
