#ifndef MAPLIST_H_
#define MAPLIST_H_

/** \file MapList.h
 *  \brief  MapList is a map list. Each element in the list contains
 *          a 'name' and a 'map'. The name is a 'map' identifier in the list.
 *          The 'name' must be unique.
 *
 *  In a MapList, the 'name' values are generally used to sort and uniquely identify
 *  the 'map', while the mapped values store the content associated to this 'name'.
 *
 *  This contains the prototypes for the MapList container and eventually
 *  any macros, constants, or global variables you will need.
 *
 *  \author Dunian Coutinho Sampa (duniansampa)
 *  \bug    No known bugs.
 */

/** Error codes
 */
enum MapListErrorCode_e {
    MapListErrorCode_SUCCESS = 0,
    MapListErrorCode_NO_MEMORY = 1,
    MapListErrorCode_ALREADY_EXIST = 2,
    MapListErrorCode_NULL = -2
};

//--------------------------------------------

/** \brief  Returns the label of the item in the map whose name mapName,
 *          based on a search value.
 *
 *  \param  mapName - the name of the map to be used for searching
 *  \param  value - the lookup value
 *  \return If successful, returns label. If not, returns the error code MapListErrorCode_NULL.
 */
char* MapList_findLabel( const char* mapName,
    int value );

/** \brief  Returns the value of the item in the map whose name mapName,
 *          based on a search label.
 *
 *  \param  mapName - the name of the map to be used for searching
 *  \param  label - the lookup label
 *  \return If successful, returns value. If not, returns the error code MapListErrorCode_NULL.
 */
int MapList_findValue( const char* mapName,
    const char* label );

/** \brief  Add the pair (label, value) to the map with name mapName. Transfers
 *          ownership of the memory pointed to by label to the map:
 *          MapList_clear() deallocates that memory.
 *
 *  \param  mapName - the map to be used for search
 *  \param  label - the  label of the item to be added
 *  \param  value - the  value of the item to be added
 *
 *  \return If successful, returns MapListErrorCode_SUCCESS.
 *          if it already exists, returns MapListErrorCode_ALREADY_EXIST.
 *          If not, returns the error code MapListErrorCode_NULL or MapListErrorCode_NO_MEMORY.
 */
int MapList_addPair( const char* mapName, char* label,
    int value );

/** \brief  Deallocate the memory allocated by Enum_initEnum(): remove all key/value
 *          pairs stored by MapList_add*() calls.
 */
void MapList_clear( void );

#endif // MAPLIST_H_
