

#include "mteScalars.h"
#include "Client.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "Scalar.h"
#include "ScalarGroup.h"
#include "mteTrigger.h"

/** Initializes the mteScalars module */
void init_mteScalars( void )
{
    static oid mteResource_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 1 };
    static oid mteTriggerFail_oid[] = { 1, 3, 6, 1, 2, 1, 88, 1, 2, 1 };

    DEBUG_MSGTL( ( "mteScalars", "Initializing\n" ) );

    ScalarGroup_registerScalarGroup(
        AgentHandler_createHandlerRegistration( "mteResource", handle_mteResourceGroup,
            mteResource_oid, ASN01_OID_LENGTH( mteResource_oid ),
            HANDLER_CAN_RONLY ),
        MTE_RESOURCE_SAMPLE_MINFREQ, MTE_RESOURCE_SAMPLE_LACKS );

    Scalar_registerScalar(
        AgentHandler_createHandlerRegistration( "mteTriggerFailures",
            handle_mteTriggerFailures,
            mteTriggerFail_oid, ASN01_OID_LENGTH( mteTriggerFail_oid ),
            HANDLER_CAN_RONLY ) );
}

int handle_mteResourceGroup( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    oid obj;
    long value = 0;

    switch ( reqinfo->mode ) {
    case MODE_GET:
        obj = requests->requestvb->name[ requests->requestvb->nameLength - 2 ];
        switch ( obj ) {
        case MTE_RESOURCE_SAMPLE_MINFREQ:
            value = 1; /* Fixed minimum sample frequency */
            Client_setVarTypedValue( requests->requestvb, ASN01_INTEGER,
                ( u_char* )&value, sizeof( value ) );
            break;

        case MTE_RESOURCE_SAMPLE_MAX_INST:
            value = 0; /* No fixed maximum */
            Client_setVarTypedValue( requests->requestvb, ASN01_UNSIGNED,
                ( u_char* )&value, sizeof( value ) );
            break;

        case MTE_RESOURCE_SAMPLE_INSTANCES:
            value = mteTrigger_getNumEntries( 0 );

            Client_setVarTypedValue( requests->requestvb, ASN01_GAUGE,
                ( u_char* )&value, sizeof( value ) );
            break;

        case MTE_RESOURCE_SAMPLE_HIGH:
            value = mteTrigger_getNumEntries( 1 );

            Client_setVarTypedValue( requests->requestvb, ASN01_GAUGE,
                ( u_char* )&value, sizeof( value ) );
            break;

        case MTE_RESOURCE_SAMPLE_LACKS:
            value = 0; /* mteResSampleInstMax not used */
            Client_setVarTypedValue( requests->requestvb, ASN01_COUNTER,
                ( u_char* )&value, sizeof( value ) );
            break;

        default:
            Logger_log( LOGGER_PRIORITY_ERR,
                "unknown object (%d) in handle_mteResourceGroup\n", ( int )obj );
            return PRIOT_ERR_GENERR;
        }
        break;

    default:
        /*
         * Although mteResourceSampleMinimum and mteResourceSampleInstanceMaximum
         *  are defined with MAX-ACCESS read-write, this version hardcodes
         *  these values, so doesn't need to implement write access.
         */
        Logger_log( LOGGER_PRIORITY_ERR,
            "unknown mode (%d) in handle_mteResourceGroup\n",
            reqinfo->mode );
        return PRIOT_ERR_GENERR;
    }

    return PRIOT_ERR_NOERROR;
}

int handle_mteTriggerFailures( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    extern long mteTriggerFailures;

    switch ( reqinfo->mode ) {
    case MODE_GET:
        Client_setVarTypedValue( requests->requestvb, ASN01_COUNTER,
            ( u_char* )&mteTriggerFailures,
            sizeof( mteTriggerFailures ) );
        break;

    default:
        /*
         * we should never get here, so this is a really bad error 
         */
        Logger_log( LOGGER_PRIORITY_ERR,
            "unknown mode (%d) in handle_mteTriggerFailures\n",
            reqinfo->mode );
        return PRIOT_ERR_GENERR;
    }

    return PRIOT_ERR_NOERROR;
}
