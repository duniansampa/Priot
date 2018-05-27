#ifndef IOT_LIST_H
#define IOT_LIST_H

/** @file List.h
 *  @brief Lists are sequence containers that allow constant time insert and erase operations
 *         anywhere within the sequence.
 *
 *  This contains the prototypes for the list container and eventually
 *  any macros, constants, or global variables you will need.
 *
 *  @author Dunian Coutinho Sampa (duniansampa)
 *  @bug    No known bugs.
 */

#include "Generals.h"
#include "System/Util/Callback.h"

/** ============================[ Types ]================== */

/** Function type */
typedef void( ListFree_f )( void* );

/** \struct List_s
* Used to iterate through list of elements
*/
typedef struct List_s {

    /** Pointer to the next element */
    struct List_s* next;

    /** Each element in a list stores some data */
    void* data;

    /** The function to release the List->data. Must know how to free List->data */
    ListFree_f* freeFunction;

} List;

/** =============================[ Functions Prototypes ]================== */

/** @brief  List_newElement
 *          creates a List element given a data and a free function pointer.
 *
 *  @param  data - the data to be stored
 *  @param  freeFunction - a function that can free the data pointer (in the future)
 *  @return a newly created List element which can be given to the List_insertElement function.
 */
List* List_newElement( void* data, ListFree_f* freeFunction );

/** @brief  List_insert
 *          extends the container by inserting new elements, effectively
 *          increasing the container size by the number of elements inserted.
 *
 * @param   listHead - a pointer to the head element of a list
 * @param   element - a element to be inserted in the list
 */
void List_insert( List** listHead, List* element );

/** @brief  List_emplace
 *          inserts a new element in the list. This new element is constructed in
 *          place using args as the arguments for the construction of a element.
 *
 * @param   listHead - a pointer to the head element of a list
 * @param   data - the data to be stored
 * @param   freeFunction - a function that can free the data pointer (in the future)
 * @return  a newly created list element which was inserted in the list
 */
List* List_emplace( List** head, void* data, ListFree_f* freeFunction );

/** @brief  List_free
 *          frees the data at a given list element.
 *          Note that this doesn't free the element itself.
 *
 * @param   element - the element for which the data should be freed
 */
void List_free( List* head );

/** @brief  List_clear
 *          removes all elements from the list container (which are destroyed),
 *          leaving the container with a size of 0.
 *
 * @param   listHead - the top element of the list to be clear.
 */
void List_clear( List* listHead );

/** @brief  Removes from the list container a single element  (and frees it)
 *
 * @param   realhead - a pointer to the head element of a list
 * @param   element - the element to find and remove
 * @return  0 on successful find-and-delete, 1 otherwise.
 */
int List_erase( List** realHead, List* element );

#endif // IOT_LIST_H
