#ifndef STASHCACHE_H
#define STASHCACHE_H

#include "AgentHandler.h"
#include "OidStash.h"

#define STASH_CACHE_NAME "stash_cache"

typedef struct StashCacheInfo_s {
   int            cache_valid;
   markerT        cache_time;
   OidStash_Node* cache;
   int            cache_length;
} StashCacheInfo;

typedef struct StashCacheData_s {
   void*    data;
   size_t   data_len;
   u_char   data_type;
} StashCacheData;

/* function prototypes */
void
StashCache_initStashCacheHelper(void);

MibHandler*
StashCache_getBareStashCacheHandler(void);

MibHandler*
StashCache_getStashCacheHandler(void);

MibHandler*
StashCache_getTimedBareStashCacheHandler( int    timeout,
                                          oid*   rootoid,
                                          size_t rootoid_len );

MibHandler*
StashCache_getTimedStashCacheHandler( int    timeout,
                                      oid*   rootoid,
                                      size_t rootoid_len );

NodeHandlerFT StashCache_helper;

OidStash_Node**
StashCache_extractStashCache( AgentRequestInfo *reqinfo);



#endif // STASHCACHE_H
