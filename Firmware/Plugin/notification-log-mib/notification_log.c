

#include "notification_log.h"
#include "Alarm.h"
#include "Assert.h"
#include "Debug.h"
#include "DefaultStore.h"
#include "DsAgent.h"
#include "Instance.h"
#include "SysORTable.h"
#include "TableData.h"
#include "TableDataset.h"
#include "Tc.h"
#include "Trap.h"

/*
 * column number definitions for table nlmLogTable
 */

#define COLUMN_NLMLOGINDEX 1
#define COLUMN_NLMLOGTIME 2
#define COLUMN_NLMLOGDATEANDTIME 3
#define COLUMN_NLMLOGENGINEID 4
#define COLUMN_NLMLOGENGINETADDRESS 5
#define COLUMN_NLMLOGENGINETDOMAIN 6
#define COLUMN_NLMLOGCONTEXTENGINEID 7
#define COLUMN_NLMLOGCONTEXTNAME 8
#define COLUMN_NLMLOGNOTIFICATIONID 9

/*
 * column number definitions for table nlmLogVariableTable
 */
#define COLUMN_NLMLOGVARIABLEINDEX 1
#define COLUMN_NLMLOGVARIABLEID 2
#define COLUMN_NLMLOGVARIABLEVALUETYPE 3
#define COLUMN_NLMLOGVARIABLECOUNTER32VAL 4
#define COLUMN_NLMLOGVARIABLEUNSIGNED32VAL 5
#define COLUMN_NLMLOGVARIABLETIMETICKSVAL 6
#define COLUMN_NLMLOGVARIABLEINTEGER32VAL 7
#define COLUMN_NLMLOGVARIABLEOCTETSTRINGVAL 8
#define COLUMN_NLMLOGVARIABLEIPADDRESSVAL 9
#define COLUMN_NLMLOGVARIABLEOIDVAL 10
#define COLUMN_NLMLOGVARIABLECOUNTER64VAL 11
#define COLUMN_NLMLOGVARIABLEOPAQUEVAL 12

static u_long num_received = 0;
static u_long num_deleted = 0;

static u_long max_logged = 1000; /* goes against the mib default of infinite */
static u_long max_age = 1440; /* 1440 = 24 hours, which is the mib default */

static TableDataSet* nlmLogTable;
static TableDataSet* nlmLogVarTable;

static oid nlm_module_oid[] = { PRIOT_OID_MIB2, 92 }; /* NOTIFICATION-LOG-MIB::notificationLogMIB */

static void
netsnmp_notif_log_remove_oldest( int count )
{
    TableRow *deleterow, *tmprow, *deletevarrow;

    DEBUG_MSGTL( ( "notification_log", "deleting %d log entry(s)\n", count ) );

    deleterow = TableDataset_getFirstRow( nlmLogTable );
    for ( ; count && deleterow; deleterow = tmprow, --count ) {
        /*
         * delete contained varbinds
         * xxx-rks: note that this assumes that only the default
         * log is used (ie for the first nlmLogTable row, the
         * first nlmLogVarTable rows will be the right ones).
         * the right thing to do would be to do a find based on
         * the nlmLogTable oid.
         */
        DEBUG_MSGTL( ( "9:notification_log", "  deleting notification\n" ) );
        DEBUG_IF( "9:notification_log" )
        {
            DEBUG_MSGTL( ( "9:notification_log",
                " base oid:" ) );
            DEBUG_MSGOID( ( "9:notification_log", deleterow->index_oid,
                deleterow->index_oid_len ) );
            DEBUG_MSG( ( "9:notification_log", "\n" ) );
        }
        deletevarrow = TableDataset_getFirstRow( nlmLogVarTable );
        for ( ; deletevarrow; deletevarrow = tmprow ) {

            tmprow = TableDataset_getNextRow( nlmLogVarTable,
                deletevarrow );

            DEBUG_IF( "9:notification_log" )
            {
                DEBUG_MSGTL( ( "9:notification_log",
                    "         :" ) );
                DEBUG_MSGOID( ( "9:notification_log", deletevarrow->index_oid,
                    deletevarrow->index_oid_len ) );
                DEBUG_MSG( ( "9:notification_log", "\n" ) );
            }
            if ( ( deleterow->index_oid_len == deletevarrow->index_oid_len - 1 ) && Api_oidCompare( deleterow->index_oid, deleterow->index_oid_len, deletevarrow->index_oid, deleterow->index_oid_len ) == 0 ) {
                DEBUG_MSGTL( ( "9:notification_log", "    deleting varbind\n" ) );
                TableDataset_removeAndDeleteRow( nlmLogVarTable,
                    deletevarrow );
            } else
                break;
        }

        /*
         * delete the master row
         */
        tmprow = TableDataset_getNextRow( nlmLogTable, deleterow );
        TableDataset_removeAndDeleteRow( nlmLogTable,
            deleterow );
        num_deleted++;
    }
    /** should have deleted all of them */
    Assert_assert( 0 == count );
}

static void
check_log_size( unsigned int clientreg, void* clientarg )
{
    TableRow* row;
    TableDataSetStorage* data;
    u_long count = 0;
    u_long uptime;

    uptime = Agent_getAgentUptime();

    if ( !nlmLogTable || !nlmLogTable->table ) {
        DEBUG_MSGTL( ( "notification_log", "missing log table\n" ) );
        return;
    }

    /*
     * check max allowed count
     */
    count = TableDataset_numRows( nlmLogTable );
    DEBUG_MSGTL( ( "notification_log",
        "logged notifications %lu; max %lu\n",
        count, max_logged ) );
    if ( count > max_logged ) {
        count = count - max_logged;
        DEBUG_MSGTL( ( "notification_log", "removing %lu extra notifications\n",
            count ) );
        netsnmp_notif_log_remove_oldest( count );
    }

    /*
     * check max age
     */
    if ( 0 == max_age )
        return;
    count = 0;
    for ( row = TableDataset_getFirstRow( nlmLogTable );
          row;
          row = TableDataset_getNextRow( nlmLogTable, row ) ) {

        data = ( TableDataSetStorage* )row->data;
        data = TableDataset_findColumn( data, COLUMN_NLMLOGTIME );

        if ( uptime < ( ( u_long )( *( data->data.integer ) + max_age * 100 * 60 ) ) )
            break;
        ++count;
    }

    if ( count ) {
        DEBUG_MSGTL( ( "notification_log", "removing %lu expired notifications\n",
            count ) );
        netsnmp_notif_log_remove_oldest( count );
    }
}

/** Initialize the nlmLogVariableTable table by defining its contents and how it's structured */
static void
initialize_table_nlmLogVariableTable( const char* context )
{
    static oid nlmLogVariableTable_oid[] = { 1, 3, 6, 1, 2, 1, 92, 1, 3, 2 };
    size_t nlmLogVariableTable_oid_len = ASN01_OID_LENGTH( nlmLogVariableTable_oid );
    TableDataSet* table_set;
    HandlerRegistration* reginfo;

    /*
     * create the table structure itself
     */
    table_set = TableDataset_createTableDataSet( "nlmLogVariableTable" );
    nlmLogVarTable = table_set;
    nlmLogVarTable->table->store_indexes = 1;

    /***************************************************
     * Adding indexes
     */
    /*
     * declaring the nlmLogName index
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding index nlmLogName of type ASN01_OCTET_STR to table nlmLogVariableTable\n" ) );
    TableDataset_addIndex( table_set, ASN01_OCTET_STR );
    /*
     * declaring the nlmLogIndex index
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding index nlmLogIndex of type ASN01_UNSIGNED to table nlmLogVariableTable\n" ) );
    TableDataset_addIndex( table_set, ASN01_UNSIGNED );
    /*
     * declaring the nlmLogVariableIndex index
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding index nlmLogVariableIndex of type ASN01_UNSIGNED to table nlmLogVariableTable\n" ) );
    TableDataset_addIndex( table_set, ASN01_UNSIGNED );

    /*
     * adding column nlmLogVariableID of type ASN01_OBJECT_ID and access of
     * ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding column nlmLogVariableID (#2) of type ASN01_OBJECT_ID to table nlmLogVariableTable\n" ) );
    TableDataset_addDefaultRow( table_set, COLUMN_NLMLOGVARIABLEID,
        ASN01_OBJECT_ID, 0, NULL, 0 );
    /*
     * adding column nlmLogVariableValueType of type ASN01_INTEGER and
     * access of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding column nlmLogVariableValueType (#3) of type ASN01_INTEGER to table nlmLogVariableTable\n" ) );
    TableDataset_addDefaultRow( table_set,
        COLUMN_NLMLOGVARIABLEVALUETYPE,
        ASN01_INTEGER, 0, NULL, 0 );
    /*
     * adding column nlmLogVariableCounter32Val of type ASN01_COUNTER and
     * access of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding column nlmLogVariableCounter32Val (#4) of type ASN01_COUNTER to table nlmLogVariableTable\n" ) );
    TableDataset_addDefaultRow( table_set,
        COLUMN_NLMLOGVARIABLECOUNTER32VAL,
        ASN01_COUNTER, 0, NULL, 0 );
    /*
     * adding column nlmLogVariableUnsigned32Val of type ASN01_UNSIGNED and
     * access of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding column nlmLogVariableUnsigned32Val (#5) of type ASN01_UNSIGNED to table nlmLogVariableTable\n" ) );
    TableDataset_addDefaultRow( table_set,
        COLUMN_NLMLOGVARIABLEUNSIGNED32VAL,
        ASN01_UNSIGNED, 0, NULL, 0 );
    /*
     * adding column nlmLogVariableTimeTicksVal of type ASN01_TIMETICKS and
     * access of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding column nlmLogVariableTimeTicksVal (#6) of type ASN01_TIMETICKS to table nlmLogVariableTable\n" ) );
    TableDataset_addDefaultRow( table_set,
        COLUMN_NLMLOGVARIABLETIMETICKSVAL,
        ASN01_TIMETICKS, 0, NULL, 0 );
    /*
     * adding column nlmLogVariableInteger32Val of type ASN01_INTEGER and
     * access of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding column nlmLogVariableInteger32Val (#7) of type ASN01_INTEGER to table nlmLogVariableTable\n" ) );
    TableDataset_addDefaultRow( table_set,
        COLUMN_NLMLOGVARIABLEINTEGER32VAL,
        ASN01_INTEGER, 0, NULL, 0 );
    /*
     * adding column nlmLogVariableOctetStringVal of type ASN01_OCTET_STR
     * and access of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding column nlmLogVariableOctetStringVal (#8) of type ASN01_OCTET_STR to table nlmLogVariableTable\n" ) );
    TableDataset_addDefaultRow( table_set,
        COLUMN_NLMLOGVARIABLEOCTETSTRINGVAL,
        ASN01_OCTET_STR, 0, NULL, 0 );
    /*
     * adding column nlmLogVariableIpAddressVal of type ASN01_IPADDRESS and
     * access of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding column nlmLogVariableIpAddressVal (#9) of type ASN01_IPADDRESS to table nlmLogVariableTable\n" ) );
    TableDataset_addDefaultRow( table_set,
        COLUMN_NLMLOGVARIABLEIPADDRESSVAL,
        ASN01_IPADDRESS, 0, NULL, 0 );
    /*
     * adding column nlmLogVariableOidVal of type ASN01_OBJECT_ID and access
     * of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding column nlmLogVariableOidVal (#10) of type ASN01_OBJECT_ID to table nlmLogVariableTable\n" ) );
    TableDataset_addDefaultRow( table_set,
        COLUMN_NLMLOGVARIABLEOIDVAL,
        ASN01_OBJECT_ID, 0, NULL, 0 );
    /*
     * adding column nlmLogVariableCounter64Val of type ASN01_COUNTER64 and
     * access of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding column nlmLogVariableCounter64Val (#11) of type ASN01_COUNTER64 to table nlmLogVariableTable\n" ) );
    TableDataset_addDefaultRow( table_set,
        COLUMN_NLMLOGVARIABLECOUNTER64VAL,
        ASN01_COUNTER64, 0, NULL, 0 );
    /*
     * adding column nlmLogVariableOpaqueVal of type ASN01_OPAQUE and access
     * of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogVariableTable",
        "adding column nlmLogVariableOpaqueVal (#12) of type ASN01_OPAQUE to table nlmLogVariableTable\n" ) );
    TableDataset_addDefaultRow( table_set,
        COLUMN_NLMLOGVARIABLEOPAQUEVAL,
        ASN01_OPAQUE, 0, NULL, 0 );

    /*
     * registering the table with the master agent
     */
    /*
     * note: if you don't need a subhandler to deal with any aspects of
     * the request, change nlmLogVariableTable_handler to "NULL"
     */
    reginfo = AgentHandler_createHandlerRegistration( "nlmLogVariableTable",
        NULL,
        nlmLogVariableTable_oid,
        nlmLogVariableTable_oid_len,
        HANDLER_CAN_RWRITE );
    if ( NULL != context )
        reginfo->contextName = strdup( context );
    TableDataset_registerTableDataSet( reginfo, table_set, NULL );
}

/** Initialize the nlmLogTable table by defining its contents and how it's structured */
static void
initialize_table_nlmLogTable( const char* context )
{
    static oid nlmLogTable_oid[] = { 1, 3, 6, 1, 2, 1, 92, 1, 3, 1 };
    size_t nlmLogTable_oid_len = ASN01_OID_LENGTH( nlmLogTable_oid );
    HandlerRegistration* reginfo;

    /*
     * create the table structure itself
     */
    nlmLogTable = TableDataset_createTableDataSet( "nlmLogTable" );

    /***************************************************
     * Adding indexes
     */
    /*
     * declaring the nlmLogIndex index
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogTable",
        "adding index nlmLogName of type ASN01_OCTET_STR to table nlmLogTable\n" ) );
    TableDataset_addIndex( nlmLogTable, ASN01_OCTET_STR );

    DEBUG_MSGTL( ( "initialize_table_nlmLogTable",
        "adding index nlmLogIndex of type ASN01_UNSIGNED to table nlmLogTable\n" ) );
    TableDataset_addIndex( nlmLogTable, ASN01_UNSIGNED );

    /*
     * adding column nlmLogTime of type ASN01_TIMETICKS and access of
     * ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogTable",
        "adding column nlmLogTime (#2) of type ASN01_TIMETICKS to table nlmLogTable\n" ) );
    TableDataset_addDefaultRow( nlmLogTable, COLUMN_NLMLOGTIME,
        ASN01_TIMETICKS, 0, NULL, 0 );
    /*
     * adding column nlmLogDateAndTime of type ASN01_OCTET_STR and access of
     * ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogTable",
        "adding column nlmLogDateAndTime (#3) of type ASN01_OCTET_STR to table nlmLogTable\n" ) );
    TableDataset_addDefaultRow( nlmLogTable,
        COLUMN_NLMLOGDATEANDTIME,
        ASN01_OCTET_STR, 0, NULL, 0 );
    /*
     * adding column nlmLogEngineID of type ASN01_OCTET_STR and access of
     * ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogTable",
        "adding column nlmLogEngineID (#4) of type ASN01_OCTET_STR to table nlmLogTable\n" ) );
    TableDataset_addDefaultRow( nlmLogTable, COLUMN_NLMLOGENGINEID,
        ASN01_OCTET_STR, 0, NULL, 0 );
    /*
     * adding column nlmLogEngineTAddress of type ASN01_OCTET_STR and access
     * of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogTable",
        "adding column nlmLogEngineTAddress (#5) of type ASN01_OCTET_STR to table nlmLogTable\n" ) );
    TableDataset_addDefaultRow( nlmLogTable,
        COLUMN_NLMLOGENGINETADDRESS,
        ASN01_OCTET_STR, 0, NULL, 0 );
    /*
     * adding column nlmLogEngineTDomain of type ASN01_OBJECT_ID and access
     * of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogTable",
        "adding column nlmLogEngineTDomain (#6) of type ASN01_OBJECT_ID to table nlmLogTable\n" ) );
    TableDataset_addDefaultRow( nlmLogTable,
        COLUMN_NLMLOGENGINETDOMAIN,
        ASN01_OBJECT_ID, 0, NULL, 0 );
    /*
     * adding column nlmLogContextEngineID of type ASN01_OCTET_STR and
     * access of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogTable",
        "adding column nlmLogContextEngineID (#7) of type ASN01_OCTET_STR to table nlmLogTable\n" ) );
    TableDataset_addDefaultRow( nlmLogTable,
        COLUMN_NLMLOGCONTEXTENGINEID,
        ASN01_OCTET_STR, 0, NULL, 0 );
    /*
     * adding column nlmLogContextName of type ASN01_OCTET_STR and access of
     * ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogTable",
        "adding column nlmLogContextName (#8) of type ASN01_OCTET_STR to table nlmLogTable\n" ) );
    TableDataset_addDefaultRow( nlmLogTable,
        COLUMN_NLMLOGCONTEXTNAME,
        ASN01_OCTET_STR, 0, NULL, 0 );
    /*
     * adding column nlmLogNotificationID of type ASN01_OBJECT_ID and access
     * of ReadOnly
     */
    DEBUG_MSGTL( ( "initialize_table_nlmLogTable",
        "adding column nlmLogNotificationID (#9) of type ASN01_OBJECT_ID to table nlmLogTable\n" ) );
    TableDataset_addDefaultRow( nlmLogTable,
        COLUMN_NLMLOGNOTIFICATIONID,
        ASN01_OBJECT_ID, 0, NULL, 0 );

    /*
     * registering the table with the master agent
     */
    /*
     * note: if you don't need a subhandler to deal with any aspects of
     * the request, change nlmLogTable_handler to "NULL"
     */
    reginfo = AgentHandler_createHandlerRegistration( "nlmLogTable", NULL,
        nlmLogTable_oid,
        nlmLogTable_oid_len,
        HANDLER_CAN_RWRITE );
    if ( NULL != context )
        reginfo->contextName = strdup( context );
    TableDataset_registerTableDataSet( reginfo, nlmLogTable, NULL );

    /*
     * hmm...  5 minutes seems like a reasonable time to check for out
     * dated notification logs right?
     */
    Alarm_register( 300, ALARM_SA_REPEAT, check_log_size, NULL );
}

static int
notification_log_config_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    /*
     *this handler exists only to act as a trigger when the
     * configuration variables get set to a value and thus
     * notifications must be possibly deleted from our archives.
     */
    if ( reqinfo->mode == MODE_SET_COMMIT )
        check_log_size( 0, NULL );
    return PRIOT_ERR_NOERROR;
}

void init_notification_log( void )
{
    static oid my_nlmStatsGlobalNotificationsLogged_oid[] = { 1, 3, 6, 1, 2, 1, 92, 1, 2, 1, 0 };
    static oid my_nlmStatsGlobalNotificationsBumped_oid[] = { 1, 3, 6, 1, 2, 1, 92, 1, 2, 2, 0 };
    static oid my_nlmConfigGlobalEntryLimit_oid[] = { 1, 3, 6, 1, 2, 1, 92, 1, 1, 1, 0 };
    static oid my_nlmConfigGlobalAgeOut_oid[] = { 1, 3, 6, 1, 2, 1, 92, 1, 1, 2, 0 };
    char* context;
    char* apptype;

    context = DefaultStore_getString( DsStorage_APPLICATION_ID,
        DsAgentString_NOTIF_LOG_CTX );

    DEBUG_MSGTL( ( "notification_log", "registering with '%s' context\n",
        TOOLS_STRORNULL( context ) ) );

    /*
     * static variables
     */
    Instance_registerReadOnlyCounter32InstanceContext( "nlmStatsGlobalNotificationsLogged",
        my_nlmStatsGlobalNotificationsLogged_oid,
        ASN01_OID_LENGTH( my_nlmStatsGlobalNotificationsLogged_oid ),
        &num_received, NULL, context );

    Instance_registerReadOnlyCounter32InstanceContext( "nlmStatsGlobalNotificationsBumped",
        my_nlmStatsGlobalNotificationsBumped_oid,
        ASN01_OID_LENGTH( my_nlmStatsGlobalNotificationsBumped_oid ),
        &num_deleted, NULL, context );

    Instance_registerUlongInstanceContext( "nlmConfigGlobalEntryLimit",
        my_nlmConfigGlobalEntryLimit_oid,
        ASN01_OID_LENGTH( my_nlmConfigGlobalEntryLimit_oid ),
        &max_logged,
        notification_log_config_handler,
        context );

    Instance_registerUlongInstanceContext( "nlmConfigGlobalAgeOut",
        my_nlmConfigGlobalAgeOut_oid,
        ASN01_OID_LENGTH( my_nlmConfigGlobalAgeOut_oid ),
        &max_age,
        notification_log_config_handler,
        context );

    /*
     * tables
     */
    initialize_table_nlmLogVariableTable( context );
    initialize_table_nlmLogTable( context );

    /*
     * disable flag
     */
    apptype = DefaultStore_getString( DsStorage_LIBRARY_ID,
        DsStr_APPTYPE );
    DefaultStore_registerConfig( ASN01_BOOLEAN, apptype, "dontRetainLogs",
        DsStorage_APPLICATION_ID,
        DsAgentBoolean_DONT_RETAIN_NOTIFICATIONS );
    DefaultStore_registerConfig( ASN01_BOOLEAN, apptype, "doNotRetainNotificationLogs",
        DsStorage_APPLICATION_ID,
        DsAgentBoolean_DONT_RETAIN_NOTIFICATIONS );

    REGISTER_SYSOR_ENTRY( nlm_module_oid,
        "The MIB module for logging SNMP Notifications." );
}

void shutdown_notification_log( void )
{
    max_logged = 0;
    check_log_size( 0, NULL );
    TableDataset_deleteTableDataSet( nlmLogTable );
    nlmLogTable = NULL;

    UNREGISTER_SYSOR_ENTRY( nlm_module_oid );
}

void log_notification( Types_Pdu* pdu, Transport_Transport* transport )
{
    long tmpl;
    TableRow* row;

    static u_long default_num = 0;

    static oid snmptrapoid[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
    size_t snmptrapoid_len = ASN01_OID_LENGTH( snmptrapoid );
    Types_VariableList* vptr;
    u_char* logdate;
    size_t logdate_size;
    time_t timetnow;

    u_long vbcount = 0;
    u_long tmpul;
    int col;
    Types_Pdu* orig_pdu = pdu;

    if ( !nlmLogVarTable
        || DefaultStore_getBoolean( DsStorage_APPLICATION_ID,
               DsAgentBoolean_APP_DONT_LOG ) ) {
        return;
    }

    DEBUG_MSGTL( ( "notification_log", "logging something\n" ) );
    row = TableData_createTableDataRow();

    ++num_received;
    default_num++;

    /*
     * indexes to the table
     */
    TableData_rowAddIndex( row, ASN01_OCTET_STR, "default",
        strlen( "default" ) );
    TableData_rowAddIndex( row, ASN01_UNSIGNED, &default_num,
        sizeof( default_num ) );

    /*
     * add the data
     */
    tmpl = Agent_getAgentUptime();
    TableDataset_setRowColumn( row, COLUMN_NLMLOGTIME, ASN01_TIMETICKS,
        &tmpl, sizeof( tmpl ) );
    time( &timetnow );
    logdate = Tc_dateNTime( &timetnow, &logdate_size );
    TableDataset_setRowColumn( row, COLUMN_NLMLOGDATEANDTIME, ASN01_OCTET_STR,
        logdate, logdate_size );
    TableDataset_setRowColumn( row, COLUMN_NLMLOGENGINEID, ASN01_OCTET_STR,
        pdu->securityEngineID,
        pdu->securityEngineIDLen );
    if ( transport && transport->domain == transport_uDPDomain ) {
        /*
         * check for the udp domain
         */
        struct sockaddr_in* addr = ( struct sockaddr_in* )pdu->transportData;
        if ( addr ) {
            char buf[ sizeof( in_addr_t ) + sizeof( addr->sin_port ) ];
            in_addr_t locaddr = htonl( addr->sin_addr.s_addr );
            u_short portnum = htons( addr->sin_port );
            memcpy( buf, &locaddr, sizeof( in_addr_t ) );
            memcpy( buf + sizeof( in_addr_t ), &portnum,
                sizeof( addr->sin_port ) );
            TableDataset_setRowColumn( row, COLUMN_NLMLOGENGINETADDRESS,
                ASN01_OCTET_STR, buf,
                sizeof( in_addr_t ) + sizeof( addr->sin_port ) );
        }
    }
    if ( transport )
        TableDataset_setRowColumn( row, COLUMN_NLMLOGENGINETDOMAIN,
            ASN01_OBJECT_ID,
            transport->domain,
            sizeof( oid ) * transport->domain_length );
    TableDataset_setRowColumn( row, COLUMN_NLMLOGCONTEXTENGINEID,
        ASN01_OCTET_STR, pdu->contextEngineID,
        pdu->contextEngineIDLen );
    TableDataset_setRowColumn( row, COLUMN_NLMLOGCONTEXTNAME, ASN01_OCTET_STR,
        pdu->contextName, pdu->contextNameLen );

    if ( pdu->command == PRIOT_MSG_TRAP )
        pdu = Trap_convertV1pduToV2( orig_pdu );
    for ( vptr = pdu->variables; vptr; vptr = vptr->nextVariable ) {
        if ( Api_oidCompare( snmptrapoid, snmptrapoid_len,
                 vptr->name, vptr->nameLength )
            == 0 ) {
            TableDataset_setRowColumn( row, COLUMN_NLMLOGNOTIFICATIONID,
                ASN01_OBJECT_ID, vptr->val.string,
                vptr->valLen );
        } else {
            TableRow* myrow;
            myrow = TableData_createTableDataRow();

            /*
             * indexes to the table
             */
            TableData_rowAddIndex( myrow, ASN01_OCTET_STR, "default",
                strlen( "default" ) );
            TableData_rowAddIndex( myrow, ASN01_UNSIGNED, &default_num,
                sizeof( default_num ) );
            vbcount++;
            TableData_rowAddIndex( myrow, ASN01_UNSIGNED, &vbcount,
                sizeof( vbcount ) );

            /*
             * OID
             */
            TableDataset_setRowColumn( myrow, COLUMN_NLMLOGVARIABLEID,
                ASN01_OBJECT_ID, vptr->name,
                vptr->nameLength * sizeof( oid ) );

            /*
             * value
             */
            switch ( vptr->type ) {
            case ASN01_OBJECT_ID:
                tmpul = 7;
                col = COLUMN_NLMLOGVARIABLEOIDVAL;
                break;

            case ASN01_INTEGER:
                tmpul = 4;
                col = COLUMN_NLMLOGVARIABLEINTEGER32VAL;
                break;

            case ASN01_UNSIGNED:
                tmpul = 2;
                col = COLUMN_NLMLOGVARIABLEUNSIGNED32VAL;
                break;

            case ASN01_COUNTER:
                tmpul = 1;
                col = COLUMN_NLMLOGVARIABLECOUNTER32VAL;
                break;

            case ASN01_TIMETICKS:
                tmpul = 3;
                col = COLUMN_NLMLOGVARIABLETIMETICKSVAL;
                break;

            case ASN01_OCTET_STR:
                tmpul = 6;
                col = COLUMN_NLMLOGVARIABLEOCTETSTRINGVAL;
                break;

            case ASN01_IPADDRESS:
                tmpul = 5;
                col = COLUMN_NLMLOGVARIABLEIPADDRESSVAL;
                break;

            case ASN01_COUNTER64:
                tmpul = 8;
                col = COLUMN_NLMLOGVARIABLECOUNTER64VAL;
                break;

            case ASN01_OPAQUE:
                tmpul = 9;
                col = COLUMN_NLMLOGVARIABLEOPAQUEVAL;
                break;

            default:
                /*
                 * unsupported
                 */
                DEBUG_MSGTL( ( "notification_log",
                    "skipping type %d\n", vptr->type ) );
                ( void )TableDataset_deleteRow( myrow );
                continue;
            }
            TableDataset_setRowColumn( myrow, COLUMN_NLMLOGVARIABLEVALUETYPE,
                ASN01_INTEGER, &tmpul,
                sizeof( tmpul ) );
            TableDataset_setRowColumn( myrow, col, vptr->type,
                vptr->val.string, vptr->valLen );
            DEBUG_MSGTL( ( "notification_log",
                "adding a row to the variables table\n" ) );
            TableDataset_addRow( nlmLogVarTable, myrow );
        }
    }

    if ( pdu != orig_pdu )
        Api_freePdu( pdu );

    /*
     * store the row
     */
    TableDataset_addRow( nlmLogTable, row );

    check_log_size( 0, NULL );
    DEBUG_MSGTL( ( "notification_log", "done logging something\n" ) );
}
