/*
 *  ICMP MIB group implementation - icmp.c
 */

#include "icmp.h"
#include "Api.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "System/Containers/MapList.h"
#include "System/Util/Logger.h"
#include "PluginSettings.h"
#include "Priot.h"
#include "ScalarGroup.h"
#include "SysORTable.h"
#include "TableIterator.h"
#include "kernel_linux.h"
#include "kernel_mib.h"
#include "utilities/MIB_STATS_CACHE_TIMEOUT.h"
#include <netinet/ip_icmp.h>

#define ICMP_STATS_CACHE_TIMEOUT MIB_STATS_CACHE_TIMEOUT

/* redefine ICMP6 message types from glibc < 2.4 to newer names */
#define MLD_LISTENER_QUERY ICMP6_MEMBERSHIP_QUERY
#define MLD_LISTENER_REPORT ICMP6_MEMBERSHIP_REPORT
#define MLD_LISTENER_REDUCTION ICMP6_MEMBERSHIP_REDUCTION

/*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

/*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

struct icmp_mib icmpstat;

/*
 * Define the OID pointer to the top of the mib tree that we're
 * registering underneath 
 */
static const oid icmp_oid[] = { PRIOT_OID_MIB2, 5 };
static const oid icmp_stats_tbl_oid[] = { PRIOT_OID_MIB2, 5, 29 };
static const oid icmp_msg_stats_tbl_oid[] = { PRIOT_OID_MIB2, 5, 30 };
extern oid ip_module_oid[];
extern int ip_module_oid_len;
extern int ip_module_count;

struct icmp_stats_table_entry {
    uint32_t ipVer;
    uint32_t icmpStatsInMsgs;
    uint32_t icmpStatsInErrors;
    uint32_t icmpStatsOutMsgs;
    uint32_t icmpStatsOutErrors;
};

struct icmp_stats_table_entry icmp_stats_table[ 2 ];

#define ICMP_MSG_STATS_HAS_IN 1
#define ICMP_MSG_STATS_HAS_OUT 2

struct icmp_msg_stats_table_entry {
    uint32_t ipVer;
    uint32_t icmpMsgStatsType;
    uint32_t icmpMsgStatsInPkts;
    uint32_t icmpMsgStatsOutPkts;
    int flags;
};

/* Linux keeps track of all possible message types */
#define ICMP_MSG_STATS_IPV4_COUNT 256

#define ICMP_MSG_STATS_IPV6_COUNT 0

struct icmp_msg_stats_table_entry icmp_msg_stats_table[ ICMP_MSG_STATS_IPV4_COUNT + ICMP_MSG_STATS_IPV6_COUNT ];

int icmp_stats_load( Cache* cache, void* vmagic )
{

    /*
     * note don't bother using the passed in cache
     * and vmagic pointers.  They are useless as they 
     * currently point to the icmp system stats cache	
     * since I see little point in registering another
     * cache for this table.  Its not really needed
     */

    int i;
    struct icmp_mib v4icmp;

    for ( i = 0; i < 2; i++ ) {
        switch ( i ) {
        case 0:
            linux_read_icmp_stat( &v4icmp );

            icmp_stats_table[ i ].icmpStatsInMsgs = v4icmp.icmpInMsgs;
            icmp_stats_table[ i ].icmpStatsInErrors = v4icmp.icmpInErrors;
            icmp_stats_table[ i ].icmpStatsOutMsgs = v4icmp.icmpOutMsgs;
            icmp_stats_table[ i ].icmpStatsOutErrors = v4icmp.icmpOutErrors;
            break;
        case 1:
            break;
        }
        icmp_stats_table[ i ].ipVer = i + 1;
    }

    return 0;
}

int icmp_msg_stats_load( Cache* cache, void* vmagic )
{
    struct icmp_mib v4icmp;
    struct icmp4_msg_mib v4icmpmsg;

    int i, k, flag, inc;

    memset( &icmp_msg_stats_table, 0, sizeof( icmp_msg_stats_table ) );

    i = 0;
    flag = 0;
    k = 0;
    inc = 0;
    linux_read_icmp_msg_stat( &v4icmp, &v4icmpmsg, &flag );

    if ( flag ) {
        while ( 255 >= k ) {
            if ( v4icmpmsg.vals[ k ].InType ) {
                icmp_msg_stats_table[ i ].ipVer = 1;
                icmp_msg_stats_table[ i ].icmpMsgStatsType = k;
                icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmpmsg.vals[ k ].InType;
                icmp_msg_stats_table[ i ].flags = icmp_msg_stats_table[ i ].flags | ICMP_MSG_STATS_HAS_IN;
                inc = 1; /* Set this if we found a valid entry */
            }
            if ( v4icmpmsg.vals[ k ].OutType ) {
                icmp_msg_stats_table[ i ].ipVer = 1;
                icmp_msg_stats_table[ i ].icmpMsgStatsType = k;
                icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmpmsg.vals[ k ].OutType;
                icmp_msg_stats_table[ i ].flags = icmp_msg_stats_table[ i ].flags | ICMP_MSG_STATS_HAS_OUT;
                inc = 1; /* Set this if we found a valid entry */
            }
            if ( inc ) {
                i++;
                inc = 0;
            }
            k++;
        }
    } else {
        icmp_msg_stats_table[ i ].icmpMsgStatsType = ICMP_ECHOREPLY;
        icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmp.icmpInEchoReps;
        icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmp.icmpOutEchoReps;
        icmp_msg_stats_table[ i ].flags = ICMP_MSG_STATS_HAS_IN | ICMP_MSG_STATS_HAS_OUT;
        icmp_msg_stats_table[ i ].ipVer = 1;
        i++;

        icmp_msg_stats_table[ i ].icmpMsgStatsType = ICMP_DEST_UNREACH;
        icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmp.icmpInDestUnreachs;
        icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmp.icmpOutDestUnreachs;
        icmp_msg_stats_table[ i ].flags = ICMP_MSG_STATS_HAS_IN | ICMP_MSG_STATS_HAS_OUT;
        icmp_msg_stats_table[ i ].ipVer = 1;
        i++;

        icmp_msg_stats_table[ i ].icmpMsgStatsType = ICMP_SOURCE_QUENCH;
        icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmp.icmpInSrcQuenchs;
        icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmp.icmpOutSrcQuenchs;
        icmp_msg_stats_table[ i ].flags = ICMP_MSG_STATS_HAS_IN | ICMP_MSG_STATS_HAS_OUT;
        icmp_msg_stats_table[ i ].ipVer = 1;
        i++;

        icmp_msg_stats_table[ i ].icmpMsgStatsType = ICMP_REDIRECT;
        icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmp.icmpInRedirects;
        icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmp.icmpOutRedirects;
        icmp_msg_stats_table[ i ].flags = ICMP_MSG_STATS_HAS_IN | ICMP_MSG_STATS_HAS_OUT;
        icmp_msg_stats_table[ i ].ipVer = 1;
        i++;

        icmp_msg_stats_table[ i ].icmpMsgStatsType = ICMP_ECHO;
        icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmp.icmpInEchos;
        icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmp.icmpOutEchos;
        icmp_msg_stats_table[ i ].flags = ICMP_MSG_STATS_HAS_IN | ICMP_MSG_STATS_HAS_OUT;
        icmp_msg_stats_table[ i ].ipVer = 1;
        i++;

        icmp_msg_stats_table[ i ].icmpMsgStatsType = ICMP_TIME_EXCEEDED;
        icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmp.icmpInTimeExcds;
        icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmp.icmpOutTimeExcds;
        icmp_msg_stats_table[ i ].flags = ICMP_MSG_STATS_HAS_IN | ICMP_MSG_STATS_HAS_OUT;
        icmp_msg_stats_table[ i ].ipVer = 1;
        i++;

        icmp_msg_stats_table[ i ].icmpMsgStatsType = ICMP_PARAMETERPROB;
        icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmp.icmpInParmProbs;
        icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmp.icmpOutParmProbs;
        icmp_msg_stats_table[ i ].flags = ICMP_MSG_STATS_HAS_IN | ICMP_MSG_STATS_HAS_OUT;
        icmp_msg_stats_table[ i ].ipVer = 1;
        i++;

        icmp_msg_stats_table[ i ].icmpMsgStatsType = ICMP_TIMESTAMP;
        icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmp.icmpInTimestamps;
        icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmp.icmpOutTimestamps;
        icmp_msg_stats_table[ i ].flags = ICMP_MSG_STATS_HAS_IN | ICMP_MSG_STATS_HAS_OUT;
        icmp_msg_stats_table[ i ].ipVer = 1;
        i++;

        icmp_msg_stats_table[ i ].icmpMsgStatsType = ICMP_TIMESTAMPREPLY;
        icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmp.icmpInTimestampReps;
        icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmp.icmpOutTimestampReps;
        icmp_msg_stats_table[ i ].flags = ICMP_MSG_STATS_HAS_IN | ICMP_MSG_STATS_HAS_OUT;
        icmp_msg_stats_table[ i ].ipVer = 1;
        i++;

        icmp_msg_stats_table[ i ].icmpMsgStatsType = ICMP_ADDRESS;
        icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmp.icmpInAddrMasks;
        icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmp.icmpOutAddrMasks;
        icmp_msg_stats_table[ i ].flags = ICMP_MSG_STATS_HAS_IN | ICMP_MSG_STATS_HAS_OUT;
        icmp_msg_stats_table[ i ].ipVer = 1;
        i++;

        icmp_msg_stats_table[ i ].icmpMsgStatsType = ICMP_ADDRESSREPLY;
        icmp_msg_stats_table[ i ].icmpMsgStatsInPkts = v4icmp.icmpInAddrMaskReps;
        icmp_msg_stats_table[ i ].icmpMsgStatsOutPkts = v4icmp.icmpOutAddrMaskReps;
        icmp_msg_stats_table[ i ].flags = ICMP_MSG_STATS_HAS_IN | ICMP_MSG_STATS_HAS_OUT;
        icmp_msg_stats_table[ i ].ipVer = 1;
        i++;
    }

    return 0;
}

VariableList*
icmp_stats_next_entry( void** loop_context,
    void** data_context,
    VariableList* index,
    IteratorInfo* data )
{
    int i = ( int )( intptr_t )( *loop_context );
    VariableList* idx = index;

    if ( i > 1 )
        return NULL;

    /*
     *set IP version
     */
    Client_setVarTypedValue( idx, ASN01_INTEGER, ( u_char* )&icmp_stats_table[ i ].ipVer,
        sizeof( uint32_t ) );
    idx = idx->next;

    *data_context = &icmp_stats_table[ i ];

    *loop_context = ( void* )( intptr_t )( ++i );

    return index;
}

VariableList*
icmp_stats_first_entry( void** loop_context,
    void** data_context,
    VariableList* index,
    IteratorInfo* data )
{
    *loop_context = NULL;
    *data_context = NULL;
    return icmp_stats_next_entry( loop_context, data_context, index, data );
}

VariableList*
icmp_msg_stats_next_entry( void** loop_context,
    void** data_context,
    VariableList* index,
    IteratorInfo* data )
{
    int i = ( int )( intptr_t )( *loop_context );
    VariableList* idx = index;

    if ( i >= ICMP_MSG_STATS_IPV4_COUNT + ICMP_MSG_STATS_IPV6_COUNT )
        return NULL;

    /* set IP version */
    Client_setVarTypedValue( idx, ASN01_INTEGER,
        ( u_char* )&icmp_msg_stats_table[ i ].ipVer,
        sizeof( uint32_t ) );

    /* set packet type */
    idx = idx->next;
    Client_setVarTypedValue( idx, ASN01_INTEGER,
        ( u_char* )&icmp_msg_stats_table[ i ].icmpMsgStatsType,
        sizeof( uint32_t ) );

    *data_context = &icmp_msg_stats_table[ i ];
    *loop_context = ( void* )( intptr_t )( ++i );

    return index;
}

VariableList*
icmp_msg_stats_first_entry( void** loop_context,
    void** data_context,
    VariableList* index,
    IteratorInfo* data )
{
    *loop_context = NULL;
    *data_context = NULL;
    return icmp_msg_stats_next_entry( loop_context, data_context, index, data );
}

void init_icmp( void )
{
    HandlerRegistration* msg_stats_reginfo = NULL;
    HandlerRegistration* table_reginfo = NULL;
    IteratorInfo* iinfo;
    IteratorInfo* msg_stats_iinfo;
    TableRegistrationInfo* table_info;
    TableRegistrationInfo* msg_stats_table_info;
    HandlerRegistration* scalar_reginfo = NULL;
    int rc;

    /*
     * register ourselves with the agent as a group of scalars...
     */
    DEBUG_MSGTL( ( "mibII/icmp", "Initialising ICMP group\n" ) );
    scalar_reginfo = AgentHandler_createHandlerRegistration( "icmp", icmp_handler,
        icmp_oid, ASN01_OID_LENGTH( icmp_oid ), HANDLER_CAN_RONLY );
    rc = ScalarGroup_registerScalarGroup( scalar_reginfo, ICMPINMSGS, ICMPOUTADDRMASKREPS );
    if ( rc != ErrorCode_SUCCESS )
        return;
    /*
     * .... with a local cache
     *    (except for HP-UX 11, which extracts objects individually)
     */
    rc = AgentHandler_injectHandler( scalar_reginfo,
        CacheHandler_getCacheHandler( ICMP_STATS_CACHE_TIMEOUT,
                                         icmp_load, icmp_free,
                                         icmp_oid, ASN01_OID_LENGTH( icmp_oid ) ) );
    if ( rc != ErrorCode_SUCCESS )
        goto bail;

    /* register icmpStatsTable */
    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    if ( !table_info )
        goto bail;
    Table_helperAddIndexes( table_info, ASN01_INTEGER, 0 );
    table_info->min_column = ICMP_STAT_INMSG;
    table_info->max_column = ICMP_STAT_OUTERR;

    iinfo = MEMORY_MALLOC_TYPEDEF( IteratorInfo );
    if ( !iinfo )
        goto bail;
    iinfo->get_first_data_point = icmp_stats_first_entry;
    iinfo->get_next_data_point = icmp_stats_next_entry;
    iinfo->table_reginfo = table_info;

    table_reginfo = AgentHandler_createHandlerRegistration( "icmpStatsTable",
        icmp_stats_table_handler, icmp_stats_tbl_oid,
        ASN01_OID_LENGTH( icmp_stats_tbl_oid ), HANDLER_CAN_RONLY );

    rc = TableIterator_registerTableIterator2( table_reginfo, iinfo );
    if ( rc != ErrorCode_SUCCESS ) {
        table_reginfo = NULL;
        goto bail;
    }
    AgentHandler_injectHandler( table_reginfo,
        CacheHandler_getCacheHandler( ICMP_STATS_CACHE_TIMEOUT,
                                    icmp_load, icmp_free,
                                    icmp_stats_tbl_oid, ASN01_OID_LENGTH( icmp_stats_tbl_oid ) ) );

    /* register icmpMsgStatsTable */
    msg_stats_table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    if ( !msg_stats_table_info )
        goto bail;
    Table_helperAddIndexes( msg_stats_table_info, ASN01_INTEGER, ASN01_INTEGER, 0 );
    msg_stats_table_info->min_column = ICMP_MSG_STAT_IN_PKTS;
    msg_stats_table_info->max_column = ICMP_MSG_STAT_OUT_PKTS;

    msg_stats_iinfo = MEMORY_MALLOC_TYPEDEF( IteratorInfo );
    if ( !msg_stats_iinfo )
        goto bail;
    msg_stats_iinfo->get_first_data_point = icmp_msg_stats_first_entry;
    msg_stats_iinfo->get_next_data_point = icmp_msg_stats_next_entry;
    msg_stats_iinfo->table_reginfo = msg_stats_table_info;

    msg_stats_reginfo = AgentHandler_createHandlerRegistration( "icmpMsgStatsTable",
        icmp_msg_stats_table_handler, icmp_msg_stats_tbl_oid,
        ASN01_OID_LENGTH( icmp_msg_stats_tbl_oid ), HANDLER_CAN_RONLY );

    rc = TableIterator_registerTableIterator2( msg_stats_reginfo, msg_stats_iinfo );
    if ( rc != ErrorCode_SUCCESS ) {
        msg_stats_reginfo = NULL;
        goto bail;
    }

    AgentHandler_injectHandler( msg_stats_reginfo,
        CacheHandler_getCacheHandler( ICMP_STATS_CACHE_TIMEOUT,
                                    icmp_load, icmp_free,
                                    icmp_msg_stats_tbl_oid, ASN01_OID_LENGTH( icmp_msg_stats_tbl_oid ) ) );

    if ( ++ip_module_count == 2 )
        REGISTER_SYSOR_TABLE( ip_module_oid, ip_module_oid_len,
            "The MIB module for managing IP and ICMP implementations" );

    return;

bail:

    if ( msg_stats_reginfo )
        AgentHandler_handlerRegistrationFree( msg_stats_reginfo );
    if ( table_reginfo )
        AgentHandler_handlerRegistrationFree( table_reginfo );
    if ( scalar_reginfo )
        AgentHandler_handlerRegistrationFree( scalar_reginfo );
}

/*********************
	 *
	 *  System specific data formats
	 *
	 *********************/

/*********************
	 *
	 *  System independent handler
	 *       (mostly!)
	 *
	 *********************/

int icmp_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    VariableList* requestvb;
    long ret_value;
    oid subid;

    /*
     * The cached data should already have been loaded by the
     *    cache handler, higher up the handler chain.
     */

    /*
     * 
     *
     */
    DEBUG_MSGTL( ( "mibII/icmp", "Handler - mode %s\n",
        MapList_findLabel( "agentMode", reqinfo->mode ) ) );
    switch ( reqinfo->mode ) {
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            requestvb = request->requestvb;
            subid = requestvb->name[ ASN01_OID_LENGTH( icmp_oid ) ]; /* XXX */
            DEBUG_MSGTL( ( "mibII/icmp", "oid: " ) );
            DEBUG_MSGOID( ( "mibII/icmp", requestvb->name,
                requestvb->nameLength ) );
            DEBUG_MSG( ( "mibII/icmp", "\n" ) );

            switch ( subid ) {
            case ICMPINMSGS:
                ret_value = icmpstat.icmpInMsgs;
                break;
            case ICMPINERRORS:
                ret_value = icmpstat.icmpInErrors;
                break;
            case ICMPINDESTUNREACHS:
                ret_value = icmpstat.icmpInDestUnreachs;
                break;
            case ICMPINTIMEEXCDS:
                ret_value = icmpstat.icmpInTimeExcds;
                break;
            case ICMPINPARMPROBS:
                ret_value = icmpstat.icmpInParmProbs;
                break;
            case ICMPINSRCQUENCHS:
                ret_value = icmpstat.icmpInSrcQuenchs;
                break;
            case ICMPINREDIRECTS:
                ret_value = icmpstat.icmpInRedirects;
                break;
            case ICMPINECHOS:
                ret_value = icmpstat.icmpInEchos;
                break;
            case ICMPINECHOREPS:
                ret_value = icmpstat.icmpInEchoReps;
                break;
            case ICMPINTIMESTAMPS:
                ret_value = icmpstat.icmpInTimestamps;
                break;
            case ICMPINTIMESTAMPREPS:
                ret_value = icmpstat.icmpInTimestampReps;
                break;
            case ICMPINADDRMASKS:
                ret_value = icmpstat.icmpInAddrMasks;
                break;
            case ICMPINADDRMASKREPS:
                ret_value = icmpstat.icmpInAddrMaskReps;
                break;
            case ICMPOUTMSGS:
                ret_value = icmpstat.icmpOutMsgs;
                break;
            case ICMPOUTERRORS:
                ret_value = icmpstat.icmpOutErrors;
                break;
            case ICMPOUTDESTUNREACHS:
                ret_value = icmpstat.icmpOutDestUnreachs;
                break;
            case ICMPOUTTIMEEXCDS:
                ret_value = icmpstat.icmpOutTimeExcds;
                break;
            case ICMPOUTPARMPROBS:
                ret_value = icmpstat.icmpOutParmProbs;
                break;
            case ICMPOUTSRCQUENCHS:
                ret_value = icmpstat.icmpOutSrcQuenchs;
                break;
            case ICMPOUTREDIRECTS:
                ret_value = icmpstat.icmpOutRedirects;
                break;
            case ICMPOUTECHOS:
                ret_value = icmpstat.icmpOutEchos;
                break;
            case ICMPOUTECHOREPS:
                ret_value = icmpstat.icmpOutEchoReps;
                break;
            case ICMPOUTTIMESTAMPS:
                ret_value = icmpstat.icmpOutTimestamps;
                break;
            case ICMPOUTTIMESTAMPREPS:
                ret_value = icmpstat.icmpOutTimestampReps;
                break;
            case ICMPOUTADDRMASKS:
                ret_value = icmpstat.icmpOutAddrMasks;
                break;
            case ICMPOUTADDRMASKREPS:
                ret_value = icmpstat.icmpOutAddrMaskReps;
                break;
            }
            Client_setVarTypedValue( request->requestvb, ASN01_COUNTER,
                ( u_char* )&ret_value, sizeof( ret_value ) );
        }
        break;

    case MODE_GETNEXT:
    case MODE_GETBULK:
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/icmp: Unsupported mode (%d)\n",
            reqinfo->mode );
        break;
    default:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/icmp: Unrecognised mode (%d)\n",
            reqinfo->mode );
        break;
    }

    return PRIOT_ERR_NOERROR;
}

int icmp_stats_table_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    VariableList* requestvb;
    TableRequestInfo* table_info;
    struct icmp_stats_table_entry* entry;
    oid subid;

    switch ( reqinfo->mode ) {
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            requestvb = request->requestvb;
            entry = ( struct icmp_stats_table_entry* )TableIterator_extractIteratorContext( request );
            if ( !entry )
                continue;
            table_info = Table_extractTableInfo( request );
            subid = table_info->colnum;
            DEBUG_MSGTL( ( "mibII/icmpStatsTable", "oid: " ) );
            DEBUG_MSGOID( ( "mibII/icmpStatsTable", request->requestvb->name,
                request->requestvb->nameLength ) );
            DEBUG_MSG( ( "mibII/icmpStatsTable", " In %d InErr %d Out %d OutErr %d\n",
                entry->icmpStatsInMsgs, entry->icmpStatsInErrors,
                entry->icmpStatsOutMsgs, entry->icmpStatsOutErrors ) );

            switch ( subid ) {
            case ICMP_STAT_INMSG:
                Client_setVarTypedValue( requestvb, ASN01_COUNTER,
                    ( u_char* )&entry->icmpStatsInMsgs, sizeof( uint32_t ) );
                break;
            case ICMP_STAT_INERR:
                Client_setVarTypedValue( requestvb, ASN01_COUNTER,
                    ( u_char* )&entry->icmpStatsInErrors, sizeof( uint32_t ) );
                break;
            case ICMP_STAT_OUTMSG:
                Client_setVarTypedValue( requestvb, ASN01_COUNTER,
                    ( u_char* )&entry->icmpStatsOutMsgs, sizeof( uint32_t ) );
                break;
            case ICMP_STAT_OUTERR:
                Client_setVarTypedValue( requestvb, ASN01_COUNTER,
                    ( u_char* )&entry->icmpStatsOutErrors, sizeof( uint32_t ) );
                break;
            default:
                Logger_log( LOGGER_PRIORITY_WARNING, "mibII/icmpStatsTable: Unrecognised column (%d)\n", ( int )subid );
            }
        }
        break;
    case MODE_GETNEXT:
    case MODE_GETBULK:
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/icmpStatsTable: Unsupported mode (%d)\n",
            reqinfo->mode );
        break;
    default:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/icmpStatsTable: Unrecognised mode (%d)\n",
            reqinfo->mode );
        break;
    }

    return PRIOT_ERR_NOERROR;
}

int icmp_msg_stats_table_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    VariableList* requestvb;
    TableRequestInfo* table_info;
    struct icmp_msg_stats_table_entry* entry;
    oid subid;

    switch ( reqinfo->mode ) {
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            requestvb = request->requestvb;
            entry = ( struct icmp_msg_stats_table_entry* )TableIterator_extractIteratorContext( request );
            if ( !entry )
                continue;
            table_info = Table_extractTableInfo( request );
            subid = table_info->colnum;
            DEBUG_MSGTL( ( "mibII/icmpMsgStatsTable", "oid: " ) );
            DEBUG_MSGOID( ( "mibII/icmpMsgStatsTable", request->requestvb->name,
                request->requestvb->nameLength ) );
            DEBUG_MSG( ( "mibII/icmpMsgStatsTable", " In %d Out %d Flags 0x%x\n",
                entry->icmpMsgStatsInPkts, entry->icmpMsgStatsOutPkts, entry->flags ) );

            switch ( subid ) {
            case ICMP_MSG_STAT_IN_PKTS:
                if ( entry->flags & ICMP_MSG_STATS_HAS_IN ) {
                    Client_setVarTypedValue( requestvb, ASN01_COUNTER,
                        ( u_char* )&entry->icmpMsgStatsInPkts, sizeof( uint32_t ) );
                } else {
                    requestvb->type = PRIOT_NOSUCHINSTANCE;
                }
                break;
            case ICMP_MSG_STAT_OUT_PKTS:
                if ( entry->flags & ICMP_MSG_STATS_HAS_OUT ) {
                    Client_setVarTypedValue( requestvb, ASN01_COUNTER,
                        ( u_char* )&entry->icmpMsgStatsOutPkts, sizeof( uint32_t ) );
                } else {
                    requestvb->type = PRIOT_NOSUCHINSTANCE;
                }
                break;
            default:
                Logger_log( LOGGER_PRIORITY_WARNING, "mibII/icmpMsgStatsTable: Unrecognised column (%d)\n", ( int )subid );
            }
        }
        break;
    case MODE_GETNEXT:
    case MODE_GETBULK:
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/icmpStatsTable: Unsupported mode (%d)\n",
            reqinfo->mode );
        break;
    default:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/icmpStatsTable: Unrecognised mode (%d)\n",
            reqinfo->mode );
        break;
    }

    return PRIOT_ERR_NOERROR;
}

/*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

int icmp_load( Cache* cache, void* vmagic )
{
    long ret_value = -1;

    ret_value = linux_read_icmp_stat( &icmpstat );

    if ( ret_value < 0 ) {
        DEBUG_MSGTL( ( "mibII/icmp", "Failed to load ICMP Group (linux)\n" ) );
    } else {
        DEBUG_MSGTL( ( "mibII/icmp", "Loaded ICMP Group (linux)\n" ) );
    }
    icmp_stats_load( cache, vmagic );
    icmp_msg_stats_load( cache, vmagic );
    return ret_value;
}

void icmp_free( Cache* cache, void* magic )
{
    memset( &icmpstat, 0, sizeof( icmpstat ) );
}
