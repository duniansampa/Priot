#ifndef IOT_MAP_H
#define IOT_MAP_H

/** \file Map.h
 *  @brief  Maps are associative containers that store elements
 *          formed by a combination of a key value and a mapped
 *          value, following a specific order.
 *
 *  In a map, the key values are generally used to sort and uniquely identify
 *  the elements, while the mapped values store the content associated to this key.
 *  The types of key and mapped value may differ.
 *
 *  This contains the prototypes for the map container and eventually
 *  any macros, constants, or global variables you will need.
 *
 *  \author Dunian Coutinho Sampa (duniansampa)
 *  \bug    No known bugs.
 */

#include "Generals.h"
#include "System/Util/Callback.h"

/** ============================[ Types ]================== */

/** Function type */
typedef void( MapFree_f )( void* );

/** \struct Map_s
* Used to iterate through map of elements
*/
typedef struct Map_s {

    /** Pointer to the next element */
    struct Map_s* next;

    /** Each element in a map is uniquely identified by its key value */
    char* key;

    /** Each element in a map stores some data as its mapped value*/
    void* value;

    /** The function to release the Map->value. Must know how to free Map->value */
    MapFree_f* freeFunction;

} Map;

/** =============================[ Functions Prototypes ]================== */

/** @brief  Creates a Map element given a key, value and a free function pointer.
 *
 *  @param  key - the key of the element to cache the value.
 *  @param  value - the value to be stored under that key
 *  @param  freeFunction - a function that can free the value pointer (in the future)
 *  @return a newly created Map element which can be given to the Map_insertElement function.
 */
Map* Map_newElement( const char* key, void* value, MapFree_f* freeFunction );

/** @brief  Extends the container by inserting new elements, effectively
 *          increasing the container size by the number of elements inserted.
 *
 * @param   mapHead - a pointer to the head element of a map
 * @param   element - a element to be inserted in the map
 */
void Map_insert( Map** mapHead, Map* element );

/** @brief  Inserts a new element in the map if its key is unique.
 *          This new element is constructed in place using args as the arguments
 *          for the construction of a element.
 *
 * @param   mapHead - a pointer to the head element of a map
 * @param   key - the key of the element to cache the value.
 * @param   value - the value to be stored under that key
 * @param   freeFunction - a function that can free the value pointer (in the future)
 * @return  a newly created map element which was inserted in the map
 */
Map* Map_emplace( Map** head, const char* key, void* value,
    MapFree_f* freeFunction );

/** @brief  returns a map element's value for a given key within a map
 *
 * @param   mapHead - the head element of a map
 * @param   key - the key to find
 * @return  a pointer to the value cached at that element
 */
void* Map_at( Map* mapHead, const char* key );

/** @brief  returns a map element for a given key within a map
 *
 * @param   mapHead - the head element of a map
 * @param   key - the key to find
 * @return  a pointer to the Map element
 */
Map* Map_find( Map* mapHead, const char* key );

/** @brief  Frees the value and a key at a given map element.
 *          Note that this doesn't free the element itself.
 *
 * @param   element - the element for which the data should be freed
 */
void Map_free( Map* head ); /** single */

/** @brief  Removes all elements from the map container (which are destroyed),
 *          leaving the container with a size of 0.
 *
 * @param   mapHead - the top element of the map to be clear.
 */
void Map_clear( Map* mapHead );

/** @brief  Removes from the map container a single element  (and frees it)
 *
 *a@param   realhead - a pointer to the head element of a map
 * @param   key - the key to find and remove
 * @return  0 on successful find-and-delete, 1 otherwise.
 */
int Map_erase( Map** realHead, const char* key );

#endif // IOT_MAP_H
