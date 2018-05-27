#ifndef TABLEITERATOR_H
#define TABLEITERATOR_H

#include "AgentHandler.h"
#include "Table.h"

struct IteratorInfo_s;

typedef VariableList*( FirstDataPointFT )( void** loop_context,
    void** data_context,
    VariableList*,
    struct IteratorInfo_s* );

typedef VariableList*( NextDataPointFT )( void** loop_context,
    void** data_context,
    VariableList*,
    struct IteratorInfo_s* );

typedef void*( MakeDataContextFT )( void* loop_context,
    struct IteratorInfo_s* );

typedef void( FreeLoopContextFT )( void*,
    struct IteratorInfo_s* );

typedef void( FreeDataContextFT )( void*,
    struct IteratorInfo_s* );

/** @typedef struct IteratorInfo_s IteratorInfo
 * Typedefs the IteratorInfo_s struct into IteratorInfo */

/** @struct IteratorInfo_s

 * Holds iterator information containing functions which should be
   called by the iterator_handler to loop over your data set and
   sort it in a SNMP specific manner.

   The IteratorInfo typedef can be used instead of directly calling this struct if you would prefer.
 */
typedef struct IteratorInfo_s {
    /** Number of handlers that own this data structure. */
    int refcnt;

    /** Responsible for: returning the first set of "index" data, a
       loop-context pointer, and optionally a data context
       pointer */
    FirstDataPointFT* get_first_data_point;

    /** Given the previous loop context, this should return the
       next loop context, associated index set and optionally a
       data context */
    NextDataPointFT* get_next_data_point;

    /** If a data context wasn't supplied by the
       get_first_data_point or get_next_data_point functions and
       the make_data_context pointer is defined, it will be called
       to convert a loop context into a data context. */
    MakeDataContextFT* make_data_context;

    /** A function which should free the loop context.  This
       function is called at *each* iteration step, which is
       not-optimal for speed purposes.  The use of
       free_loop_context_at_end instead is strongly
       encouraged. This can be set to NULL to avoid its usage. */
    FreeLoopContextFT* free_loop_context;

    /** Frees a data context.  This will be called at any time a
       data context needs to be freed.  This may be at the same
       time as a correspondng loop context is freed, or much much
       later.  Multiple data contexts may be kept in existence at
       any time. */
    FreeDataContextFT* free_data_context;

    /** Frees a loop context at the end of the entire iteration
       sequence.  Generally, this would free the loop context
       allocated by the get_first_data_point function (which would
       then be updated by each call to the get_next_data_point
       function).  It is not called until the get_next_data_point
       function returns a NULL */
    FreeLoopContextFT* free_loop_context_at_end;

    /** This can be used by client handlers to store any
       information they need */
    void* myvoid;
    int flags;
#define NETSNMP_ITERATOR_FLAG_SORTED 0x01
#define NETSNMP_HANDLER_OWNS_IINFO 0x02

    /** A pointer to the TableRegistrationInfo object
       this iterator is registered along with. */
    TableRegistrationInfo* table_reginfo;

    /* Experimental extension - Use At Your Own Risk
       (these two fields may change/disappear without warning) */
    FirstDataPointFT* get_row_indexes;
    VariableList* indexes;
} IteratorInfo;

#define TABLE_ITERATOR_NAME "tableIterator"

/* ============================
* Iterator API: Table maintenance
* ============================ */
/* N/A */

/* ============================
* Iterator API: MIB maintenance
* ============================ */

void TableIterator_handlerOwnsIteratorInfo( MibHandler* h );

MibHandler*
TableIterator_getTableIteratorHandler( IteratorInfo* iinfo );

int TableIterator_registerTableIterator( HandlerRegistration* reginfo,
    IteratorInfo* iinfo );

void TableIterator_deleteTable( IteratorInfo* iinfo );

void* TableIterator_extractIteratorContext( RequestInfo* );

void TableIterator_insertIteratorContext( RequestInfo*, void* );

NodeHandlerFT TableIterator_helperHandler;

#define TableIterator_registerTableIterator2( reginfo, iinfo ) \
    ( ( ( iinfo )->flags |= NETSNMP_HANDLER_OWNS_IINFO ),      \
        TableIterator_registerTableIterator( ( reginfo ), ( iinfo ) ) )

/* ============================
* Iterator API: Row operations
* ============================ */

void* TableIterator_rowFirst( IteratorInfo* );

void* TableIterator_rowGet( IteratorInfo*, void* );

void* TableIterator_rowNext( IteratorInfo*, void* );

void* TableIterator_rowGetByidx( IteratorInfo*,
    VariableList* );

void* TableIterator_rowNextByidx( IteratorInfo*,
    VariableList* );

void* TableIterator_rowGetByoid( IteratorInfo*,
    oid*,
    size_t );

void* TableIterator_rowNextByoid( IteratorInfo*,
    oid*,
    size_t );

int TableIterator_rowCount( IteratorInfo* );

#endif // TABLEITERATOR_H
