#include "MapList.h"
#include "System/Containers/Map.h"
#include "System/Util/Utilities.h"
#include "Types.h"
#include <stdlib.h>

struct MapList_s {
    /** The name of the Map */
    char* name;
    Map* map;
    struct MapList_s* next;
};

/** ============================ Private Variables ================== */

/** Pointer to the List of Map */
static struct MapList_s* _mapList_storage;

/** ================== Prototypes of Private Functions =============== */

static void _MapList_freeFunction( void* value );
static int _MapList_addPair( Map** map, char* label, int value );
static int _MapList_findValue( Map* map, const char* label );
static char* _MapList_findLabel( Map* map, int value );
static Map** _MapList_findMap( const char* mapName );
static Map* _MapList_find( const char* mapName );

/** ============================= Public Functions ================== */

char* MapList_findLabel( const char* mapName, int value )
{
    return ( _MapList_findLabel( _MapList_find( mapName ), value ) );
}

int MapList_findValue( const char* mapName, const char* label )
{
    return ( _MapList_findValue( _MapList_find( mapName ), label ) );
}

int MapList_addPair( const char* mapName, char* label, int value )
{
    Map* map = _MapList_find( mapName );
    int created = ( map ) ? 1 : 0;

    /** if map == NULL, then created = 0
    if created == 0, it means that a map need to be created
    Add label and value to the map if value not exist
    If the map is null, create first item of the map */
    int ret = _MapList_addPair( &map, label, value );

    if ( !created ) {
        /** the map was created */
        struct MapList_s* sptr = MEMORY_MALLOC_STRUCT( MapList_s );
        if ( !sptr ) {
            /** momory was not allocated
            clean the map */
            Map_clear( map );
            return MapListErrorCode_NO_MEMORY;
        }
        sptr->next = _mapList_storage;
        sptr->name = strdup( mapName );
        sptr->map = map;
        _mapList_storage = sptr;
    }
    return ret;
}

void MapList_clear( void )
{
    struct MapList_s *sptr = _mapList_storage, *next = NULL;

    while ( sptr != NULL ) {
        next = sptr->next;
        Map_clear( sptr->map );
        MEMORY_FREE( sptr->name );
        MEMORY_FREE( sptr );
        sptr = next;
    }
    _mapList_storage = NULL;
}

/** @brief  Returns a value of item in the map based on a lookup label
 *
 *  @param  map - the map to be used for search
 *  @param  label - the lookup label
 *  @return If successful, returns value.
 *          If not, returns the error code MapListErrorCode_NULL.
 */
static int _MapList_findValue( Map* map, const char* label )
{
    if ( !map )
        return MapListErrorCode_NULL; /* XXX: um, no good solution here */

    /** map->key = value */
    /** map->value = label */

    while ( map ) {
        if ( strcmp( map->value, label ) == 0 )
            return atoi( map->key );
        map = map->next;
    }

    return MapListErrorCode_NULL; /* XXX: um, no good solution here */
}

/** ============================= Private Functions ================== */

/** @brief  Returns a label of item in the map based on a lookup value.
 *          operates directly on a possibly external map.
 *
 *  @param  map - the map to be used for search
 *  @param  value - the lookup value
 *  @return If successful, returns value. If not,
 *          returns NULL.
 */
static char* _MapList_findLabel( Map* map, int value )
{

    if ( !map )
        return NULL;

    /** map->key = value */
    /** map->value = label */

    char key[ 8 ];
    sprintf( key, "%d", value );
    return Map_at( map, key );
}

/** @brief  adds label and value to the map if value not exist
*           if the map is null, create first item of the map
*
*   @param  map - the map to be used for search
*   @param  value - the lookup value
*   @return MapListErrorCode_*
*/
static int _MapList_addPair( Map** map, char* label, int value )
{
    if ( !map )
        return MapListErrorCode_NULL;

    char key[ 8 ];
    sprintf( key, "%d", value );

    if ( Map_at( *map, key ) != NULL ) {
        free( label );
        return ( MapListErrorCode_ALREADY_EXIST );
    }

    Map_emplace( map, key, label, _MapList_freeFunction );

    return ( MapListErrorCode_SUCCESS );
}

/** @brief  Returns a map of enums based on a lookup name
 *
 *  @param mapName - the lookup name
 *  @return If successful, returns map. If not, returns NULL.
 */
static Map* _MapList_find( const char* mapName )
{
    Map** ptr = _MapList_findMap( mapName );
    return ptr ? *ptr : NULL;
}

static void _MapList_freeFunction( void* value )
{
    MEMORY_FREE( value );
}

static Map** _MapList_findMap( const char* mapName )
{
    struct MapList_s* sptr;
    if ( !mapName )
        return NULL;

    for ( sptr = _mapList_storage; sptr != NULL; sptr = sptr->next )
        if ( sptr->name && strcmp( sptr->name, mapName ) == 0 )
            return &sptr->map;

    return NULL;
}
