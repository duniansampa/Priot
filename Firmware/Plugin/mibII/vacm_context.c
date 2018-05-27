#include "vacm_context.h"
#include "AgentRegistry.h"
#include "Client.h"
#include "System/Util/Logger.h"
#include "TableIterator.h"
#include <string.h>

static oid vacm_context_oid[] = { 1, 3, 6, 1, 6, 3, 16, 1, 1 };

#define CONTEXTNAME_COLUMN 1

/*
 * return the index data from the first node in the agent's
 * SubtreeContextCache list.
 */
VariableList*
get_first_context( void** my_loop_context, void** my_data_context,
    VariableList* put_data,
    IteratorInfo* iinfo )
{
    SubtreeContextCache* context_ptr;
    context_ptr = AgentRegistry_getTopContextCache();

    if ( !context_ptr )
        return NULL;

    *my_loop_context = context_ptr;
    *my_data_context = context_ptr;

    Client_setVarValue( put_data, context_ptr->context_name,
        strlen( context_ptr->context_name ) );
    return put_data;
}

/*
 * return the next index data from the first node in the agent's
 * SubtreeContextCache list.
 */
VariableList*
get_next_context( void** my_loop_context,
    void** my_data_context,
    VariableList* put_data,
    IteratorInfo* iinfo )
{
    SubtreeContextCache* context_ptr;

    if ( !my_loop_context || !*my_loop_context )
        return NULL;

    context_ptr = ( SubtreeContextCache* )( *my_loop_context );
    context_ptr = context_ptr->next;
    *my_loop_context = context_ptr;
    *my_data_context = context_ptr;

    if ( !context_ptr )
        return NULL;

    Client_setVarValue( put_data, context_ptr->context_name,
        strlen( context_ptr->context_name ) );
    return put_data;
}

void init_vacm_context( void )
{
    /*
     * table vacm_context
     */
    HandlerRegistration* my_handler;
    TableRegistrationInfo* table_info;
    IteratorInfo* iinfo;

    my_handler = AgentHandler_createHandlerRegistration( "vacm_context",
        vacm_context_handler,
        vacm_context_oid,
        sizeof( vacm_context_oid ) / sizeof( oid ),
        HANDLER_CAN_RONLY );

    if ( !my_handler )
        return;

    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    iinfo = MEMORY_MALLOC_TYPEDEF( IteratorInfo );

    if ( !table_info || !iinfo ) {
        MEMORY_FREE( table_info );
        MEMORY_FREE( iinfo );
        MEMORY_FREE( my_handler );
        return;
    }

    Table_helperAddIndex( table_info, ASN01_OCTET_STR )
        table_info->min_column
        = 1;
    table_info->max_column = 1;
    iinfo->get_first_data_point = get_first_context;
    iinfo->get_next_data_point = get_next_context;
    iinfo->table_reginfo = table_info;
    TableIterator_registerTableIterator2( my_handler, iinfo );
}

/*
 * returns a list of known context names
 */

int vacm_context_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    SubtreeContextCache* context_ptr;

    for ( ; requests; requests = requests->next ) {
        VariableList* var = requests->requestvb;

        if ( requests->processed != 0 )
            continue;

        context_ptr = ( SubtreeContextCache* )
            TableIterator_extractIteratorContext( requests );

        if ( context_ptr == NULL ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "vacm_context_handler called without data\n" );
            continue;
        }

        switch ( reqinfo->mode ) {
        case MODE_GET:
            /*
             * if here we should have a context_ptr passed in already 
             */
            /*
             * only one column should ever reach us, so don't check it 
             */
            Client_setVarTypedValue( var, ASN01_OCTET_STR,
                context_ptr->context_name,
                strlen( context_ptr->context_name ) );

            break;
        default:
            /*
             * We should never get here, getnext already have been
             * handled by the table_iterator and we're read_only 
             */
            Logger_log( LOGGER_PRIORITY_ERR,
                "vacm_context table accessed as mode=%d.  We're improperly registered!",
                reqinfo->mode );
            break;
        }
    }

    return PRIOT_ERR_NOERROR;
}
