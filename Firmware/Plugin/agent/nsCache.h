#ifndef NSCACHE_H
#define NSCACHE_H

#include "AgentHandler.h"
#include "TableIterator.h"

/*
 * function declarations 
 */
void            init_nsCache(void);

/*
 * Handlers for the scalar objects
 */
NodeHandlerFT handle_nsCacheTimeout;
NodeHandlerFT handle_nsCacheEnabled;

/*
 * Handler and iterators for the cache table
 */
NodeHandlerFT handle_nsCacheTable;
FirstDataPointFT  get_first_cache_entry;
NextDataPointFT   get_next_cache_entry;

#endif /* NSCACHE_H */
