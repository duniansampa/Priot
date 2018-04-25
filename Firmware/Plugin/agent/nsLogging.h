#ifndef NSLOGGING_H
#define NSLOGGING_H

#include "TableIterator.h"
#include "AgentHandler.h"
/*
 * function declarations 
 */
void            init_nsLogging(void);

/*
 * Handler and iterators for the logging table
 */
NodeHandlerFT handle_nsLoggingTable;
FirstDataPointFT  get_first_logging_entry;
NextDataPointFT   get_next_logging_entry;

#endif /* NSLOGGING_H */
