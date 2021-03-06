/*
 *  udpEndpointTable MIB architecture support
 *
 * $Id$
 */

#include "ReadConfig.h"
#include "System/Util/Trace.h"
#include "System/Util/File.h"
#include "System/Util/FileParser.h"
#include "udp_endpoint_private.h"
#include "utilities/get_pid_from_inode.h"

static int _load4( Container_Container* container, u_int flags );

/*
 * initialize arch specific storage
 *
 * @retval  0: success
 * @retval <0: error
 */
int netsnmp_arch_udp_endpoint_entry_init( netsnmp_udp_endpoint_entry* entry )
{
    /*
     * init
     */
    return 0;
}

/*
 * cleanup arch specific storage
 */
void netsnmp_arch_udp_endpoint_entry_cleanup( netsnmp_udp_endpoint_entry* entry )
{
    /*
     * cleanup
     */
}

/*
 * copy arch specific storage
 */
int netsnmp_arch_udp_endpoint_entry_copy( netsnmp_udp_endpoint_entry* lhs,
    netsnmp_udp_endpoint_entry* rhs )
{
    return 0;
}

/*
 * delete an entry
 */
int netsnmp_arch_udp_endpoint_delete( netsnmp_udp_endpoint_entry* entry )
{
    if ( NULL == entry )
        return -1;
    /** xxx-rks:9 udp_endpoint delete not implemented */
    return -1;
}

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
int netsnmp_arch_udp_endpoint_container_load( Container_Container* container,
    u_int load_flags )
{
    int rc = 0;

    /* Setup the pid_from_inode table, and fill it.*/
    netsnmp_get_pid_from_inode_init();

    rc = _load4( container, load_flags );
    if ( rc < 0 ) {
        u_int flags = NETSNMP_ACCESS_UDP_ENDPOINT_FREE_KEEP_CONTAINER;
        netsnmp_access_udp_endpoint_container_free( container, flags );
        return rc;
    }

    return 0;
}

/**
 * @internal
 * process token value index line
 */
static int
_process_line_udp_ep( TextLineInfo_t* line_info, void* mem,
    struct TextLineProcessInfo_s* lpi )
{
    netsnmp_udp_endpoint_entry* ep = ( netsnmp_udp_endpoint_entry* )mem;
    char *ptr, *sep;
    u_char* u_ptr;
    size_t u_ptr_len, offset, len;
    unsigned long long inode;
    size_t count = 0;

    /*
     * skip 'sl'
     */
    ptr = ReadConfig_skipNotWhite( line_info->start );
    if ( NULL == ptr ) {
        DEBUG_MSGTL( ( "access:udp_endpoint", "no sl '%s'\n",
            line_info->start ) );
        return fpRETURN_CODE_MEMORY_UNUSED;
    }
    ptr = ReadConfig_skipWhite( ptr );
    if ( NULL == ptr ) {
        DEBUG_MSGTL( ( "text:util:tvi", "no space after sl '%s'\n",
            line_info->start ) );
        return fpRETURN_CODE_MEMORY_UNUSED;
    }

    /*
     * get local address. ignore error on hex conversion, since that
     * function doesn't like the ':' between address and port. check the
     * offset to see if it worked. May need to flip string too.
     */
    u_ptr = ep->loc_addr;
    u_ptr_len = sizeof( ep->loc_addr );
    sep = strchr( ptr, ':' );
    if ( NULL == sep ) {
        DEBUG_MSGTL( ( "text:util:tvi", "no ':' '%s'\n",
            line_info->start ) );
        return fpRETURN_CODE_MEMORY_UNUSED;
    }
    len = ( sep - ptr );
    if ( -1 == Utilities_addrStringHton( ptr, len ) ) {
        DEBUG_MSGTL( ( "text:util:tvi", "bad length %d for loc addr '%s'\n",
            ( int )u_ptr_len, line_info->start ) );
        return fpRETURN_CODE_MEMORY_UNUSED;
    }
    offset = 0;
    Convert_hexStringToBinaryString( &u_ptr, &u_ptr_len, &offset, 0, ptr, NULL );
    if ( ( 4 != offset ) && ( 16 != offset ) ) {
        DEBUG_MSGTL( ( "text:util:tvi", "bad offset %d for loc addr '%s'\n",
            ( int )offset, line_info->start ) );
        return fpRETURN_CODE_MEMORY_UNUSED;
    }
    ep->loc_addr_len = offset;
    ptr += ( offset * 2 );
    ++ptr; /* skip ':' */

    /*
     * get local port
     */
    ep->loc_port = strtol( ptr, &ptr, 16 );
    ptr = ReadConfig_skipWhite( ptr );

    /*
     * get remote address. ignore error on hex conversion, since that
     * function doesn't like the ':' between address and port. check the
     * offset to see if it worked. May need to flip string too.
     */
    u_ptr = ep->rmt_addr;
    u_ptr_len = sizeof( ep->rmt_addr );
    sep = strchr( ptr, ':' );
    if ( NULL == sep ) {
        DEBUG_MSGTL( ( "text:util:tvi", "no ':' '%s'\n",
            line_info->start ) );
        return fpRETURN_CODE_MEMORY_UNUSED;
    }
    len = ( sep - ptr );
    if ( -1 == Utilities_addrStringHton( ptr, len ) ) {
        DEBUG_MSGTL( ( "text:util:tvi", "bad length %d for rmt addr '%s'\n",
            ( int )u_ptr_len, line_info->start ) );
        return fpRETURN_CODE_MEMORY_UNUSED;
    }
    offset = 0;
    Convert_hexStringToBinaryString( &u_ptr, &u_ptr_len, &offset, 0, ptr, NULL );
    if ( ( 4 != offset ) && ( 16 != offset ) ) {
        DEBUG_MSGTL( ( "text:util:tvi", "bad offset %d for rmt addr '%s'\n",
            ( int )offset, line_info->start ) );
        return fpRETURN_CODE_MEMORY_UNUSED;
    }
    ep->rmt_addr_len = offset;
    ptr += ( offset * 2 );
    ++ptr; /* skip ':' */

    /*
     * get remote port
     */
    ep->rmt_port = strtol( ptr, &ptr, 16 );
    ptr = ReadConfig_skipWhite( ptr );

    /*
     * get state too
     */
    ep->state = strtol( ptr, &ptr, 16 );

    /*
     * Use inode as instance value.
     */
    while ( count != 5 ) {
        ptr = ReadConfig_skipWhite( ptr );
        ptr = ReadConfig_skipNotWhite( ptr );
        count++;
    }
    inode = strtoull( ptr, &ptr, 0 );
    ep->instance = ( u_int )inode;

    /*
     * get the pid also
     */
    ep->pid = netsnmp_get_pid_from_inode( inode );

    ep->index = ( uintptr_t )( lpi->userContext );
    lpi->userContext = ( void* )( ( char* )( lpi->userContext ) + 1 );

    ep->oid_index.oids = &ep->index;
    ep->oid_index.len = 1;

    return fpRETURN_CODE_MEMORY_USED;
}

/**
 *
 * @retval  0 no errors
 * @retval !0 errors
 */
static int
_load4( Container_Container* container, u_int load_flags )
{
    File* fp;
    TextLineProcessInfo_t textLineProcessInfo;

    if ( NULL == container )
        return -1;

    /*
     * allocate file resources
     */
    fp = File_fill( NULL, "/proc/net/udp", O_RDONLY, 0, 0 );
    if ( NULL == fp ) /** msg already logged */
        return -2;

    memset( &textLineProcessInfo, 0x0, sizeof( textLineProcessInfo ) );
    textLineProcessInfo.memSize = sizeof( netsnmp_udp_endpoint_entry );
    textLineProcessInfo.processFunc = _process_line_udp_ep;
    textLineProcessInfo.userContext = ( void* )0;

    container = FileParser_parse( fp, container, fpPARSE_MODE_USER_FUNCTION,
        0, &textLineProcessInfo );
    File_release( fp );
    return ( NULL == container );
}
