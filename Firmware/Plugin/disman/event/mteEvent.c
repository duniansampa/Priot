/*
 * DisMan Event MIB:
 *     Core implementation of the event handling behaviour
 */

#include "mteEvent.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "System/Util/DefaultStore.h"
#include "DsAgent.h"
#include "TableData.h"
#include "Trap.h"
#include "mteObjects.h"

Tdata* event_table_data;

/*
     * Initialize the container for the (combined) mteEvent*Table,
     * regardless of which table initialisation routine is called first.
     */

void init_event_table_data( void )
{
    DEBUG_MSGTL( ( "disman:event:init", "init event container\n" ) );
    if ( !event_table_data ) {
        event_table_data = TableTdata_createTable( "mteEventTable", 0 );
        DEBUG_MSGTL( ( "disman:event:init", "create event container (%p)\n",
            event_table_data ) );
    }
}

void _init_default_mteEvent( const char* event, const char* oname, int specific );
void _init_link_mteEvent( const char* event, const char* oname, int specific );
void _init_builtin_mteEvent( const char* event, const char* oname,
    oid* trapOID, size_t trapOID_len );

/** Initializes the mteEvent module */
void init_mteEvent( void )
{
    static int _defaults_init = 0;
    init_event_table_data();

    /*
     * Insert fixed events for the default trigger notifications
     * 
     * NB: internal events (with an owner of "_snmpd") will not in
     * fact refer to the mteObjectsTable for the payload varbinds.
     * The routine mteObjects_internal_vblist() hardcodes the 
     * appropriate varbinds for these internal events.
     *   This routine will need to be updated whenever a new 
     * internal event is added.
     */
    if ( _defaults_init )
        return;

    _init_default_mteEvent( "mteTriggerFired", "_triggerFire", 1 );
    _init_default_mteEvent( "mteTriggerRising", "_triggerFire", 2 );
    _init_default_mteEvent( "mteTriggerFalling", "_triggerFire", 3 );
    _init_default_mteEvent( "mteTriggerFailure", "_triggerFail", 4 );

    _init_link_mteEvent( "linkDown", "_linkUpDown", 3 );
    _init_link_mteEvent( "linkUp", "_linkUpDown", 4 );
    _defaults_init = 1;
}

void _init_builtin_mteEvent( const char* event, const char* oname, oid* trapOID, size_t trapOID_len )
{
    char ename[ MTE_STR1_LEN + 1 ];
    TdataRow* row;
    struct mteEvent* entry;

    memset( ename, 0, sizeof( ename ) );
    ename[ 0 ] = '_';
    memcpy( ename + 1, event, strlen( event ) );

    row = mteEvent_createEntry( "_snmpd", ename, 1 );
    if ( !row || !row->data )
        return;
    entry = ( struct mteEvent* )row->data;

    entry->mteEventActions = MTE_EVENT_NOTIFICATION;
    entry->mteNotification_len = trapOID_len;
    memcpy( entry->mteNotification, trapOID, trapOID_len * sizeof( oid ) );
    memcpy( entry->mteNotifyOwner, "_snmpd", 6 );
    memcpy( entry->mteNotifyObjects, oname, strlen( oname ) );
    entry->flags |= MTE_EVENT_FLAG_ENABLED | MTE_EVENT_FLAG_ACTIVE | MTE_EVENT_FLAG_VALID;
}

void _init_default_mteEvent( const char* event, const char* oname, int specific )
{
    oid mteTrapOID[] = { 1, 3, 6, 1, 2, 1, 88, 2, 0, 99 /* placeholder */ };
    size_t mteTrapOID_len = ASN01_OID_LENGTH( mteTrapOID );

    mteTrapOID[ mteTrapOID_len - 1 ] = specific;
    _init_builtin_mteEvent( event, oname, mteTrapOID, mteTrapOID_len );
}

void _init_link_mteEvent( const char* event, const char* oname, int specific )
{
    oid mteTrapOID[] = { 1, 3, 6, 1, 6, 3, 1, 1, 5, 99 /* placeholder */ };
    size_t mteTrapOID_len = ASN01_OID_LENGTH( mteTrapOID );

    mteTrapOID[ mteTrapOID_len - 1 ] = specific;
    _init_builtin_mteEvent( event, oname, mteTrapOID, mteTrapOID_len );
}

/* ===================================================
     *
     * APIs for maintaining the contents of the (combined)
     *    mteEvent*Table container.
     *
     * =================================================== */

void _mteEvent_dump( void )
{
    struct mteEvent* entry;
    TdataRow* row;
    int i = 0;

    for ( row = TableTdata_rowFirst( event_table_data );
          row;
          row = TableTdata_rowNext( event_table_data, row ) ) {
        entry = ( struct mteEvent* )row->data;
        DEBUG_MSGTL( ( "disman:event:dump", "EventTable entry %d: ", i ) );
        DEBUG_MSGOID( ( "disman:event:dump", row->oid_index.oids, row->oid_index.len ) );
        DEBUG_MSG( ( "disman:event:dump", "(%s, %s)",
            row->indexes->value.string,
            row->indexes->next->value.string ) );
        DEBUG_MSG( ( "disman:event:dump", ": %p, %p\n", row, entry ) );
        i++;
    }
    DEBUG_MSGTL( ( "disman:event:dump", "EventTable %d entries\n", i ) );
}

/*
 * Create a new row in the event table 
 */
TdataRow*
mteEvent_createEntry( const char* mteOwner, const char* mteEName, int fixed )
{
    struct mteEvent* entry;
    TdataRow* row;
    size_t mteOwner_len = ( mteOwner ) ? strlen( mteOwner ) : 0;
    size_t mteEName_len = ( mteEName ) ? strlen( mteEName ) : 0;

    DEBUG_MSGTL( ( "disman:event:table", "Create event entry (%s, %s)\n",
        mteOwner, mteEName ) );
    /*
     * Create the mteEvent entry, and the
     * (table-independent) row wrapper structure...
     */
    entry = MEMORY_MALLOC_TYPEDEF( struct mteEvent );
    if ( !entry )
        return NULL;

    row = TableTdata_createRow();
    if ( !row ) {
        MEMORY_FREE( entry );
        return NULL;
    }
    row->data = entry;

    /*
     * ... initialize this row with the indexes supplied
     *     and the default values for the row...
     */
    if ( mteOwner )
        memcpy( entry->mteOwner, mteOwner, mteOwner_len );
    TableData_rowAddIndex( row, ASN01_OCTET_STR,
        entry->mteOwner, mteOwner_len );
    if ( mteEName )
        memcpy( entry->mteEName, mteEName, mteEName_len );
    TableData_rowAddIndex( row, ASN01_PRIV_IMPLIED_OCTET_STR,
        entry->mteEName, mteEName_len );

    entry->mteNotification_len = 2; /* .0.0 */
    if ( fixed )
        entry->flags |= MTE_EVENT_FLAG_FIXED;

    /*
     * ... and insert the row into the (common) table container
     */
    TableTdata_addRow( event_table_data, row );
    DEBUG_MSGTL( ( "disman:event:table", "Event entry created\n" ) );
    return row;
}

/*
 * Remove a row from the event table 
 */
void mteEvent_removeEntry( TdataRow* row )
{
    struct mteEvent* entry;

    if ( !row )
        return; /* Nothing to remove */
    entry = ( struct mteEvent* )
        TableTdata_removeAndDeleteRow( event_table_data, row );
    MEMORY_FREE( entry );
}

/* ===================================================
     *
     * APIs for processing the firing of an event
     *
     * =================================================== */

int _mteEvent_fire_notify( struct mteEvent* event,
    struct mteTrigger* trigger,
    oid* suffix, size_t sfx_len );
int _mteEvent_fire_set( struct mteEvent* event,
    struct mteTrigger* trigger,
    oid* suffix, size_t sfx_len );

int mteEvent_fire( char* owner, char* event, /* Event to invoke    */
    struct mteTrigger* trigger, /* Trigger that fired */
    oid* suffix, size_t s_len ) /* Matching instance  */
{
    struct mteEvent* entry;
    int fired = 0;
    VariableList owner_var, event_var;

    DEBUG_MSGTL( ( "disman:event:fire", "Event fired (%s, %s)\n",
        owner, event ) );

    /*
     * Retrieve the entry for the specified event
     */
    memset( &owner_var, 0, sizeof( owner_var ) );
    memset( &event_var, 0, sizeof( event_var ) );
    Client_setVarTypedValue( &owner_var, ASN01_OCTET_STR, owner, strlen( owner ) );
    Client_setVarTypedValue( &event_var, ASN01_PRIV_IMPLIED_OCTET_STR,
        event, strlen( event ) );
    owner_var.next = &event_var;
    entry = ( struct mteEvent* )
        TableTdata_rowEntry(
            TableTdata_rowGetByidx( event_table_data, &owner_var ) );
    if ( !entry ) {
        DEBUG_MSGTL( ( "disman:event:fire", "No matching event\n" ) );
        return -1;
    }

    if ( entry->mteEventActions & MTE_EVENT_NOTIFICATION ) {
        DEBUG_MSGTL( ( "disman:event:fire", "Firing notification event\n" ) );
        _mteEvent_fire_notify( entry, trigger, suffix, s_len );
        fired = 1;
    }
    if ( entry->mteEventActions & MTE_EVENT_SET ) {
        DEBUG_MSGTL( ( "disman:event:fire", "Firing set event\n" ) );
        _mteEvent_fire_set( entry, trigger, suffix, s_len );
        fired = 1;
    }

    if ( !fired )
        DEBUG_MSGTL( ( "disman:event:fire", "Matched event is empty\n" ) );

    return fired;
}

int _mteEvent_fire_notify( struct mteEvent* entry, /* The event to fire  */
    struct mteTrigger* trigger, /* Trigger that fired */
    oid* suffix, size_t sfx_len ) /* Matching instance  */
{
    VariableList *var, *v2;
    extern const oid trap_priotTrapOid[];
    extern const size_t trap_priotTrapOidLen;
    Types_Session* s;

    /*
          * The Event-MIB specification says that objects from the
          *   mteEventTable should come after those from the trigger,
          *   but things actually work better if these come first.
          * Allow the agent to be configured either way.
          */
    int strictOrdering = DefaultStore_getBoolean(
        DsStore_APPLICATION_ID,
        DsAgentBoolean_STRICT_DISMAN );

    var = ( VariableList* )MEMORY_MALLOC_TYPEDEF( VariableList );
    if ( !var )
        return -1;

    /*
     * Set the basic notification OID...
     */
    memset( var, 0, sizeof( VariableList ) );
    Client_setVarObjid( var, trap_priotTrapOid, trap_priotTrapOidLen );
    Client_setVarTypedValue( var, ASN01_OBJECT_ID,
        ( u_char* )entry->mteNotification,
        entry->mteNotification_len * sizeof( oid ) );

    /*
     * ... then add the specified objects from the Objects Table.
     *
     * Strictly speaking, the objects from the EventTable are meant
     *   to be listed last (after the various trigger objects).
     * But logically things actually work better if the event objects
     *   are placed first.  So this code handles things either way :-)
     */

    if ( !strictOrdering ) {
        DEBUG_MSGTL( ( "disman:event:fire", "Adding event objects (first)\n" ) );
        if ( strcmp( entry->mteNotifyOwner, "_snmpd" ) != 0 )
            mteObjects_vblist( var, entry->mteNotifyOwner,
                entry->mteNotifyObjects,
                suffix, sfx_len );
    }

    DEBUG_MSGTL( ( "disman:event:fire", "Adding trigger objects (general)\n" ) );
    mteObjects_vblist( var, trigger->mteTriggerOOwner,
        trigger->mteTriggerObjects,
        suffix, sfx_len );
    DEBUG_MSGTL( ( "disman:event:fire", "Adding trigger objects (specific)\n" ) );
    mteObjects_vblist( var, trigger->mteTriggerXOwner,
        trigger->mteTriggerXObjects,
        suffix, sfx_len );

    if ( strictOrdering ) {
        DEBUG_MSGTL( ( "disman:event:fire", "Adding event objects (last)\n" ) );
        if ( strcmp( entry->mteNotifyOwner, "_snmpd" ) != 0 )
            mteObjects_vblist( var, entry->mteNotifyOwner,
                entry->mteNotifyObjects,
                suffix, sfx_len );
    }

    /*
     * Query the agent to retrieve the necessary values...
     *   (skipping the initial snmpTrapOID varbind)
     */
    v2 = var->next;
    if ( entry->session )
        s = entry->session;
    else
        s = trigger->session;
    Client_queryGet( v2, s );

    /*
     * ... add any "internal" objects...
     * (skipped by the processing above, and best handled directly)
     */
    if ( strcmp( entry->mteNotifyOwner, "_snmpd" ) == 0 ) {
        DEBUG_MSGTL( ( "disman:event:fire", "Adding event objects (internal)\n" ) );
        if ( !strictOrdering ) {
            mteObjects_internal_vblist( var, entry->mteNotifyObjects, trigger, s );
        } else {
            for ( v2 = var; v2 && v2->next; v2 = v2->next )
                ;
            mteObjects_internal_vblist( v2, entry->mteNotifyObjects, trigger, s );
        }
    }

    /*
     * ... and send the resulting varbind list as a notification
     */
    Trap_sendV2trap( var );
    Api_freeVarbind( var );
    return 0;
}

int _mteEvent_fire_set( struct mteEvent* entry, /* The event to fire */
    struct mteTrigger* trigger, /* Trigger that fired */
    oid* suffix, size_t sfx_len ) /* Matching instance */
{
    VariableList var;
    oid set_oid[ ASN01_MAX_OID_LEN ];
    size_t set_len;

    /*
     * Set the basic assignment OID...
     */
    memset( set_oid, 0, sizeof( set_oid ) );
    memcpy( set_oid, entry->mteSetOID, entry->mteSetOID_len * sizeof( oid ) );
    set_len = entry->mteSetOID_len;

    /*
     * ... if the trigger value is wildcarded (sfx_len > 0),
     *       *and* the SET event entry is wildcarded,
     *        then add the supplied instance suffix...
     */
    if ( sfx_len && entry->flags & MTE_SET_FLAG_OBJWILD ) {
        memcpy( &set_oid[ set_len ], suffix, sfx_len * sizeof( oid ) );
        set_len += sfx_len;
    }

    /*
     * ... finally build the assignment varbind,
     *        and pass it to be acted on.
     *
     * XXX: Need to handle (remote) targets and non-default contexts
     */
    memset( &var, 0, sizeof( var ) );
    Client_setVarObjid( &var, set_oid, set_len );
    Client_setVarTypedInteger( &var, ASN01_INTEGER, entry->mteSetValue );
    if ( entry->session )
        return Client_querySet( &var, entry->session );
    else
        return Client_querySet( &var, trigger->session );

    /* XXX - Need to check result */
}
