/*
 * Note: this file originally auto-generated by mib2c using
 *  : mib2c.iterate.conf,v 5.17 2005/05/09 08:13:45 dts12 Exp $
 */

#include "nsVacmAccessTable.h"
#include "CheckVarbind.h"
#include "Client.h"
#include "System/Util/Debug.h"
#include "System/Containers/MapList.h"
#include "Table.h"
#include "Tc.h"
#include "Vacm.h"

/** Initializes the nsVacmAccessTable module */
void init_register_nsVacm_context( const char* context )
{
    /*
     * Initialize the nsVacmAccessTable table by defining its
     *   contents and how it's structured
     */
    const oid nsVacmAccessTable_oid[] = { 1, 3, 6, 1, 4, 1, 8072, 1, 9, 1 };
    HandlerRegistration* reg;
    IteratorInfo* iinfo;
    TableRegistrationInfo* table_info;

    reg = AgentHandler_createHandlerRegistration(
        "nsVacmAccessTable", nsVacmAccessTable_handler,
        nsVacmAccessTable_oid, ASN01_OID_LENGTH( nsVacmAccessTable_oid ),
        HANDLER_CAN_RWRITE );

    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    Table_helperAddIndexes( table_info,
        ASN01_OCTET_STR, /* index: vacmGroupName */
        ASN01_OCTET_STR, /* index: vacmAccessContextPrefix */
        ASN01_INTEGER, /* index: vacmAccessSecurityModel */
        ASN01_INTEGER, /* index: vacmAccessSecurityLevel */
        ASN01_OCTET_STR, /* index: nsVacmAuthType */
        0 );
    table_info->min_column = COLUMN_NSVACMCONTEXTMATCH;
    table_info->max_column = COLUMN_NSVACMACCESSSTATUS;

    iinfo = MEMORY_MALLOC_TYPEDEF( IteratorInfo );
    iinfo->get_first_data_point = nsVacmAccessTable_get_first_data_point;
    iinfo->get_next_data_point = nsVacmAccessTable_get_next_data_point;
    iinfo->table_reginfo = table_info;

    if ( context && context[ 0 ] )
        reg->contextName = strdup( context );

    TableIterator_registerTableIterator2( reg, iinfo );
}

void init_nsVacmAccessTable( void )
{
    init_register_nsVacm_context( "" );
}

/*
 * Iterator hook routines
 */
static int nsViewIdx; /* This should really be handled via the 'loop_context'
                           parameter, but it's easier (read lazier) to use a
                           global variable as well.  Bad David! */

Types_VariableList*
nsVacmAccessTable_get_first_data_point( void** my_loop_context,
    void** my_data_context,
    Types_VariableList* put_index_data,
    IteratorInfo* mydata )
{
    Vacm_scanAccessInit();
    *my_loop_context = Vacm_scanAccessNext();
    nsViewIdx = 0;
    return nsVacmAccessTable_get_next_data_point( my_loop_context,
        my_data_context,
        put_index_data, mydata );
}

Types_VariableList*
nsVacmAccessTable_get_next_data_point( void** my_loop_context,
    void** my_data_context,
    Types_VariableList* put_index_data,
    IteratorInfo* mydata )
{
    struct Vacm_AccessEntry_s* entry = ( struct Vacm_AccessEntry_s* )*my_loop_context;
    Types_VariableList* idx;
    int len;
    char* cp;

newView:
    idx = put_index_data;
    if ( nsViewIdx == VACM_MAX_VIEWS ) {
        entry = Vacm_scanAccessNext();
        nsViewIdx = 0;
    }
    if ( entry ) {
        len = entry->groupName[ 0 ];
        Client_setVarValue( idx, ( u_char* )entry->groupName + 1, len );
        idx = idx->nextVariable;
        len = entry->contextPrefix[ 0 ];
        Client_setVarValue( idx, ( u_char* )entry->contextPrefix + 1, len );
        idx = idx->nextVariable;
        Client_setVarValue( idx, ( u_char* )&entry->securityModel,
            sizeof( entry->securityModel ) );
        idx = idx->nextVariable;
        Client_setVarValue( idx, ( u_char* )&entry->securityLevel,
            sizeof( entry->securityLevel ) );
        /*
         * Find the next valid authType view - skipping unused entries
         */
        idx = idx->nextVariable;
        for ( ; nsViewIdx < VACM_MAX_VIEWS; nsViewIdx++ ) {
            if ( entry->views[ nsViewIdx ][ 0 ] )
                break;
        }
        if ( nsViewIdx == VACM_MAX_VIEWS )
            goto newView;
        cp = MapList_findLabel( VACM_VIEW_ENUM_NAME, nsViewIdx++ );
        DEBUG_MSGTL( ( "nsVacm", "nextDP %s:%s (%d)\n", entry->groupName + 1, cp, nsViewIdx - 1 ) );
        Client_setVarValue( idx, ( u_char* )cp, strlen( cp ) );
        idx = idx->nextVariable;
        *my_data_context = ( void* )entry;
        *my_loop_context = ( void* )entry;
        return put_index_data;
    } else {
        return NULL;
    }
}

/** handles requests for the nsVacmAccessTable table */
int nsVacmAccessTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    RequestInfo* request;
    TableRequestInfo* table_info;
    Types_VariableList* idx;
    struct Vacm_AccessEntry_s* entry;
    char atype[ 20 ];
    int viewIdx, ret;
    char *gName, *cPrefix;
    int model, level;

    switch ( reqinfo->mode ) {
    /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            entry = ( struct Vacm_AccessEntry_s* )
                TableIterator_extractIteratorContext( request );
            table_info = Table_extractTableInfo( request );

            /* Extract the authType token from the list of indexes */
            idx = table_info->indexes->nextVariable->nextVariable->nextVariable->nextVariable;
            memset( atype, 0, sizeof( atype ) );
            memcpy( atype, ( char* )idx->val.string, idx->valLen );
            viewIdx = MapList_findValue( VACM_VIEW_ENUM_NAME, atype );
            DEBUG_MSGTL( ( "nsVacm", "GET %s (%d)\n", idx->val.string, viewIdx ) );

            if ( !entry || viewIdx < 0 )
                continue;

            switch ( table_info->colnum ) {
            case COLUMN_NSVACMCONTEXTMATCH:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->contextMatch );
                break;
            case COLUMN_NSVACMVIEWNAME:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )entry->views[ viewIdx ],
                    strlen( entry->views[ viewIdx ] ) );
                break;
            case COLUMN_VACMACCESSSTORAGETYPE:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->storageType );
                break;
            case COLUMN_NSVACMACCESSSTATUS:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    entry->status );
                break;
            }
        }
        break;

    /*
         * Write-support
         */
    case MODE_SET_RESERVE1:
        for ( request = requests; request; request = request->next ) {
            entry = ( struct Vacm_AccessEntry_s* )
                TableIterator_extractIteratorContext( request );
            table_info = Table_extractTableInfo( request );
            ret = PRIOT_ERR_NOERROR;

            switch ( table_info->colnum ) {
            case COLUMN_NSVACMCONTEXTMATCH:
                ret = CheckVarbind_intRange( request->requestvb, 1, 2 );
                break;
            case COLUMN_NSVACMVIEWNAME:
                ret = CheckVarbind_typeAndMaxSize( request->requestvb,
                    ASN01_OCTET_STR,
                    VACM_MAX_STRING );
                break;
            case COLUMN_VACMACCESSSTORAGETYPE:
                ret = CheckVarbind_storageType( request->requestvb,
                    ( /*entry ? entry->storageType :*/ PRIOT_STORAGE_NONE ) );
                break;
            case COLUMN_NSVACMACCESSSTATUS:
                /*
                 * The usual 'check_vb_rowstatus' call is too simplistic
                 *   to be used here.  Because we're implementing a table
                 *   within an existing table, it's quite possible for a
                 *   the vacmAccessTable entry to exist, even if this is
                 *   a "new" nsVacmAccessEntry.
                 *
                 * We can check that the value being assigned is suitable
                 *   for a RowStatus syntax object, but the transition
                 *   checks need to be done explicitly.
                 */
                ret = CheckVarbind_rowStatusValue( request->requestvb );
                if ( ret != PRIOT_ERR_NOERROR )
                    break;

                /*
                 * Extract the authType token from the list of indexes
                 */
                idx = table_info->indexes->nextVariable->nextVariable->nextVariable->nextVariable;
                memset( atype, 0, sizeof( atype ) );
                memcpy( atype, ( char* )idx->val.string, idx->valLen );
                viewIdx = MapList_findValue( VACM_VIEW_ENUM_NAME, atype );
                if ( viewIdx < 0 ) {
                    ret = PRIOT_ERR_NOCREATION;
                    break;
                }
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_ACTIVE:
                case TC_RS_NOTINSERVICE:
                    /* Check that this particular view is already set */
                    if ( !entry || !entry->views[ viewIdx ][ 0 ] )
                        ret = PRIOT_ERR_INCONSISTENTVALUE;
                    break;
                case TC_RS_CREATEANDWAIT:
                case TC_RS_CREATEANDGO:
                    /* Check that this particular view is not yet set */
                    if ( entry && entry->views[ viewIdx ][ 0 ] )
                        ret = PRIOT_ERR_INCONSISTENTVALUE;
                    break;
                }
                break;
            } /* switch(colnum) */
            if ( ret != PRIOT_ERR_NOERROR ) {
                Agent_setRequestError( reqinfo, request, ret );
                return PRIOT_ERR_NOERROR;
            }
        }
        break;

    case MODE_SET_RESERVE2:
        for ( request = requests; request; request = request->next ) {
            entry = ( struct Vacm_AccessEntry_s* )
                TableIterator_extractIteratorContext( request );
            table_info = Table_extractTableInfo( request );

            switch ( table_info->colnum ) {
            case COLUMN_NSVACMACCESSSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_CREATEANDGO:
                case TC_RS_CREATEANDWAIT:
                    if ( !entry ) {
                        idx = table_info->indexes;
                        gName = ( char* )idx->val.string;
                        idx = idx->nextVariable;
                        cPrefix = ( char* )idx->val.string;
                        idx = idx->nextVariable;
                        model = *idx->val.integer;
                        idx = idx->nextVariable;
                        level = *idx->val.integer;
                        entry = Vacm_createAccessEntry( gName, cPrefix, model, level );
                        entry->storageType = TC_ST_NONVOLATILE;
                        TableIterator_insertIteratorContext( request, ( void* )entry );
                    }
                }
            }
        }
        break;

    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        /* XXX - TODO */
        break;

    case MODE_SET_ACTION:
        /* ??? Empty ??? */
        break;

    case MODE_SET_COMMIT:
        for ( request = requests; request; request = request->next ) {
            entry = ( struct Vacm_AccessEntry_s* )
                TableIterator_extractIteratorContext( request );
            table_info = Table_extractTableInfo( request );
            if ( !entry )
                continue; /* Shouldn't happen */

            /* Extract the authType token from the list of indexes */
            idx = table_info->indexes->nextVariable->nextVariable->nextVariable->nextVariable;
            memset( atype, 0, sizeof( atype ) );
            memcpy( atype, ( char* )idx->val.string, idx->valLen );
            viewIdx = MapList_findValue( VACM_VIEW_ENUM_NAME, atype );
            if ( viewIdx < 0 )
                continue;

            switch ( table_info->colnum ) {
            case COLUMN_NSVACMCONTEXTMATCH:
                entry->contextMatch = *request->requestvb->val.integer;
                break;
            case COLUMN_NSVACMVIEWNAME:
                memset( entry->views[ viewIdx ], 0, VACM_VACMSTRINGLEN );
                memcpy( entry->views[ viewIdx ], request->requestvb->val.string,
                    request->requestvb->valLen );
                break;
            case COLUMN_VACMACCESSSTORAGETYPE:
                entry->storageType = *request->requestvb->val.integer;
                break;
            case COLUMN_NSVACMACCESSSTATUS:
                switch ( *request->requestvb->val.integer ) {
                case TC_RS_DESTROY:
                    memset( entry->views[ viewIdx ], 0, VACM_VACMSTRINGLEN );
                    break;
                }
                break;
            }
        }
        break;
    }
    return PRIOT_ERR_NOERROR;
}
