/*
 * Note: this file originally auto-generated by mib2c using
 *        : mib2c.scalar.conf,v 1.8 2004/10/14 12:57:34 dts12 Exp $
 */

#include "siglog/data_access/ip_scalars.h"
#include "AgentHandler.h"
#include "Client.h"
#include "DataList.h"
#include "Debug.h"
#include "Logger.h"
#include "Scalar.h"
#include "ip_scalars.h"

static int
handle_ipForwarding( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests );

static int
handle_ipDefaultTTL( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests );

static int
handle_ipv6IpForwarding( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests );

static int
handle_ipv6IpDefaultHopLimit( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests );

static int ipAddressSpinLockValue;

static int
handle_ipAddressSpinLock( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests );

/** Initializes the ip module */
void init_ip_scalars( void )
{
    static oid ipForwarding_oid[] = { 1, 3, 6, 1, 2, 1, 4, 1 };
    static oid ipDefaultTTL_oid[] = { 1, 3, 6, 1, 2, 1, 4, 2 };
    static oid ipv6IpForwarding_oid[] = { 1, 3, 6, 1, 2, 1, 4, 25 };
    static oid ipv6IpDefaultHopLimit_oid[] = { 1, 3, 6, 1, 2, 1, 4, 26 };
    static oid ipAddressSpinLock_oid[] = { 1, 3, 6, 1, 2, 1, 4, 33 };

    DEBUG_MSGTL( ( "ip_scalar", "Initializing\n" ) );

    Scalar_registerScalar( AgentHandler_createHandlerRegistration( "ipForwarding", handle_ipForwarding,
        ipForwarding_oid,
        ASN01_OID_LENGTH( ipForwarding_oid ),
        HANDLER_CAN_RWRITE ) );

    Scalar_registerScalar( AgentHandler_createHandlerRegistration( "ipDefaultTTL", handle_ipDefaultTTL,
        ipDefaultTTL_oid,
        ASN01_OID_LENGTH( ipDefaultTTL_oid ),
        HANDLER_CAN_RWRITE ) );

    Scalar_registerScalar( AgentHandler_createHandlerRegistration( "ipv6IpForwarding", handle_ipv6IpForwarding,
        ipv6IpForwarding_oid,
        ASN01_OID_LENGTH( ipv6IpForwarding_oid ),
        HANDLER_CAN_RWRITE ) );

    Scalar_registerScalar( AgentHandler_createHandlerRegistration( "ipv6IpDefaultHopLimit", handle_ipv6IpDefaultHopLimit,
        ipv6IpDefaultHopLimit_oid,
        ASN01_OID_LENGTH( ipv6IpDefaultHopLimit_oid ),
        HANDLER_CAN_RWRITE ) );

    Scalar_registerScalar( AgentHandler_createHandlerRegistration( "ipAddressSpinLock", handle_ipAddressSpinLock,
        ipAddressSpinLock_oid,
        ASN01_OID_LENGTH( ipAddressSpinLock_oid ),
        HANDLER_CAN_RWRITE ) );

    /* Initialize spin lock with random value */
    ipAddressSpinLockValue = ( int )random();
}

static int
handle_ipForwarding( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    int rc;
    u_long value;

    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch ( reqinfo->mode ) {

    case MODE_GET:
        rc = netsnmp_arch_ip_scalars_ipForwarding_get( &value );
        if ( rc != 0 ) {
            Agent_setRequestError( reqinfo, requests,
                PRIOT_NOSUCHINSTANCE );
        } else {
            value = value ? 1 : 2;
            Client_setVarTypedValue( requests->requestvb, ASN01_INTEGER,
                ( u_char* )&value, sizeof( value ) );
        }
        break;

    /*
         * SET REQUEST
         *
         * multiple states in the transaction.  See:
         * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
         */
    case MODE_SET_RESERVE1:
        break;

    case MODE_SET_RESERVE2:
        /*
             * store old info for undo later
             */
        rc = netsnmp_arch_ip_scalars_ipForwarding_get( &value );
        if ( rc < 0 ) {
            Agent_setRequestError( reqinfo, requests,
                PRIOT_ERR_NOCREATION );
        } else {
            u_long* value_save;
            value_save = ( u_long* )Tools_memdup( &value, sizeof( value ) );
            if ( NULL == value_save )
                Agent_setRequestError( reqinfo, requests, PRIOT_ERR_RESOURCEUNAVAILABLE );
            else
                AgentHandler_requestAddListData( requests,
                    DataList_create( "ipfw", value_save,
                                                     free ) );
        }
        break;

    case MODE_SET_FREE:
        /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
        break;

    case MODE_SET_ACTION:
        /* XXX: perform the value change here */
        value = *( requests->requestvb->val.integer );
        rc = netsnmp_arch_ip_scalars_ipForwarding_set( value );
        if ( 0 != rc ) {
            Agent_setRequestError( reqinfo, requests, rc );
        }
        break;

    case MODE_SET_COMMIT:
        break;

    case MODE_SET_UNDO:
        value = *( ( u_long* )AgentHandler_requestGetListData( requests,
            "ipfw" ) );
        rc = netsnmp_arch_ip_scalars_ipForwarding_set( value );
        if ( 0 != rc ) {
            Agent_setRequestError( reqinfo, requests, PRIOT_ERR_UNDOFAILED );
        }
        break;

    default:
        /* we should never get here, so this is a really bad error */
        Logger_log( LOGGER_PRIORITY_ERR, "unknown mode (%d) in handle_ipForwarding\n", reqinfo->mode );
        return PRIOT_ERR_GENERR;
    }

    return PRIOT_ERR_NOERROR;
}

static int
handle_ipDefaultTTL( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    int rc;
    u_long value;

    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch ( reqinfo->mode ) {

    case MODE_GET:
        rc = netsnmp_arch_ip_scalars_ipDefaultTTL_get( &value );
        if ( rc != 0 ) {
            Agent_setRequestError( reqinfo, requests,
                PRIOT_NOSUCHINSTANCE );
        } else {
            Client_setVarTypedValue( requests->requestvb, ASN01_INTEGER,
                ( u_char* )&value, sizeof( value ) );
        }
        break;

    /*
         * SET REQUEST
         *
         * multiple states in the transaction.  See:
         * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
         */
    case MODE_SET_RESERVE1:
        break;

    case MODE_SET_RESERVE2:
        /*
             * store old info for undo later
             */
        rc = netsnmp_arch_ip_scalars_ipDefaultTTL_get( &value );
        if ( rc < 0 ) {
            Agent_setRequestError( reqinfo, requests,
                PRIOT_ERR_NOCREATION );
        } else {
            u_long* value_save;
            value_save = ( u_long* )Tools_memdup( &value, sizeof( value ) );
            if ( NULL == value_save )
                Agent_setRequestError( reqinfo, requests, PRIOT_ERR_RESOURCEUNAVAILABLE );
            else
                AgentHandler_requestAddListData( requests,
                    DataList_create( "ipttl", value_save,
                                                     free ) );
        }
        break;

    case MODE_SET_FREE:
        /* XXX: free resources allocated in RESERVE1 and/or
               RESERVE2.  Something failed somewhere, and the states
               below won't be called. */
        break;

    case MODE_SET_ACTION:
        /* XXX: perform the value change here */
        value = *( requests->requestvb->val.integer );
        rc = netsnmp_arch_ip_scalars_ipDefaultTTL_set( value );
        if ( 0 != rc ) {
            Agent_setRequestError( reqinfo, requests, rc );
        }
        break;

    case MODE_SET_COMMIT:
        break;

    case MODE_SET_UNDO:
        value = *( ( u_long* )AgentHandler_requestGetListData( requests,
            "ipttl" ) );
        rc = netsnmp_arch_ip_scalars_ipDefaultTTL_set( value );
        if ( 0 != rc ) {
            Agent_setRequestError( reqinfo, requests, PRIOT_ERR_UNDOFAILED );
        }
        break;

    default:
        /* we should never get here, so this is a really bad error */
        Logger_log( LOGGER_PRIORITY_ERR, "unknown mode (%d) in handle_ipDefaultTTL\n", reqinfo->mode );
        return PRIOT_ERR_GENERR;
    }

    return PRIOT_ERR_NOERROR;
}

static int
handle_ipv6IpForwarding( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    int rc;
    u_long value;

    /*
     * We are never called for a GETNEXT if it's registered as a
     * "instance", as it's "magically" handled for us.  
     */

    /*
     * a instance handler also only hands us one request at a time, so
     * we don't need to loop over a list of requests; we'll only get one. 
     */
    switch ( reqinfo->mode ) {

    case MODE_GET:
        rc = netsnmp_arch_ip_scalars_ipv6IpForwarding_get( &value );
        if ( rc != 0 ) {
            Agent_setRequestError( reqinfo, requests,
                PRIOT_NOSUCHINSTANCE );
        } else {
            value = value ? 1 : 2;
            Client_setVarTypedValue( requests->requestvb, ASN01_INTEGER,
                ( u_char* )&value, sizeof( value ) );
        }
        break;

    /*
         * SET REQUEST
         *
         * multiple states in the transaction.  See:
         * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
         */
    case MODE_SET_RESERVE1:
        break;

    case MODE_SET_RESERVE2:
        /*
         * store old info for undo later 
         */
        rc = netsnmp_arch_ip_scalars_ipv6IpForwarding_get( &value );
        if ( rc < 0 ) {
            Agent_setRequestError( reqinfo, requests,
                PRIOT_ERR_NOCREATION );
        } else {
            u_long* value_save;

            value_save = ( u_long* )Tools_memdup( &value, sizeof( value ) );
            if ( NULL == value_save ) {
                Agent_setRequestError( reqinfo, requests,
                    PRIOT_ERR_RESOURCEUNAVAILABLE );
            } else {
                AgentHandler_requestAddListData( requests,
                    DataList_create( "ip6fw", value_save,
                                                     free ) );
            }
        }
        break;

    case MODE_SET_FREE:
        break;

    case MODE_SET_ACTION:
        value = *( requests->requestvb->val.integer );
        rc = netsnmp_arch_ip_scalars_ipv6IpForwarding_set( value );
        if ( 0 != rc ) {
            Agent_setRequestError( reqinfo, requests, rc );
        }
        break;

    case MODE_SET_COMMIT:
        break;

    case MODE_SET_UNDO:
        value = *( ( u_long* )AgentHandler_requestGetListData( requests,
            "ip6fw" ) );
        rc = netsnmp_arch_ip_scalars_ipv6IpForwarding_set( value );
        if ( 0 != rc ) {
            Agent_setRequestError( reqinfo, requests, PRIOT_ERR_UNDOFAILED );
        }
        break;

    default:
        /*
         * we should never get here, so this is a really bad error 
         */
        Logger_log( LOGGER_PRIORITY_ERR, "unknown mode (%d) in handle_ipv6IpForwarding\n",
            reqinfo->mode );
        return PRIOT_ERR_GENERR;
    }

    return PRIOT_ERR_NOERROR;
}

static int
handle_ipAddressSpinLock( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    long value;

    /* We are never called for a GETNEXT if it's registered as a
       "instance", as it's "magically" handled for us.  */

    /* a instance handler also only hands us one request at a time, so
       we don't need to loop over a list of requests; we'll only get one. */

    switch ( reqinfo->mode ) {

    case MODE_GET:
        Client_setVarTypedValue( requests->requestvb, ASN01_INTEGER,
            ( u_char* )&ipAddressSpinLockValue,
            sizeof( ipAddressSpinLockValue ) );
        break;

    /*
         * SET REQUEST
         *
         * multiple states in the transaction.  See:
         * http://www.net-snmp.org/tutorial-5/toolkit/mib_module/set-actions.jpg
         */
    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
        /* just check the value */
        value = *( requests->requestvb->val.integer );
        if ( value != ipAddressSpinLockValue )
            Agent_setRequestError( reqinfo, requests, PRIOT_ERR_INCONSISTENTVALUE );
        break;

    case MODE_SET_FREE:
        break;

    case MODE_SET_ACTION:
        /* perform the final spinlock check and increase its value */
        value = *( requests->requestvb->val.integer );
        if ( value != ipAddressSpinLockValue ) {
            Agent_setRequestError( reqinfo, requests, PRIOT_ERR_INCONSISTENTVALUE );
        } else {
            ipAddressSpinLockValue++;
            /* and check it for overflow */
            if ( ipAddressSpinLockValue > 2147483647 || ipAddressSpinLockValue < 0 )
                ipAddressSpinLockValue = 0;
        }
        break;

    case MODE_SET_COMMIT:
        break;

    case MODE_SET_UNDO:
        break;

    default:
        /* we should never get here, so this is a really bad error */
        Logger_log( LOGGER_PRIORITY_ERR, "unknown mode (%d) in handle_ipAddressSpinLock\n", reqinfo->mode );
        return PRIOT_ERR_GENERR;
    }

    return PRIOT_ERR_NOERROR;
}

static int
handle_ipv6IpDefaultHopLimit( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    u_long value;
    int rc;
    /*
     * We are never called for a GETNEXT if it's registered as a
     * "instance", as it's "magically" handled for us.  
     */

    /*
     * a instance handler also only hands us one request at a time, so
     * we don't need to loop over a list of requests; we'll only get one. 
     */

    switch ( reqinfo->mode ) {

    case MODE_GET:

        rc = netsnmp_arch_ip_scalars_ipv6IpDefaultHopLimit_get( &value );
        if ( rc != 0 ) {
            Agent_setRequestError( reqinfo, requests,
                PRIOT_NOSUCHINSTANCE );
        } else {
            Client_setVarTypedValue( requests->requestvb, ASN01_INTEGER,
                ( u_char* )&value, sizeof( value ) );
        }

        break;

    default:
        /*
         * we should never get here, so this is a really bad error 
         */
        Logger_log( LOGGER_PRIORITY_ERR,
            "unknown mode (%d) in handle_ipv6IpDefaultHopLimit\n",
            reqinfo->mode );
        return PRIOT_ERR_GENERR;
    }

    return PRIOT_ERR_NOERROR;
}
