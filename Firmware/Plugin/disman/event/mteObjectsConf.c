/*
 * DisMan Event MIB:
 *     Implementation of the object table configure handling
 */

#include "mteObjectsConf.h"
#include "AgentCallbacks.h"
#include "AgentReadConfig.h"
#include "Client.h"
#include "Debug.h"
#include "ReadConfig.h"
#include "Tc.h"
#include "mteObjects.h"

/** Initializes the mteObjectsConf module */
void init_mteObjectsConf( void )
{
    init_objects_table_data();

    /*
     * Register config handlers for current and previous style
     *   persistent configuration directives
     */
    AgentReadConfig_priotdRegisterConfigHandler( "_mteOTable",
        parse_mteOTable, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "mteObjectsTable",
        parse_mteOTable, NULL, NULL );
    /*
     * Register to save (non-fixed) entries when the agent shuts down
     */
    Callback_registerCallback( CALLBACK_LIBRARY, CALLBACK_STORE_DATA,
        store_mteOTable, NULL );
    Callback_registerCallback( CALLBACK_APPLICATION,
        PriotdCallback_PRE_UPDATE_CONFIG,
        clear_mteOTable, NULL );
}

void parse_mteOTable( const char* token, char* line )
{
    char owner[ MTE_STR1_LEN + 1 ];
    char oname[ MTE_STR1_LEN + 1 ];
    void* vp;
    u_long index;
    size_t tmpint;
    size_t len;
    TdataRow* row;
    struct mteObject* entry;

    DEBUG_MSGTL( ( "disman:event:conf", "Parsing mteObjectTable config...  " ) );

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof( owner ) );
    memset( oname, 0, sizeof( oname ) );
    len = MTE_STR1_LEN;
    vp = owner;
    line = ReadConfig_readData( ASN01_OCTET_STR, line, &vp, &len );
    len = MTE_STR1_LEN;
    vp = oname;
    line = ReadConfig_readData( ASN01_OCTET_STR, line, &vp, &len );
    line = ReadConfig_readData( ASN01_UNSIGNED, line, &index, &len );

    DEBUG_MSG( ( "disman:event:conf", "(%s, %s, %lu) ", owner, oname, index ) );

    row = mteObjects_createEntry( owner, oname, index, 0 );
    /* entry = (struct mteObject *)TableTdata_rowEntry( row ); */
    entry = ( struct mteObject* )row->data;

    /*
     * Read in the accessible column values
     */
    entry->mteObjectID_len = ASN01_MAX_OID_LEN;
    vp = entry->mteObjectID;
    line = ReadConfig_readData( ASN01_OBJECT_ID, line, &vp,
        &entry->mteObjectID_len );

    if ( !strcasecmp( token, "mteObjectsTable" ) ) {
        /*
         * The previous Event-MIB implementation saved
         *   these fields as separate (integer) values
         * Accept this (for backwards compatability)
         */
        line = ReadConfig_readData( ASN01_UNSIGNED, line, &tmpint, &len );
        if ( tmpint == TC_TV_TRUE )
            entry->flags |= MTE_OBJECT_FLAG_WILD;
        line = ReadConfig_readData( ASN01_UNSIGNED, line, &tmpint, &len );
        if ( tmpint == TC_RS_ACTIVE )
            entry->flags |= MTE_OBJECT_FLAG_ACTIVE;
    } else {
        /*
         * This implementation saves the (relevant) flag bits directly
         */
        line = ReadConfig_readData( ASN01_UNSIGNED, line, &tmpint, &len );
        if ( tmpint & MTE_OBJECT_FLAG_WILD )
            entry->flags |= MTE_OBJECT_FLAG_WILD;
        if ( tmpint & MTE_OBJECT_FLAG_ACTIVE )
            entry->flags |= MTE_OBJECT_FLAG_ACTIVE;
    }

    entry->flags |= MTE_OBJECT_FLAG_VALID;

    DEBUG_MSG( ( "disman:event:conf", "\n" ) );
}

int store_mteOTable( int majorID, int minorID, void* serverarg, void* clientarg )
{
    char line[ TOOLS_MAXBUF ];
    char *cptr, *cp;
    void* vp;
    size_t tint;
    TdataRow* row;
    struct mteObject* entry;

    DEBUG_MSGTL( ( "disman:event:conf", "Storing mteObjectTable config:\n" ) );

    for ( row = TableTdata_rowFirst( objects_table_data );
          row;
          row = TableTdata_rowNext( objects_table_data, row ) ) {

        /*
         * Skip entries that were set up via static config directives
         */
        entry = ( struct mteObject* )TableTdata_rowEntry( row );
        if ( entry->flags & MTE_OBJECT_FLAG_FIXED )
            continue;

        DEBUG_MSGTL( ( "disman:event:conf", "  Storing (%s %s %ld)\n",
            entry->mteOwner, entry->mteOName, entry->mteOIndex ) );
        memset( line, 0, sizeof( line ) );
        strcat( line, "_mteOTable " );
        cptr = line + strlen( line );

        cp = entry->mteOwner;
        tint = strlen( cp );
        cptr = ReadConfig_storeData( ASN01_OCTET_STR, cptr, &cp, &tint );
        cp = entry->mteOName;
        tint = strlen( cp );
        cptr = ReadConfig_storeData( ASN01_OCTET_STR, cptr, &cp, &tint );
        cptr = ReadConfig_storeData( ASN01_UNSIGNED, cptr,
            &entry->mteOIndex, NULL );
        vp = entry->mteObjectID;
        cptr = ReadConfig_storeData( ASN01_OBJECT_ID, cptr, &vp,
            &entry->mteObjectID_len );
        tint = entry->flags & ( MTE_OBJECT_FLAG_WILD | MTE_OBJECT_FLAG_ACTIVE );
        cptr = ReadConfig_storeData( ASN01_UNSIGNED, cptr, &tint, NULL );
        AgentReadConfig_priotdStoreConfig( line );
    }

    DEBUG_MSGTL( ( "disman:event:conf", "  done.\n" ) );
    return ErrorCode_SUCCESS;
}

int clear_mteOTable( int majorID, int minorID, void* serverarg, void* clientarg )
{
    TdataRow* row;
    Types_VariableList owner_var;

    /*
     * We're only interested in entries set up via the config files
     */
    memset( &owner_var, 0, sizeof( Types_VariableList ) );
    Client_setVarTypedValue( &owner_var, ASN01_OCTET_STR,
        "priotd.conf", strlen( "priotd.conf" ) );
    while ( ( row = TableTdata_rowNextByidx( objects_table_data,
                  &owner_var ) ) ) {
        /*
         * XXX - check for owner of "priotd.conf"
         *       and break at the end of these
         */
        TableTdata_removeAndDeleteRow( objects_table_data, row );
    }
    return ErrorCode_SUCCESS;
}
