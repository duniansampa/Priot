#include "List.h"
#include "System/Util/Assert.h"
#include "System/Util/Trace.h"
#include "System/Util/Memory.h"

/** ============================= Public Functions ================== */

void List_free( List* element )
{
    ListFree_f* freeFunction;
    if ( !element )
        return;

    freeFunction = element->freeFunction;
    if ( freeFunction )
        ( freeFunction )( element->data );
}

void List_clear( List* listHead )
{
    List* tmpptr;
    for ( ; listHead; ) {
        List_free( listHead );
        tmpptr = listHead;
        listHead = listHead->next;
        MEMORY_FREE( tmpptr );
    }
}

List* List_newElement( void* data, ListFree_f* freeFunction )
{
    List* element;

    element = MEMORY_MALLOC_TYPEDEF( List );
    if ( !element )
        return NULL;

    element->data = data;
    element->freeFunction = freeFunction;

    return element;
}

void List_insert( List** listHead, List* element )
{
    List* ptr;

    Assert_assert( NULL != listHead );
    Assert_assert( NULL != element );

    if ( !*listHead ) {
        *listHead = element;
        return;
    }

    for ( ptr = *listHead; ptr->next != NULL; ptr = ptr->next )
        ;

    Assert_assert( NULL != ptr );
    if ( ptr ) /* should always be true */
        ptr->next = element;
}

List* List_emplace( List** listHead, void* data, ListFree_f* freeFunction )
{
    List* node;

    node = List_newElement( data, freeFunction );
    if ( NULL == node ) {
        Logger_log( LOGGER_PRIORITY_ERR, "could not allocate memory for node." );
        return NULL;
    }

    List_insert( listHead, node );

    return node;
}

int List_erase( List** realhead, List* element )
{
    List *head, *prev;
    if ( !element )
        return 1;
    for ( head = *realhead, prev = NULL; head; prev = head, head = head->next ) {
        if ( head == element ) {
            if ( prev )
                prev->next = head->next;
            else
                *realhead = head->next;
            List_free( head );
            MEMORY_FREE( head );
            return 0;
        }
    }
    return 1;
}
