/*
 * Note: this file originally auto-generated by mib2c using
 *        : mib2c.scalar.conf,v 1.9 2005/01/07 09:37:18 dts12 Exp $
 */

#include "memory.h"
#include "AgentReadConfig.h"
#include "Client.h"
#include "Debug.h"
#include "Logger.h"
#include "Scalar.h"
#include "ScalarGroup.h"
#include "siglog/agent/hardware/memory.h"

#include "PluginSettings.h"

#define DEFAULTMINIMUMSWAP 16000 /* kilobytes */
static int minimum_swap;

/** Initializes the memory module */
void init_memory( void )
{
    const oid memory_oid[] = { 1, 3, 6, 1, 4, 1, 2021, 4 };
    const oid memSwapError_oid[] = { 1, 3, 6, 1, 4, 1, 2021, 4, 100 };
    const oid memSwapErrMsg_oid[] = { 1, 3, 6, 1, 4, 1, 2021, 4, 101 };

    DEBUG_MSGTL( ( "memory", "Initializing\n" ) );

    ScalarGroup_registerScalarGroup(
        AgentHandler_createHandlerRegistration( "memory", handle_memory,
            memory_oid, ASN01_OID_LENGTH( memory_oid ),
            HANDLER_CAN_RONLY ),
        1, 17 );
    Scalar_registerScalar(
        AgentHandler_createHandlerRegistration( "memSwapError", handle_memory,
            memSwapError_oid, ASN01_OID_LENGTH( memSwapError_oid ),
            HANDLER_CAN_RONLY ) );
    Scalar_registerScalar(
        AgentHandler_createHandlerRegistration( "memSwapErrMsg", handle_memory,
            memSwapErrMsg_oid, ASN01_OID_LENGTH( memSwapErrMsg_oid ),
            HANDLER_CAN_RONLY ) );

    AgentReadConfig_priotdRegisterConfigHandler( "swap", memory_parse_config,
        memory_free_config, "min-avail" );
}

void memory_parse_config( const char* token, char* cptr )
{
    minimum_swap = atoi( cptr );
}

void memory_free_config( void )
{
    minimum_swap = DEFAULTMINIMUMSWAP;
}

int handle_memory( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    netsnmp_memory_info* mem_info;
    int val;
    char buf[ 1024 ];

    /*
     * We just need to handle valid GET requests, as invalid instances
     *   are rejected automatically, and (valid) GETNEXT requests are
     *   converted into the appropriate GET request.
     *
     * We also only ever receive one request at a time.
     */
    switch ( reqinfo->mode ) {
    case MODE_GET:
        netsnmp_memory_load();
        switch ( requests->requestvb->name[ reginfo->rootoid_len - 2 ] ) {
        case MEMORY_INDEX:
            val = 0;
            break;
        case MEMORY_ERRNAME:
            sprintf( buf, "swap" );
            Client_setVarTypedValue( requests->requestvb, ASN01_OCTET_STR,
                ( u_char* )buf, strlen( buf ) );
            return PRIOT_ERR_NOERROR;
        case MEMORY_SWAP_TOTAL:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 0 );
            if ( !mem_info )
                goto NOSUCH;
            val = mem_info->size; /* swaptotal */
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_SWAP_AVAIL:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 0 );
            if ( !mem_info )
                goto NOSUCH;
            val = mem_info->free; /* swapfree */
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_REAL_TOTAL:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 0 );
            if ( !mem_info )
                goto NOSUCH;
            val = mem_info->size; /* memtotal */
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_REAL_AVAIL:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_PHYSMEM, 0 );
            if ( !mem_info )
                goto NOSUCH;
            val = mem_info->free; /* memfree */
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_STXT_TOTAL:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_STEXT, 0 );
            if ( !mem_info )
                goto NOSUCH;
            val = mem_info->size;
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_STXT_AVAIL: /* Deprecated */
        case MEMORY_STXT_USED:
            /*
             *   The original MIB description of memAvailSwapTXT
             * was inconsistent with that implied by the name.
             *   Retain the actual behaviour for the (sole)
             * implementation of this object, but deprecate it in
             * favour of a more consistently named replacement object.
             */
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_STEXT, 0 );
            if ( !mem_info )
                goto NOSUCH;
            val = ( mem_info->size - mem_info->free );
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_RTXT_TOTAL:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_RTEXT, 0 );
            if ( !mem_info )
                goto NOSUCH;
            val = mem_info->size;
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_RTXT_AVAIL: /* Deprecated */
        case MEMORY_RTXT_USED:
            /*
             *   The original MIB description of memAvailRealTXT
             * was inconsistent with that implied by the name.
             *   Retain the actual behaviour for the (sole)
             * implementation of this object, but deprecate it in
             * favour of a more consistently named replacement object.
             */
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_RTEXT, 0 );
            if ( !mem_info )
                goto NOSUCH;
            val = ( mem_info->size - mem_info->free );
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_FREE:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_VIRTMEM, 0 );
            if ( !mem_info )
                goto NOSUCH;
            val = mem_info->free; /* memfree + swapfree */
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_SWAP_MIN:
            val = minimum_swap;
            break;
        case MEMORY_SHARED:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SHARED, 0 );
            if ( !mem_info )
                goto NOSUCH;
            val = mem_info->size; /* memshared */
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_BUFFER:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_MBUF, 0 );
            if ( !mem_info || mem_info->size == -1 )
                goto NOSUCH;
            val = ( mem_info->size - mem_info->free ); /* buffers */
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_CACHED:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_CACHED, 0 );
            if ( !mem_info || mem_info->size == -1 )
                goto NOSUCH;
            val = ( mem_info->size - mem_info->free ); /* cached */
            val *= ( mem_info->units / 1024 );
            break;
        case MEMORY_SWAP_ERROR:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 0 );
            if ( !mem_info )
                goto NOSUCH;
            val = ( ( mem_info->units / 1024 ) * mem_info->free > minimum_swap ) ? 0 : 1;
            break;
        case MEMORY_SWAP_ERRMSG:
            mem_info = netsnmp_memory_get_byIdx( NETSNMP_MEM_TYPE_SWAP, 0 );
            if ( !mem_info )
                goto NOSUCH;
            if ( ( mem_info->units / 1024 ) * mem_info->free > minimum_swap )
                buf[ 0 ] = 0;
            else
                sprintf( buf, "Running out of swap space (%ld)", mem_info->free );
            Client_setVarTypedValue( requests->requestvb, ASN01_OCTET_STR,
                ( u_char* )buf, strlen( buf ) );
            return PRIOT_ERR_NOERROR;
        default:
            Logger_log( LOGGER_PRIORITY_ERR,
                "unknown object (%"
                "l"
                "u) in handle_memory\n",
                requests->requestvb->name[ reginfo->rootoid_len - 2 ] );
        NOSUCH:
            Agent_setRequestError( reqinfo, requests, PRIOT_NOSUCHOBJECT );
            return PRIOT_ERR_NOERROR;
        }
        /*
         * All non-integer objects (and errors) have already been
         * processed.  So return the integer value.
         */
        Client_setVarTypedValue( requests->requestvb, ASN01_INTEGER,
            ( u_char* )&val, sizeof( val ) );
        break;

    default:
        /*
         * we should never get here, so this is a really bad error 
         */
        Logger_log( LOGGER_PRIORITY_ERR, "unknown mode (%d) in handle_memory\n",
            reqinfo->mode );
        return PRIOT_ERR_GENERR;
    }

    return PRIOT_ERR_NOERROR;
}
