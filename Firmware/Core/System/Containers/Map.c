#include "Map.h"
#include "System/Util/Assert.h"
#include "System/Util/Trace.h"

/** ============================= Public Functions ================== */

void Map_free( Map* element )
{
    MapFree_f* freeFunction;
    if ( !element )
        return;

    freeFunction = element->freeFunction;
    if ( freeFunction )
        ( freeFunction )( element->value );
    MEMORY_FREE( element->key );
}

void Map_clear( Map* mapHead )
{
    Map* tmpptr;
    for ( ; mapHead; ) {
        Map_free( mapHead );
        tmpptr = mapHead;
        mapHead = mapHead->next;
        MEMORY_FREE( tmpptr );
    }
}

Map* Map_newElement( const char* key, void* value, MapFree_f* freeFunction )
{
    Map* element;

    if ( !key )
        return NULL;
    element = MEMORY_MALLOC_TYPEDEF( Map );
    if ( !element )
        return NULL;
    element->key = strdup( key );
    if ( !element->key ) {
        free( element );
        return NULL;
    }
    element->value = value;
    element->freeFunction = freeFunction;

    return element;
}

void Map_insert( Map** mapHead, Map* element )
{
    Map* ptr;

    Assert_assert( NULL != mapHead );
    Assert_assert( NULL != element );
    Assert_assert( NULL != element->key );

    DEBUG_MSGTL( ( "data_list", "adding key '%s'\n", element->key ) );

    if ( !*mapHead ) {
        *mapHead = element;
        return;
    }

    if ( 0 == strcmp( element->key, ( *mapHead )->key ) ) {
        Assert_assert( !"list key == is unique" ); /* always fail */
        Logger_log( LOGGER_PRIORITY_WARNING,
            "WARNING: adding duplicate key '%s' to data list\n",
            element->key );
    }

    for ( ptr = *mapHead; ptr->next != NULL; ptr = ptr->next ) {
        Assert_assert( NULL != ptr->key );
        if ( 0 == strcmp( element->key, ptr->key ) ) {
            Assert_assert( !"list key == is unique" ); /* always fail */
            Logger_log( LOGGER_PRIORITY_WARNING,
                "WARNING: adding duplicate key '%s' to data list\n",
                element->key );
        }
    }

    Assert_assert( NULL != ptr );
    if ( ptr ) /* should always be true */
        ptr->next = element;
}

Map* Map_emplace( Map** mapHead, const char* key,
    void* value, MapFree_f* freeFunction )
{
    Map* node;
    if ( !key ) {
        Logger_log( LOGGER_PRIORITY_ERR, "no name provided." );
        return NULL;
    }
    node = Map_newElement( key, value, freeFunction );
    if ( NULL == node ) {
        Logger_log( LOGGER_PRIORITY_ERR, "could not allocate memory for node." );
        return NULL;
    }

    Map_insert( mapHead, node );

    return node;
}

void* Map_at( Map* mapHead, const char* key )
{
    if ( !key )
        return NULL;
    for ( ; mapHead; mapHead = mapHead->next )
        if ( mapHead->key && strcmp( mapHead->key, key ) == 0 )
            break;
    if ( mapHead )
        return mapHead->value;
    return NULL;
}

Map* Map_find( Map* mapHead, const char* key )
{
    if ( !key )
        return NULL;
    for ( ; mapHead; mapHead = mapHead->next )
        if ( mapHead->key && strcmp( mapHead->key, key ) == 0 )
            break;
    if ( mapHead )
        return mapHead;
    return NULL;
}

int Map_erase( Map** realhead, const char* key )
{
    Map *head, *prev;
    if ( !key )
        return 1;
    for ( head = *realhead, prev = NULL; head;
          prev = head, head = head->next ) {
        if ( head->key && strcmp( head->key, key ) == 0 ) {
            if ( prev )
                prev->next = head->next;
            else
                *realhead = head->next;
            Map_free( head );
            Memory_free( head );
            return 0;
        }
    }
    return 1;
}
