#ifndef NSDEBUG_H
#define NSDEBUG_H

#include "AgentHandler.h"
#include "TableIterator.h"

/*
 * function declarations 
 */
void            init_nsDebug(void);

/*
 * Handlers for the scalar objects
 */
NodeHandlerFT handle_nsDebugEnabled;
NodeHandlerFT handle_nsDebugOutputAll;
NodeHandlerFT handle_nsDebugDumpPdu;

/*
 * Handler and iterators for the debug table
 */
NodeHandlerFT handle_nsDebugTable;
FirstDataPointFT  get_first_debug_entry;
NextDataPointFT   get_next_debug_entry;

#endif /* NSDEBUG_H */
