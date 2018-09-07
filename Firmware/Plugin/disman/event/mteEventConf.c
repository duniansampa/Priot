/*
 * DisMan Event MIB:
 *     Implementation of the event table configure handling
 */

#include "mteEventConf.h"
#include "AgentCallbacks.h"
#include "AgentReadConfig.h"
#include "Client.h"
#include "System/Util/DefaultStore.h"
#include "DsAgent.h"
#include "Impl.h"
#include "Mib.h"
#include "Parse.h"
#include "ReadConfig.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "mteEvent.h"
#include "mteObjects.h"

/** Initializes the mteEventsConf module */
void init_mteEventConf( void )
{
    init_event_table_data();

    /*
     * Register config handlers for user-level (fixed) events....
     */
    AgentReadConfig_priotdRegisterConfigHandler( "notificationEvent",
        parse_notificationEvent, NULL,
        "eventname notifyOID [-m] [-i OID|-o OID]*" );
    AgentReadConfig_priotdRegisterConfigHandler( "setEvent",
        parse_setEvent, NULL,
        "eventname [-I] OID = value" );

    DefaultStore_registerConfig( asnBOOLEAN,
        DefaultStore_getString( DsStore_LIBRARY_ID,
                                     DsStr_APPTYPE ),
        "strictDisman", DsStore_APPLICATION_ID,
        DsAgentBoolean_STRICT_DISMAN );

    /*
     * ... and for persistent storage of dynamic event table entries.
     *
     * (The previous implementation didn't store these entries,
     *  so we don't need to worry about backwards compatability)
     */
    AgentReadConfig_priotdRegisterConfigHandler( "_mteETable",
        parse_mteETable, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "_mteENotTable",
        parse_mteENotTable, NULL, NULL );
    AgentReadConfig_priotdRegisterConfigHandler( "_mteESetTable",
        parse_mteESetTable, NULL, NULL );

    /*
     * Register to save (non-fixed) entries when the agent shuts down
     */
    Callback_register( CallbackMajor_LIBRARY, CallbackMinor_STORE_DATA,
        store_mteETable, NULL );
    Callback_register( CallbackMajor_APPLICATION,
        PriotdCallback_PRE_UPDATE_CONFIG,
        clear_mteETable, NULL );
}

/* ==============================
 *
 *       utility routines
 *
 * ============================== */

/*
     * Find or create the specified event entry
     */
static struct mteEvent*
_find_mteEvent_entry( const char* owner, const char* ename )
{
    VariableList owner_var, ename_var;
    TdataRow* row;
    /*
         * If there's already an existing entry,
         *   then use that...
         */
    memset( &owner_var, 0, sizeof( VariableList ) );
    memset( &ename_var, 0, sizeof( VariableList ) );
    Client_setVarTypedValue( &owner_var, asnOCTET_STR, owner, strlen( owner ) );
    Client_setVarTypedValue( &ename_var, asnPRIV_IMPLIED_OCTET_STR,
        ename, strlen( ename ) );
    owner_var.next = &ename_var;
    row = TableTdata_rowGetByidx( event_table_data, &owner_var );
    /*
         * ... otherwise, create a new one
         */
    if ( !row )
        row = mteEvent_createEntry( owner, ename, 0 );
    if ( !row )
        return NULL;

    /* return (struct mteEvent *)TableTdata_rowEntry( row ); */
    return ( struct mteEvent* )row->data;
}

static struct mteEvent*
_find_typed_mteEvent_entry( const char* owner, const char* ename, int type )
{
    struct mteEvent* entry = _find_mteEvent_entry( owner, ename );
    if ( !entry )
        return NULL;

    /*
     *  If this is an existing (i.e. valid) entry of the
     *    same type, then throw an error and discard it.
     *  But allow combined Set/Notification events.
     */
    if ( entry && ( entry->flags & MTE_EVENT_FLAG_VALID ) && ( entry->mteEventActions & type ) ) {
        ReadConfig_configPerror( "error: duplicate event name" );
        return NULL;
    }
    return entry;
}

/* ==============================
 *
 *  User-configured (static) events
 *
 * ============================== */

void parse_notificationEvent( const char* token, char* line )
{
    char ename[ MTE_STR1_LEN + 1 ];
    char buf[ IMPL_SPRINT_MAX_LEN ];
    oid name_buf[ asnMAX_OID_LEN ];
    size_t name_buf_len;
    struct mteEvent* entry;
    struct mteObject* object;
    int wild = 1;
    int idx = 0;
    char* cp;
    struct Parse_Tree_s* tp;
    struct Parse_VarbindList_s* var;

    DEBUG_MSGTL( ( "disman:event:conf", "Parsing notificationEvent config\n" ) );

    /*
     * The event name could be used directly to index the mteObjectsTable.
     * But it's quite possible that the same name could also be used to
     * set up a mteTriggerTable entry (with trigger-specific objects).
     *
     * To avoid such a clash, we'll add a prefix ("_E").
     */
    memset( ename, 0, sizeof( ename ) );
    ename[ 0 ] = '_';
    ename[ 1 ] = 'E';
    cp = ReadConfig_copyNword( line, ename + 2, MTE_STR1_LEN - 2 );
    if ( !cp || ename[ 2 ] == '\0' ) {
        ReadConfig_configPerror( "syntax error: no event name" );
        return;
    }

    /*
     *  Parse the notification OID field ...
     */
    cp = ReadConfig_copyNword( cp, buf, IMPL_SPRINT_MAX_LEN );
    if ( buf[ 0 ] == '\0' ) {
        ReadConfig_configPerror( "syntax error: no notification OID" );
        return;
    }
    name_buf_len = asnMAX_OID_LEN;
    if ( !Mib_parseOid( buf, name_buf, &name_buf_len ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "notificationEvent OID: %s\n", buf );
        ReadConfig_configPerror( "unknown notification OID" );
        return;
    }

    /*
     *  ... and the relevant object/instances.
     */
    if ( cp && *cp == '-' && *( cp + 1 ) == 'm' ) {

        /*
         * Use the MIB definition to add the standard
         *   notification payload to the mteObjectsTable.
         */
        cp = ReadConfig_skipToken( cp );
        tp = Mib_getTree( name_buf, name_buf_len, Mib_getTreeHead() );
        if ( !tp ) {
            ReadConfig_configPerror( "Can't locate notification payload info" );
            return;
        }
        for ( var = tp->varbinds; var; var = var->next ) {
            idx++;
            object = mteObjects_addOID( "priotd.conf", ename, idx,
                var->vblabel, wild );
            idx = object->mteOIndex;
        }
    }
    while ( cp ) {
        if ( *cp == '-' ) {
            switch ( *( cp + 1 ) ) {
            case 'm':
                ReadConfig_configPerror( "-m option must come first" );
                return;
            case 'i': /* exact instance */
            case 'w': /* "not-wild" (backward compatability) */
                wild = 0;
                break;
            case 'o': /* wildcarded object  */
                wild = 1;
                break;
            default:
                ReadConfig_configPerror( "unrecognised option" );
                return;
            }
            cp = ReadConfig_skipToken( cp );
            if ( !cp ) {
                ReadConfig_configPerror( "missing parameter" );
                return;
            }
        }
        idx++;
        cp = ReadConfig_copyNword( cp, buf, IMPL_SPRINT_MAX_LEN );
        object = mteObjects_addOID( "priotd.conf", ename, idx, buf, wild );
        idx = object->mteOIndex;
        wild = 1; /* default to wildcarded objects */
    }

    /*
     *  If the entry has parsed successfully, then create,
     *     populate and activate the new event entry.
     */
    entry = _find_typed_mteEvent_entry( "priotd.conf", ename + 2,
        MTE_EVENT_NOTIFICATION );
    if ( !entry ) {
        mteObjects_removeEntries( "priotd.conf", ename );
        return;
    }
    entry->mteNotification_len = name_buf_len;
    memcpy( entry->mteNotification, name_buf, name_buf_len * sizeof( oid ) );
    memcpy( entry->mteNotifyOwner, "priotd.conf", 10 );
    memcpy( entry->mteNotifyObjects, ename, MTE_STR1_LEN );
    entry->mteEventActions |= MTE_EVENT_NOTIFICATION;
    entry->flags |= MTE_EVENT_FLAG_ENABLED | MTE_EVENT_FLAG_ACTIVE | MTE_EVENT_FLAG_FIXED | MTE_EVENT_FLAG_VALID;
    return;
}

void parse_setEvent( const char* token, char* line )
{
    char ename[ MTE_STR1_LEN + 1 ];
    char buf[ IMPL_SPRINT_MAX_LEN ];
    oid name_buf[ asnMAX_OID_LEN ];
    size_t name_buf_len;
    long value;
    int wild = 1;
    struct mteEvent* entry;
    char* cp;

    DEBUG_MSGTL( ( "disman:event:conf", "Parsing setEvent config...  " ) );

    memset( ename, 0, sizeof( ename ) );
    cp = ReadConfig_copyNword( line, ename, MTE_STR1_LEN );
    if ( !cp || ename[ 0 ] == '\0' ) {
        ReadConfig_configPerror( "syntax error: no event name" );
        return;
    }

    if ( cp && *cp == '-' && *( cp + 1 ) == 'I' ) {
        wild = 0; /* an instance assignment */
        cp = ReadConfig_skipToken( cp );
    }

    /*
     *  Parse the SET assignment in the form "OID = value"
     */
    cp = ReadConfig_copyNword( cp, buf, IMPL_SPRINT_MAX_LEN );
    if ( buf[ 0 ] == '\0' ) {
        ReadConfig_configPerror( "syntax error: no set OID" );
        return;
    }
    name_buf_len = asnMAX_OID_LEN;
    if ( !Mib_parseOid( buf, name_buf, &name_buf_len ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "setEvent OID: %s\n", buf );
        ReadConfig_configPerror( "unknown set OID" );
        return;
    }
    if ( cp && *cp == '=' ) {
        cp = ReadConfig_skipToken( cp ); /* skip the '=' assignment character */
    }
    if ( !cp ) {
        ReadConfig_configPerror( "syntax error: missing set value" );
        return;
    }

    value = strtol( cp, NULL, 0 );

    /*
     *  If the entry has parsed successfully, then create,
     *     populate and activate the new event entry.
     */
    entry = _find_typed_mteEvent_entry( "priotd.conf", ename, MTE_EVENT_SET );
    if ( !entry ) {
        return;
    }
    memcpy( entry->mteSetOID, name_buf, name_buf_len * sizeof( oid ) );
    entry->mteSetOID_len = name_buf_len;
    entry->mteSetValue = value;
    if ( wild )
        entry->flags |= MTE_SET_FLAG_OBJWILD;
    entry->mteEventActions |= MTE_EVENT_SET;
    entry->flags |= MTE_EVENT_FLAG_ENABLED | MTE_EVENT_FLAG_ACTIVE | MTE_EVENT_FLAG_FIXED | MTE_EVENT_FLAG_VALID;
    return;
}

/* ==============================
 *
 *  Persistent (dynamic) configuration
 *
 * ============================== */

void parse_mteETable( const char* token, char* line )
{
    char owner[ MTE_STR1_LEN + 1 ];
    char ename[ MTE_STR1_LEN + 1 ];
    void* vp;
    size_t tmp;
    size_t len;
    struct mteEvent* entry;

    DEBUG_MSGTL( ( "disman:event:conf", "Parsing mteEventTable config...  " ) );

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof( owner ) );
    memset( ename, 0, sizeof( ename ) );
    len = MTE_STR1_LEN;
    vp = owner;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    len = MTE_STR1_LEN;
    vp = ename;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    entry = _find_mteEvent_entry( owner, ename );

    DEBUG_MSG( ( "disman:event:conf", "(%s, %s) ", owner, ename ) );

    /*
     * Read in the accessible (event-independent) column values.
     */
    len = MTE_STR2_LEN;
    vp = entry->mteEventComment;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    /*
     * Skip the mteEventAction field, and note that the
     *   boolean values are combined into a single field.
     */
    line = ReadConfig_readData( asnUNSIGNED, line, &tmp, NULL );
    entry->flags |= ( tmp & ( MTE_EVENT_FLAG_ENABLED | MTE_EVENT_FLAG_ACTIVE ) );
    /*
     * XXX - Will need to read in the 'iquery' access information
     */
    entry->flags |= MTE_EVENT_FLAG_VALID;

    DEBUG_MSG( ( "disman:event:conf", "\n" ) );
}

void parse_mteENotTable( const char* token, char* line )
{
    char owner[ MTE_STR1_LEN + 1 ];
    char ename[ MTE_STR1_LEN + 1 ];
    void* vp;
    size_t len;
    struct mteEvent* entry;

    DEBUG_MSGTL( ( "disman:event:conf", "Parsing mteENotifyTable config...  " ) );

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof( owner ) );
    memset( ename, 0, sizeof( ename ) );
    len = MTE_STR1_LEN;
    vp = owner;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    len = MTE_STR1_LEN;
    vp = ename;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    entry = _find_mteEvent_entry( owner, ename );

    DEBUG_MSG( ( "disman:event:conf", "(%s, %s) ", owner, ename ) );

    /*
     * Read in the accessible column values.
     */
    vp = entry->mteNotification;
    line = ReadConfig_readData( asnOBJECT_ID, line, &vp,
        &entry->mteNotification_len );
    len = MTE_STR1_LEN;
    vp = entry->mteNotifyOwner;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    len = MTE_STR1_LEN;
    vp = entry->mteNotifyObjects;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );

    entry->mteEventActions |= MTE_EVENT_NOTIFICATION;
    entry->flags |= MTE_EVENT_FLAG_VALID;

    DEBUG_MSG( ( "disman:event:conf", "\n" ) );
}

void parse_mteESetTable( const char* token, char* line )
{
    char owner[ MTE_STR1_LEN + 1 ];
    char ename[ MTE_STR1_LEN + 1 ];
    void* vp;
    size_t len;
    struct mteEvent* entry;

    DEBUG_MSGTL( ( "disman:event:conf", "Parsing mteESetTable config...  " ) );

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof( owner ) );
    memset( ename, 0, sizeof( ename ) );
    len = MTE_STR1_LEN;
    vp = owner;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    len = MTE_STR1_LEN;
    vp = ename;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    entry = _find_mteEvent_entry( owner, ename );

    DEBUG_MSG( ( "disman:event:conf", "(%s, %s) ", owner, ename ) );

    /*
     * Read in the accessible column values.
     */
    vp = entry->mteSetOID;
    line = ReadConfig_readData( asnOBJECT_ID, line, &vp,
        &entry->mteSetOID_len );
    line = ReadConfig_readData( asnUNSIGNED, line,
        &entry->mteSetValue, &len );
    len = MTE_STR2_LEN;
    vp = entry->mteSetTarget;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    len = MTE_STR2_LEN;
    vp = entry->mteSetContext;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );

    entry->mteEventActions |= MTE_EVENT_SET;
    entry->flags |= MTE_EVENT_FLAG_VALID;

    DEBUG_MSG( ( "disman:event:conf", "\n" ) );
}

int store_mteETable( int majorID, int minorID, void* serverarg, void* clientarg )
{
    char line[ UTILITIES_MAX_BUFFER ];
    char *cptr, *cp;
    void* vp;
    size_t tint;
    TdataRow* row;
    struct mteEvent* entry;

    DEBUG_MSGTL( ( "disman:event:conf", "Storing mteEventTable config:\n" ) );

    for ( row = TableTdata_rowFirst( event_table_data );
          row;
          row = TableTdata_rowNext( event_table_data, row ) ) {

        /*
         * Skip entries that were set up via static config directives
         */
        entry = ( struct mteEvent* )TableTdata_rowEntry( row );
        if ( entry->flags & MTE_EVENT_FLAG_FIXED )
            continue;

        DEBUG_MSGTL( ( "disman:event:conf", "  Storing (%s %s)\n",
            entry->mteOwner, entry->mteEName ) );

        /*
         * Save the basic mteEventTable entry...
         */
        memset( line, 0, sizeof( line ) );
        strcat( line, "_mteETable " );
        cptr = line + strlen( line );

        cp = entry->mteOwner;
        tint = strlen( cp );
        cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
        cp = entry->mteEName;
        tint = strlen( cp );
        cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
        cp = entry->mteEventComment;
        tint = strlen( cp );
        cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
        /* ... (but skip the mteEventAction field)... */
        tint = entry->flags & ( MTE_EVENT_FLAG_ENABLED | MTE_EVENT_FLAG_ACTIVE );
        cptr = ReadConfig_storeData( asnUNSIGNED, cptr, &tint, NULL );
        /* XXX - Need to store the 'iquery' access information */
        AgentReadConfig_priotdStoreConfig( line );

        /*
         * ... then save Notify and/or Set entries separately
         *   (The mteEventAction bits will be set when these are read in).
         */
        if ( entry->mteEventActions & MTE_EVENT_NOTIFICATION ) {
            memset( line, 0, sizeof( line ) );
            strcat( line, "_mteENotTable " );
            cptr = line + strlen( line );

            cp = entry->mteOwner;
            tint = strlen( cp );
            cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
            cp = entry->mteEName;
            tint = strlen( cp );
            cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
            vp = entry->mteNotification;
            cptr = ReadConfig_storeData( asnOBJECT_ID, cptr, &vp,
                &entry->mteNotification_len );
            cp = entry->mteNotifyOwner;
            tint = strlen( cp );
            cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
            cp = entry->mteNotifyObjects;
            tint = strlen( cp );
            cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
            AgentReadConfig_priotdStoreConfig( line );
        }

        if ( entry->mteEventActions & MTE_EVENT_SET ) {
            memset( line, 0, sizeof( line ) );
            strcat( line, "_mteESetTable " );
            cptr = line + strlen( line );

            cp = entry->mteOwner;
            tint = strlen( cp );
            cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
            cp = entry->mteEName;
            tint = strlen( cp );
            cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
            vp = entry->mteSetOID;
            cptr = ReadConfig_storeData( asnOBJECT_ID, cptr, &vp,
                &entry->mteSetOID_len );
            tint = entry->mteSetValue;
            cptr = ReadConfig_storeData( asnINTEGER, cptr, &tint, NULL );
            cp = entry->mteSetTarget;
            tint = strlen( cp );
            cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
            cp = entry->mteSetContext;
            tint = strlen( cp );
            cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
            tint = entry->flags & ( MTE_SET_FLAG_OBJWILD | MTE_SET_FLAG_CTXWILD );
            cptr = ReadConfig_storeData( asnUNSIGNED, cptr, &tint, NULL );
            AgentReadConfig_priotdStoreConfig( line );
        }
    }

    DEBUG_MSGTL( ( "disman:event:conf", "  done.\n" ) );
    return ErrorCode_SUCCESS;
}

int clear_mteETable( int majorID, int minorID, void* serverarg, void* clientarg )
{
    TdataRow* row;
    VariableList owner_var;

    /*
     * We're only interested in entries set up via the config files
     */
    memset( &owner_var, 0, sizeof( VariableList ) );
    Client_setVarTypedValue( &owner_var, asnOCTET_STR,
        "priotd.conf", strlen( "priotd.conf" ) );
    while ( ( row = TableTdata_rowNextByidx( event_table_data,
                  &owner_var ) ) ) {
        /*
         * XXX - check for owner of "priotd.conf"
         *       and break at the end of these
         */
        TableTdata_removeAndDeleteRow( event_table_data, row );
    }
    return ErrorCode_SUCCESS;
}
