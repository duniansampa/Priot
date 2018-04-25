

#include "CacheHandler.h"
#include "Logger.h"
#include "siglog/agent/hardware/memory.h"

extern CacheLoadFT netsnmp_mem_arch_load;

netsnmp_memory_info* _mem_head = NULL;
Cache* _mem_cache = NULL;

#define MEMORY_CACHE_SECONDS 5

void init_hw_mem( void )
{
    oid nsMemory[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 31 };
    _mem_cache = CacheHandler_create( MEMORY_CACHE_SECONDS, netsnmp_mem_arch_load, NULL,
        nsMemory, ASN01_OID_LENGTH( nsMemory ) );
}

netsnmp_memory_info* netsnmp_memory_get_first( int type )
{
    netsnmp_memory_info* mem;

    for ( mem = _mem_head; mem; mem = mem->next )
        if ( mem->type == type ) /* Or treat as bits? */
            return mem;
    return NULL;
}

netsnmp_memory_info* netsnmp_memory_get_next( netsnmp_memory_info* this_ptr, int type )
{
    netsnmp_memory_info* mem;

    if ( this_ptr )
        for ( mem = this_ptr->next; mem; mem = mem->next )
            if ( mem->type == type ) /* Or treat as bits? */
                return mem;
    return NULL;
}

/*
     * Work with a list of Memory entries, indexed numerically
     */
netsnmp_memory_info* netsnmp_memory_get_byIdx( int idx, int create )
{
    netsnmp_memory_info *mem, *mem2;

    /*
         * Find the specified Memory entry
         */
    for ( mem = _mem_head; mem; mem = mem->next ) {
        if ( mem->idx == idx )
            return mem;
    }
    if ( !create )
        return NULL;

    /*
         * Create a new memory entry, and insert it into the list....
         */
    mem = TOOLS_MALLOC_TYPEDEF( netsnmp_memory_info );
    if ( !mem )
        return NULL;
    mem->idx = idx;
    /* ... either as the first (or only) entry....  */
    if ( !_mem_head || _mem_head->idx > idx ) {
        mem->next = _mem_head;
        _mem_head = mem;
        return mem;
    }
    /* ... or in the appropriate position  */
    for ( mem2 = _mem_head; mem2; mem2 = mem2->next ) {
        if ( !mem2->next || mem2->next->idx > idx ) {
            mem->next = mem2->next;
            mem2->next = mem;
            return mem;
        }
    }
    TOOLS_FREE( mem );
    return NULL; /* Shouldn't happen! */
}

netsnmp_memory_info* netsnmp_memory_get_next_byIdx( int idx, int type )
{
    netsnmp_memory_info* mem;

    for ( mem = _mem_head; mem; mem = mem->next )
        if ( mem->type == type && mem->idx > idx ) /* Or treat as bits? */
            return mem;
    return NULL;
}

Cache* netsnmp_memory_get_cache( void )
{
    return _mem_cache;
}

int netsnmp_memory_load( void )
{
    return CacheHandler_checkAndReload( _mem_cache );
}
