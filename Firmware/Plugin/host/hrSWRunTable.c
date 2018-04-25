/*
 * Note: this file originally auto-generated by mib2c using
 *  : mib2c.container.conf,v 1.8 2006/07/26 15:58:26 dts12 Exp $
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright (C) 2007 Apple, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include "hrSWRunTable.h"
#include "Client.h"
#include "Debug.h"
#include "Logger.h"
#include "Table.h"
#include "TableContainer.h"
#include "Vars.h"
#include "data_access/swrun.h"
#include "siglog/data_access/swrun.h"

#define MYTABLE "hrSWRunTable"

static TableRegistrationInfo* table_info;

/** Initializes the hrSWRunTable module */
void init_hrSWRunTable( void )
{
    /*
     * here we initialize all the tables we're planning on supporting 
     */
    initialize_table_hrSWRunTable();
}

void shutdown_hrSWRunTable( void )
{
    if ( table_info ) {
        Table_registrationInfoFree( table_info );
        table_info = NULL;
    }
}

oid hrSWRunTable_oid[] = { 1, 3, 6, 1, 2, 1, 25, 4, 2 };
size_t hrSWRunTable_oid_len = ASN01_OID_LENGTH( hrSWRunTable_oid );

/** Initialize the hrSWRunTable table by defining its contents and how it's structured */
void initialize_table_hrSWRunTable( void )
{
    HandlerRegistration* reg;
    MibHandler* handler = NULL;

#define SWRUN_ACCESS_LEVEL HANDLER_CAN_RONLY

    reg = AgentHandler_createHandlerRegistration( MYTABLE,
        hrSWRunTable_handler,
        hrSWRunTable_oid,
        hrSWRunTable_oid_len,
        SWRUN_ACCESS_LEVEL );
    if ( NULL == reg ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error creating handler registration for " MYTABLE "\n" );
        goto bail;
    }
    reg->modes |= HANDLER_CAN_NOT_CREATE;

    table_info = TOOLS_MALLOC_TYPEDEF( TableRegistrationInfo );
    if ( NULL == table_info ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error allocating table registration for " MYTABLE "\n" );
        goto bail;
    }

    Table_helperAddIndexes( table_info, ASN01_INTEGER, /* index: hrSWRunIndex */
        0 );
    table_info->min_column = COLUMN_HRSWRUNINDEX;
    table_info->max_column = COLUMN_HRSWRUNSTATUS;

    /*************************************************
     *
     * inject container_table helper
     */
    handler = TableContainer_handlerGet( table_info, netsnmp_swrun_container(),
        TABLE_CONTAINER_KEY_NETSNMP_INDEX );
    if ( NULL == handler ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error allocating table registration for " MYTABLE "\n" );
        goto bail;
    }
    if ( ErrorCode_SUCCESS != AgentHandler_injectHandler( reg, handler ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error injecting container_table handler for " MYTABLE "\n" );
        goto bail;
    }
    handler = NULL; /* reg has it, will reuse below */

    /*************************************************
     *
     * inject cache helper
     */
    handler = CacheHandler_handlerGet( netsnmp_swrun_cache() );
    if ( NULL == handler ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error creating cache handler for " MYTABLE "\n" );
        goto bail;
    }

    if ( ErrorCode_SUCCESS != AgentHandler_injectHandler( reg, handler ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error injecting cache handler for " MYTABLE "\n" );
        goto bail;
    }
    handler = NULL; /* reg has it*/

    if ( ErrorCode_SUCCESS != Table_registerTable( reg, table_info ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error registering table handler for " MYTABLE "\n" );
        reg = NULL; /* it was freed inside Table_registerTable */
        goto bail;
    }

    return; /* ok */

bail: /* not ok */

    if ( handler )
        AgentHandler_handlerFree( handler );

    if ( table_info )
        Table_registrationInfoFree( table_info );

    if ( reg )
        AgentHandler_handlerRegistrationFree( reg );
}

/** handles requests for the hrSWRunTable table */
int hrSWRunTable_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    RequestInfo* request;
    TableRequestInfo* table_info;
    netsnmp_swrun_entry* table_entry;

    switch ( reqinfo->mode ) {
    /*
         * Read-support (also covers GetNext requests)
         */
    case MODE_GET:
        for ( request = requests; request; request = request->next ) {
            if ( request->processed )
                continue;
            table_entry = ( netsnmp_swrun_entry* )
                TableContainer_extractContext( request );
            table_info = Table_extractTableInfo( request );
            if ( ( NULL == table_entry ) || ( NULL == table_info ) ) {
                Logger_log( LOGGER_PRIORITY_ERR, "could not extract table entry or info for " MYTABLE "\n" );
                Client_setVarTypedValue( request->requestvb,
                    PRIOT_ERR_GENERR, NULL, 0 );
                continue;
            }

            switch ( table_info->colnum ) {
            case COLUMN_HRSWRUNINDEX:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    table_entry->hrSWRunIndex );
                break;
            case COLUMN_HRSWRUNNAME:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )table_entry->hrSWRunName,
                    table_entry->hrSWRunName_len );
                break;
            case COLUMN_HRSWRUNID:
                Client_setVarTypedValue( request->requestvb, ASN01_OBJECT_ID,

                    ( u_char* )&vars_nullOid, vars_nullOidLen );
                break;
            case COLUMN_HRSWRUNPATH:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )table_entry->hrSWRunPath,
                    table_entry->hrSWRunPath_len );
                break;
            case COLUMN_HRSWRUNPARAMETERS:
                Client_setVarTypedValue( request->requestvb, ASN01_OCTET_STR,
                    ( u_char* )table_entry->hrSWRunParameters,
                    table_entry->hrSWRunParameters_len );
                break;
            case COLUMN_HRSWRUNTYPE:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    table_entry->hrSWRunType );
                break;
            case COLUMN_HRSWRUNSTATUS:
                Client_setVarTypedInteger( request->requestvb, ASN01_INTEGER,
                    table_entry->hrSWRunStatus );
                break;
            default:
                /*
                 * An unsupported/unreadable column (if applicable) 
                 */
                Client_setVarTypedValue( request->requestvb,
                    PRIOT_NOSUCHOBJECT, NULL, 0 );
            }
        }
        break;
    }
    return PRIOT_ERR_NOERROR;
}