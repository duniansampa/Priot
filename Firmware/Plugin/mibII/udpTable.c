/*
 *  UDP MIB group Table implementation - udpTable.c
 *
 */

/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright � 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include "udpTable.h"
#include "Api.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "System/Containers/MapList.h"
#include "System/Util/Logger.h"
#include "tcpTable.h"
#include <netinet/udp.h>

#define INP_NEXT_SYMBOL inp_next

#define UDPTABLE_LOCALADDRESS inp_laddr.s_addr
#define UDPTABLE_LOCALPORT inp_lport
#define UDPTABLE_IS_LINKED_LIST

/* Head of linked list, or root of table */
struct inpcb* udp_head = NULL;
int udp_size = 0; /* Only used for table-based systems */

/*
	 *
	 * Initialization and handler routines are common to all architectures
	 *
	 */
#ifndef MIB_STATS_CACHE_TIMEOUT
#define MIB_STATS_CACHE_TIMEOUT 5
#endif
#ifndef UDP_STATS_CACHE_TIMEOUT
#define UDP_STATS_CACHE_TIMEOUT MIB_STATS_CACHE_TIMEOUT
#endif

#define UDP_ADDRESS_TO_HOST_ORDER( x ) ntohl( x )
#define UDP_ADDRESS_TO_NETWORK_ORDER( x ) x

#define UDP_PORT_TO_HOST_ORDER( x ) ntohs( x )

oid udpTable_oid[] = { PRIOT_OID_MIB2, 7, 5 };

void init_udpTable( void )
{
    TableRegistrationInfo* table_info;
    IteratorInfo* iinfo;
    HandlerRegistration* reginfo;
    int rc;

    DEBUG_MSGTL( ( "mibII/udpTable", "Initialising UDP Table\n" ) );
    /*
     * Create the table data structure, and define the indexing....
     */
    table_info = MEMORY_MALLOC_TYPEDEF( TableRegistrationInfo );
    if ( !table_info ) {
        return;
    }
    Table_helperAddIndexes( table_info, ASN01_IPADDRESS,
        ASN01_INTEGER, 0 );
    table_info->min_column = UDPLOCALADDRESS;
    table_info->max_column = UDPLOCALPORT;

    /*
     * .... and iteration information ....
     */
    iinfo = MEMORY_MALLOC_TYPEDEF( IteratorInfo );
    if ( !iinfo ) {
        Table_registrationInfoFree( table_info );
        return;
    }
    iinfo->get_first_data_point = udpTable_first_entry;
    iinfo->get_next_data_point = udpTable_next_entry;
    iinfo->table_reginfo = table_info;

    /*
     * .... and register the table with the agent.
     */
    reginfo = AgentHandler_createHandlerRegistration( "udpTable",
        udpTable_handler,
        udpTable_oid, ASN01_OID_LENGTH( udpTable_oid ),
        HANDLER_CAN_RONLY ),
    rc = TableIterator_registerTableIterator2( reginfo, iinfo );
    if ( rc != ErrorCode_SUCCESS )
        return;

    /*
     * .... with a local cache
     */
    AgentHandler_injectHandler( reginfo,
        CacheHandler_getCacheHandler( UDP_STATS_CACHE_TIMEOUT,
                                    udpTable_load, udpTable_free,
                                    udpTable_oid, ASN01_OID_LENGTH( udpTable_oid ) ) );
}

int udpTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    VariableList* requestvb;
    TableRequestInfo* table_info;
    struct inpcb* entry;
    oid subid;
    long port;
    in_addr_t addr;

    DEBUG_MSGTL( ( "mibII/udpTable", "Handler - mode %s\n",
        MapList_findLabel( "agentMode", reqinfo->mode ) ) );
    switch ( reqinfo->mode ) {
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            requestvb = request->requestvb;
            DEBUG_MSGTL( ( "mibII/udpTable", "oid: " ) );
            DEBUG_MSGOID( ( "mibII/udpTable", requestvb->name,
                requestvb->nameLength ) );
            DEBUG_MSG( ( "mibII/udpTable", "\n" ) );

            entry = ( struct inpcb* )TableIterator_extractIteratorContext( request );
            if ( !entry )
                continue;
            table_info = Table_extractTableInfo( request );
            subid = table_info->colnum;

            switch ( subid ) {
            case UDPLOCALADDRESS:
                addr = UDP_ADDRESS_TO_HOST_ORDER( entry->UDPTABLE_LOCALADDRESS );
                Client_setVarTypedValue( requestvb, ASN01_IPADDRESS,
                    ( u_char* )&addr,
                    sizeof( uint32_t ) );
                break;
            case UDPLOCALPORT:
                port = UDP_PORT_TO_HOST_ORDER( ( u_short )entry->UDPTABLE_LOCALPORT );
                Client_setVarTypedValue( requestvb, ASN01_INTEGER,
                    ( u_char* )&port, sizeof( port ) );
                break;
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
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/udpTable: Unsupported mode (%d)\n",
            reqinfo->mode );
        break;
    default:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/udpTable: Unrecognised mode (%d)\n",
            reqinfo->mode );
        break;
    }

    return PRIOT_ERR_NOERROR;
}

VariableList*
udpTable_first_entry( void** loop_context,
    void** data_context,
    VariableList* index,
    IteratorInfo* data )
{
    /*
     * XXX - How can we tell if the cache is valid?
     *       No access to 'reqinfo'
     */
    if ( udp_head == NULL )
        return NULL;

    /*
     * Point to the first entry, and use the
     * 'next_entry' hook to retrieve this row
     */
    *loop_context = ( void* )udp_head;
    return udpTable_next_entry( loop_context, data_context, index, data );
}

VariableList*
udpTable_next_entry( void** loop_context,
    void** data_context,
    VariableList* index,
    IteratorInfo* data )
{
    struct inpcb* entry = ( struct inpcb* )*loop_context;
    long port;
    long addr;

    if ( !entry )
        return NULL;

    /*
     * Set up the indexing for the specified row...
     */

    addr = UDP_ADDRESS_TO_NETWORK_ORDER( ( in_addr_t )entry->UDPTABLE_LOCALADDRESS );
    Client_setVarValue( index, ( u_char* )&addr,
        sizeof( addr ) );
    port = UDP_PORT_TO_HOST_ORDER( entry->UDPTABLE_LOCALPORT );
    Client_setVarValue( index->next,
        ( u_char* )&port, sizeof( port ) );

    /*
     * ... return the data structure for this row,
     * and update the loop context ready for the next one.
     */
    *data_context = ( void* )entry;
    *loop_context = ( void* )entry->INP_NEXT_SYMBOL;
    return index;
}

void udpTable_free( Cache* cache, void* magic )
{
    struct inpcb* p;
    while ( udp_head ) {
        p = udp_head;
        udp_head = udp_head->INP_NEXT_SYMBOL;
        free( p );
    }

    udp_head = NULL;
}

int udpTable_load( Cache* cache, void* vmagic )
{
    FILE* in;
    char line[ 256 ];

    udpTable_free( cache, NULL );

    if ( !( in = fopen( "/proc/net/udp", "r" ) ) ) {
        DEBUG_MSGTL( ( "mibII/udpTable", "Failed to load UDP Table (linux)\n" ) );
        LOGGER_LOGONCE( ( LOGGER_PRIORITY_ERR, "snmpd: cannot open /proc/net/udp ...\n" ) );
        return -1;
    }

    /*
     * scan proc-file and build up a linked list 
     * This will actually be built up in reverse,
     *   but since the entries are unsorted, that doesn't matter.
     */
    while ( line == fgets( line, sizeof( line ), in ) ) {
        struct inpcb pcb, *nnew;
        unsigned int state, lport;

        memset( &pcb, 0, sizeof( pcb ) );

        if ( 3 != sscanf( line, "%*d: %x:%x %*x:%*x %x",
                      &pcb.inp_laddr.s_addr, &lport, &state ) )
            continue;

        if ( state != 7 ) /* fix me:  UDP_LISTEN ??? */
            continue;

        /* store in network byte order */
        pcb.inp_laddr.s_addr = htonl( pcb.inp_laddr.s_addr );
        pcb.inp_lport = htons( ( unsigned short )( lport ) );

        nnew = MEMORY_MALLOC_TYPEDEF( struct inpcb );
        if ( nnew == NULL )
            break;
        memcpy( nnew, &pcb, sizeof( struct inpcb ) );
        nnew->inp_next = udp_head;
        udp_head = nnew;
    }

    fclose( in );

    DEBUG_MSGTL( ( "mibII/udpTable", "Loaded UDP Table (linux)\n" ) );
    return 0;
}
