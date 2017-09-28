#include "Scalar.h"
#include "Debug.h"
#include "ReadOnly.h"
#include "Instance.h"
#include "Serialize.h"
/**
 * Creates a scalar handler calling AgentHandler_createHandler with a
 * handler name defaulted to "scalar" and access method,
 * Scalar_helperHandler.
 *
 * @return Returns a pointer to a MibHandler struct which contains
 *	the handler's name and the access method
 *
 * @see Scalar_getScalarHandler
 * @see Scalar_registerScalar
 */
MibHandler *
Scalar_getScalarHandler(void)
{
    return AgentHandler_createHandler("scalar",
                                  Scalar_helperHandler);
}

/**
 * This function registers a scalar helper handler.  The registered OID,
 * reginfo->rootoid, space is extended for the instance subid using
 * realloc() but the reginfo->rootoid_len length is not extended just yet.
 * .This function subsequently injects the instance, scalar, and serialize
 * helper handlers before actually registering reginfo.
 *
 * Each handler is injected/pushed to the top of the handler chain list
 * and will be processed last in first out, LIFO.
 *
 * @param reginfo a handler registration structure which could get created
 *                using netsnmp_create_handler_registration.  Used to register
 *                a scalar helper handler.
 *
 * @return MIB_REGISTERED_OK is returned if the registration was a success.
 *	Failures are MIB_REGISTRATION_FAILURE and MIB_DUPLICATE_REGISTRATION.
 *
 * @see Scalar_registerReadOnlyScalar
 * @see Scalar_getScalarHandler
 */

int
Scalar_registerScalar(HandlerRegistration *reginfo)
{
    /*
     * Extend the registered OID with space for the instance subid
     * (but don't extend the length just yet!)
     */
    reginfo->rootoid = (oid*)realloc(reginfo->rootoid,
                                    (reginfo->rootoid_len+1) * sizeof(oid) );
    reginfo->rootoid[ reginfo->rootoid_len ] = 0;

    AgentHandler_injectHandler(reginfo, Instance_getInstanceHandler());
    AgentHandler_injectHandler(reginfo, Scalar_getScalarHandler());
    return Serialize_registerSerialize(reginfo);
}


/**
 * This function registers a read only scalar helper handler. This
 * function is very similar to Scalar_registerScalar the only addition
 * is that the "read_only" handler is injected into the handler chain
 * prior to injecting the serialize handler and registering reginfo.
 *
 * @param reginfo a handler registration structure which could get created
 *                using netsnmp_create_handler_registration.  Used to register
 *                a read only scalar helper handler.
 *
 * @return  MIB_REGISTERED_OK is returned if the registration was a success.
 *  	Failures are MIB_REGISTRATION_FAILURE and MIB_DUPLICATE_REGISTRATION.
 *
 * @see Scalar_registerScalar
 * @see Scalar_getScalarHandler
 *
 */

int
Scalar_registerReadOnlyScalar(HandlerRegistration *reginfo)
{
    /*
     * Extend the registered OID with space for the instance subid
     * (but don't extend the length just yet!)
     */
    reginfo->rootoid = (oid*)realloc(reginfo->rootoid,
                                    (reginfo->rootoid_len+1) * sizeof(oid) );
    reginfo->rootoid[ reginfo->rootoid_len ] = 0;

    AgentHandler_injectHandler(reginfo, Instance_getInstanceHandler());
    AgentHandler_injectHandler(reginfo, Scalar_getScalarHandler());
    AgentHandler_injectHandler(reginfo, ReadOnly_getReadOnlyHandler());
    return Serialize_registerSerialize(reginfo);
}



int
Scalar_helperHandler(MibHandler *handler,
                                HandlerRegistration *reginfo,
                                AgentRequestInfo *reqinfo,
                                RequestInfo *requests)
{

    Types_VariableList *var = requests->requestvb;

    int             ret, cmp;
    int             namelen;

    DEBUG_MSGTL(("helper:scalar", "Got request:\n"));
    namelen = TOOLS_MIN(requests->requestvb->nameLength,
                       reginfo->rootoid_len);
    cmp = Api_oidCompare(requests->requestvb->name, namelen,
                           reginfo->rootoid, reginfo->rootoid_len);

    DEBUG_MSGTL(("helper:scalar", "  oid:"));
    DEBUG_MSGOID(("helper:scalar", var->name, var->nameLength));
    DEBUG_MSG(("helper:scalar", "\n"));

    switch (reqinfo->mode) {
    case MODE_GET:
        if (cmp != 0) {
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_NOSUCHOBJECT);
            return PRIOT_ERR_NOERROR;
        } else {
            reginfo->rootoid[reginfo->rootoid_len++] = 0;
            ret = AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                             requests);
            reginfo->rootoid_len--;
            return ret;
        }
        break;

    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_UNDO:
    case MODE_SET_FREE:
        if (cmp != 0) {
            Agent_setRequestError(reqinfo, requests,
                                      PRIOT_ERR_NOCREATION);
            return PRIOT_ERR_NOERROR;
        } else {
            reginfo->rootoid[reginfo->rootoid_len++] = 0;
            ret = AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                             requests);
            reginfo->rootoid_len--;
            return ret;
        }
        break;

    case MODE_GETNEXT:
        reginfo->rootoid[reginfo->rootoid_len++] = 0;
        ret = AgentHandler_callNextHandler(handler, reginfo, reqinfo, requests);
        reginfo->rootoid_len--;
        return ret;
    }
    /*
     * got here only if illegal mode found
     */
    return PRIOT_ERR_GENERR;
}
