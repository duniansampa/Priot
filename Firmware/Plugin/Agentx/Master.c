#include "Master.h"
#include "../Plugin/Agentx/MasterAdmin.h"
#include "BulkToNext.h"
#include "Client.h"
#include "Debug.h"
#include "DefaultStore.h"
#include "DsAgent.h"
#include "Logger.h"
#include "PriotSettings.h"
#include "Protocol.h"
#include "Session.h"
#include "Transports/UnixDomain.h"
#include "VarStruct.h"

void Master_realInitMaster( void )
{
    Types_Session sess, *session = NULL;
    char* agentx_sockets;
    char* cp1;

    if ( DefaultStore_getBoolean( DsStorage_APPLICATION_ID, DsAgentBoolean_ROLE ) != MASTER_AGENT )
        return;

    if ( DefaultStore_getString( DsStorage_APPLICATION_ID,
             DsAgentString_X_SOCKET ) ) {
        agentx_sockets = strdup( DefaultStore_getString( DsStorage_APPLICATION_ID,
            DsAgentString_X_SOCKET ) );
    } else {
        agentx_sockets = strdup( "" );
    }

    DEBUG_MSGTL( ( "agentx/master", "initializing...\n" ) );
    Api_sessInit( &sess );
    sess.version = AGENTX_VERSION_1;
    sess.flags |= API_FLAGS_STREAM_SOCKET;
    sess.timeout = DefaultStore_getInt( DsStorage_APPLICATION_ID,
        DsAgentInterger_AGENTX_TIMEOUT );
    sess.retries = DefaultStore_getInt( DsStorage_APPLICATION_ID,
        DsAgentInterger_AGENTX_RETRIES );

    {
        int agentx_dir_perm = DefaultStore_getInt( DsStorage_APPLICATION_ID,
            DsAgentInterger_X_DIR_PERM );
        if ( agentx_dir_perm == 0 )
            agentx_dir_perm = AGENT_DIRECTORY_MODE;
        UnixDomain_createPathWithMode( agentx_dir_perm );
    }

    cp1 = agentx_sockets;
    while ( cp1 ) {
        Transport_Transport* t;
        /*
         *  If the AgentX socket string contains multiple descriptors,
         *  then pick this apart and handle them one by one.
         *
         */
        sess.peername = cp1;
        cp1 = strchr( sess.peername, ',' );
        if ( cp1 != NULL ) {
            *cp1++ = '\0';
        }

        /*
         *  Let 'snmp_open' interpret the descriptor.
         */
        sess.local_port = AGENTX_PORT; /* Indicate server & set default port */
        sess.remote_port = 0;
        sess.callback = MasterAdmin_handleMasterAgentxPacket;
        errno = 0;
        t = Transport_openServer( "agentx", sess.peername );
        if ( t == NULL ) {
            /*
             * diagnose snmp_open errors with the input Types_Session
             * pointer.
             */
            char buf[ 1024 ];
            if ( !DefaultStore_getBoolean( DsStorage_APPLICATION_ID,
                     DsAgentBoolean_NO_ROOT_ACCESS ) ) {
                snprintf( buf, sizeof( buf ),
                    "Error: Couldn't open a master agentx socket to "
                    "listen on (%s)",
                    sess.peername );
                Api_sessPerror( buf, &sess );
                exit( 1 );
            } else {
                snprintf( buf, sizeof( buf ),
                    "Warning: Couldn't open a master agentx socket to "
                    "listen on (%s)",
                    sess.peername );
                Api_sessLogError( LOGGER_PRIORITY_WARNING, buf, &sess );
            }
        } else {
            if ( t->domain == unixDomain_priotUnixDomain && t->local != NULL ) {
                /*
                 * Apply any settings to the ownership/permissions of the
                 * AgentX socket
                 */
                int agentx_sock_perm = DefaultStore_getInt( DsStorage_APPLICATION_ID,
                    DsAgentInterger_X_SOCK_PERM );
                int agentx_sock_user = DefaultStore_getInt( DsStorage_APPLICATION_ID,
                    DsAgentInterger_X_SOCK_USER );
                int agentx_sock_group = DefaultStore_getInt( DsStorage_APPLICATION_ID,
                    DsAgentInterger_X_SOCK_GROUP );

                char name[ sizeof( struct sockaddr_un ) + 1 ];
                memcpy( name, t->local, t->local_length );
                name[ t->local_length ] = '\0';

                if ( agentx_sock_perm != 0 )
                    chmod( name, agentx_sock_perm );

                if ( agentx_sock_user || agentx_sock_group ) {
                    /*
                     * If either of user or group haven't been set,
                     *  then leave them unchanged.
                     */
                    if ( agentx_sock_user == 0 )
                        agentx_sock_user = -1;
                    if ( agentx_sock_group == 0 )
                        agentx_sock_group = -1;
                    chown( name, agentx_sock_user, agentx_sock_group );
                }
            }
            session = Api_addFull( &sess, t, NULL, Protocol_parse, NULL, NULL,
                Protocol_reallocBuild, Protocol_checkPacket, NULL );
        }
        if ( session == NULL ) {
            Transport_free( t );
        }
    }

    UnixDomain_dontCreatePath();

    TOOLS_FREE( agentx_sockets );

    DEBUG_MSGTL( ( "agentx/master", "initializing...   DONE\n" ) );
}

/*
         * Handle the response from an AgentX subagent,
         *   merging the answers back into the original query
         */
int Master_gotResponse( int operation,
    Types_Session* session,
    int reqid, Types_Pdu* pdu, void* magic )
{
    DelegatedCache* cache = ( DelegatedCache* )magic;
    int i, ret;
    RequestInfo *requests, *request;
    Types_VariableList* var;
    Types_Session* ax_session;

    cache = AgentHandler_handlerCheckCache( cache );
    if ( !cache ) {

        DEBUG_MSGTL( ( "agentx/master", "response too late on session %8p\n",
            session ) );
        return 0;
    }
    requests = cache->requests;

    switch ( operation ) {
    case API_CALLBACK_OP_TIMED_OUT: {
        void* s = Api_sessPointer( session );

        DEBUG_MSGTL( ( "agentx/master", "timeout on session %8p req=0x%x\n",
            session, ( unsigned )reqid ) );

        AgentHandler_handlerMarkRequestsAsDelegated( requests,
            REQUEST_IS_NOT_DELEGATED );
        Agent_setRequestError( cache->reqinfo, requests,
            /* XXXWWW: should be index=0 */
            PRIOT_ERR_GENERR );

        /*
             * This is a bit sledgehammer because the other sessions on this
             * transport may be okay (e.g. some thread in the subagent has
             * wedged, but the others are alright).  OTOH the overwhelming
             * probability is that the whole agent has died somehow.
             */

        if ( s != NULL ) {
            Transport_Transport* t = Api_sessTransport( s );
            MasterAdmin_closeAgentxSession( session, -1 );

            if ( t != NULL ) {

                DEBUG_MSGTL( ( "agentx/master", "close transport\n" ) );
                t->f_close( t );
            } else {

                DEBUG_MSGTL( ( "agentx/master", "NULL transport??\n" ) );
            }
        } else {

            DEBUG_MSGTL( ( "agentx/master", "NULL sess_pointer??\n" ) );
        }
        ax_session = ( Types_Session* )cache->localinfo;
        Agent_freeAgentPriotSessionBySession( ax_session, NULL );
        AgentHandler_freeDelegatedCache( cache );
        return 0;
    }

    case API_CALLBACK_OP_DISCONNECT:
    case API_CALLBACK_OP_SEND_FAILED:
        if ( operation == API_CALLBACK_OP_DISCONNECT ) {

            DEBUG_MSGTL( ( "agentx/master", "disconnect on session %8p\n",
                session ) );
        } else {

            DEBUG_MSGTL( ( "agentx/master", "send failed on session %8p\n",
                session ) );
        }
        MasterAdmin_closeAgentxSession( session, -1 );
        AgentHandler_handlerMarkRequestsAsDelegated( requests,
            REQUEST_IS_NOT_DELEGATED );
        Agent_setRequestError( cache->reqinfo, requests, /* XXXWWW: should be index=0 */
            PRIOT_ERR_GENERR );
        AgentHandler_freeDelegatedCache( cache );
        return 0;

    case API_CALLBACK_OP_RECEIVED_MESSAGE:
        /*
         * This session is alive
         */
        API_CLEAR_PRIOT_STRIKE_FLAGS( session->flags );
        break;
    default:
        Logger_log( LOGGER_PRIORITY_ERR, "Unknown operation %d in Master_gotResponse\n",
            operation );
        AgentHandler_freeDelegatedCache( cache );
        return 0;
    }

    DEBUG_MSGTL( ( "agentx/master", "got response errstat=%ld, (req=0x%x,trans="
                                    "0x%x,sess=0x%x)\n",
        pdu->errstat, ( unsigned )pdu->reqid, ( unsigned )pdu->transid,
        ( unsigned )pdu->sessid ) );

    if ( pdu->errstat != AGENTX_ERR_NOERROR ) {
        /* [RFC 2471 - 7.2.5.2.]
         *
         *   1) For any received AgentX response PDU, if res.error is
         *      not `noError', the SNMP response PDU's error code is
         *      set to this value.  If res.error contains an AgentX
         *      specific value (e.g.  `parseError'), the SNMP response
         *      PDU's error code is set to a value of genErr instead.
         *      Also, the SNMP response PDU's error index is set to
         *      the index of the variable binding corresponding to the
         *      failed VarBind in the subagent's AgentX response PDU.
         *
         *      All other AgentX response PDUs received due to
         *      processing this SNMP request are ignored.  Processing
         *      is complete; the SNMP Response PDU is ready to be sent
         *      (see section 7.2.6, "Sending the SNMP Response-PDU").
         */
        int err;

        DEBUG_MSGTL( ( "agentx/master",
            "Master_gotResponse() error branch\n" ) );

        switch ( pdu->errstat ) {
        case AGENTX_ERR_PARSE_FAILED:
        case AGENTX_ERR_REQUEST_DENIED:
        case AGENTX_ERR_PROCESSING_ERROR:
            err = PRIOT_ERR_GENERR;
            break;
        default:
            err = pdu->errstat;
        }

        ret = 0;
        for ( request = requests, i = 1; request;
              request = request->next, i++ ) {
            if ( i == pdu->errindex ) {
                /*
                 * Mark this varbind as the one generating the error.
                 * Note that the AgentX errindex may not match the
                 * position in the original SNMP PDU (request->index)
                 */
                Agent_setRequestError( cache->reqinfo, request,
                    err );
                ret = 1;
            }
            request->delegated = REQUEST_IS_NOT_DELEGATED;
        }
        if ( !ret ) {
            /*
             * ack, unknown, mark the first one
             */
            Agent_setRequestError( cache->reqinfo, requests,
                PRIOT_ERR_GENERR );
        }
        AgentHandler_freeDelegatedCache( cache );

        DEBUG_MSGTL( ( "agentx/master", "end error branch\n" ) );
        return 1;
    } else if ( cache->reqinfo->mode == MODE_GET || cache->reqinfo->mode == MODE_GETNEXT || cache->reqinfo->mode == MODE_GETBULK ) {
        /*
         * Replace varbinds for data request types, but not SETs.
         */

        DEBUG_MSGTL( ( "agentx/master",
            "Master_gotResponse() beginning...\n" ) );
        for ( var = pdu->variables, request = requests; request && var;
              request = request->next, var = var->nextVariable ) {
            /*
             * Otherwise, process successful requests
             */

            DEBUG_MSGTL( ( "agentx/master",
                "  handle_agentx_response: processing: " ) );

            DEBUG_MSGOID( ( "agentx/master", var->name, var->nameLength ) );

            DEBUG_MSG( ( "agentx/master", "\n" ) );
            if ( DefaultStore_getBoolean( DsStorage_APPLICATION_ID, DsAgentBoolean_VERBOSE ) ) {

                DEBUG_MSGTL( ( "agentx/master", "    >> " ) );

                DEBUG_MSGVAR( ( "agentx/master", var ) );

                DEBUG_MSG( ( "agentx/master", "\n" ) );
            }

            /*
             * update the oid in the original request
             */
            if ( var->type != PRIOT_ENDOFMIBVIEW ) {
                Client_setVarTypedValue( request->requestvb, var->type,
                    var->val.string, var->valLen );
                Client_setVarObjid( request->requestvb, var->name,
                    var->nameLength );
            }
            request->delegated = REQUEST_IS_NOT_DELEGATED;
        }

        if ( request || var ) {
            /*
             * ack, this is bad.  The # of varbinds don't match and
             * there is no way to fix the problem
             */
            Logger_log( LOGGER_PRIORITY_ERR,
                "response to agentx request illegal.  bailing out.\n" );
            Agent_setRequestError( cache->reqinfo, requests,
                PRIOT_ERR_GENERR );
        }

        if ( cache->reqinfo->mode == MODE_GETBULK )
            BulkToNext_fixRequests( requests );
    } else {
        /*
         * mark set requests as handled
         */
        for ( request = requests; request; request = request->next ) {
            request->delegated = REQUEST_IS_NOT_DELEGATED;
        }
    }

    DEBUG_MSGTL( ( "agentx/master",
        "handle_agentx_response() finishing...\n" ) );
    AgentHandler_freeDelegatedCache( cache );
    return 1;
}

/*
 *
 * AgentX State diagram.  [mode] = internal mode it's mapped from:
 *
 * TESTSET -success-> IMPL_COMMIT -success-> CLEANUP
 * [IMPL_RESERVE1]         [IMPL_ACTION]          [IMPL_COMMIT]
 *    |                 |
 *    |                 \--failure-> IMPL_UNDO
 *    |                              [IMPL_UNDO]
 *    |
 *     --failure-> CLEANUP
 *                 [IMPL_FREE]
 */
int Master_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    Types_Session* ax_session = ( Types_Session* )handler->myvoid;
    RequestInfo* request = requests;
    Types_Pdu* pdu;
    void* cb_data;
    int result;

    DEBUG_MSGTL( ( "agentx/master",
        "agentx master handler starting, mode = 0x%02x\n",
        reqinfo->mode ) );

    if ( !ax_session ) {
        Agent_setRequestError( reqinfo, requests, PRIOT_ERR_GENERR );
        return PRIOT_ERR_NOERROR;
    }

    /*
     * build a new pdu based on the pdu type coming in
     */
    switch ( reqinfo->mode ) {
    case MODE_GET:
        pdu = Client_pduCreate( AGENTX_MSG_GET );
        break;

    case MODE_GETNEXT:
        pdu = Client_pduCreate( AGENTX_MSG_GETNEXT );
        break;

    case MODE_GETBULK: /* WWWXXX */
        pdu = Client_pduCreate( AGENTX_MSG_GETNEXT );
        break;

    case MODE_SET_RESERVE1:
        pdu = Client_pduCreate( AGENTX_MSG_TESTSET );
        break;

    case MODE_SET_RESERVE2:
        /*
         * don't do anything here for AgentX.  Assume all is fine
         * and go on since AgentX only has one test phase.
         */
        return PRIOT_ERR_NOERROR;

    case MODE_SET_ACTION:
        pdu = Client_pduCreate( AGENTX_MSG_COMMITSET );
        break;

    case MODE_SET_UNDO:
        pdu = Client_pduCreate( AGENTX_MSG_UNDOSET );
        break;

    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
        pdu = Client_pduCreate( AGENTX_MSG_CLEANUPSET );
        break;

    default:
        Logger_log( LOGGER_PRIORITY_WARNING,
            "unsupported mode for agentx/master called\n" );
        return PRIOT_ERR_NOERROR;
    }

    if ( !pdu ) {
        Agent_setRequestError( reqinfo, requests, PRIOT_ERR_GENERR );
        return PRIOT_ERR_NOERROR;
    }

    pdu->version = AGENTX_VERSION_1;
    pdu->reqid = Api_getNextTransid();
    pdu->transid = reqinfo->asp->pdu->transid;
    pdu->sessid = ax_session->subsession->sessid;
    if ( reginfo->contextName ) {
        pdu->community = ( u_char* )strdup( reginfo->contextName );
        pdu->communityLen = strlen( reginfo->contextName );
        pdu->flags |= AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT;
    }
    if ( ax_session->subsession->flags & AGENTX_MSG_FLAG_NETWORK_BYTE_ORDER )
        pdu->flags |= AGENTX_MSG_FLAG_NETWORK_BYTE_ORDER;

    while ( request ) {

        size_t nlen = request->requestvb->nameLength;
        oid* nptr = request->requestvb->name;

        DEBUG_MSGTL( ( "agentx/master", "request for variable (" ) );

        DEBUG_MSGOID( ( "agentx/master", nptr, nlen ) );

        DEBUG_MSG( ( "agentx/master", ")\n" ) );

        /*
         * loop through all the requests and create agentx ones out of them
         */

        if ( reqinfo->mode == MODE_GETNEXT || reqinfo->mode == MODE_GETBULK ) {

            if ( Api_oidCompare( nptr, nlen, request->subtree->start_a,
                     request->subtree->start_len )
                < 0 ) {

                DEBUG_MSGTL( ( "agentx/master", "inexact request preceeding region (" ) );

                DEBUG_MSGOID( ( "agentx/master", request->subtree->start_a,
                    request->subtree->start_len ) );

                DEBUG_MSG( ( "agentx/master", ")\n" ) );
                nptr = request->subtree->start_a;
                nlen = request->subtree->start_len;
                request->inclusive = 1;
            }

            if ( request->inclusive ) {

                DEBUG_MSGTL( ( "agentx/master", "INCLUSIVE varbind " ) );

                DEBUG_MSGOID( ( "agentx/master", nptr, nlen ) );

                DEBUG_MSG( ( "agentx/master", " scoped to " ) );

                DEBUG_MSGOID( ( "agentx/master", request->range_end,
                    request->range_end_len ) );

                DEBUG_MSG( ( "agentx/master", "\n" ) );
                Api_pduAddVariable( pdu, nptr, nlen, ASN01_PRIV_INCL_RANGE,
                    ( u_char* )request->range_end,
                    request->range_end_len * sizeof( oid ) );
                request->inclusive = 0;
            } else {

                DEBUG_MSGTL( ( "agentx/master", "EXCLUSIVE varbind " ) );

                DEBUG_MSGOID( ( "agentx/master", nptr, nlen ) );

                DEBUG_MSG( ( "agentx/master", " scoped to " ) );

                DEBUG_MSGOID( ( "agentx/master", request->range_end,
                    request->range_end_len ) );

                DEBUG_MSG( ( "agentx/master", "\n" ) );
                Api_pduAddVariable( pdu, nptr, nlen, ASN01_PRIV_EXCL_RANGE,
                    ( u_char* )request->range_end,
                    request->range_end_len * sizeof( oid ) );
            }
        } else {
            Api_pduAddVariable( pdu, request->requestvb->name,
                request->requestvb->nameLength,
                request->requestvb->type,
                request->requestvb->val.string,
                request->requestvb->valLen );
        }

        /*
         * mark the request as delayed
         */
        if ( pdu->command != AGENTX_MSG_CLEANUPSET )
            request->delegated = REQUEST_IS_DELEGATED;
        else
            request->delegated = REQUEST_IS_NOT_DELEGATED;

        /*
         * next...
         */
        request = request->next;
    }

    /*
     * When the master sends a CleanupSet PDU, it will never get a response
     * back from the subagent. So we shouldn't allocate the
     * netsnmp_delegated_cache structure in this case.
     */
    if ( pdu->command != AGENTX_MSG_CLEANUPSET )
        cb_data = AgentHandler_createDelegatedCache( handler, reginfo,
            reqinfo, requests,
            ( void* )ax_session );
    else
        cb_data = NULL;

    /*
     * send the requests out.
     */

    DEBUG_MSGTL( ( "agentx/master", "sending pdu (req=0x%x,trans=0x%x,sess=0x%x)\n",
        ( unsigned )pdu->reqid, ( unsigned )pdu->transid, ( unsigned )pdu->sessid ) );
    result = Session_asyncSend( ax_session, pdu, Master_gotResponse, cb_data );
    if ( result == 0 ) {
        Api_freePdu( pdu );
    }

    return PRIOT_ERR_NOERROR;
}
