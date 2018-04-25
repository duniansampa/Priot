/*
 *  TCP MIB group Table implementation - tcpTable.c
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

#include "tcpTable.h"
#include "Api.h"
#include "Client.h"
#include "Debug.h"
#include "Enum.h"
#include "Logger.h"
#include "siglog/system/generic.h"

#define TCPTABLE_ENTRY_TYPE struct inpcb
#define TCPTABLE_STATE inp_state
#define TCPTABLE_LOCALADDRESS inp_laddr.s_addr
#define TCPTABLE_LOCALPORT inp_lport
#define TCPTABLE_REMOTEADDRESS inp_faddr.s_addr
#define TCPTABLE_REMOTEPORT inp_fport
#define TCPTABLE_IS_LINKED_LIST

/* Head of linked list, or root of table */
TCPTABLE_ENTRY_TYPE* tcp_head = NULL;
int tcp_size = 0; /* Only used for table-based systems */
int tcp_estab = 0;

/*
	 *
	 * Initialization and handler routines are common to all architectures
	 *
	 */
#define MIB_STATS_CACHE_TIMEOUT 5
#define TCP_STATS_CACHE_TIMEOUT MIB_STATS_CACHE_TIMEOUT

#define TCP_PORT_TO_HOST_ORDER( x ) ntohs( x )

void init_tcpTable( void )
{
    const oid tcpTable_oid[] = { PRIOT_OID_MIB2, 6, 13 };

    TableRegistrationInfo* table_info;
    IteratorInfo* iinfo;
    HandlerRegistration* reginfo;
    int rc;

    DEBUG_MSGTL( ( "mibII/tcpTable", "Initialising TCP Table\n" ) );
    /*
     * Create the table data structure, and define the indexing....
     */
    table_info = TOOLS_MALLOC_TYPEDEF( TableRegistrationInfo );
    if ( !table_info ) {
        return;
    }
    Table_helperAddIndexes( table_info, ASN01_IPADDRESS,
        ASN01_INTEGER,
        ASN01_IPADDRESS,
        ASN01_INTEGER, 0 );
    table_info->min_column = TCPCONNSTATE;
    table_info->max_column = TCPCONNREMOTEPORT;

    /*
     * .... and iteration information ....
     */
    iinfo = TOOLS_MALLOC_TYPEDEF( IteratorInfo );
    if ( !iinfo ) {
        return;
    }
    iinfo->get_first_data_point = tcpTable_first_entry;
    iinfo->get_next_data_point = tcpTable_next_entry;
    iinfo->table_reginfo = table_info;

    /*
     * .... and register the table with the agent.
     */
    reginfo = AgentHandler_createHandlerRegistration( "tcpTable",
        tcpTable_handler,
        tcpTable_oid, ASN01_OID_LENGTH( tcpTable_oid ),
        HANDLER_CAN_RONLY ),
    rc = TableIterator_registerTableIterator2( reginfo, iinfo );
    if ( rc != ErrorCode_SUCCESS )
        return;

    /*
     * .... with a local cache
     *    (except for Solaris, which uses a different approach)
     */
    AgentHandler_injectHandler( reginfo,
        CacheHandler_getCacheHandler( TCP_STATS_CACHE_TIMEOUT,
                                    tcpTable_load, tcpTable_free,
                                    tcpTable_oid, ASN01_OID_LENGTH( tcpTable_oid ) ) );
}

int tcpTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    Types_VariableList* requestvb;
    TableRequestInfo* table_info;
    TCPTABLE_ENTRY_TYPE* entry;
    oid subid;
    long port;
    long state;

    DEBUG_MSGTL( ( "mibII/tcpTable", "Handler - mode %s\n",
        Enum_seFindLabelInSlist( "agentMode", reqinfo->mode ) ) );
    switch ( reqinfo->mode ) {
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            requestvb = request->requestvb;
            DEBUG_MSGTL( ( "mibII/tcpTable", "oid: " ) );
            DEBUG_MSGOID( ( "mibII/tcpTable", requestvb->name,
                requestvb->nameLength ) );
            DEBUG_MSG( ( "mibII/tcpTable", "\n" ) );

            entry = ( TCPTABLE_ENTRY_TYPE* )TableIterator_extractIteratorContext( request );
            if ( !entry )
                continue;
            table_info = Table_extractTableInfo( request );
            subid = table_info->colnum;

            switch ( subid ) {
            case TCPCONNSTATE:
                state = entry->TCPTABLE_STATE;
                Client_setVarTypedValue( requestvb, ASN01_INTEGER,
                    ( u_char* )&state, sizeof( state ) );
                break;
            case TCPCONNLOCALADDRESS:

                Client_setVarTypedValue( requestvb, ASN01_IPADDRESS,
                    ( u_char* )&entry->TCPTABLE_LOCALADDRESS,
                    sizeof( entry->TCPTABLE_LOCALADDRESS ) );

                break;
            case TCPCONNLOCALPORT:
                port = TCP_PORT_TO_HOST_ORDER( ( u_short )entry->TCPTABLE_LOCALPORT );
                Client_setVarTypedValue( requestvb, ASN01_INTEGER,
                    ( u_char* )&port, sizeof( port ) );
                break;
            case TCPCONNREMOTEADDRESS:

                Client_setVarTypedValue( requestvb, ASN01_IPADDRESS,
                    ( u_char* )&entry->TCPTABLE_REMOTEADDRESS,
                    sizeof( entry->TCPTABLE_REMOTEADDRESS ) );
                break;
            case TCPCONNREMOTEPORT:
                port = TCP_PORT_TO_HOST_ORDER( ( u_short )entry->TCPTABLE_REMOTEPORT );
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
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/tcpTable: Unsupported mode (%d)\n",
            reqinfo->mode );
        break;
    default:
        Logger_log( LOGGER_PRIORITY_WARNING, "mibII/tcpTable: Unrecognised mode (%d)\n",
            reqinfo->mode );
        break;
    }

    return PRIOT_ERR_NOERROR;
}

int TCP_Count_Connections( void )
{
    tcpTable_load( NULL, NULL );
    return tcp_estab;
}

Types_VariableList*
tcpTable_first_entry( void** loop_context,
    void** data_context,
    Types_VariableList* index,
    IteratorInfo* data )
{
    /*
     * XXX - How can we tell if the cache is valid?
     *       No access to 'reqinfo'
     */
    if ( tcp_head == NULL )
        return NULL;

    /*
     * Point to the first entry, and use the
     * 'next_entry' hook to retrieve this row
     */
    *loop_context = ( void* )tcp_head;
    return tcpTable_next_entry( loop_context, data_context, index, data );
}

Types_VariableList*
tcpTable_next_entry( void** loop_context,
    void** data_context,
    Types_VariableList* index,
    IteratorInfo* data )
{
    TCPTABLE_ENTRY_TYPE* entry = ( TCPTABLE_ENTRY_TYPE* )*loop_context;
    Types_VariableList* idx;
    long addr, port;

    if ( !entry )
        return NULL;

    /*
     * Set up the indexing for the specified row...
     */
    idx = index;

    addr = ntohl( entry->TCPTABLE_LOCALADDRESS );
    Client_setVarValue( idx, ( u_char* )&addr, sizeof( addr ) );

    port = TCP_PORT_TO_HOST_ORDER( entry->TCPTABLE_LOCALPORT );
    idx = idx->nextVariable;
    Client_setVarValue( idx, ( u_char* )&port, sizeof( port ) );

    idx = idx->nextVariable;

    addr = ntohl( entry->TCPTABLE_REMOTEADDRESS );
    Client_setVarValue( idx, ( u_char* )&addr, sizeof( addr ) );

    port = TCP_PORT_TO_HOST_ORDER( entry->TCPTABLE_REMOTEPORT );
    idx = idx->nextVariable;
    Client_setVarValue( idx, ( u_char* )&port, sizeof( port ) );

    /*
     * ... return the data structure for this row,
     * and update the loop context ready for the next one.
     */
    *data_context = ( void* )entry;
    *loop_context = ( void* )entry->INP_NEXT_SYMBOL;
    return index;
}

void tcpTable_free( Cache* cache, void* magic )
{
    TCPTABLE_ENTRY_TYPE* p;
    while ( tcp_head ) {
        p = tcp_head;
        tcp_head = tcp_head->INP_NEXT_SYMBOL;
        free( p );
    }

    tcp_head = NULL;
    tcp_size = 0;
    tcp_estab = 0;
}

/*
	 *
	 * The cache-handler loading routine is the main
	 *    place for architecture-specific code
	 *
	 * Load into either a table structure, or a linked list
	 *    depending on the system architecture
	 */

/*  see <netinet/tcp.h> */
#define TCP_ALL ( ( 1 << ( TCP_CLOSING + 1 ) ) - 1 )

const static int linux_states[ 12 ] = { 1, 5, 3, 4, 6, 7, 11, 1, 8, 9, 2, 10 };

int tcpTable_load( Cache* cache, void* vmagic )
{
    FILE* in;
    char line[ 256 ];

    tcpTable_free( cache, NULL );

    if ( !( in = fopen( "/proc/net/tcp", "r" ) ) ) {
        DEBUG_MSGTL( ( "mibII/tcpTable", "Failed to load TCP Table (linux1)\n" ) );
        LOGGER_LOGONCE( ( LOGGER_PRIORITY_ERR, "snmpd: cannot open /proc/net/tcp ...\n" ) );
        return -1;
    }

    /*
     * scan proc-file and build up a linked list 
     * This will actually be built up in reverse,
     *   but since the entries are unsorted, that doesn't matter.
     */
    while ( line == fgets( line, sizeof( line ), in ) ) {
        struct inpcb pcb, *nnew;
        unsigned int lp, fp;
        int state, uid;

        if ( 6 != sscanf( line,
                      "%*d: %x:%x %x:%x %x %*X:%*X %*X:%*X %*X %d",
                      &pcb.inp_laddr.s_addr, &lp,
                      &pcb.inp_faddr.s_addr, &fp, &state, &uid ) )
            continue;

        pcb.inp_lport = htons( ( unsigned short )lp );
        pcb.inp_fport = htons( ( unsigned short )fp );

        pcb.inp_state = ( state & 0xf ) < 12 ? linux_states[ state & 0xf ] : 2;
        if ( pcb.inp_state == 5 /* established */ || pcb.inp_state == 8 /*  closeWait  */ )
            tcp_estab++;
        pcb.uid = uid;

        nnew = TOOLS_MALLOC_TYPEDEF( struct inpcb );
        if ( nnew == NULL )
            break;
        memcpy( nnew, &pcb, sizeof( struct inpcb ) );
        nnew->inp_next = tcp_head;
        tcp_head = nnew;
    }

    fclose( in );

    DEBUG_MSGTL( ( "mibII/tcpTable", "Loaded TCP Table (linux)\n" ) );
    return 0;
}
