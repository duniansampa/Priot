/*
 * Note: this file originally auto-generated by mib2c using
 *       version : 1.17 $ of : mfd-data-access.m2c,v $ 
 *
 * $Id$
 */
/*
 * standard Net-SNMP includes 
 */

/*
 * include our parent header 
 */

#include "ifTable_data_access.h"
#include "AgentReadConfig.h"
#include "Assert.h"
#include "CacheHandler.h"
#include "Debug.h"
#include "Logger.h"
#include "Trap.h"
#include "ifTable_defs.h"
#include "ifTable_interface.h"
#include "siglog/agent/mfd.h"

typedef struct cd_container_s {
    Container_Container* current;
    Container_Container* deleted;
} cd_container;

/*
 * flag so we know not to set row/table last change times
 * during startup.
 */
static int _first_load = 1;

/*
 * Value of interface_fadeout config option
 */
static int fadeout = IFTABLE_REMOVE_MISSING_AFTER;
/*
 * Value of interface_replace_old config option
 */
static int replace_old = 0;

static void
_delete_missing_interface( ifTable_rowreq_ctx* rowreq_ctx,
    Container_Container* container );

/** @ingroup interface 
 * @defgroup data_access data_access: Routines to access data
 *
 * These routines are used to locate the data used to satisfy
 * requests.
 * 
 * @{
 */
/**********************************************************************
 **********************************************************************
 ***
 *** Table ifTable
 ***
 **********************************************************************
 **********************************************************************/
/*
 * IF-MIB::ifTable is subid 2 of interfaces.
 * Its status is Current.
 * OID: .1.3.6.1.2.1.2.2, length: 8
 */

static void
parse_interface_fadeout( const char* token, char* line )
{
    fadeout = atoi( line );
}
static void
parse_interface_replace_old( const char* token, char* line )
{
    if ( strcmp( line, "yes" ) == 0
        || strcmp( line, "y" ) == 0
        || strcmp( line, "true" ) == 0
        || strcmp( line, "1" ) == 0 ) {
        replace_old = 1;
        return;
    }
    if ( strcmp( line, "no" ) == 0
        || strcmp( line, "n" ) == 0
        || strcmp( line, "false" ) == 0
        || strcmp( line, "0" ) == 0 ) {
        replace_old = 0;
        return;
    }
    Logger_log( LOGGER_PRIORITY_ERR, "Invalid value of interface_replace_old parameter: '%s'\n",
        line );
}

/**
 * initialization for ifTable data access
 *
 * This function is called during startup to allow you to
 * allocate any resources you need for the data table.
 *
 * @param ifTable_reg
 *        Pointer to ifTable_registration
 *
 * @retval MFD_SUCCESS : success.
 * @retval MFD_ERROR   : unrecoverable error.
 */
int ifTable_init_data( ifTable_registration* ifTable_reg )
{
    DEBUG_MSGTL( ( "verbose:ifTable:ifTable_init_data", "called\n" ) );

    /*
     * TODO:303:o: Initialize ifTable data.
     */
    AgentReadConfig_priotdRegisterConfigHandler( "interface_fadeout", parse_interface_fadeout, NULL,
        "interface_fadeout seconds" );
    AgentReadConfig_priotdRegisterConfigHandler( "interface_replace_old",
        parse_interface_replace_old, NULL, "interface_replace_old yes|no" );

    return MFD_SUCCESS;
} /* ifTable_init_data */

/**
 * container overview
 *
 */

/**
 * container initialization
 *
 * @param container_ptr_ptr A pointer to a container pointer. If you
 *        create a custom container, use this parameter to return it
 *        to the MFD helper. If set to NULL, the MFD helper will
 *        allocate a container for you.
 * @param  cache A pointer to a cache structure. You can set the timeout
 *         and other cache flags using this pointer.
 *
 *  This function is called at startup to allow you to customize certain
 *  aspects of the access method. For the most part, it is for advanced
 *  users. The default code should suffice for most cases. If no custom
 *  container is allocated, the MFD code will create one for your.
 *
 *  This is also the place to set up cache behavior. The default, to
 *  simply set the cache timeout, will work well with the default
 *  container. If you are using a custom container, you may want to
 *  look at the cache helper documentation to see if there are any
 *  flags you want to set.
 *
 * @remark
 *  This would also be a good place to do any initialization needed
 *  for you data source. For example, opening a connection to another
 *  process that will supply the data, opening a database, etc.
 */
void ifTable_container_init( Container_Container** container_ptr_ptr,
    Cache* cache )
{
    DEBUG_MSGTL( ( "verbose:ifTable:ifTable_container_init", "called\n" ) );

    if ( NULL == container_ptr_ptr ) {
        Logger_log( LOGGER_PRIORITY_ERR,
            "bad container param to ifTable_container_init\n" );
        return;
    }

    /*
     * For advanced users, you can use a custom container. If you
     * do not create one, one will be created for you.
     */
    *container_ptr_ptr = NULL;

    if ( NULL == cache ) {
        Logger_log( LOGGER_PRIORITY_ERR, "bad cache param to ifTable_container_init\n" );
        return;
    }

    /*
     * TODO:345:A: Set up ifTable cache properties.
     *
     * Also for advanced users, you can set parameters for the
     * cache. Do not change the magic pointer, as it is used
     * by the MFD helper. To completely disable caching, set
     * cache->enabled to 0.
     */
    /*
     * since we set AUTO_RELOAD below, this timer controls how
     * often the cache is reloaded. A 10 Mbps stream can wrap if*Octets in ~57 minutes.
     * At 100 Mbps it is ~5 minutes, and at 1 Gbps, ~34 seconds.
     */
    cache->timeout = IFTABLE_CACHE_TIMEOUT; /* seconds */

    /*
     * don't release resources
     */
    cache->flags |= ( CacheOperation_DONT_AUTO_RELEASE | CacheOperation_DONT_FREE_EXPIRED
        | CacheOperation_DONT_FREE_BEFORE_LOAD | CacheOperation_PRELOAD | CacheOperation_AUTO_RELOAD | CacheOperation_DONT_INVALIDATE_ON_SET );
} /* ifTable_container_init */

void send_linkUpDownNotifications( oid* notification_oid, size_t notification_oid_len, int if_index, int if_admin_status, int if_oper_status )
{
    /*
     * In the notification, we have to assign our notification OID to
     * the snmpTrapOID.0 object. Here is it's definition. 
     */
    oid objid_snmptrap[] = { 1, 3, 6, 1, 6, 3, 1, 1, 4, 1, 0 };
    size_t objid_snmptrap_len = ASN01_OID_LENGTH( objid_snmptrap );

    /*
     * define the OIDs for the varbinds we're going to include
     *  with the notification -
     * IF-MIB::ifIndex,
     * IF-MIB::ifAdminStatus, and
     * IF-MIB::ifOperStatus
     */
    oid if_index_oid[] = { 1, 3, 6, 1, 2, 1, 2, 2, 1, 1, 0 };
    size_t if_index_oid_len = ASN01_OID_LENGTH( if_index_oid );
    oid if_admin_status_oid[] = { 1, 3, 6, 1, 2, 1, 2, 2, 1, 7, 0 };
    size_t if_admin_status_oid_len = ASN01_OID_LENGTH( if_admin_status_oid );
    oid if_oper_status_oid[] = { 1, 3, 6, 1, 2, 1, 2, 2, 1, 8, 0 };
    size_t if_oper_status_oid_len = ASN01_OID_LENGTH( if_oper_status_oid );

    /*
     * here is where we store the variables to be sent in the trap 
     */
    Types_VariableList* notification_vars = NULL;

    DEBUG_MSGTL( ( "rsys:linkUpDownNotifications", "defining the trap\n" ) );

    /*
     * update the instance for each variable to be sent in the trap
     */
    if_index_oid[ 10 ] = if_index;
    if_admin_status_oid[ 10 ] = if_index;
    if_oper_status_oid[ 10 ] = if_index;

    /*
     * add in the trap definition object 
     */
    Api_varlistAddVariable( &notification_vars,
        /*
                               * the snmpTrapOID.0 variable 
                               */
        objid_snmptrap, objid_snmptrap_len,
        /*
                               * value type is an OID 
                               */
        ASN01_OBJECT_ID,
        /*
                               * value contents is our notification OID 
                               */
        ( u_char* )notification_oid,
        /*
                               * size in bytes = oid length * sizeof(oid) 
                               */
        notification_oid_len * sizeof( oid ) );

    /*
     * add in the additional objects defined as part of the trap
     */
    Api_varlistAddVariable( &notification_vars,
        if_index_oid, if_index_oid_len,
        ASN01_INTEGER,
        ( u_char* )&if_index,
        sizeof( if_index ) );

    /*
     * if we want to insert additional objects, we do it here 
     */
    Api_varlistAddVariable( &notification_vars,
        if_admin_status_oid, if_admin_status_oid_len,
        ASN01_INTEGER,
        ( u_char* )&if_admin_status,
        sizeof( if_admin_status ) );

    Api_varlistAddVariable( &notification_vars,
        if_oper_status_oid, if_oper_status_oid_len,
        ASN01_INTEGER,
        ( u_char* )&if_oper_status,
        sizeof( if_oper_status ) );

    /*
     * send the trap out.  This will send it to all registered
     * receivers (see the "SETTING UP TRAP AND/OR INFORM DESTINATIONS"
     * section of the snmpd.conf manual page. 
     */
    DEBUG_MSGTL( ( "rsys:linkUpDownNotifications", "sending the trap\n" ) );
    Trap_sendV2trap( notification_vars );

    /*
     * free the created notification variable list 
     */
    DEBUG_MSGTL( ( "rsys:linkUpDownNotifications", "cleaning up\n" ) );
    Api_freeVarbind( notification_vars );
}

/**
 * check entry for update
 *
 */
static void
_check_interface_entry_for_updates( ifTable_rowreq_ctx* rowreq_ctx,
    cd_container* cdc )
{
    char oper_changed = 0;
    int lastchanged = rowreq_ctx->data.ifLastChange;
    Container_Container* ifcontainer = cdc->current;

    /*
     * check for matching entry. We can do this directly, since
     * both containers use the same index.
     */
    netsnmp_interface_entry* ifentry = ( netsnmp_interface_entry* )CONTAINER_FIND( ifcontainer, rowreq_ctx );

    if ( NULL == ifentry ) {
        /*
         * if this is the first time we detected that this interface is
         * missing, set admin/oper status down, and set last change.
         *
         * yyy-rks: when, if ever, would we consider an entry
         * deleted (and thus need to update ifTableLastChanged)?
         */
        if ( !rowreq_ctx->known_missing ) {
            rowreq_ctx->known_missing = 1;
            DEBUG_MSGTL( ( "ifTable:access", "updating missing entry %s\n", rowreq_ctx->data.ifName ) );
            rowreq_ctx->data.ifAdminStatus = IFADMINSTATUS_DOWN;
            rowreq_ctx->data.ifOperStatus = IFOPERSTATUS_DOWN;
            oper_changed = 1;
        }
        if ( rowreq_ctx->known_missing ) {
            time_t now = Agent_getAgentUptime();
            u_long diff = ( now - rowreq_ctx->data.ifLastChange ) / 100;
            DEBUG_MSGTL( ( "verbose:ifTable:access", "missing entry for %ld seconds\n", diff ) );
            if ( diff >= fadeout ) {
                DEBUG_MSGTL( ( "ifTable:access", "marking missing entry %s for "
                                                 "removal after %d seconds\n",
                    rowreq_ctx->data.ifName,
                    fadeout ) );
                if ( NULL == cdc->deleted )
                    cdc->deleted = Container_find( "ifTable_deleted:linked_list" );
                if ( NULL == cdc->deleted )
                    Logger_log( LOGGER_PRIORITY_ERR, "couldn't create container for deleted interface\n" );
                else {
                    CONTAINER_INSERT( cdc->deleted, rowreq_ctx );
                }
            }
        }
    } else {
        DEBUG_MSGTL( ( "ifTable:access", "updating existing entry %s\n",
            rowreq_ctx->data.ifName ) );

        {
            int rc = strcmp( rowreq_ctx->data.ifName,
                ifentry->name );
            if ( rc != 0 ) {
                LOGGER_LOGONCE( ( LOGGER_PRIORITY_ERR, "Name of an interface changed. Such "
                                                       "interfaces will keep its old name in IF-MIB.\n" ) );
                DEBUG_MSGTL( ( "ifTable:access", "interface %s changed name to %s, ignoring\n",
                    rowreq_ctx->data.ifName, ifentry->name ) );
            }
        }
        /*
         * if the interface was missing, but came back, clear the
         * missing flag and set the discontinuity time. (if an os keeps
         * persistent counters, tough cookies. We'll cross that 
         * bridge if we come to it).
         */
        if ( rowreq_ctx->known_missing ) {
            rowreq_ctx->known_missing = 0;
            rowreq_ctx->data.ifCounterDiscontinuityTime = Agent_getAgentUptime();
        }

        /*
         * Check for changes, then update
         */
        if ( ( !( ifentry->ns_flags & NETSNMP_INTERFACE_FLAGS_HAS_LASTCHANGE ) )
            && ( rowreq_ctx->data.ifOperStatus != ifentry->oper_status ) )
            oper_changed = 1;
        netsnmp_access_interface_entry_copy( rowreq_ctx->data.ifentry,
            ifentry );

        /*
         * remove entry from temporary ifcontainer
         */
        CONTAINER_REMOVE( ifcontainer, ifentry );
        netsnmp_access_interface_entry_free( ifentry );
    }

    /*
     * if ifOperStatus changed, update ifLastChange
     */
    if ( oper_changed ) {
        rowreq_ctx->data.ifLastChange = Agent_getAgentUptime();
        if ( rowreq_ctx->data.ifLinkUpDownTrapEnable == 1 ) {
            if ( rowreq_ctx->data.ifOperStatus == IFOPERSTATUS_UP ) {
                oid notification_oid[] = { 1, 3, 6, 1, 6, 3, 1, 1, 5, 4 };
                send_linkUpDownNotifications( notification_oid, ASN01_OID_LENGTH( notification_oid ),
                    rowreq_ctx->tbl_idx.ifIndex,
                    rowreq_ctx->data.ifAdminStatus,
                    rowreq_ctx->data.ifOperStatus );
            } else if ( rowreq_ctx->data.ifOperStatus == IFOPERSTATUS_DOWN ) {
                oid notification_oid[] = { 1, 3, 6, 1, 6, 3, 1, 1, 5, 3 };
                send_linkUpDownNotifications( notification_oid, ASN01_OID_LENGTH( notification_oid ),
                    rowreq_ctx->tbl_idx.ifIndex,
                    rowreq_ctx->data.ifAdminStatus,
                    rowreq_ctx->data.ifOperStatus );
            }
        }
    }

    else
        rowreq_ctx->data.ifLastChange = lastchanged;
}

/**
 * Remove all old interfaces with the same name as the newly added one.
 */
static void
_check_and_replace_old( netsnmp_interface_entry* ifentry,
    Container_Container* container )
{
    Container_Iterator* it;
    ifTable_rowreq_ctx* rowreq_ctx;
    Container_Container* to_delete;

    to_delete = Container_find( "ifTable_deleted:linked_list" );
    if ( NULL == to_delete ) {
        Logger_log( LOGGER_PRIORITY_ERR, "couldn't create container for deleted interface\n" );
        return;
    }

    it = CONTAINER_ITERATOR( container );
    for ( rowreq_ctx = ( ifTable_rowreq_ctx* )CONTAINER_ITERATOR_FIRST( it ); rowreq_ctx; rowreq_ctx = ( ifTable_rowreq_ctx* )CONTAINER_ITERATOR_NEXT( it ) ) {
        if ( strcmp( ifentry->name, rowreq_ctx->data.ifentry->name ) == 0 ) {
            DEBUG_MSGTL( ( "ifTable:access",
                "removing interface %ld due to new %s\n",
                ( long )rowreq_ctx->data.ifentry->index, ifentry->name ) );
            CONTAINER_INSERT( to_delete, rowreq_ctx );
        }
    }
    CONTAINER_ITERATOR_RELEASE( it );

    CONTAINER_FOR_EACH( to_delete,
        ( Container_FuncObjFunc* )_delete_missing_interface,
        container );
    CONTAINER_FREE( to_delete );
}

/**
 * add new entry
 */
static void
_add_new_interface( netsnmp_interface_entry* ifentry,
    Container_Container* container )
{
    ifTable_rowreq_ctx* rowreq_ctx;

    DEBUG_MSGTL( ( "ifTable:access", "creating new entry\n" ) );

    /*
     * allocate an row context and set the index(es), then add it to
     * the container and set ifTableLastChanged.
     */
    rowreq_ctx = ifTable_allocate_rowreq_ctx( ifentry );
    if ( ( NULL != rowreq_ctx ) && ( MFD_SUCCESS == ifTable_indexes_set( rowreq_ctx, ifentry->index ) ) ) {
        if ( replace_old )
            _check_and_replace_old( ifentry, container );

        CONTAINER_INSERT( container, rowreq_ctx );
        if ( 0 == _first_load ) {
            rowreq_ctx->data.ifLastChange = Agent_getAgentUptime();
            ifTable_lastChange_set( rowreq_ctx->data.ifLastChange );
        }

    } else {
        if ( rowreq_ctx ) {
            Logger_log( LOGGER_PRIORITY_ERR, "error setting index while loading "
                                             "ifTable cache.\n" );
            ifTable_release_rowreq_ctx( rowreq_ctx );
        } else {
            Logger_log( LOGGER_PRIORITY_ERR, "memory allocation failed while loading "
                                             "ifTable cache.\n" );
            netsnmp_access_interface_entry_free( ifentry );
        }
    }
}

/**
 * add new entry
 */
static void
_delete_missing_interface( ifTable_rowreq_ctx* rowreq_ctx,
    Container_Container* container )
{
    DEBUG_MSGTL( ( "ifTable:access", "removing missing entry %s\n",
        rowreq_ctx->data.ifName ) );

    CONTAINER_REMOVE( container, rowreq_ctx );

    ifTable_release_rowreq_ctx( rowreq_ctx );
}

/**
 * container shutdown
 *
 * @param container_ptr A pointer to the container.
 *
 *  This function is called at shutdown to allow you to customize certain
 *  aspects of the access method. For the most part, it is for advanced
 *  users. The default code should suffice for most cases.
 *
 *  This function is called before ifTable_container_free().
 *
 * @remark
 *  This would also be a good place to do any cleanup needed
 *  for you data source. For example, closing a connection to another
 *  process that supplied the data, closing a database, etc.
 */
void ifTable_container_shutdown( Container_Container* container_ptr )
{
    DEBUG_MSGTL( ( "verbose:ifTable:ifTable_container_shutdown", "called\n" ) );

    if ( NULL == container_ptr ) {
        Logger_log( LOGGER_PRIORITY_ERR, "bad params to ifTable_container_shutdown\n" );
        return;
    }

} /* ifTable_container_shutdown */

/**
 * load initial data
 *
 * TODO:350:M: Implement ifTable data load
 * This function will also be called by the cache helper to load
 * the container again (after the container free function has been
 * called to free the previous contents).
 *
 * @param container container to which items should be inserted
 *
 * @retval MFD_SUCCESS              : success.
 * @retval MFD_RESOURCE_UNAVAILABLE : Can't access data source
 * @retval MFD_ERROR                : other error.
 *
 *  This function is called to load the index(es) (and data, optionally)
 *  for the every row in the data set.
 *
 * @remark
 *  While loading the data, the only important thing is the indexes.
 *  If access to your data is cheap/fast (e.g. you have a pointer to a
 *  structure in memory), it would make sense to update the data here.
 *  If, however, the accessing the data invovles more work (e.g. parsing
 *  some other existing data, or peforming calculations to derive the data),
 *  then you can limit yourself to setting the indexes and saving any
 *  information you will need later. Then use the saved information in
 *  ifTable_row_prep() for populating data.
 *
 * @note
 *  If you need consistency between rows (like you want statistics
 *  for each row to be from the same time frame), you should set all
 *  data here.
 *
 */
int ifTable_container_load( Container_Container* container )
{
    cd_container cdc;

    DEBUG_MSGTL( ( "verbose:ifTable:ifTable_container_load", "called\n" ) );

    /*
     * TODO:351:M: |-> Load/update data in the ifTable container.
     * loop over your ifTable data, allocate a rowreq context,
     * set the index(es) [and data, optionally] and insert into
     * the container.
     */
    /*
     * ifTable gets its data from the netsnmp_interface API.
     */
    cdc.current = netsnmp_access_interface_container_load( NULL,
        NETSNMP_ACCESS_INTERFACE_INIT_NOFLAGS );
    if ( NULL == cdc.current )
        return MFD_RESOURCE_UNAVAILABLE; /* msg already logged */

    cdc.deleted = NULL; /* created as needed */

    /*
     * we just got a fresh copy of interface data. compare it to
     * what we've already got, and make any adjustements...
     */
    CONTAINER_FOR_EACH( container, ( Container_FuncObjFunc* )
                                       _check_interface_entry_for_updates,
        &cdc );

    /*
     * now remove any missing interfaces
     */
    if ( NULL != cdc.deleted ) {
        CONTAINER_FOR_EACH( cdc.deleted,
            ( Container_FuncObjFunc* )_delete_missing_interface,
            container );
        CONTAINER_FREE( cdc.deleted );
    }

    /*
     * now add any new interfaces
     */
    CONTAINER_FOR_EACH( cdc.current,
        ( Container_FuncObjFunc* )_add_new_interface,
        container );

    /*
     * free the container. we've either claimed each ifentry, or released it,
     * so the dal function doesn't need to clear the container.
     */
    netsnmp_access_interface_container_free( cdc.current,
        NETSNMP_ACCESS_INTERFACE_FREE_DONT_CLEAR );

    DEBUG_MSGT( ( "verbose:ifTable:ifTable_cache_load",
        "%lu records\n", ( unsigned long )CONTAINER_SIZE( container ) ) );

    if ( _first_load )
        _first_load = 0;

    return MFD_SUCCESS;
} /* ifTable_container_load */

/**
 * container clean up
 *
 * @param container container with all current items
 *
 *  This optional callback is called prior to all
 *  item's being removed from the container. If you
 *  need to do any processing before that, do it here.
 *
 * @note
 *  The MFD helper will take care of releasing all the row contexts.
 *
 */
void ifTable_container_free( Container_Container* container )
{
    DEBUG_MSGTL( ( "verbose:ifTable:ifTable_container_free", "called\n" ) );

    /*
     * TODO:380:M: Free ifTable container data.
     */
} /* ifTable_container_free */

/**
 * prepare row for processing.
 *
 *  When the agent has located the row for a request, this function is
 *  called to prepare the row for processing. If you fully populated
 *  the data context during the index setup phase, you may not need to
 *  do anything.
 *
 * @param rowreq_ctx pointer to a context.
 *
 * @retval MFD_SUCCESS     : success.
 * @retval MFD_ERROR       : other error.
 */
int ifTable_row_prep( ifTable_rowreq_ctx* rowreq_ctx )
{
    DEBUG_MSGTL( ( "verbose:ifTable:ifTable_row_prep", "called\n" ) );

    Assert_assert( NULL != rowreq_ctx );

    /*
     * TODO:390:o: Prepare row for request.
     * If populating row data was delayed, this is the place to
     * fill in the row for this request.
     */

    return MFD_SUCCESS;
} /* ifTable_row_prep */

/** @} */