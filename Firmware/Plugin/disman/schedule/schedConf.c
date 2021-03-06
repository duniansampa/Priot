/*
 * DisMan Schedule MIB:
 *     Implementation of the schedule MIB config handling
 */

#include "schedConf.h"
#include "AgentReadConfig.h"
#include "Client.h"
#include "Impl.h"
#include "ReadConfig.h"
#include "System/Util/Trace.h"
#include "TextualConvention.h"
#include "schedCore.h"

static int schedEntries;

/** Initializes the schedConf module */
void init_schedConf( void )
{
    DEBUG_MSGTL( ( "disman:schedule:init", "Initializing config module\n" ) );
    init_schedule_container();

    /*
     * Register public configuration directives
     */
    AgentReadConfig_priotdRegisterConfigHandler( "repeat", parse_sched_periodic, NULL,
        "repeat period  OID = value" );
    AgentReadConfig_priotdRegisterConfigHandler( "cron", parse_sched_timed, NULL,
        "cron * * * * * OID = value" );
    AgentReadConfig_priotdRegisterConfigHandler( "at", parse_sched_timed, NULL,
        "at   * * * * * OID = value" );

    /*
     * Register internal configuration directive,
     *   and arrange for dynamically configured entries to be saved
     */
    AgentReadConfig_priotdRegisterConfigHandler( "_schedTable", parse_schedTable, NULL, NULL );
    Callback_register( CallbackMajor_LIBRARY, CallbackMinor_STORE_DATA,
        store_schedTable, NULL );
    schedEntries = 0;
}

/* =======================================================
 *
 * Handlers for user-configured (static) scheduled actions
 *
 * ======================================================= */

void parse_sched_periodic( const char* token, char* line )
{
    TdataRow* row;
    struct schedTable_entry* entry;
    char buf[ 24 ], tmpbuf[ IMPL_SPRINT_MAX_LEN ];
    long frequency;
    long value;
    size_t tmpint;
    oid variable[ asnMAX_OID_LEN ], *var_ptr = variable;
    size_t var_len = asnMAX_OID_LEN;

    schedEntries++;
    sprintf( buf, "_conf%03d", schedEntries );

    DEBUG_MSGTL( ( "disman:schedule:conf", "periodic: %s %s\n", token, line ) );
    /*
     *  Parse the configure directive line
     */
    line = ReadConfig_copyNword( line, tmpbuf, sizeof( tmpbuf ) );
    frequency = Convert_stringTimeToSeconds( tmpbuf );
    if ( frequency == -1 ) {
        ReadConfig_configPerror( "Illegal frequency specified" );
        return;
    }

    line = ReadConfig_readData( asnOBJECT_ID, line, &var_ptr, &var_len );
    if ( var_len == 0 ) {
        ReadConfig_configPerror( "invalid specification for schedVariable" );
        return;
    }
    /*
     * Skip over optional assignment in "var = value"
     */
    while ( line && isspace( ( unsigned char )( *line ) ) )
        line++;
    if ( line && *line == '=' ) {
        line++;
        while ( line && isspace( ( unsigned char )( *line ) ) ) {
            line++;
        }
    }
    line = ReadConfig_readData( asnINTEGER, line, &value, &tmpint );

    /*
     * Create an entry in the schedTable
     */
    row = schedTable_createEntry( "priotd.conf", buf );
    if ( !row || !row->data ) {
        ReadConfig_configPerror( "create schedule entry failure" );
        return;
    }
    entry = ( struct schedTable_entry* )row->data;

    entry->schedInterval = frequency;
    entry->schedValue = value;
    entry->schedVariable_len = var_len;
    memcpy( entry->schedVariable, variable, var_len * sizeof( oid ) );

    entry->schedType = SCHED_TYPE_PERIODIC;
    entry->schedStorageType = tcSTORAGE_TYPE_READONLY; /* or PERMANENT */
    entry->flags = SCHEDULE_FLAG_ENABLED | SCHEDULE_FLAG_ACTIVE | SCHEDULE_FLAG_VALID;
    entry->session = Client_queryGetDefaultSession();
    sched_nextTime( entry );
}

/*
 * Timed-schedule utility:
 *     Convert from a cron-style specification to the equivalent set
 *     of bits. Note that minute, hour and weekday crontab fields are
 *     0-based, while day and month more naturally start from 1.
 */
void _sched_convert_bits( char* cron_spec, char* bit_buf,
    int bit_buf_len, int max_val, int startAt1 )
{
    char* cp = cron_spec;
    u_char b[] = { 0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01 };
    int val, major, minor;
    int overshoot;

    if ( !cron_spec || !bit_buf )
        return;

    /*
     * Wildcard field - set all bits
     */
    if ( *cp == '*' ) {
        memset( bit_buf, 0xff, bit_buf_len );

        /*
         * An "all-bits" specification may not be an exact multiple of 8.
         * Work out how far we've overshot things, and tidy up the excess.
         */
        overshoot = 8 * bit_buf_len - max_val;
        while ( overshoot > 0 ) {
            bit_buf[ bit_buf_len - 1 ] ^= b[ 8 - overshoot ];
            overshoot--;
        }
        return;
    }

    /*
     * Otherwise, clear the bit string buffer,
     * and start calculating which bits to set
     */
    memset( bit_buf, 0, bit_buf_len );

    while ( 1 ) {
        sscanf( cp, "%d", &val );
        /* Handle negative day specification */
        if ( val < 0 ) {
            val = max_val - val;
        }
        if ( startAt1 )
            val--;
        major = val / 8;
        minor = val % 8;
        bit_buf[ major ] |= b[ minor ];

        /* XXX - ideally we should handle "X-Y" syntax as well */
        while ( *cp && *cp != ',' )
            cp++;
        if ( !*cp )
            break;
        cp++;
    }
}

void parse_sched_timed( const char* token, char* line )
{
    TdataRow* row;
    struct schedTable_entry* entry;
    char buf[ 24 ], *cp;

    char minConf[ 512 ];
    size_t min_len = sizeof( minConf );
    char minVal[ 8 ];
    char hourConf[ 512 ];
    size_t hour_len = sizeof( hourConf );
    char hourVal[ 3 ];
    char dateConf[ 512 ];
    size_t date_len = sizeof( dateConf );
    char dateVal[ 8 ];
    char monConf[ 512 ];
    size_t mon_len = sizeof( monConf );
    char monVal[ 2 ];
    char dayConf[ 512 ];
    size_t day_len = sizeof( dayConf );
    char dayVal;

    long value;
    size_t tmpint;
    oid variable[ asnMAX_OID_LEN ], *var_ptr = variable;
    size_t var_len = asnMAX_OID_LEN;

    schedEntries++;
    sprintf( buf, "_conf%03d", schedEntries );

    DEBUG_MSGTL( ( "sched", "config: %s %s\n", token, line ) );
    /*
     *  Parse the configure directive line
     */
    cp = minConf;
    line = ReadConfig_readData( asnOCTET_STR, line, &cp, &min_len );
    cp = hourConf;
    line = ReadConfig_readData( asnOCTET_STR, line, &cp, &hour_len );
    cp = dateConf;
    line = ReadConfig_readData( asnOCTET_STR, line, &cp, &date_len );
    cp = monConf;
    line = ReadConfig_readData( asnOCTET_STR, line, &cp, &mon_len );
    cp = dayConf;
    line = ReadConfig_readData( asnOCTET_STR, line, &cp, &day_len );
    if ( !line ) {
        ReadConfig_configPerror( "invalid schedule time specification" );
        return;
    }

    line = ReadConfig_readData( asnOBJECT_ID, line, &var_ptr, &var_len );
    if ( var_len == 0 ) {
        ReadConfig_configPerror( "invalid specification for schedVariable" );
        return;
    }
    /*
     * Skip over optional assignment in "var = value"
     */
    while ( line && isspace( ( unsigned char )( *line ) ) )
        line++;
    if ( *line == '=' ) {
        line++;
        while ( line && isspace( ( unsigned char )( *line ) ) ) {
            line++;
        }
    }
    line = ReadConfig_readData( asnINTEGER, line, &value, &tmpint );

    /*
     * Convert from cron-style specifications into bits
     */
    _sched_convert_bits( minConf, minVal, 8, 60, 0 );
    _sched_convert_bits( hourConf, hourVal, 3, 24, 0 );
    memset( dateVal + 4, 0, 4 ); /* Clear the reverse day bits */
    _sched_convert_bits( dateConf, dateVal, 4, 31, 1 );
    _sched_convert_bits( monConf, monVal, 2, 12, 1 );
    _sched_convert_bits( dayConf, &dayVal, 1, 8, 0 );
    if ( dayVal & 0x01 ) { /* sunday(7) = sunday(0) */
        dayVal |= 0x80;
        dayVal &= 0xfe;
    }

    /*
     * Create an entry in the schedTable
     */
    row = schedTable_createEntry( "priotd.conf", buf );
    if ( !row || !row->data ) {
        ReadConfig_configPerror( "create schedule entry failure" );
        return;
    }
    entry = ( struct schedTable_entry* )row->data;

    entry->schedWeekDay = dayVal;
    memcpy( entry->schedMonth, monVal, 2 );
    memcpy( entry->schedDay, dateVal, 4 + 4 );
    memcpy( entry->schedHour, hourVal, 3 );
    memcpy( entry->schedMinute, minVal, 8 );

    memcpy( entry->schedVariable, variable, var_len * sizeof( oid ) );
    entry->schedVariable_len = var_len;
    entry->schedValue = value;

    if ( !strcmp( token, "at" ) )
        entry->schedType = SCHED_TYPE_ONESHOT;
    else
        entry->schedType = SCHED_TYPE_CALENDAR;
    entry->schedStorageType = tcSTORAGE_TYPE_READONLY; /* or PERMANENT */
    entry->flags = SCHEDULE_FLAG_ENABLED | SCHEDULE_FLAG_ACTIVE | SCHEDULE_FLAG_VALID;
    entry->session = Client_queryGetDefaultSession();
    sched_nextTime( entry );
}

/* ========================================
 *
 * Handlers for persistent schedule entries
 *
 * ======================================== */

void parse_schedTable( const char* token, char* line )
{
    char owner[ SCHED_STR1_LEN + 1 ];
    char name[ SCHED_STR1_LEN + 1 ];
    char time_bits[ 22 ]; /* schedWeekDay..schedMinute */
    void* vp;
    size_t len;
    TdataRow* row;
    struct schedTable_entry* entry;

    DEBUG_MSGTL( ( "disman:schedule:conf", "Parsing schedTable config...  " ) );

    /*
     * Read in the index information for this entry
     *  and create a (non-fixed) data structure for it.
     */
    memset( owner, 0, sizeof( owner ) );
    memset( name, 0, sizeof( name ) );
    len = SCHED_STR1_LEN;
    vp = owner;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    len = SCHED_STR1_LEN;
    vp = name;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    row = schedTable_createEntry( owner, name );
    if ( !row || !row->data ) {
        ReadConfig_configPerror( "create schedule entry failure" );
        return;
    }
    entry = ( struct schedTable_entry* )row->data;
    DEBUG_MSG( ( "disman:schedule:conf", "(%s, %s) ", owner, name ) );

    /*
     * Read in the column values.
     */
    len = SCHED_STR2_LEN;
    vp = entry->schedDescr;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    line = ReadConfig_readData( asnUNSIGNED, line,
        &entry->schedInterval, NULL );
    /* Unpick the various timed bits */
    len = 22;
    vp = time_bits;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    entry->schedWeekDay = time_bits[ 0 ];
    entry->schedMonth[ 0 ] = time_bits[ 1 ];
    entry->schedMonth[ 1 ] = time_bits[ 2 ];
    entry->schedHour[ 0 ] = time_bits[ 11 ];
    entry->schedHour[ 1 ] = time_bits[ 12 ];
    entry->schedHour[ 2 ] = time_bits[ 13 ];
    memcpy( entry->schedDay, time_bits + 3, 8 );
    memcpy( entry->schedMinute, time_bits + 14, 8 );

    len = SCHED_STR1_LEN;
    vp = entry->schedContextName;
    line = ReadConfig_readData( asnOCTET_STR, line, &vp, &len );
    len = asnMAX_OID_LEN;
    vp = entry->schedVariable;
    line = ReadConfig_readData( asnOBJECT_ID, line, &vp, &len );
    entry->schedVariable_len = len;
    line = ReadConfig_readData( asnINTEGER, line,
        &entry->schedValue, NULL );
    line = ReadConfig_readData( asnUNSIGNED, line,
        &entry->schedType, NULL );
    line = ReadConfig_readData( asnUNSIGNED, line, &len, NULL );
    entry->flags |= ( len /* & WHAT ?? */ );
    /* XXX - Will need to read in the 'iquery' access information */
    entry->flags |= SCHEDULE_FLAG_VALID;

    DEBUG_MSG( ( "disman:schedule:conf", "\n" ) );
}

/*
 * Save dynamically-configured schedTable entries into persistent storage
 */
int store_schedTable( int majorID, int minorID, void* serverarg, void* clientarg )
{
    char line[ UTILITIES_MAX_BUFFER ];
    char time_bits[ 22 ]; /* schedWeekDay..schedMinute */
    char *cptr, *cp;
    void* vp;
    size_t tint;
    TdataRow* row;
    struct schedTable_entry* entry;

    DEBUG_MSGTL( ( "disman:schedule:conf", "Storing schedTable:\n" ) );

    for ( row = TableTdata_rowFirst( schedule_table );
          row;
          row = TableTdata_rowNext( schedule_table, row ) ) {

        if ( !row->data )
            continue;
        entry = ( struct schedTable_entry* )row->data;

        /*
         * Only save (dynamically-created) 'nonVolatile' entries
         *    (XXX - what about dynamic 'permanent' entries ??)
         */
        if ( entry->schedStorageType != tcSTORAGE_TYPE_NONVOLATILE )
            continue;
        DEBUG_MSGTL( ( "disman:schedule:conf", "  Storing (%s, %s)\n",
            entry->schedOwner, entry->schedName ) );

        memset( line, 0, sizeof( line ) );
        strcpy( line, "_schedTable " );
        cptr = line + strlen( line );

        cp = entry->schedOwner;
        tint = strlen( cp );
        cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
        cp = entry->schedName;
        tint = strlen( cp );
        cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
        cp = entry->schedDescr;
        tint = strlen( cp );
        cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
        tint = entry->schedInterval;
        cptr = ReadConfig_storeData( asnUNSIGNED, cptr, &tint, NULL );

        /* Combine all the timed bits into a single field */
        time_bits[ 0 ] = entry->schedWeekDay;
        time_bits[ 1 ] = entry->schedMonth[ 0 ];
        time_bits[ 2 ] = entry->schedMonth[ 1 ];
        time_bits[ 11 ] = entry->schedHour[ 0 ];
        time_bits[ 12 ] = entry->schedHour[ 1 ];
        time_bits[ 13 ] = entry->schedHour[ 2 ];
        memcpy( time_bits + 3, entry->schedDay, 8 );
        memcpy( time_bits + 14, entry->schedMinute, 8 );
        vp = time_bits;
        tint = 22;
        cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &vp, &tint );

        cp = entry->schedContextName;
        tint = strlen( cp );
        cptr = ReadConfig_storeData( asnOCTET_STR, cptr, &cp, &tint );
        vp = entry->schedVariable;
        tint = entry->schedVariable_len;
        cptr = ReadConfig_storeData( asnOBJECT_ID, cptr, &vp, &tint );
        tint = entry->schedValue;
        cptr = ReadConfig_storeData( asnINTEGER, cptr, &tint, NULL );
        tint = entry->schedType;
        cptr = ReadConfig_storeData( asnUNSIGNED, cptr, &tint, NULL );
        tint = entry->flags /* & WHAT ?? */;
        cptr = ReadConfig_storeData( asnUNSIGNED, cptr, &tint, NULL );
        /* XXX - Need to store the 'iquery' access information */
        AgentReadConfig_priotdStoreConfig( line );
    }
    DEBUG_MSGTL( ( "disman:schedule:conf", "  done.\n" ) );
    return ErrorCode_SUCCESS;
}
