#include "updates.h"

static int
handle_updates( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    int* set = ( int* )handler->myvoid;

    if ( reqinfo->mode == MODE_SET_RESERVE1 && *set < 0 )
        Agent_requestSetError( requests, PRIOT_ERR_NOTWRITABLE );
    else if ( reqinfo->mode == MODE_SET_COMMIT ) {
        *set = 1;
        Api_storeNeeded( reginfo->handlerName );
    }
    return PRIOT_ERR_NOERROR;
}

HandlerRegistration*
netsnmp_create_update_handler_registration(
    const char* name, const oid* id, size_t idlen, int mode, int* set )
{
    HandlerRegistration* res;
    MibHandler* hnd;

    hnd = AgentHandler_createHandler( "update", handle_updates );
    if ( hnd == NULL )
        return NULL;

    hnd->flags |= MIB_HANDLER_AUTO_NEXT;
    hnd->myvoid = set;

    res = AgentHandler_registrationCreate( name, hnd, id, idlen, mode );
    if ( res == NULL ) {
        AgentHandler_handlerFree( hnd );
        return NULL;
    }

    return res;
}
