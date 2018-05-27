#include "Iquery.h"
#include "Agent.h"
#include "AgentReadConfig.h"
#include "Api.h"
#include "System/Util/Callback.h"
#include "Client.h"
#include "System/Util/DefaultStore.h"
#include "DsAgent.h"
#include "Priot.h"
#include "PriotSettings.h"
#include "ReadConfig.h"
#include "Transports/CallbackDomain.h"
#include "V3.h"
void Iquery_parseIquerySecLevel( const char* token, char* line )
{
    int secLevel;

    if ( ( secLevel = V3_parseSecLevelConf( token, line ) ) >= 0 ) {
        DefaultStore_setInt( DsStore_APPLICATION_ID,
            DsAgentInterger_INTERNAL_SECLEVEL, secLevel );
    } else {
        ReadConfig_error( "Unknown security level: %s", line );
    }
}

void Iquery_parseIqueryVersion( const char* token, char* line )
{

    if ( !strcmp( line, "3" ) )
        DefaultStore_setInt( DsStore_APPLICATION_ID,
            DsAgentInterger_INTERNAL_VERSION, PRIOT_VERSION_3 );
    else {
        ReadConfig_error( "Unknown version: %s", line );
    }
}

/*
   * Set up a default session for running internal queries.
   * This needs to be done before the config files are read,
   *  so that it is available for "monitor" directives...
   */
static int
_Iquery_initDefaultIquerySession( int majorID, int minorID,
    void* serverargs, void* clientarg )
{
    char* secName = DefaultStore_getString( DsStore_APPLICATION_ID,
        DsAgentString_INTERNAL_SECNAME );
    if ( secName )
        Client_querySetDefaultSession(
            Iquery_userSession( secName ) );
    return ErrorCode_SUCCESS;
}

/*
   * ... Unfortunately, the internal engine ID is not set up
   * until later, so this default session is incomplete.
   * The resulting engineID probe runs into problems,
   * causing the very first internal query to time out.
   *   Updating the default session with the internal engineID
   * once it has been set, fixes this problem.
   */
static int
_Iquery_tweakDefaultIquerySession( int majorID, int minorID,
    void* serverargs, void* clientarg )
{
    u_char eID[ UTILITIES_MAX_BUFFER_SMALL ];
    size_t elen;
    Types_Session* s = Client_queryGetDefaultSessionUnchecked();

    if ( s && s->securityEngineIDLen == 0 ) {
        elen = V3_getEngineID( eID, sizeof( eID ) );
        s->securityEngineID = ( u_char* )Memory_memdup( eID, elen );
        s->securityEngineIDLen = elen;
    }
    return ErrorCode_SUCCESS;
}

void Iquery_initIquery( void )
{
    char* type = DefaultStore_getString( DsStore_LIBRARY_ID,
        DsStr_APPTYPE );
    DefaultStore_registerPremib( ASN01_OCTET_STR, type, "agentSecName",
        DsStore_APPLICATION_ID,
        DsAgentString_INTERNAL_SECNAME );
    DefaultStore_registerPremib( ASN01_OCTET_STR, type, "iquerySecName",
        DsStore_APPLICATION_ID,
        DsAgentString_INTERNAL_SECNAME );

    AgentReadConfig_priotdRegisterConfigHandler( "iqueryVersion",
        Iquery_parseIqueryVersion, NULL,
        "1 | 2c | 3" );
    AgentReadConfig_priotdRegisterConfigHandler( "iquerySecLevel",
        Iquery_parseIquerySecLevel, NULL,
        "noAuthNoPriv | authNoPriv | authPriv" );

    /*
     * Set defaults
     */
    DefaultStore_setInt( DsStore_APPLICATION_ID,
        DsAgentInterger_INTERNAL_VERSION, PRIOT_VERSION_3 );
    DefaultStore_setInt( DsStore_APPLICATION_ID,
        DsAgentInterger_INTERNAL_SECLEVEL, PRIOT_SEC_LEVEL_AUTHNOPRIV );

    Callback_register( CallbackMajor_LIBRARY,
        CallbackMinor_POST_PREMIB_READ_CONFIG,
        _Iquery_initDefaultIquerySession, NULL );
    Callback_register( CallbackMajor_LIBRARY,
        CallbackMinor_POST_READ_CONFIG,
        _Iquery_tweakDefaultIquerySession, NULL );
}

/**************************
     *
     *  APIs to construct an "internal query" session
     *
     **************************/

Types_Session* Iquery_pduSession( Types_Pdu* pdu )
{
    if ( !pdu )
        return NULL;
    if ( pdu->version == PRIOT_VERSION_3 )
        return Iquery_session( pdu->securityName,
            pdu->version,
            pdu->securityModel,
            pdu->securityLevel,
            pdu->securityEngineID,
            pdu->securityEngineIDLen );
    else
        return Iquery_session( ( char* )pdu->community,
            pdu->version,
            pdu->version + 1,
            PRIOT_SEC_LEVEL_NOAUTH,
            pdu->securityEngineID,
            pdu->securityEngineIDLen );
}

Types_Session* Iquery_userSession( char* secName )
{
    u_char eID[ UTILITIES_MAX_BUFFER_SMALL ];
    size_t elen = V3_getEngineID( eID, sizeof( eID ) );

    return Iquery_session( secName,
        PRIOT_VERSION_3,
        PRIOT_SEC_MODEL_USM,
        PRIOT_SEC_LEVEL_AUTHNOPRIV, eID, elen );
}

Types_Session* Iquery_communitySession( char* community, int version )
{
    u_char eID[ UTILITIES_MAX_BUFFER_SMALL ];
    size_t elen = V3_getEngineID( eID, sizeof( eID ) );

    return Iquery_session( community, version, version + 1,
        PRIOT_SEC_LEVEL_NOAUTH, eID, elen );
}

Types_Session* Iquery_session( char* secName, int version,
    int secModel, int secLevel,
    u_char* engineID, size_t engIDLen )
{

    /*
     * This routine creates a completely new session every time.
     * It might be worth keeping track of which 'secNames' already
     * have iquery sessions created, and re-using the appropriate one.
     */
    extern int vars_callbackMasterNum;
    Types_Session* ss = NULL;

    ss = CallbackDomain_open( vars_callbackMasterNum, NULL, NULL, NULL );
    if ( ss ) {
        ss->version = version;
        ss->securityModel = secModel;
        ss->securityLevel = secLevel;
        ss->securityEngineID = ( u_char* )Memory_memdup( engineID, engIDLen );
        ss->securityEngineIDLen = engIDLen;
        if ( version == PRIOT_VERSION_3 ) {
            ss->securityNameLen = strlen( secName );
            ss->securityName = ( char* )Memory_memdup( secName, ss->securityNameLen );
        } else {
            ss->community = ( u_char* )Memory_memdup( secName, strlen( secName ) );
            ss->communityLen = strlen( secName );
        }
        ss->myvoid = ( void* )Agent_checkOutstandingAgentRequests;
        ss->flags |= API_FLAGS_RESP_CALLBACK | API_FLAGS_DONT_PROBE;
    }

    return ss;
}
