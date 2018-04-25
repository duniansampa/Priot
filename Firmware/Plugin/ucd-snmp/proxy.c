/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright @ 2009 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */

#include "proxy.h"
#include "AgentReadConfig.h"
#include "AgentRegistry.h"
#include "BulkToNext.h"
#include "Client.h"
#include "Debug.h"
#include "DefaultStore.h"
#include "Impl.h"
#include "Logger.h"
#include "Mib.h"
#include "ParseArgs.h"
#include "Priot.h"
#include "ReadConfig.h"
#include "Session.h"

static struct simple_proxy* proxies = NULL;

oid testoid[] = { 1, 3, 6, 1, 4, 1, 2021, 8888, 1 };

/*
 * this must be standardized somewhere, right? 
 */
#define MAX_ARGS 128

char* context_string;

static void
proxyOptProc( int argc, char* const* argv, int opt )
{
    switch ( opt ) {
    case 'C':
        while ( *optarg ) {
            switch ( *optarg++ ) {
            case 'n':
                optind++;
                if ( optind < argc ) {
                    context_string = argv[ optind - 1 ];
                } else {
                    ReadConfig_configPerror( "No context name passed to -Cn" );
                }
                break;
            case 'c':
                DefaultStore_setBoolean( DsStorage_LIBRARY_ID,
                    DsBool_IGNORE_NO_COMMUNITY, 1 );
                break;
            default:
                ReadConfig_configPerror( "unknown argument passed to -C" );
                break;
            }
        }
        break;
    default:
        break;
        /*
         * shouldn't get here 
         */
    }
}

void proxy_parse_config( const char* token, char* line )
{
    /*
     * proxy args [base-oid] [remap-to-remote-oid] 
     */

    Types_Session session, *ss;
    struct simple_proxy *newp, **listpp;
    char args[ MAX_ARGS ][ IMPL_SPRINT_MAX_LEN ], *argv[ MAX_ARGS ];
    int argn, arg;
    char* cp;
    HandlerRegistration* reg;

    context_string = NULL;

    DEBUG_MSGTL( ( "proxy_config", "entering\n" ) );

    /*
     * create the argv[] like array 
     */
    strcpy( argv[ 0 ] = args[ 0 ], "snmpd-proxy" ); /* bogus entry for getopt() */
    for ( argn = 1, cp = line; cp && argn < MAX_ARGS; ) {
        argv[ argn ] = args[ argn ];
        cp = ReadConfig_copyNword( cp, argv[ argn ], IMPL_SPRINT_MAX_LEN );
        argn++;
    }

    for ( arg = 0; arg < argn; arg++ ) {
        DEBUG_MSGTL( ( "proxy_args", "final args: %d = %s\n", arg,
            argv[ arg ] ) );
    }

    DEBUG_MSGTL( ( "proxy_config", "parsing args: %d\n", argn ) );
    /* Call special parse_args that allows for no specified community string */
    arg = ParseArgs_parseArgs( argn, argv, &session, "C:", proxyOptProc,
        PARSEARGS_NOLOGGING | PARSEARGS_NOZERO );

    /* reset this in case we modified it */
    DefaultStore_setBoolean( DsStorage_LIBRARY_ID,
        DsBool_IGNORE_NO_COMMUNITY, 0 );

    if ( arg < 0 ) {
        ReadConfig_configPerror( "failed to parse proxy args" );
        return;
    }
    DEBUG_MSGTL( ( "proxy_config", "done parsing args\n" ) );

    if ( arg >= argn ) {
        ReadConfig_configPerror( "missing base oid" );
        return;
    }

    /*
     * usm_set_reportErrorOnUnknownID(0); 
     *
     * hack, stupid v3 ASIs. 
     */
    /*
     * XXX: on a side note, we don't really need to be a reference
     * platform any more so the proper thing to do would be to fix
     * snmplib/snmpusm.c to pass in the pdu type to usm_process_incoming
     * so this isn't needed. 
     */
    ss = Api_open( &session );
    /*
     * usm_set_reportErrorOnUnknownID(1); 
     */
    if ( ss == NULL ) {
        /*
         * diagnose snmp_open errors with the input Types_Session pointer 
         */
        Api_sessPerror( "snmpget", &session );
        return;
    }

    newp = ( struct simple_proxy* )calloc( 1, sizeof( struct simple_proxy ) );

    newp->sess = ss;
    DEBUG_MSGTL( ( "proxy_init", "name = %s\n", args[ arg ] ) );
    newp->name_len = ASN01_MAX_OID_LEN;
    if ( !Mib_parseOid( args[ arg++ ], newp->name, &newp->name_len ) ) {
        Api_perror( "proxy" );
        ReadConfig_configPerror( "illegal proxy oid specified\n" );
        return;
    }

    if ( arg < argn ) {
        DEBUG_MSGTL( ( "proxy_init", "base = %s\n", args[ arg ] ) );
        newp->base_len = ASN01_MAX_OID_LEN;
        if ( !Mib_parseOid( args[ arg++ ], newp->base, &newp->base_len ) ) {
            Api_perror( "proxy" );
            ReadConfig_configPerror( "illegal variable name specified (base oid)\n" );
            return;
        }
    }
    if ( context_string )
        newp->context = strdup( context_string );

    DEBUG_MSGTL( ( "proxy_init", "registering at: " ) );
    DEBUG_MSGOID( ( "proxy_init", newp->name, newp->name_len ) );
    DEBUG_MSG( ( "proxy_init", "\n" ) );

    /*
     * add to our chain 
     */
    /*
     * must be sorted! 
     */
    listpp = &proxies;
    while ( *listpp && Api_oidCompare( newp->name, newp->name_len, ( *listpp )->name, ( *listpp )->name_len ) > 0 ) {
        listpp = &( ( *listpp )->next );
    }

    /*
     * listpp should be next in line from us. 
     */
    if ( *listpp ) {
        /*
         * make our next in the link point to the current link 
         */
        newp->next = *listpp;
    }
    /*
     * replace current link with us 
     */
    *listpp = newp;

    reg = AgentHandler_createHandlerRegistration( "proxy",
        proxy_handler,
        newp->name,
        newp->name_len,
        HANDLER_CAN_RWRITE );
    reg->handler->myvoid = newp;
    if ( context_string )
        reg->contextName = strdup( context_string );

    AgentHandler_registerHandler( reg );
}

void proxy_free_config( void )
{
    struct simple_proxy* rm;

    DEBUG_MSGTL( ( "proxy_free_config", "Free config\n" ) );
    while ( proxies ) {
        rm = proxies;
        proxies = rm->next;

        DEBUG_MSGTL( ( "proxy_free_config", "freeing " ) );
        DEBUG_MSGOID( ( "proxy_free_config", rm->name, rm->name_len ) );
        DEBUG_MSG( ( "proxy_free_config", " (%s)\n", rm->context ) );
        AgentRegistry_unregisterMibContext( rm->name, rm->name_len,
            DEFAULT_MIB_PRIORITY, 0, 0,
            rm->context );
        TOOLS_FREE( rm->variables );
        TOOLS_FREE( rm->context );
        Api_close( rm->sess );
        TOOLS_FREE( rm );
    }
}

/*
 * Configure special parameters on the session.
 * Currently takes the parameter configured and changes it if something 
 * was configured.  It becomes "-c" if the community string from the pdu
 * is placed on the session.
 */
int proxy_fill_in_session( MibHandler* handler,
    AgentRequestInfo* reqinfo,
    void** configured )
{
    Types_Session* session;
    struct simple_proxy* sp;

    sp = ( struct simple_proxy* )handler->myvoid;
    if ( !sp ) {
        return 0;
    }
    session = sp->sess;
    if ( !session ) {
        return 0;
    }

    return 1;
}

/*
 * Free any specially configured parameters used on the session.
 */
void proxy_free_filled_in_session_args( Types_Session* session, void** configured )
{

    /* Only do comparisions, etc., if something was configured */
    if ( *configured == NULL ) {
        return;
    }

    /* If used community string from pdu, release it from session now */
    if ( strcmp( ( const char* )( *configured ), "-c" ) == 0 ) {
        free( session->community );
        session->community = NULL;
        //session->community_len = 0;
    }

    free( ( u_char* )( *configured ) );
    *configured = NULL;
}

void init_proxy( void )
{
    AgentReadConfig_priotdRegisterConfigHandler( "proxy", proxy_parse_config,
        proxy_free_config,
        "[snmpcmd args] host oid [remoteoid]" );
}

void shutdown_proxy( void )
{
    proxy_free_config();
}

int proxy_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    Types_Pdu* pdu;
    struct simple_proxy* sp;
    oid* ourname;
    size_t ourlength;
    RequestInfo* request = requests;
    u_char* configured = NULL;

    DEBUG_MSGTL( ( "proxy", "proxy handler starting, mode = %d\n",
        reqinfo->mode ) );

    switch ( reqinfo->mode ) {
    case MODE_GET:
    case MODE_GETNEXT:
    case MODE_GETBULK: /* WWWXXX */
        pdu = Client_pduCreate( reqinfo->mode );
        break;

    case MODE_SET_ACTION:
        pdu = Client_pduCreate( PRIOT_MSG_SET );
        break;

    case MODE_SET_UNDO:
        /*
         *  If we set successfully (status == NOERROR),
         *     we can't back out again, so need to report the fact.
         *  If we failed to set successfully, then we're fine.
         */
        for ( request = requests; request; request = request->next ) {
            if ( request->status == PRIOT_ERR_NOERROR ) {
                Agent_setRequestError( reqinfo, requests,
                    PRIOT_ERR_UNDOFAILED );
                return PRIOT_ERR_UNDOFAILED;
            }
        }
        return PRIOT_ERR_NOERROR;

    case MODE_SET_RESERVE1:
    case MODE_SET_RESERVE2:
    case MODE_SET_FREE:
    case MODE_SET_COMMIT:
        /*
         *  Nothing to do in this pass
         */
        return PRIOT_ERR_NOERROR;

    default:
        Logger_log( LOGGER_PRIORITY_WARNING, "unsupported mode for proxy called (%d)\n",
            reqinfo->mode );
        return PRIOT_ERR_NOERROR;
    }

    sp = ( struct simple_proxy* )handler->myvoid;

    if ( !pdu || !sp ) {
        Agent_setRequestError( reqinfo, requests, PRIOT_ERR_GENERR );
        if ( pdu )
            Api_freePdu( pdu );
        return PRIOT_ERR_NOERROR;
    }

    while ( request ) {
        ourname = request->requestvb->name;
        ourlength = request->requestvb->nameLength;

        if ( sp->base_len && reqinfo->mode == MODE_GETNEXT && ( Api_oidCompare( ourname, ourlength,
                                                                    sp->base, sp->base_len )
                                                                  < 0 ) ) {
            DEBUG_MSGTL( ( "proxy", "request is out of registered range\n" ) );
            /*
             * Create GETNEXT request with an OID so the
             * master returns the first OID in the registered range.
             */
            memcpy( ourname, sp->base, sp->base_len * sizeof( oid ) );
            ourlength = sp->base_len;
            if ( ourname[ ourlength - 1 ] <= 1 ) {
                /*
                 * The registered range ends with x.y.z.1
                 * -> ask for the next of x.y.z
                 */
                ourlength--;
            } else {
                /*
                 * The registered range ends with x.y.z.A
                 * -> ask for the next of x.y.z.A-1.MAX_SUBID
                 */
                ourname[ ourlength - 1 ]--;
                ourname[ ourlength ] = TYPES_OID_MAX_SUBID;
                ourlength++;
            }
        } else if ( sp->base_len > 0 ) {
            if ( ( ourlength - sp->name_len + sp->base_len ) > ASN01_MAX_OID_LEN ) {
                /*
                 * too large 
                 */
                if ( pdu )
                    Api_freePdu( pdu );
                Logger_log( LOGGER_PRIORITY_ERR,
                    "proxy oid request length is too long\n" );
                return PRIOT_ERR_NOERROR;
            }
            /*
             * suffix appended? 
             */
            DEBUG_MSGTL( ( "proxy", "length=%d, base_len=%d, name_len=%d\n",
                ( int )ourlength, ( int )sp->base_len, ( int )sp->name_len ) );
            if ( ourlength > sp->name_len )
                memcpy( &( sp->base[ sp->base_len ] ), &( ourname[ sp->name_len ] ),
                    sizeof( oid ) * ( ourlength - sp->name_len ) );
            ourlength = ourlength - sp->name_len + sp->base_len;
            ourname = sp->base;
        }

        Api_pduAddVariable( pdu, ourname, ourlength,
            request->requestvb->type,
            request->requestvb->val.string,
            request->requestvb->valLen );
        request->delegated = 1;
        request = request->next;
    }

    /*
     * Customize session parameters based on request information
     */
    if ( !proxy_fill_in_session( handler, reqinfo, ( void** )&configured ) ) {
        Agent_setRequestError( reqinfo, requests, PRIOT_ERR_GENERR );
        if ( pdu )
            Api_freePdu( pdu );
        return PRIOT_ERR_NOERROR;
    }

    /*
     * send the request out 
     */
    DEBUG_MSGTL( ( "proxy", "sending pdu\n" ) );
    Session_asyncSend( sp->sess, pdu, proxy_got_response,
        AgentHandler_createDelegatedCache( handler, reginfo,
                           reqinfo, requests,
                           ( void* )sp ) );

    /* Free any special parameters generated on the session */
    proxy_free_filled_in_session_args( sp->sess, ( void** )&configured );

    return PRIOT_ERR_NOERROR;
}

int proxy_got_response( int operation, Types_Session* sess, int reqid,
    Types_Pdu* pdu, void* cb_data )
{
    DelegatedCache* cache = ( DelegatedCache* )cb_data;
    RequestInfo *requests, *request = NULL;
    Types_VariableList *vars, *var = NULL;

    struct simple_proxy* sp;
    oid myname[ ASN01_MAX_OID_LEN ];
    size_t myname_len = ASN01_MAX_OID_LEN;

    cache = AgentHandler_handlerCheckCache( cache );

    if ( !cache ) {
        DEBUG_MSGTL( ( "proxy", "a proxy request was no longer valid.\n" ) );
        return PRIOT_ERR_NOERROR;
    }

    requests = cache->requests;

    sp = ( struct simple_proxy* )cache->localinfo;

    if ( !sp ) {
        DEBUG_MSGTL( ( "proxy", "a proxy request was no longer valid.\n" ) );
        return PRIOT_ERR_NOERROR;
    }

    switch ( operation ) {
    case API_CALLBACK_OP_TIMED_OUT:
        /*
         * WWWXXX: don't leave requests delayed if operation is
         * something like TIMEOUT 
         */
        DEBUG_MSGTL( ( "proxy", "got timed out... requests = %8p\n", requests ) );

        AgentHandler_handlerMarkRequestsAsDelegated( requests,
            REQUEST_IS_NOT_DELEGATED );
        if ( cache->reqinfo->mode != MODE_GETNEXT ) {
            DEBUG_MSGTL( ( "proxy", "  ignoring timeout\n" ) );
            Agent_setRequestError( cache->reqinfo, requests, /* XXXWWW: should be index = 0 */
                PRIOT_ERR_GENERR );
        }
        AgentHandler_freeDelegatedCache( cache );
        return 0;

    case API_CALLBACK_OP_RECEIVED_MESSAGE:
        vars = pdu->variables;

        if ( pdu->errstat != PRIOT_ERR_NOERROR ) {
            /*
             *  If we receive an error from the proxy agent, pass it on up.
             *  The higher-level processing seems to Do The Right Thing.
             *
             * 2005/06 rks: actually, it doesn't do the right thing for
             * a get-next request that returns NOSUCHNAME. If we do nothing,
             * it passes that error back to the comman initiator. What it should
             * do is ignore the error and move on to the next tree. To
             * accomplish that, all we need to do is clear the delegated flag.
             * Not sure if any other error codes need the same treatment. Left
             * as an exercise to the reader...
             */
            DEBUG_MSGTL( ( "proxy", "got error response (%ld)\n", pdu->errstat ) );
            if ( ( cache->reqinfo->mode == MODE_GETNEXT ) && ( PRIOT_ERR_NOSUCHNAME == pdu->errstat ) ) {
                DEBUG_MSGTL( ( "proxy", "  ignoring error response\n" ) );
                AgentHandler_handlerMarkRequestsAsDelegated( requests,
                    REQUEST_IS_NOT_DELEGATED );
            } else if ( cache->reqinfo->mode == MODE_SET_ACTION ) {
                /*
		 * In order for netsnmp_wrap_up_request to consider the
		 * SET request complete,
		 * there must be no delegated requests pending.
		 * https://sourceforge.net/tracker/
		 *	?func=detail&atid=112694&aid=1554261&group_id=12694
		 */
                DEBUG_MSGTL( ( "proxy",
                    "got SET error %s, index %ld\n",
                    Client_errstring( pdu->errstat ), pdu->errindex ) );
                AgentHandler_handlerMarkRequestsAsDelegated(
                    requests, REQUEST_IS_NOT_DELEGATED );
                Agent_requestSetErrorIdx( requests, pdu->errstat,
                    pdu->errindex );
            } else {
                AgentHandler_handlerMarkRequestsAsDelegated( requests,
                    REQUEST_IS_NOT_DELEGATED );
                Agent_requestSetErrorIdx( requests, pdu->errstat,
                    pdu->errindex );
            }

            /*
         * update the original request varbinds with the results 
         */
        } else
            for ( var = vars, request = requests;
                  request && var;
                  request = request->next, var = var->nextVariable ) {
                /*
             * XXX - should this be done here?
             *       Or wait until we know it's OK?
             */
                Client_setVarTypedValue( request->requestvb, var->type,
                    var->val.string, var->valLen );

                DEBUG_MSGTL( ( "proxy", "got response... " ) );
                DEBUG_MSGOID( ( "proxy", var->name, var->nameLength ) );
                DEBUG_MSG( ( "proxy", "\n" ) );
                request->delegated = 0;

                /*
             * Check the response oid is legitimate,
             *   and discard the value if not.
             *
             * XXX - what's the difference between these cases?
             */
                if ( sp->base_len && ( var->nameLength < sp->base_len || Api_oidCompare( var->name, sp->base_len, sp->base, sp->base_len ) != 0 ) ) {
                    DEBUG_MSGTL( ( "proxy", "out of registered range... " ) );
                    DEBUG_MSGOID( ( "proxy", var->name, sp->base_len ) );
                    DEBUG_MSG( ( "proxy", " (%d) != ", ( int )sp->base_len ) );
                    DEBUG_MSGOID( ( "proxy", sp->base, sp->base_len ) );
                    DEBUG_MSG( ( "proxy", "\n" ) );
                    Client_setVarTypedValue( request->requestvb, ASN01_NULL, NULL, 0 );

                    continue;
                } else if ( !sp->base_len && ( var->nameLength < sp->name_len || Api_oidCompare( var->name, sp->name_len, sp->name, sp->name_len ) != 0 ) ) {
                    DEBUG_MSGTL( ( "proxy", "out of registered base range... " ) );
                    DEBUG_MSGOID( ( "proxy", var->name, sp->name_len ) );
                    DEBUG_MSG( ( "proxy", " (%d) != ", ( int )sp->name_len ) );
                    DEBUG_MSGOID( ( "proxy", sp->name, sp->name_len ) );
                    DEBUG_MSG( ( "proxy", "\n" ) );
                    Client_setVarTypedValue( request->requestvb, ASN01_NULL, NULL, 0 );
                    continue;
                } else {
                    /*
                 * If the returned OID is legitimate, then update
                 *   the original request varbind accordingly.
                 */
                    if ( sp->base_len ) {
                        /*
                     * XXX: oid size maxed? 
                     */
                        memcpy( myname, sp->name, sizeof( oid ) * sp->name_len );
                        myname_len = sp->name_len + var->nameLength - sp->base_len;
                        if ( myname_len > ASN01_MAX_OID_LEN ) {
                            Logger_log( LOGGER_PRIORITY_WARNING,
                                "proxy OID return length too long.\n" );
                            Agent_setRequestError( cache->reqinfo, requests,
                                PRIOT_ERR_GENERR );
                            if ( pdu )
                                Api_freePdu( pdu );
                            AgentHandler_freeDelegatedCache( cache );
                            return 1;
                        }

                        if ( var->nameLength > sp->base_len )
                            memcpy( &myname[ sp->name_len ],
                                &var->name[ sp->base_len ],
                                sizeof( oid ) * ( var->nameLength - sp->base_len ) );
                        Client_setVarObjid( request->requestvb, myname,
                            myname_len );
                    } else {
                        Client_setVarObjid( request->requestvb, var->name,
                            var->nameLength );
                    }
                }
            }

        if ( request || var ) {
            /*
             * ack, this is bad.  The # of varbinds don't match and
             * there is no way to fix the problem 
             */
            if ( pdu )
                Api_freePdu( pdu );
            Logger_log( LOGGER_PRIORITY_ERR,
                "response to proxy request illegal.  We're screwed.\n" );
            Agent_setRequestError( cache->reqinfo, requests,
                PRIOT_ERR_GENERR );
        }

        /* fix bulk_to_next operations */
        if ( cache->reqinfo->mode == MODE_GETBULK )
            BulkToNext_fixRequests( requests );

        /*
         * free the response 
         */
        if ( pdu && 0 )
            Api_freePdu( pdu );
        break;

    default:
        DEBUG_MSGTL( ( "proxy", "no response received: op = %d\n",
            operation ) );
        break;
    }

    AgentHandler_freeDelegatedCache( cache );
    return 1;
}
