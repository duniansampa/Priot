/*
 *  tcpConnTable MIB architecture support
 *
 * $Id$
 */

#include "siglog/data_access/tcpConn.h"
#include "System/Util/Assert.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "tcp-mib/tcpConnectionTable/tcpConnectionTable_constants.h"
#include "tcpConn.h"
#include "tcpConn_private.h"
#include "utilities/get_pid_from_inode.h"

static int
    linux_states[ 12 ]
    = { 1, 5, 3, 4, 6, 7, 11, 1, 8, 9, 2, 10 };

static int _load4( Container_Container* container, u_int flags );

/*
 * initialize arch specific storage
 *
 * @retval  0: success
 * @retval <0: error
 */
int netsnmp_arch_tcpconn_entry_init( netsnmp_tcpconn_entry* entry )
{
    /*
     * init
     */
    return 0;
}

/*
 * cleanup arch specific storage
 */
void netsnmp_arch_tcpconn_entry_cleanup( netsnmp_tcpconn_entry* entry )
{
    /*
     * cleanup
     */
}

/*
 * copy arch specific storage
 */
int netsnmp_arch_tcpconn_entry_copy( netsnmp_tcpconn_entry* lhs,
    netsnmp_tcpconn_entry* rhs )
{
    return 0;
}

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
int netsnmp_arch_tcpconn_container_load( Container_Container* container,
    u_int load_flags )
{
    int rc = 0;

    DEBUG_MSGTL( ( "access:tcpconn:container",
        "tcpconn_container_arch_load (flags %x)\n", load_flags ) );

    /* Setup the pid_from_inode table, and fill it.*/
    netsnmp_get_pid_from_inode_init();

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "no container specified/found for access_tcpconn\n" );
        return -1;
    }

    rc = _load4( container, load_flags );

    return rc;
}

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
static int
_load4( Container_Container* container, u_int load_flags )
{
    int rc = 0;
    FILE* in;
    char line[ 160 ];

    Assert_assert( NULL != container );

#define PROCFILE "/proc/net/tcp"
    if ( !( in = fopen( PROCFILE, "r" ) ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "could not open " PROCFILE "\n" );
        return -2;
    }

    fgets( line, sizeof( line ), in ); /* skip header */

    /*
     *   sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  timeout inode
     *   0: 00000000:8000 00000000:0000 0A 00000000:00000000 00:00000000 00000000    29        0 1028 1 df7b1b80 300 0 0 2 -1
     */
    while ( fgets( line, sizeof( line ), in ) ) {
        netsnmp_tcpconn_entry* entry;
        unsigned int state, local_port, remote_port, tmp_state;
        unsigned long long inode;
        size_t buf_len, offset;
        char local_addr[ 10 ], remote_addr[ 10 ];
        u_char* tmp_ptr;

        if ( 6 != ( rc = sscanf( line, "%*d: %8[0-9A-Z]:%x %8[0-9A-Z]:%x %x %*x:%*x %*x:%*x %*x %*x %*x %llu",
                        local_addr, &local_port,
                        remote_addr, &remote_port, &tmp_state, &inode ) ) ) {
            DEBUG_MSGT( ( "access:tcpconn:container",
                "error parsing line (%d != 6)\n", rc ) );
            DEBUG_MSGT( ( "access:tcpconn:container", " line '%s'\n", line ) );
            Logger_log( LOGGER_PRIORITY_ERR, "tcp:_load4: bad line in " PROCFILE ": %s\n", line );
            rc = 0;
            continue;
        }
        DEBUG_MSGT( ( "verbose:access:tcpconn:container", " line '%s'\n", line ) );

        /*
         * check if we care about listen state
         */
        state = ( tmp_state & 0xf ) < 12 ? linux_states[ tmp_state & 0xf ] : 2;
        if ( load_flags ) {
            if ( TCPCONNECTIONSTATE_LISTEN == state ) {
                if ( load_flags & NETSNMP_ACCESS_TCPCONN_LOAD_NOLISTEN ) {
                    DEBUG_MSGT( ( "verbose:access:tcpconn:container",
                        " skipping listen\n" ) );
                    continue;
                }
            } else if ( load_flags & NETSNMP_ACCESS_TCPCONN_LOAD_ONLYLISTEN ) {
                DEBUG_MSGT( ( "verbose:access:tcpconn:container",
                    " skipping non-listen\n" ) );
                continue;
            }
        }

        /*
         */
        entry = netsnmp_access_tcpconn_entry_create();
        if ( NULL == entry ) {
            rc = -3;
            break;
        }

        /** oddly enough, these appear to already be in network order */
        entry->loc_port = ( unsigned short )local_port;
        entry->rmt_port = ( unsigned short )remote_port;
        entry->tcpConnState = state;
        entry->pid = netsnmp_get_pid_from_inode( inode );

        /** the addr string may need work */
        buf_len = strlen( local_addr );
        if ( ( 8 != buf_len ) || ( -1 == Utilities_addrStringHton( local_addr, 8 ) ) ) {
            DEBUG_MSGT( ( "verbose:access:tcpconn:container",
                " error processing local address\n" ) );
            netsnmp_access_tcpconn_entry_free( entry );
            continue;
        }
        offset = 0;
        tmp_ptr = entry->loc_addr;
        rc = Convert_hexStringToBinaryString( &tmp_ptr, &buf_len,
            &offset, 0, local_addr, NULL );
        entry->loc_addr_len = offset;
        if ( 4 != entry->loc_addr_len ) {
            DEBUG_MSGT( ( "access:tcpconn:container",
                "error parsing local addr (%d != 4)\n",
                entry->loc_addr_len ) );
            DEBUG_MSGT( ( "access:tcpconn:container", " line '%s'\n", line ) );
            netsnmp_access_tcpconn_entry_free( entry );
            continue;
        }

        /** the addr string may need work */
        buf_len = strlen( ( char* )remote_addr );
        if ( ( 8 != buf_len ) || ( -1 == Utilities_addrStringHton( remote_addr, 8 ) ) ) {
            DEBUG_MSGT( ( "verbose:access:tcpconn:container",
                " error processing remote address\n" ) );
            netsnmp_access_tcpconn_entry_free( entry );
            continue;
        }
        offset = 0;
        tmp_ptr = entry->rmt_addr;
        rc = Convert_hexStringToBinaryString( &tmp_ptr, &buf_len,
            &offset, 0, remote_addr, NULL );
        entry->rmt_addr_len = offset;
        if ( 4 != entry->rmt_addr_len ) {
            DEBUG_MSGT( ( "access:tcpconn:container",
                "error parsing remote addr (%d != 4)\n",
                entry->rmt_addr_len ) );
            DEBUG_MSGT( ( "access:tcpconn:container", " line '%s'\n", line ) );
            netsnmp_access_tcpconn_entry_free( entry );
            continue;
        }

        /*
         * add entry to container
         */
        entry->arbitrary_index = CONTAINER_SIZE( container ) + 1;
        CONTAINER_INSERT( container, entry );
    }

    fclose( in );

    if ( rc < 0 )
        return rc;

    return 0;
}
