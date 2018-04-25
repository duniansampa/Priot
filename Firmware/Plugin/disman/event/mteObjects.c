/*
 * DisMan Event MIB:
 *     Core implementation of the object handling behaviour
 */

#include "mteObjects.h"
#include "Client.h"
#include "Debug.h"
#include "Logger.h"
#include "Mib.h"
#include "ReadConfig.h"

Tdata* objects_table_data;

/*
     * Initialize the container for the object table
     * regardless of which initialisation routine is called first.
     */

void init_objects_table_data( void )
{
    if ( !objects_table_data )
        objects_table_data = TableTdata_createTable( "mteObjectsTable", 0 );
}

Callback_CallbackFT _init_default_mteObject_lists;

/** Initializes the mteObjects module */
void init_mteObjects( void )
{
    init_objects_table_data();

    /*
     * Insert fixed object lists for the default trigger
     * notifications, once the MIB files have been read in.
     */
    Callback_registerCallback( CALLBACK_LIBRARY,
        CALLBACK_POST_READ_CONFIG,
        _init_default_mteObject_lists, NULL );
}

void _init_default_mteObject( const char* oname, const char* object, int index, int wcard )
{
    struct mteObject* entry;

    entry = mteObjects_addOID( "_snmpd", oname, index, object, 0 );
    if ( entry ) {
        entry->flags |= MTE_OBJECT_FLAG_ACTIVE | MTE_OBJECT_FLAG_FIXED | MTE_OBJECT_FLAG_VALID;
        if ( wcard )
            entry->flags |= MTE_OBJECT_FLAG_WILD;
    }
}

int _init_default_mteObject_lists( int majorID, int minorID,
    void* serverargs, void* clientarg )
{
    static int _defaults_init = 0;

    if ( _defaults_init )
        return 0;
    /* mteHotTrigger     */
    _init_default_mteObject( "_triggerFire", ".1.3.6.1.2.1.88.2.1.1", 1, 0 );
    /* mteHotTargetName  */
    _init_default_mteObject( "_triggerFire", ".1.3.6.1.2.1.88.2.1.2", 2, 0 );
    /* mteHotContextName */
    _init_default_mteObject( "_triggerFire", ".1.3.6.1.2.1.88.2.1.3", 3, 0 );
    /* mteHotOID         */
    _init_default_mteObject( "_triggerFire", ".1.3.6.1.2.1.88.2.1.4", 4, 0 );
    /* mteHotValue       */
    _init_default_mteObject( "_triggerFire", ".1.3.6.1.2.1.88.2.1.5", 5, 0 );

    /* mteHotTrigger     */
    _init_default_mteObject( "_triggerFail", ".1.3.6.1.2.1.88.2.1.1", 1, 0 );
    /* mteHotTargetName  */
    _init_default_mteObject( "_triggerFail", ".1.3.6.1.2.1.88.2.1.2", 2, 0 );
    /* mteHotContextName */
    _init_default_mteObject( "_triggerFail", ".1.3.6.1.2.1.88.2.1.3", 3, 0 );
    /* mteHotOID         */
    _init_default_mteObject( "_triggerFail", ".1.3.6.1.2.1.88.2.1.4", 4, 0 );
    /* mteFailedReason   */
    _init_default_mteObject( "_triggerFail", ".1.3.6.1.2.1.88.2.1.6", 5, 0 );

    /* ifIndex       */
    _init_default_mteObject( "_linkUpDown", ".1.3.6.1.2.1.2.2.1.1", 1, 1 );
    /* ifAdminStatus */
    _init_default_mteObject( "_linkUpDown", ".1.3.6.1.2.1.2.2.1.7", 2, 1 );
    /* ifOperStatus  */
    _init_default_mteObject( "_linkUpDown", ".1.3.6.1.2.1.2.2.1.8", 3, 1 );

    _defaults_init = 1;
    return 0;
}

/* ===================================================
     *
     * APIs for maintaining the contents of the mteObjectsTable container.
     *
     * =================================================== */

/*
 * Create a new row in the object table 
 */
TdataRow*
mteObjects_createEntry( const char* owner, const char* oname, int index, int flags )
{
    struct mteObject* entry;
    TdataRow *row, *row2;
    size_t owner_len = ( owner ) ? strlen( owner ) : 0;
    size_t oname_len = ( oname ) ? strlen( oname ) : 0;

    /*
     * Create the mteObjects entry, and the
     * (table-independent) row wrapper structure...
     */
    entry = TOOLS_MALLOC_TYPEDEF( struct mteObject );
    if ( !entry )
        return NULL;

    row = TableTdata_createRow();
    if ( !row ) {
        TOOLS_FREE( entry );
        return NULL;
    }
    row->data = entry;

    /*
     * ... initialize this row with the indexes supplied
     *     and the default values for the row...
     */
    if ( owner )
        memcpy( entry->mteOwner, owner, owner_len );
    TableTdata_rowAddIndex( row, ASN01_OCTET_STR,
        entry->mteOwner, owner_len );
    if ( oname )
        memcpy( entry->mteOName, oname, oname_len );
    TableTdata_rowAddIndex( row, ASN01_OCTET_STR,
        entry->mteOName, oname_len );
    entry->mteOIndex = index;
    TableTdata_rowAddIndex( row, ASN01_INTEGER,
        &entry->mteOIndex, sizeof( long ) );

    entry->mteObjectID_len = 2; /* .0.0 */
    if ( flags & MTE_OBJECT_FLAG_FIXED )
        entry->flags |= MTE_OBJECT_FLAG_FIXED;

    /*
     * Check whether there's already a row with the same indexes
     *   (XXX - relies on private internal data ???)
     */
    row2 = TableTdata_rowGetByoid( objects_table_data,
        row->oid_index.oids,
        row->oid_index.len );
    if ( row2 ) {
        if ( flags & MTE_OBJECT_FLAG_NEXT ) {
            /*
             * If appropriate, keep incrementing the final
             * index value until we find a free slot...
             */
            while ( row2 ) {
                row->oid_index.oids[ row->oid_index.len ]++;
                row2 = TableTdata_rowGetByoid( objects_table_data,
                    row->oid_index.oids,
                    row->oid_index.len );
            }
        } else {
            /*
             * ... otherwise, this is an error.
             *     Tidy up, and return failure.
             */
            TableTdata_deleteRow( row );
            TOOLS_FREE( entry );
            return NULL;
        }
    }

    /*
     * ... finally, insert the row into the (common) table container
     */
    TableTdata_addRow( objects_table_data, row );
    return row;
}

/*
 * Add a row to the object table 
 */
struct mteObject*
mteObjects_addOID( const char* owner, const char* oname, int index,
    const char* oid_name_buf, int wild )
{
    TdataRow* row;
    struct mteObject* entry;
    oid name_buf[ ASN01_MAX_OID_LEN ];
    size_t name_buf_len;

    name_buf_len = ASN01_MAX_OID_LEN;
    if ( !Mib_parseOid( oid_name_buf, name_buf, &name_buf_len ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "payload OID: %s\n", oid_name_buf );
        ReadConfig_configPerror( "unknown payload OID" );
        return NULL;
    }

    row = mteObjects_createEntry( owner, oname, index,
        MTE_OBJECT_FLAG_FIXED | MTE_OBJECT_FLAG_NEXT );
    entry = ( struct mteObject* )row->data;

    entry->mteObjectID_len = name_buf_len;
    memcpy( entry->mteObjectID, name_buf, name_buf_len * sizeof( oid ) );
    if ( wild )
        entry->flags |= MTE_OBJECT_FLAG_WILD;
    entry->flags |= MTE_OBJECT_FLAG_VALID | MTE_OBJECT_FLAG_ACTIVE;

    return entry;
}

/*
 * Remove a row from the event table 
 */
void mteObjects_removeEntry( TdataRow* row )
{
    struct mteObject* entry;

    if ( !row )
        return; /* Nothing to remove */
    entry = ( struct mteObject* )
        TableTdata_removeAndDeleteRow( objects_table_data, row );
    TOOLS_FREE( entry );
}

/*
 * Remove all matching rows from the event table 
 */
void mteObjects_removeEntries( const char* owner, char* oname )
{
    TdataRow* row;
    Types_VariableList owner_var, oname_var;

    memset( &owner_var, 0, sizeof( owner_var ) );
    memset( &oname_var, 0, sizeof( oname_var ) );
    Client_setVarTypedValue( &owner_var, ASN01_OCTET_STR,
        owner, strlen( owner ) );
    Client_setVarTypedValue( &oname_var, ASN01_OCTET_STR,
        oname, strlen( oname ) );
    owner_var.nextVariable = &oname_var;

    row = TableTdata_rowNextByidx( objects_table_data, &owner_var );

    while ( row && !TableTdata_compareSubtreeIdx( row, &owner_var ) ) {
        mteObjects_removeEntry( row );
        row = TableTdata_rowNextByidx( objects_table_data, &owner_var );
    }
    return;
}

/* ===================================================
     *
     * API for retrieving a list of matching objects
     *
     * =================================================== */

int mteObjects_vblist( Types_VariableList* vblist,
    char* owner, char* oname,
    oid* suffix, size_t sfx_len )
{
    TdataRow* row;
    struct mteObject* entry;
    Types_VariableList owner_var, oname_var;
    Types_VariableList* var = vblist;
    oid name[ ASN01_MAX_OID_LEN ];
    size_t name_len;

    if ( !oname || !*oname ) {
        DEBUG_MSGTL( ( "disman:event:objects", "No objects to add (%s)\n",
            owner ) );
        return 1; /* Empty object name means nothing to add */
    }

    DEBUG_MSGTL( ( "disman:event:objects", "Objects add (%s, %s)\n",
        owner, oname ) );

    /*
     * Retrieve any matching entries from the mteObjectTable
     *  and add them to the specified varbind list.
     */
    memset( &owner_var, 0, sizeof( owner_var ) );
    memset( &oname_var, 0, sizeof( oname_var ) );
    Client_setVarTypedValue( &owner_var, ASN01_OCTET_STR,
        owner, strlen( owner ) );
    Client_setVarTypedValue( &oname_var, ASN01_OCTET_STR,
        oname, strlen( oname ) );
    owner_var.nextVariable = &oname_var;

    row = TableTdata_rowNextByidx( objects_table_data, &owner_var );

    while ( row && !TableTdata_compareSubtreeIdx( row, &owner_var ) ) {
        entry = ( struct mteObject* )TableTdata_rowEntry( row );

        memset( name, 0, ASN01_MAX_OID_LEN );
        memcpy( name, entry->mteObjectID,
            entry->mteObjectID_len * sizeof( oid ) );
        name_len = entry->mteObjectID_len;

        /*
         * If the trigger value is wildcarded (sfx_len > 0),
         *    *and* this object entry is wildcarded,
         *    then add the supplied instance suffix.
         * Otherwise use the Object OID as it stands.
         */
        if ( sfx_len && entry->flags & MTE_OBJECT_FLAG_WILD ) {
            memcpy( &name[ name_len ], suffix, sfx_len * sizeof( oid ) );
            name_len += sfx_len;
        }
        Api_varlistAddVariable( &var, name, name_len, ASN01_NULL, NULL, 0 );

        row = TableTdata_rowNext( objects_table_data, row );
    }
    return 0;
}

int mteObjects_internal_vblist( Types_VariableList* vblist,
    char* oname,
    struct mteTrigger* trigger,
    Types_Session* sess )
{
    Types_VariableList *var = NULL, *vp;
    oid mteHotTrigger[] = { 1, 3, 6, 1, 2, 1, 88, 2, 1, 1, 0 };
    oid mteHotTarget[] = { 1, 3, 6, 1, 2, 1, 88, 2, 1, 2, 0 };
    oid mteHotContext[] = { 1, 3, 6, 1, 2, 1, 88, 2, 1, 3, 0 };
    oid mteHotOID[] = { 1, 3, 6, 1, 2, 1, 88, 2, 1, 4, 0 };
    oid mteHotValue[] = { 1, 3, 6, 1, 2, 1, 88, 2, 1, 5, 0 };

    oid ifIndexOid[] = { 1, 3, 6, 1, 2, 1, 2, 2, 1, 1, 0 };
    oid ifAdminStatus[] = { 1, 3, 6, 1, 2, 1, 2, 2, 1, 7, 0 };
    oid ifOperStatus[] = { 1, 3, 6, 1, 2, 1, 2, 2, 1, 8, 0 };

    oid if_index;

    /*
     * Construct the varbinds for this (internal) event...
     */
    if ( !strcmp( oname, "_triggerFire" ) ) {

        Api_varlistAddVariable( &var,
            mteHotTrigger, ASN01_OID_LENGTH( mteHotTrigger ),
            ASN01_OCTET_STR, trigger->mteTName,
            strlen( trigger->mteTName ) );
        Api_varlistAddVariable( &var,
            mteHotTarget, ASN01_OID_LENGTH( mteHotTarget ),
            ASN01_OCTET_STR, trigger->mteTriggerTarget,
            strlen( trigger->mteTriggerTarget ) );
        Api_varlistAddVariable( &var,
            mteHotContext, ASN01_OID_LENGTH( mteHotContext ),
            ASN01_OCTET_STR, trigger->mteTriggerContext,
            strlen( trigger->mteTriggerContext ) );
        Api_varlistAddVariable( &var,
            mteHotOID, ASN01_OID_LENGTH( mteHotOID ),
            ASN01_OBJECT_ID, ( char* )trigger->mteTriggerFired->name,
            trigger->mteTriggerFired->nameLength * sizeof( oid ) );
        Api_varlistAddVariable( &var,
            mteHotValue, ASN01_OID_LENGTH( mteHotValue ),
            trigger->mteTriggerFired->type,
            trigger->mteTriggerFired->val.string,
            trigger->mteTriggerFired->valLen );
    } else if ( ( !strcmp( oname, "_linkUpDown" ) ) ) {
        /*
         * The ifOperStatus varbind that triggered this entry
         *  is held in the trigger->mteTriggerFired field
         *
         * We can retrieve the ifIndex and ifOperStatus values
         *  from this varbind.  But first we need to tweak the
         *  static ifXXX OID arrays to include the correct index.
         *  (or this could be passed in from the calling routine?)
         *
         * Unfortunately we don't have the current AdminStatus value,
         *  so we'll need to make another query to retrieve that.
         */
        if_index = trigger->mteTriggerFired->name[ 10 ];
        ifIndexOid[ 10 ] = if_index;
        ifAdminStatus[ 10 ] = if_index;
        ifOperStatus[ 10 ] = if_index;
        Api_varlistAddVariable( &var,
            ifIndexOid, ASN01_OID_LENGTH( ifIndexOid ),
            ASN01_INTEGER, &if_index, sizeof( if_index ) );

        /* Set up a dummy varbind for ifAdminStatus... */
        Api_varlistAddVariable( &var,
            ifAdminStatus, ASN01_OID_LENGTH( ifAdminStatus ),
            ASN01_INTEGER,
            trigger->mteTriggerFired->val.integer,
            trigger->mteTriggerFired->valLen );
        /* ... then retrieve the actual value */
        Client_queryGet( var->nextVariable, sess );

        Api_varlistAddVariable( &var,
            ifOperStatus, ASN01_OID_LENGTH( ifOperStatus ),
            ASN01_INTEGER,
            trigger->mteTriggerFired->val.integer,
            trigger->mteTriggerFired->valLen );
    } else {
        DEBUG_MSGTL( ( "disman:event:objects",
            "Unknown internal objects tag (%s)\n", oname ) );
        return 1;
    }

    /*
     * ... and insert them into the main varbind list
     *     (at the point specified)
     */
    for ( vp = var; vp && vp->nextVariable; vp = vp->nextVariable )
        ;
    vp->nextVariable = vblist->nextVariable;
    vblist->nextVariable = var;
    return 0;
}
