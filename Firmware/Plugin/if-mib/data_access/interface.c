/*
 *  Interface MIB architecture support
 *
 * $Id$
 */

#include "interface.h"
#include "AgentReadConfig.h"
#include "System/Util/Assert.h"
#include "CacheHandler.h"
#include "System/Numerics/Integer64.h"
#include "ReadConfig.h"
#include "System/Containers/MapList.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "if-mib/ifTable/ifTable.h"
#include "if-mib/ifTable/ifTable_constants.h"

/**---------------------------------------------------------------------*/
/*
 * local static vars
 */
static netsnmp_conf_if_list* conf_list = NULL;
static int need_wrap_check = -1;
static int _access_interface_init = 0;

/*
 * local static prototypes
 */
static int _access_interface_entry_compare_name( const void* lhs,
    const void* rhs );
static void _access_interface_entry_release( netsnmp_interface_entry* entry,
    void* unused );
static void _access_interface_entry_save_name( const char* name, oid index );
static void _parse_interface_config( const char* token, char* cptr );
static void _free_interface_config( void );

/**---------------------------------------------------------------------*/
/*
 * external per-architecture functions prototypes
 *
 * These shouldn't be called by the general public, so they aren't in
 * the header file.
 */
extern void netsnmp_arch_interface_init( void );
extern int
netsnmp_arch_interface_container_load( Container_Container* container,
    u_int load_flags );
extern int
netsnmp_arch_set_admin_status( netsnmp_interface_entry* entry,
    int ifAdminStatus );
extern int netsnmp_arch_interface_index_find( const char* name );

/**
 * initialization
 */
void init_interface( void )
{
    AgentReadConfig_priotdRegisterConfigHandler( "interface", _parse_interface_config,
        _free_interface_config,
        "name type speed" );
}

void netsnmp_access_interface_init( void )
{
    Assert_assert( 0 == _access_interface_init ); /* who is calling twice? */

    if ( 1 == _access_interface_init )
        return;

    _access_interface_init = 1;

    {
        Container_Container* ifcontainer;

        netsnmp_arch_interface_init();

        /*
         * load once to set up ifIndexes
         */
        ifcontainer = netsnmp_access_interface_container_load( NULL, 0 );
        if ( NULL != ifcontainer )
            netsnmp_access_interface_container_free( ifcontainer, 0 );
    }
}

/**---------------------------------------------------------------------*/
/*
 * container functions
 */
/**
 * initialize interface container
 */
Container_Container*
netsnmp_access_interface_container_init( u_int flags )
{
    Container_Container* container1;

    DEBUG_MSGTL( ( "access:interface:container", "init\n" ) );

    /*
     * create the containers. one indexed by ifIndex, the other
     * indexed by ifName.
     */
    container1 = Container_find( "access_interface:tableContainer" );
    if ( NULL == container1 )
        return NULL;

    container1->containerName = strdup( "interface container" );
    if ( flags & NETSNMP_ACCESS_INTERFACE_INIT_ADDL_IDX_BY_NAME ) {
        Container_Container* container2 = Container_find( "access_interface_by_name:access_interface:tableContainer" );
        if ( NULL == container2 )
            return NULL;

        container2->containerName = strdup( "interface name container" );
        container2->compare = _access_interface_entry_compare_name;

        Container_addIndex( container1, container2 );
    }

    return container1;
}

/**
 * load interface information in specified container
 *
 * @param container empty container, or NULL to have one created for you
 * @param load_flags flags to modify behaviour. Examples:
 *                   NETSNMP_ACCESS_INTERFACE_INIT_ADDL_IDX_BY_NAME
 *
 * @retval NULL  error
 * @retval !NULL pointer to container
 */
Container_Container*
netsnmp_access_interface_container_load( Container_Container* container, u_int load_flags )
{
    int rc;

    DEBUG_MSGTL( ( "access:interface:container", "load\n" ) );
    Assert_assert( 1 == _access_interface_init );

    if ( NULL == container )
        container = netsnmp_access_interface_container_init( load_flags );
    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "no container specified/found for access_interface\n" );
        return NULL;
    }

    rc = netsnmp_arch_interface_container_load( container, load_flags );
    if ( 0 != rc ) {
        netsnmp_access_interface_container_free( container,
            NETSNMP_ACCESS_INTERFACE_FREE_NOFLAGS );
        container = NULL;
    }

    return container;
}

void netsnmp_access_interface_container_free( Container_Container* container, u_int free_flags )
{
    DEBUG_MSGTL( ( "access:interface:container", "free\n" ) );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR, "invalid container for netsnmp_access_interface_free\n" );
        return;
    }

    if ( !( free_flags & NETSNMP_ACCESS_INTERFACE_FREE_DONT_CLEAR ) ) {
        /*
         * free all items.
         */
        CONTAINER_CLEAR( container,
            ( Container_FuncObjFunc* )_access_interface_entry_release,
            NULL );
    }

    CONTAINER_FREE( container );
}

/**
 * @retval 0  interface not found
 */
oid netsnmp_access_interface_index_find( const char* name )
{
    DEBUG_MSGTL( ( "access:interface:find", "index\n" ) );
    Assert_assert( 1 == _access_interface_init );

    return netsnmp_arch_interface_index_find( name );
}

/**---------------------------------------------------------------------*/
/*
 * ifentry functions
 */
/**
 */
netsnmp_interface_entry*
netsnmp_access_interface_entry_get_by_index( Container_Container* container, oid index )
{
    Types_Index tmp;

    DEBUG_MSGTL( ( "access:interface:entry", "by_index\n" ) );
    Assert_assert( 1 == _access_interface_init );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid container for netsnmp_access_interface_entry_get_by_index\n" );
        return NULL;
    }

    tmp.len = 1;
    tmp.oids = &index;

    return ( netsnmp_interface_entry* )CONTAINER_FIND( container, &tmp );
}

/**
 */
netsnmp_interface_entry*
netsnmp_access_interface_entry_get_by_name( Container_Container* container,
    const char* name )
{
    netsnmp_interface_entry tmp;

    DEBUG_MSGTL( ( "access:interface:entry", "by_name\n" ) );
    Assert_assert( 1 == _access_interface_init );

    if ( NULL == container ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "invalid container for netsnmp_access_interface_entry_get_by_name\n" );
        return NULL;
    }

    if ( NULL == container->next ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "secondary index missing for netsnmp_access_interface_entry_get_by_name\n" );
        return NULL;
    }

    tmp.name = UTILITIES_REMOVE_CONST( char*, name );
    return ( netsnmp_interface_entry* )CONTAINER_FIND( container->next, &tmp );
}

/**
 * @retval NULL  index not found
 */
const char*
netsnmp_access_interface_name_find( oid index )
{
    DEBUG_MSGTL( ( "access:interface:find", "name\n" ) );
    Assert_assert( 1 == _access_interface_init );

    return MapList_findLabel( "interfaces", index );
}

/**
 */
netsnmp_interface_entry*
netsnmp_access_interface_entry_create( const char* name, oid if_index )
{
    netsnmp_interface_entry* entry = MEMORY_MALLOC_TYPEDEF( netsnmp_interface_entry );

    DEBUG_MSGTL( ( "access:interface:entry", "create\n" ) );
    Assert_assert( 1 == _access_interface_init );

    if ( NULL == entry )
        return NULL;

    if ( NULL != name )
        entry->name = strdup( name );

    /*
     * get if index, and save name for reverse lookup
     */
    if ( 0 == if_index )
        entry->index = netsnmp_access_interface_index_find( name );
    else
        entry->index = if_index;
    _access_interface_entry_save_name( name, entry->index );

    entry->descr = strdup( name );

    /*
     * make some assumptions
     */
    entry->connector_present = 1;

    entry->oid_index.len = 1;
    entry->oid_index.oids = ( oid* )&entry->index;

    return entry;
}

/**
 */
void netsnmp_access_interface_entry_free( netsnmp_interface_entry* entry )
{
    DEBUG_MSGTL( ( "access:interface:entry", "free\n" ) );

    if ( NULL == entry )
        return;

    /*
     * MEMORY_FREE not needed, for any of these,
     * since the whole entry is about to be freed
     */

    if ( NULL != entry->old_stats )
        free( entry->old_stats );

    if ( NULL != entry->name )
        free( entry->name );

    if ( NULL != entry->descr )
        free( entry->descr );

    if ( NULL != entry->paddr )
        free( entry->paddr );

    free( entry );
}

/*
 * Blech - backwards compatible mibII/interfaces style interface
 * functions, so we don't have to update older modules to use
 * the new code to get correct ifIndex values.
 */

static Container_Iterator* it = NULL;
static ifTable_rowreq_ctx* row = NULL;

/**
 * Setup an iterator for scanning the interfaces using the cached entry
 * from if-mib/ifTable.
 */
void Interface_Scan_Init( void )
{
    Container_Container* cont = NULL;
    Cache* cache = NULL;

    cache = CacheHandler_findByOid( ifTable_oid, ifTable_oid_size );
    if ( NULL != cache ) {
        CacheHandler_checkAndReload( cache );
        cont = ( Container_Container* )cache->magic;
    }

    if ( NULL != cont ) {
        if ( NULL != it )
            CONTAINER_ITERATOR_RELEASE( it );

        it = CONTAINER_ITERATOR( cont );
    }

    if ( NULL != it )
        row = ( ifTable_rowreq_ctx* )CONTAINER_ITERATOR_FIRST( it );
}

int Interface_Scan_Next( short* index, char* name, netsnmp_interface_entry** entry,
    void* dc )
{
    int returnIndex = 0;
    int ret;
    if ( index )
        returnIndex = *index;

    ret = Interface_Scan_NextInt( &returnIndex, name, entry, dc );
    if ( index )
        *index = ( returnIndex & 0x8fff );
    return ret;
}

int Interface_Scan_NextInt( int* index, char* name, netsnmp_interface_entry** entry,
    void* dc )
{
    netsnmp_interface_entry* e = NULL;

    if ( NULL == row )
        return 0;

    e = row->data.ifentry;
    if ( index )
        *index = e->index;

    if ( name )
        strcpy( name, e->name );

    if ( entry )
        *entry = e;

    row = ( ifTable_rowreq_ctx* )CONTAINER_ITERATOR_NEXT( it );

    return 1;
}

/**
 *
 * @retval 0   : success
 * @retval < 0 : error
 */
int netsnmp_access_interface_entry_set_admin_status( netsnmp_interface_entry* entry,
    int ifAdminStatus )
{
    int rc;

    DEBUG_MSGTL( ( "access:interface:entry", "set_admin_status\n" ) );

    if ( NULL == entry )
        return -1;

    if ( ( ifAdminStatus < IFADMINSTATUS_UP ) || ( ifAdminStatus > IFADMINSTATUS_TESTING ) )
        return -2;

    rc = netsnmp_arch_set_admin_status( entry, ifAdminStatus );
    if ( 0 == rc ) /* success */
        entry->admin_status = ifAdminStatus;

    return rc;
}

/**---------------------------------------------------------------------*/
/*
 * Utility routines
 */

/**
 */
static int
_access_interface_entry_compare_name( const void* lhs, const void* rhs )
{
    return strcmp( ( ( const netsnmp_interface_entry* )lhs )->name,
        ( ( const netsnmp_interface_entry* )rhs )->name );
}

/**
 */
static void
_access_interface_entry_release( netsnmp_interface_entry* entry, void* context )
{
    netsnmp_access_interface_entry_free( entry );
}

/**
 */
static void
_access_interface_entry_save_name( const char* name, oid index )
{
    int tmp;

    if ( NULL == name )
        return;

    tmp = MapList_findValue( "interfaces", name );
    if ( tmp == MapListErrorCode_NULL ) {
        MapList_addPair( "interfaces", strdup( name ), index );
        DEBUG_MSGTL( ( "access:interface:ifIndex",
            "saved ifIndex %"
            "l"
            "u for %s\n",
            index, name ) );
    } else if ( index != ( oid )tmp ) {
        LOGGER_LOGONCE( ( LOGGER_PRIORITY_ERR, "IfIndex of an interface changed. Such "
                                               "interfaces will appear multiple times in IF-MIB.\n" ) );
        DEBUG_MSGTL( ( "access:interface:ifIndex",
            "index %"
            "l"
            "u != tmp for %s\n",
            index, name ) );
    }
}

/**
 * update stats
 *
 * @retval  0 : success
 * @retval -1 : error
 */
int netsnmp_access_interface_entry_update_stats( netsnmp_interface_entry* prev_vals,
    netsnmp_interface_entry* new_vals )
{
    DEBUG_MSGTL( ( "access:interface", "check_wrap\n" ) );

    /*
     * sanity checks
     */
    if ( ( NULL == prev_vals ) || ( NULL == new_vals ) || ( NULL == prev_vals->name ) || ( NULL == new_vals->name ) || ( 0 != strncmp( prev_vals->name, new_vals->name, strlen( prev_vals->name ) ) ) )
        return -1;

    /*
     * if we've determined that we have 64 bit counters, just copy them.
     */
    if ( 0 == need_wrap_check ) {
        memcpy( &prev_vals->stats, &new_vals->stats, sizeof( new_vals->stats ) );
        return 0;
    }

    if ( NULL == prev_vals->old_stats ) {
        /*
         * if we don't have old stats, copy previous stats
         */
        prev_vals->old_stats = MEMORY_MALLOC_TYPEDEF( netsnmp_interface_stats );
        if ( NULL == prev_vals->old_stats ) {
            return -2;
        }
        memcpy( prev_vals->old_stats, &prev_vals->stats, sizeof( prev_vals->stats ) );
    }

    if ( 0 != Integer64_check32AndUpdate( &prev_vals->stats.ibytes,
                  &new_vals->stats.ibytes,
                  &prev_vals->old_stats->ibytes,
                  &need_wrap_check ) )
        DEBUG_MSGTL( ( "access:interface",
            "Error expanding ifHCInOctets to 64bits\n" ) );

    if ( new_vals->ns_flags & NETSNMP_INTERFACE_FLAGS_CALCULATE_UCAST ) {
        if ( 0 != Integer64_check32AndUpdate( &prev_vals->stats.iall,
                      &new_vals->stats.iall,
                      &prev_vals->old_stats->iall,
                      &need_wrap_check ) )
            DEBUG_MSGTL( ( "access:interface",
                "Error expanding packet count to 64bits\n" ) );
    } else {
        if ( 0 != Integer64_check32AndUpdate( &prev_vals->stats.iucast,
                      &new_vals->stats.iucast,
                      &prev_vals->old_stats->iucast,
                      &need_wrap_check ) )
            DEBUG_MSGTL( ( "access:interface",
                "Error expanding ifHCInUcastPkts to 64bits\n" ) );
    }

    if ( 0 != Integer64_check32AndUpdate( &prev_vals->stats.iucast,
                  &new_vals->stats.iucast,
                  &prev_vals->old_stats->iucast,
                  &need_wrap_check ) )
        DEBUG_MSGTL( ( "access:interface",
            "Error expanding ifHCInUcastPkts to 64bits\n" ) );

    if ( 0 != Integer64_check32AndUpdate( &prev_vals->stats.imcast,
                  &new_vals->stats.imcast,
                  &prev_vals->old_stats->imcast,
                  &need_wrap_check ) )
        DEBUG_MSGTL( ( "access:interface",
            "Error expanding ifHCInMulticastPkts to 64bits\n" ) );

    if ( 0 != Integer64_check32AndUpdate( &prev_vals->stats.ibcast,
                  &new_vals->stats.ibcast,
                  &prev_vals->old_stats->ibcast,
                  &need_wrap_check ) )
        DEBUG_MSGTL( ( "access:interface",
            "Error expanding ifHCInBroadcastPkts to 64bits\n" ) );

    if ( 0 != Integer64_check32AndUpdate( &prev_vals->stats.obytes,
                  &new_vals->stats.obytes,
                  &prev_vals->old_stats->obytes,
                  &need_wrap_check ) )
        DEBUG_MSGTL( ( "access:interface",
            "Error expanding ifHCOutOctets to 64bits\n" ) );

    if ( 0 != Integer64_check32AndUpdate( &prev_vals->stats.oucast,
                  &new_vals->stats.oucast,
                  &prev_vals->old_stats->oucast,
                  &need_wrap_check ) )
        DEBUG_MSGTL( ( "access:interface",
            "Error expanding ifHCOutUcastPkts to 64bits\n" ) );

    if ( 0 != Integer64_check32AndUpdate( &prev_vals->stats.omcast,
                  &new_vals->stats.omcast,
                  &prev_vals->old_stats->omcast,
                  &need_wrap_check ) )
        DEBUG_MSGTL( ( "access:interface",
            "Error expanding ifHCOutMulticastPkts to 64bits\n" ) );

    if ( 0 != Integer64_check32AndUpdate( &prev_vals->stats.obcast,
                  &new_vals->stats.obcast,
                  &prev_vals->old_stats->obcast,
                  &need_wrap_check ) )
        DEBUG_MSGTL( ( "access:interface",
            "Error expanding ifHCOutBroadcastPkts to 64bits\n" ) );

    /*
     * Copy 32 bit counters
     */
    prev_vals->stats.ierrors = new_vals->stats.ierrors;
    prev_vals->stats.idiscards = new_vals->stats.idiscards;
    prev_vals->stats.iunknown_protos = new_vals->stats.iunknown_protos;
    prev_vals->stats.inucast = new_vals->stats.inucast;
    prev_vals->stats.oerrors = new_vals->stats.oerrors;
    prev_vals->stats.odiscards = new_vals->stats.odiscards;
    prev_vals->stats.oqlen = new_vals->stats.oqlen;
    prev_vals->stats.collisions = new_vals->stats.collisions;
    prev_vals->stats.onucast = new_vals->stats.onucast;

    /*
     * if we've decided we no longer need to check wraps, free old stats
     */
    if ( 0 == need_wrap_check ) {
        MEMORY_FREE( prev_vals->old_stats );
    } else {
        /*
         * update old stats from new stats.
         * careful - old_stats is a pointer to stats...
         */
        memcpy( prev_vals->old_stats, &new_vals->stats, sizeof( new_vals->stats ) );
    }

    return 0;
}

/**
 * Calculate stats
 *
 * @retval  0 : success
 * @retval -1 : error
 */
int netsnmp_access_interface_entry_calculate_stats( netsnmp_interface_entry* entry )
{
    DEBUG_MSGTL( ( "access:interface", "calculate_stats\n" ) );
    if ( entry->ns_flags & NETSNMP_INTERFACE_FLAGS_CALCULATE_UCAST ) {
        Integer64_subtract( &entry->stats.iall, &entry->stats.imcast,
            &entry->stats.iucast );
    }
    return 0;
}

/**
 * copy interface entry data (after checking for counter wraps)
 *
 * @retval -2 : malloc failed
 * @retval -1 : interfaces not the same
 * @retval  0 : no error
 */
int netsnmp_access_interface_entry_copy( netsnmp_interface_entry* lhs,
    netsnmp_interface_entry* rhs )
{
    DEBUG_MSGTL( ( "access:interface", "copy\n" ) );

    if ( ( NULL == lhs ) || ( NULL == rhs ) || ( NULL == lhs->name ) || ( NULL == rhs->name ) || ( 0 != strncmp( lhs->name, rhs->name, strlen( rhs->name ) ) ) )
        return -1;

    /*
     * update stats
     */
    netsnmp_access_interface_entry_update_stats( lhs, rhs );
    netsnmp_access_interface_entry_calculate_stats( lhs );

    /*
     * update data
     */
    lhs->ns_flags = rhs->ns_flags;
    if ( ( NULL != lhs->descr ) && ( NULL != rhs->descr ) && ( 0 == strcmp( lhs->descr, rhs->descr ) ) )
        ;
    else {
        MEMORY_FREE( lhs->descr );
        if ( rhs->descr ) {
            lhs->descr = strdup( rhs->descr );
            if ( NULL == lhs->descr )
                return -2;
        }
    }
    lhs->type = rhs->type;
    lhs->speed = rhs->speed;
    lhs->speed_high = rhs->speed_high;
    lhs->retransmit_v6 = rhs->retransmit_v6;
    lhs->retransmit_v4 = rhs->retransmit_v4;
    lhs->reachable_time = rhs->reachable_time;
    lhs->mtu = rhs->mtu;
    lhs->lastchange = rhs->lastchange;
    lhs->discontinuity = rhs->discontinuity;
    lhs->reasm_max_v4 = rhs->reasm_max_v4;
    lhs->reasm_max_v6 = rhs->reasm_max_v6;
    lhs->admin_status = rhs->admin_status;
    lhs->oper_status = rhs->oper_status;
    lhs->promiscuous = rhs->promiscuous;
    lhs->connector_present = rhs->connector_present;
    lhs->forwarding_v6 = rhs->forwarding_v6;
    lhs->os_flags = rhs->os_flags;
    if ( lhs->paddr_len == rhs->paddr_len ) {
        if ( rhs->paddr_len )
            memcpy( lhs->paddr, rhs->paddr, rhs->paddr_len );
    } else {
        MEMORY_FREE( lhs->paddr );
        if ( rhs->paddr ) {
            lhs->paddr = ( char* )malloc( rhs->paddr_len );
            if ( NULL == lhs->paddr )
                return -2;
            memcpy( lhs->paddr, rhs->paddr, rhs->paddr_len );
        }
    }
    lhs->paddr_len = rhs->paddr_len;

    return 0;
}

void netsnmp_access_interface_entry_guess_speed( netsnmp_interface_entry* entry )
{
    if ( entry->type == IANAIFTYPE_ETHERNETCSMACD )
        entry->speed = 10000000;
    else if ( entry->type == IANAIFTYPE_SOFTWARELOOPBACK )
        entry->speed = 10000000;
    else if ( entry->type == IANAIFTYPE_ISO88025TOKENRING )
        entry->speed = 4000000;
    else
        entry->speed = 0;
    entry->speed_high = entry->speed / 1000000LL;
}

netsnmp_conf_if_list*
netsnmp_access_interface_entry_overrides_get( const char* name )
{
    netsnmp_conf_if_list* if_ptr;

    Assert_assert( 1 == _access_interface_init );
    if ( NULL == name )
        return NULL;

    for ( if_ptr = conf_list; if_ptr; if_ptr = if_ptr->next )
        if ( !strcmp( if_ptr->name, name ) )
            break;

    return if_ptr;
}

void netsnmp_access_interface_entry_overrides( netsnmp_interface_entry* entry )
{
    netsnmp_conf_if_list* if_ptr;

    Assert_assert( 1 == _access_interface_init );
    if ( NULL == entry )
        return;

    /*
     * enforce mib size limit
     */
    if ( entry->descr && ( strlen( entry->descr ) > 255 ) )
        entry->descr[ 255 ] = 0;

    if_ptr = netsnmp_access_interface_entry_overrides_get( entry->name );
    if ( if_ptr ) {
        entry->type = if_ptr->type;
        if ( if_ptr->speed > 0xffffffff ) {
            entry->speed = 0xffffffff;
        } else {
            entry->speed = if_ptr->speed;
        }
        entry->speed_high = if_ptr->speed / 1000000LL;
    }
}

/**---------------------------------------------------------------------*/
/*
 * interface config token
 */
/**
 */
static void
_parse_interface_config( const char* token, char* cptr )
{
    netsnmp_conf_if_list *if_ptr, *if_new;
    char *name, *type, *speed, *ecp;
    char* st;

    name = strtok_r( cptr, " \t", &st );
    if ( !name ) {
        ReadConfig_configPerror( "Missing NAME parameter" );
        return;
    }
    type = strtok_r( NULL, " \t", &st );
    if ( !type ) {
        ReadConfig_configPerror( "Missing TYPE parameter" );
        return;
    }
    speed = strtok_r( NULL, " \t", &st );
    if ( !speed ) {
        ReadConfig_configPerror( "Missing SPEED parameter" );
        return;
    }
    if_ptr = conf_list;
    while ( if_ptr )
        if ( strcmp( if_ptr->name, name ) )
            if_ptr = if_ptr->next;
        else
            break;
    if ( if_ptr )
        ReadConfig_configPwarn( "Duplicate interface specification" );
    if_new = MEMORY_MALLOC_TYPEDEF( netsnmp_conf_if_list );
    if ( !if_new ) {
        ReadConfig_configPerror( "Out of memory" );
        return;
    }
    if_new->speed = strtoull( speed, &ecp, 0 );
    if ( *ecp ) {
        ReadConfig_configPerror( "Bad SPEED value" );
        free( if_new );
        return;
    }
    if_new->type = strtol( type, &ecp, 0 );
    if ( *ecp || if_new->type < 0 ) {
        ReadConfig_configPerror( "Bad TYPE" );
        free( if_new );
        return;
    }
    if_new->name = strdup( name );
    if ( !if_new->name ) {
        ReadConfig_configPerror( "Out of memory" );
        free( if_new );
        return;
    }
    if_new->next = conf_list;
    conf_list = if_new;
}

static void
_free_interface_config( void )
{
    netsnmp_conf_if_list *if_ptr = conf_list, *if_next;
    while ( if_ptr ) {
        if_next = if_ptr->next;
        free( UTILITIES_REMOVE_CONST( char*, if_ptr->name ) );
        free( if_ptr );
        if_ptr = if_next;
    }
    conf_list = NULL;
}
