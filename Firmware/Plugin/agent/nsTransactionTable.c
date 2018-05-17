/*
 * Note: this file originally auto-generated by mib2c 
 */

#include "nsTransactionTable.h"
#include "Client.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"

/** Initialize the nsTransactionTable table by defining it's contents
   and how it's structured */
void initialize_table_nsTransactionTable( void )
{
    const oid nsTransactionTable_oid[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 8, 1 };
    TableRegistrationInfo* table_info;
    HandlerRegistration* my_handler;
    IteratorInfo* iinfo;

    /*
     * create the table structure itself 
     */
    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    iinfo = MEMORY_MALLOC_TYPEDEF( IteratorInfo );

    /*
     * if your table is read only, it's easiest to change the
     * HANDLER_CAN_RWRITE definition below to HANDLER_CAN_RONLY 
     */
    my_handler = AgentHandler_createHandlerRegistration(
        "nsTransactionTable", nsTransactionTable_handler,
        nsTransactionTable_oid, ASN01_OID_LENGTH( nsTransactionTable_oid ),
        HANDLER_CAN_RONLY );

    if ( !my_handler || !table_info || !iinfo ) {
        if ( my_handler )
            AgentHandler_handlerRegistrationFree( my_handler );
        MEMORY_FREE( table_info );
        MEMORY_FREE( iinfo );
        return; /* mallocs failed */
    }

    /***************************************************
     * Setting up the table's definition
     */
    Table_helperAddIndex( table_info, ASN01_INTEGER ); /* index:
                                                                 * * nsTransactionID 
                                                                 */

    table_info->min_column = 2;
    table_info->max_column = 2;
    iinfo->get_first_data_point = nsTransactionTable_get_first_data_point;
    iinfo->get_next_data_point = nsTransactionTable_get_next_data_point;
    iinfo->table_reginfo = table_info;

    /***************************************************
     * registering the table with the master agent
     */
    DEBUG_MSGTL( ( "initialize_table_nsTransactionTable",
        "Registering table nsTransactionTable as a table iterator\n" ) );
    TableIterator_registerTableIterator2( my_handler, iinfo );
}

/** Initializes the nsTransactionTable module */
void init_nsTransactionTable( void )
{

    /*
     * here we initialize all the tables we're planning on supporting 
     */
    initialize_table_nsTransactionTable();
}

/** returns the first data point within the nsTransactionTable table data.

    Set the my_loop_context variable to the first data point structure
    of your choice (from which you can find the next one).  This could
    be anything from the first node in a linked list, to an integer
    pointer containing the beginning of an array variable.

    Set the my_data_context variable to something to be returned to
    you later that will provide you with the data to return in a given
    row.  This could be the same pointer as what my_loop_context is
    set to, or something different.

    The put_index_data variable contains a list of snmp variable
    bindings, one for each index in your table.  Set the values of
    each appropriately according to the data matching the first row
    and return the put_index_data variable at the end of the function.
*/
extern AgentSession* agent_delegated_list;

Types_VariableList*
nsTransactionTable_get_first_data_point( void** my_loop_context,
    void** my_data_context,
    Types_VariableList* put_index_data,
    IteratorInfo* iinfo )
{

    Types_VariableList* vptr;

    if ( !agent_delegated_list )
        return NULL;

    *my_loop_context = ( void* )agent_delegated_list;
    *my_data_context = ( void* )agent_delegated_list;

    vptr = put_index_data;

    Client_setVarValue( vptr,
        ( u_char* )&agent_delegated_list->pdu->transid,
        sizeof( agent_delegated_list->pdu->transid ) );

    return put_index_data;
}

/** functionally the same as nsTransactionTable_get_first_data_point, but
   my_loop_context has already been set to a previous value and should
   be updated to the next in the list.  For example, if it was a
   linked list, you might want to cast it and the return
   my_loop_context->next.  The my_data_context pointer should be set
   to something you need later and the indexes in put_index_data
   updated again. */

Types_VariableList*
nsTransactionTable_get_next_data_point( void** my_loop_context,
    void** my_data_context,
    Types_VariableList* put_index_data,
    IteratorInfo* iinfo )
{

    Types_VariableList* vptr;
    AgentSession* alist = ( AgentSession* )*my_loop_context;

    if ( !alist->next )
        return NULL;

    alist = alist->next;

    *my_loop_context = ( void* )alist;
    *my_data_context = ( void* )alist;

    vptr = put_index_data;

    Client_setVarValue( vptr, ( u_char* )&alist->pdu->transid,
        sizeof( alist->pdu->transid ) );
    return put_index_data;
}

/** handles requests for the nsTransactionTable table, if anything
   else needs to be done */
int nsTransactionTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    TableRequestInfo* table_info;
    Types_VariableList* var;
    AgentSession* asp;

    for ( ; requests; requests = requests->next ) {
        var = requests->requestvb;
        if ( requests->processed != 0 )
            continue;

        /*
         * perform anything here that you need to do.  The requests have
         * already been processed by the master table_dataset handler, but
         * this gives you chance to act on the request in some other way if 
         * need be. 
         */

        /*
         * the following extracts the my_data_context pointer set in the
         * loop functions above.  You can then use the results to help
         * return data for the columns of the nsTransactionTable table in
         * question 
         */
        asp = ( AgentSession* )
            TableIterator_extractIteratorContext( requests );
        if ( asp == NULL ) {
            Agent_setRequestError( reqinfo, requests,
                PRIOT_NOSUCHINSTANCE );
            continue;
        }

        /*
         * extracts the information about the table from the request 
         */
        table_info = Table_extractTableInfo( requests );

        /*
         * table_info->colnum contains the column number requested 
         */
        /*
         * table_info->indexes contains a linked list of snmp variable
         * bindings for the indexes of the table.  Values in the list have 
         * been set corresponding to the indexes of the request 
         */
        if ( table_info == NULL ) {
            continue;
        }

        switch ( reqinfo->mode ) {
        /*
             * the table_iterator helper should change all GETNEXTs into
             * GETs for you automatically, so you don't have to worry
             * about the GETNEXT case.  Only GETs and SETs need to be
             * dealt with here 
             */
        case MODE_GET:
            switch ( table_info->colnum ) {

            case COLUMN_NSTRANSACTIONMODE:
                Client_setVarTypedValue( var, ASN01_INTEGER,
                    ( u_char* )&asp->mode,
                    sizeof( asp->mode ) );
                break;

            default:
                /*
                 * We shouldn't get here 
                 */
                Logger_log( LOGGER_PRIORITY_ERR,
                    "problem encountered in nsTransactionTable_handler: unknown column\n" );
            }
            break;

        default:
            Logger_log( LOGGER_PRIORITY_ERR,
                "problem encountered in nsTransactionTable_handler: unsupported mode\n" );
        }
    }
    return PRIOT_ERR_NOERROR;
}
