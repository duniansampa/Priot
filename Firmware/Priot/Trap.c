#include "Trap.h"
#include "Agent.h"
#include "AgentCallbacks.h"
#include "Agentx/Protocol.h"
#include "Api.h"
#include "Asn01.h"
#include "Client.h"
#include "DsAgent.h"
#include "Impl.h"
#include "ParseArgs.h"
#include "Priot.h"
#include "ReadConfig.h"
#include "System/Util/Callback.h"
#include "System/Util/DefaultStore.h"
#include "System/Util/Logger.h"
#include "System/Util/System.h"
#include "System/Util/Trace.h"
#include "System/Util/Utilities.h"
#include "Transport.h"
#include "V3.h"

struct TrapSink {
    Types_Session* sesp;
    struct TrapSink* next;
    int pdutype;
    int version;
};

struct TrapSink* trap_sinks = NULL;

const oid trap_objidEnterprisetrap[] = { NETSNMP_NOTIFICATION_MIB };
const oid trap_trapVersionId[] = { NETSNMP_SYSTEM_MIB };
const int trap_enterprisetrapLen = ASN01_OID_LENGTH( trap_objidEnterprisetrap );
const int trap_trapVersionIdLen = ASN01_OID_LENGTH( trap_trapVersionId );

#define SNMPV2_TRAPS_PREFIX PRIOT_OID_SNMPMODULES, 1, 1, 5
const oid trap_trapPrefix[] = { SNMPV2_TRAPS_PREFIX };
const oid trap_coldStartOid[] = { SNMPV2_TRAPS_PREFIX, 1 }; /* SNMPv2-MIB */

#define SNMPV2_TRAP_OBJS_PREFIX PRIOT_OID_SNMPMODULES, 1, 1, 4
const oid trap_priotTrapOid[] = { SNMPV2_TRAP_OBJS_PREFIX, 1, 0 };
const oid trap_priottrapenterpriseOid[] = { SNMPV2_TRAP_OBJS_PREFIX, 3, 0 };
const oid trap_sysuptimeOid[] = { PRIOT_OID_MIB2, 1, 3, 0 };
const size_t trap_priotTrapOidLen = ASN01_OID_LENGTH( trap_priotTrapOid );
const size_t trap_priottrapenterpriseOidLen = ASN01_OID_LENGTH( trap_priottrapenterpriseOid );
const size_t trap_sysuptimeOidLen = ASN01_OID_LENGTH( trap_sysuptimeOid );

#define SNMPV2_COMM_OBJS_PREFIX PRIOT_OID_SNMPMODULES, 18, 1
const oid trap_agentaddrOid[] = { SNMPV2_COMM_OBJS_PREFIX, 3, 0 };
const size_t trap_agentaddrOidLen = ASN01_OID_LENGTH( trap_agentaddrOid );
const oid trap_communityOid[] = { SNMPV2_COMM_OBJS_PREFIX, 4, 0 };
const size_t trap_communityOidLen = ASN01_OID_LENGTH( trap_communityOid );

#define SNMP_AUTHENTICATED_TRAPS_ENABLED 1
#define SNMP_AUTHENTICATED_TRAPS_DISABLED 2

long trap_enableauthentraps = SNMP_AUTHENTICATED_TRAPS_DISABLED;
int trap_enableauthentrapsset = 0;

/*
 * Prototypes
 */
/*
  * static int Trap_createV1TrapSession (const char *, u_short, const char *);
  * static int Trap_createV2TrapSession (const char *, u_short, const char *);
  * static int Trap_createV2InformSession (const char *, u_short, const char *);
  * static void Trap_freeTrapSession (struct TrapSink *sp);
  * static void send_v1_trap (Types_Session *, int, int);
  * static void send_v2_trap (Types_Session *, int, int, int);
  */

/*******************
     *
     * Trap session handling
     *
     *******************/

void Trap_initTraps( void )
{
}

static void
_Trap_freeTrapSession( struct TrapSink* sp )
{
    DEBUG_MSGTL( ( "trap", "freeing callback trap session (%p, %p)\n", sp, sp->sesp ) );
    Api_close( sp->sesp );
    free( sp );
}

int Trap_addTrapSession( Types_Session* ss, int pdutype, int confirm,
    int version )
{
    if ( Callback_isRegistered( CallbackMajor_APPLICATION,
             PriotdCallback_REGISTER_NOTIFICATIONS )
        == ErrorCode_SUCCESS ) {
        /*
         * something else wants to handle notification registrations
         */
        struct AgentAddTrapArgs_s args;
        DEBUG_MSGTL( ( "trap", "adding callback trap sink (%p)\n", ss ) );
        args.ss = ss;
        args.confirm = confirm;
        Callback_call( CallbackMajor_APPLICATION,
            PriotdCallback_REGISTER_NOTIFICATIONS,
            ( void* )&args );
    } else {
        /*
         * no other support exists, handle it ourselves.
         */
        struct TrapSink* new_sink;

        DEBUG_MSGTL( ( "trap", "adding internal trap sink\n" ) );
        new_sink = ( struct TrapSink* )malloc( sizeof( *new_sink ) );
        if ( new_sink == NULL )
            return 0;

        new_sink->sesp = ss;
        new_sink->pdutype = pdutype;
        new_sink->version = version;
        new_sink->next = trap_sinks;
        trap_sinks = new_sink;
    }
    return 1;
}

int Trap_removeTrapSession( Types_Session* ss )
{
    struct TrapSink *sp = trap_sinks, *prev = NULL;

    DEBUG_MSGTL( ( "trap", "removing trap sessions\n" ) );
    while ( sp ) {
        if ( sp->sesp == ss ) {
            if ( prev ) {
                prev->next = sp->next;
            } else {
                trap_sinks = sp->next;
            }
            /*
             * I don't believe you *really* want to close the session here;
             * it may still be in use for other purposes.  In particular this
             * is awkward for AgentX, since we want to call this function
             * from the session's callback.  Let's just free the trapsink
             * data structure.  [jbpn]
             */
            /*
             * Trap_freeTrapSession(sp);
             */
            DEBUG_MSGTL( ( "trap", "removing trap session (%p, %p)\n", sp, sp->sesp ) );
            free( sp );
            return 1;
        }
        prev = sp;
        sp = sp->next;
    }
    return 0;
}

void Trap_priotdFreeTrapsinks( void )
{
    struct TrapSink* sp = trap_sinks;
    DEBUG_MSGTL( ( "trap", "freeing trap sessions\n" ) );
    while ( sp ) {
        trap_sinks = trap_sinks->next;
        _Trap_freeTrapSession( sp );
        sp = trap_sinks;
    }
}

/*******************
     *
     * Trap handling
     *
     *******************/

Types_Pdu*
Trap_convertV2pduToV1( Types_Pdu* template_v2pdu )
{
    Types_Pdu* template_v1pdu;
    VariableList *first_vb, *vblist;
    VariableList* var;

    /*
     * Make a copy of the v2 Trap PDU
     *   before starting to convert this
     *   into a v1 Trap PDU.
     */
    template_v1pdu = Client_clonePdu( template_v2pdu );
    if ( !template_v1pdu ) {
        Logger_log( LOGGER_PRIORITY_WARNING,
            "send_trap: failed to copy v1 template PDU\n" );
        return NULL;
    }
    template_v1pdu->command = PRIOT_MSG_TRAP;
    first_vb = template_v1pdu->variables;
    vblist = template_v1pdu->variables;

    /*
     * The first varbind should be the system uptime.
     */
    if ( !vblist || Api_oidCompare( vblist->name, vblist->nameLength,
                        trap_sysuptimeOid, trap_sysuptimeOidLen ) ) {
        Logger_log( LOGGER_PRIORITY_WARNING,
            "send_trap: no v2 sysUptime varbind to set from\n" );
        Api_freePdu( template_v1pdu );
        return NULL;
    }
    template_v1pdu->time = *vblist->value.integer;
    vblist = vblist->next;

    /*
     * The second varbind should be the snmpTrapOID.
     */
    if ( !vblist || Api_oidCompare( vblist->name, vblist->nameLength,
                        trap_priotTrapOid, trap_priotTrapOidLen ) ) {
        Logger_log( LOGGER_PRIORITY_WARNING,
            "send_trap: no v2 trapOID varbind to set from\n" );
        Api_freePdu( template_v1pdu );
        return NULL;
    }

    /*
     * Check the v2 varbind list for any varbinds
     *  that are not valid in an SNMPv1 trap.
     *  This basically means Counter64 values.
     *
     * RFC 2089 said to omit such varbinds from the list.
     * RFC 2576/3584 say to drop the trap completely.
     */
    for ( var = vblist->next; var; var = var->next ) {
        if ( var->type == ASN01_COUNTER64 ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: v1 traps can't carry Counter64 varbinds\n" );
            Api_freePdu( template_v1pdu );
            return NULL;
        }
    }

    /*
     * Set the generic & specific trap types,
     *    and the enterprise field from the v2 varbind list.
     * If there's an agentIPAddress varbind, set the agent_addr too
     */
    if ( !Api_oidCompare( vblist->value.objectId, ASN01_OID_LENGTH( trap_trapPrefix ),
             trap_trapPrefix, ASN01_OID_LENGTH( trap_trapPrefix ) ) ) {
        /*
         * For 'standard' traps, extract the generic trap type
         *   from the snmpTrapOID value, and take the enterprise
         *   value from the 'snmpEnterprise' varbind.
         */
        template_v1pdu->trapType = vblist->value.objectId[ ASN01_OID_LENGTH( trap_trapPrefix ) ] - 1;
        template_v1pdu->specificType = 0;

        var = Client_findVarbindInList( vblist,
            trap_priottrapenterpriseOid,
            trap_priottrapenterpriseOidLen );
        if ( var ) {
            template_v1pdu->enterpriseLength = var->valueLength / sizeof( oid );
            template_v1pdu->enterprise = Api_duplicateObjid( var->value.objectId,
                template_v1pdu->enterpriseLength );
        } else {
            template_v1pdu->enterprise = NULL;
            template_v1pdu->enterpriseLength = 0; /* XXX ??? */
        }
    } else {
        /*
         * For enterprise-specific traps, split the snmpTrapOID value
         *   into enterprise and specific trap
         */
        size_t len = vblist->valueLength / sizeof( oid );
        if ( len <= 2 ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: v2 trapOID too short (%d)\n", ( int )len );
            Api_freePdu( template_v1pdu );
            return NULL;
        }
        template_v1pdu->trapType = PRIOT_TRAP_ENTERPRISESPECIFIC;
        template_v1pdu->specificType = vblist->value.objectId[ len - 1 ];
        len--;
        if ( vblist->value.objectId[ len - 1 ] == 0 )
            len--;
        MEMORY_FREE( template_v1pdu->enterprise );
        template_v1pdu->enterprise = Api_duplicateObjid( vblist->value.objectId, len );
        template_v1pdu->enterpriseLength = len;
    }
    var = Client_findVarbindInList( vblist, trap_agentaddrOid,
        trap_agentaddrOidLen );
    if ( var ) {
        memcpy( template_v1pdu->agentAddr,
            var->value.string, 4 );
    }

    /*
     * The remainder of the v2 varbind list is kept
     * as the v2 varbind list.  Update the PDU and
     * free the two redundant varbinds.
     */
    template_v1pdu->variables = vblist->next;
    vblist->next = NULL;
    Api_freeVarbind( first_vb );

    return template_v1pdu;
}

Types_Pdu*
Trap_convertV1pduToV2( Types_Pdu* template_v1pdu )
{
    Types_Pdu* template_v2pdu;
    VariableList* var;
    oid enterprise[ TYPES_MAX_OID_LEN ];
    size_t enterprise_len;

    /*
     * Make a copy of the v1 Trap PDU
     *   before starting to convert this
     *   into a v2 Trap PDU.
     */
    template_v2pdu = Client_clonePdu( template_v1pdu );
    if ( !template_v2pdu ) {
        Logger_log( LOGGER_PRIORITY_WARNING,
            "send_trap: failed to copy v2 template PDU\n" );
        return NULL;
    }
    template_v2pdu->command = PRIOT_MSG_TRAP2;

    /*
     * Insert an snmpTrapOID varbind before the original v1 varbind list
     *   either using one of the standard defined trap OIDs,
     *   or constructing this from the PDU enterprise & specific trap fields
     */
    if ( template_v1pdu->trapType == PRIOT_TRAP_ENTERPRISESPECIFIC ) {
        memcpy( enterprise, template_v1pdu->enterprise,
            template_v1pdu->enterpriseLength * sizeof( oid ) );
        enterprise_len = template_v1pdu->enterpriseLength;
        enterprise[ enterprise_len++ ] = 0;
        enterprise[ enterprise_len++ ] = template_v1pdu->specificType;
    } else {
        memcpy( enterprise, trap_coldStartOid, sizeof( trap_coldStartOid ) );
        enterprise[ 9 ] = template_v1pdu->trapType + 1;
        enterprise_len = sizeof( trap_coldStartOid ) / sizeof( oid );
    }

    var = NULL;
    if ( !Api_varlistAddVariable( &var,
             trap_priotTrapOid, trap_priotTrapOidLen,
             ASN01_OBJECT_ID,
             ( u_char* )enterprise, enterprise_len * sizeof( oid ) ) ) {
        Logger_log( LOGGER_PRIORITY_WARNING,
            "send_trap: failed to insert copied snmpTrapOID varbind\n" );
        Api_freePdu( template_v2pdu );
        return NULL;
    }
    var->next = template_v2pdu->variables;
    template_v2pdu->variables = var;

    /*
     * Insert a sysUptime varbind at the head of the v2 varbind list
     */
    var = NULL;
    if ( !Api_varlistAddVariable( &var,
             trap_sysuptimeOid, trap_sysuptimeOidLen,
             ASN01_TIMETICKS,
             ( u_char* )&( template_v1pdu->time ),
             sizeof( template_v1pdu->time ) ) ) {
        Logger_log( LOGGER_PRIORITY_WARNING,
            "send_trap: failed to insert copied sysUptime varbind\n" );
        Api_freePdu( template_v2pdu );
        return NULL;
    }
    var->next = template_v2pdu->variables;
    template_v2pdu->variables = var;

    /*
     * Append the other three conversion varbinds,
     *  (snmpTrapAgentAddr, snmpTrapCommunity & snmpTrapEnterprise)
     *  if they're not already present.
     *  But don't bomb out completely if there are problems.
     */
    var = Client_findVarbindInList( template_v2pdu->variables,
        trap_agentaddrOid, trap_agentaddrOidLen );
    if ( !var && ( template_v1pdu->agentAddr[ 0 ]
                     || template_v1pdu->agentAddr[ 1 ]
                     || template_v1pdu->agentAddr[ 2 ]
                     || template_v1pdu->agentAddr[ 3 ] ) ) {
        if ( !Api_varlistAddVariable( &( template_v2pdu->variables ),
                 trap_agentaddrOid, trap_agentaddrOidLen,
                 ASN01_IPADDRESS,
                 ( u_char* )&( template_v1pdu->agentAddr ),
                 sizeof( template_v1pdu->agentAddr ) ) )
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: failed to append snmpTrapAddr varbind\n" );
    }
    var = Client_findVarbindInList( template_v2pdu->variables,
        trap_communityOid, trap_communityOidLen );
    if ( !var && template_v1pdu->community ) {
        if ( !Api_varlistAddVariable( &( template_v2pdu->variables ),
                 trap_communityOid, trap_communityOidLen,
                 ASN01_OCTET_STR,
                 template_v1pdu->community,
                 template_v1pdu->communityLen ) )
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: failed to append snmpTrapCommunity varbind\n" );
    }
    var = Client_findVarbindInList( template_v2pdu->variables,
        trap_priottrapenterpriseOid,
        trap_priottrapenterpriseOidLen );
    if ( !var ) {
        if ( !Api_varlistAddVariable( &( template_v2pdu->variables ),
                 trap_priottrapenterpriseOid, trap_priottrapenterpriseOidLen,
                 ASN01_OBJECT_ID,
                 ( u_char* )template_v1pdu->enterprise,
                 template_v1pdu->enterpriseLength * sizeof( oid ) ) )
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: failed to append snmpEnterprise varbind\n" );
    }
    return template_v2pdu;
}

/**
 * This function allows you to make a distinction between generic
 * traps from different classes of equipment. For example, you may want
 * to handle a SNMP_TRAP_LINKDOWN trap for a particular device in a
 * different manner to a generic system SNMP_TRAP_LINKDOWN trap.
 *
 *
 * @param trap is the generic trap type.  The trap types are:
 *		- SNMP_TRAP_COLDSTART:
 *			cold start
 *		- SNMP_TRAP_WARMSTART:
 *			warm start
 *		- SNMP_TRAP_LINKDOWN:
 *			link down
 *		- SNMP_TRAP_LINKUP:
 *			link up
 *		- PRIOT_TRAP_AUTHFAIL:
 *			authentication failure
 *		- SNMP_TRAP_EGPNEIGHBORLOSS:
 *			egp neighbor loss
 *		- PRIOT_TRAP_ENTERPRISESPECIFIC:
 *			enterprise specific
 *
 * @param specific is the specific trap value.
 *
 * @param enterprise is an enterprise oid in which you want to send specific
 *	traps from.
 *
 * @param enterprise_length is the length of the enterprise oid, use macro,
 *	ASN01_OID_LENGTH, to compute length.
 *
 * @param vars is used to supply list of variable bindings to form an SNMPv2
 *	trap.
 *
 * @param context currently unused
 *
 * @param flags currently unused
 *
 * @return void
 *
 * @see Trap_sendEasyTrap
 * @see Trap_sendV2trap
 */
int Trap_sendTraps( int trap, int specific,
    const oid* enterprise, int enterprise_length,
    VariableList* vars,
    const char* context, int flags )
{
    Types_Pdu* template_v1pdu;
    Types_Pdu* template_v2pdu;
    VariableList* vblist = NULL;
    VariableList* trap_vb;
    VariableList* var;
    in_addr_t* pdu_in_addr_t;
    u_long uptime;
    struct TrapSink* sink;
    const char* v1trapaddress;
    int res = 0;

    DEBUG_MSGTL( ( "trap", "send_trap %d %d ", trap, specific ) );
    DEBUG_MSGOID( ( "trap", enterprise, enterprise_length ) );
    DEBUG_MSG( ( "trap", "\n" ) );

    if ( vars ) {
        vblist = Client_cloneVarbind( vars );
        if ( !vblist ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: failed to clone varbind list\n" );
            return -1;
        }
    }

    if ( trap == -1 ) {
        /*
         * Construct the SNMPv2-style notification PDU
         */
        if ( !vblist ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: called with NULL v2 information\n" );
            return -1;
        }
        template_v2pdu = Client_pduCreate( PRIOT_MSG_TRAP2 );
        if ( !template_v2pdu ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: failed to construct v2 template PDU\n" );
            Api_freeVarbind( vblist );
            return -1;
        }

        /*
         * Check the varbind list we've been given.
         * If it starts with a 'sysUptime.0' varbind, then use that.
         * Otherwise, prepend a suitable 'sysUptime.0' varbind.
         */
        if ( !Api_oidCompare( vblist->name, vblist->nameLength,
                 trap_sysuptimeOid, trap_sysuptimeOidLen ) ) {
            template_v2pdu->variables = vblist;
            trap_vb = vblist->next;
        } else {
            uptime = Agent_getAgentUptime();
            var = NULL;
            Api_varlistAddVariable( &var,
                trap_sysuptimeOid, trap_sysuptimeOidLen,
                ASN01_TIMETICKS, ( u_char* )&uptime, sizeof( uptime ) );
            if ( !var ) {
                Logger_log( LOGGER_PRIORITY_WARNING,
                    "send_trap: failed to insert sysUptime varbind\n" );
                Api_freePdu( template_v2pdu );
                Api_freeVarbind( vblist );
                return -1;
            }
            template_v2pdu->variables = var;
            var->next = vblist;
            trap_vb = vblist;
        }

        /*
         * 'trap_vb' should point to the snmpTrapOID.0 varbind,
         *   identifying the requested trap.  If not then bomb out.
         * If it's a 'standard' trap, then we need to append an
         *   snmpEnterprise varbind (if there isn't already one).
         */
        if ( !trap_vb || Api_oidCompare( trap_vb->name, trap_vb->nameLength,
                             trap_priotTrapOid, trap_priotTrapOidLen ) ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: no v2 trapOID varbind provided\n" );
            Api_freePdu( template_v2pdu );
            return -1;
        }
        if ( !Api_oidCompare( vblist->value.objectId, ASN01_OID_LENGTH( trap_trapPrefix ),
                 trap_trapPrefix, ASN01_OID_LENGTH( trap_trapPrefix ) ) ) {
            var = Client_findVarbindInList( template_v2pdu->variables,
                trap_priottrapenterpriseOid,
                trap_priottrapenterpriseOidLen );
            if ( !var && !Api_varlistAddVariable( &( template_v2pdu->variables ), trap_priottrapenterpriseOid, trap_priottrapenterpriseOidLen, ASN01_OBJECT_ID, enterprise, enterprise_length * sizeof( oid ) ) ) {
                Logger_log( LOGGER_PRIORITY_WARNING,
                    "send_trap: failed to add snmpEnterprise to v2 trap\n" );
                Api_freePdu( template_v2pdu );
                return -1;
            }
        }

        /*
         * If everything's OK, convert the v2 template into an SNMPv1 trap PDU.
         */
        template_v1pdu = Trap_convertV2pduToV1( template_v2pdu );
        if ( !template_v1pdu ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: failed to convert v2->v1 template PDU\n" );
        }

    } else {
        /*
         * Construct the SNMPv1 trap PDU....
         */
        template_v1pdu = Client_pduCreate( PRIOT_MSG_TRAP );
        if ( !template_v1pdu ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: failed to construct v1 template PDU\n" );
            Api_freeVarbind( vblist );
            return -1;
        }
        template_v1pdu->trapType = trap;
        template_v1pdu->specificType = specific;
        template_v1pdu->time = Agent_getAgentUptime();

        if ( Client_cloneMem( ( void** )&template_v1pdu->enterprise,
                 enterprise, enterprise_length * sizeof( oid ) ) ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: failed to set v1 enterprise OID\n" );
            Api_freeVarbind( vblist );
            Api_freePdu( template_v1pdu );
            return -1;
        }
        template_v1pdu->enterpriseLength = enterprise_length;

        template_v1pdu->flags |= PRIOT_UCD_MSG_FLAG_FORCE_PDU_COPY;
        template_v1pdu->variables = vblist;

        /*
         * ... and convert it into an SNMPv2-style notification PDU.
         */

        template_v2pdu = Trap_convertV1pduToV2( template_v1pdu );
        if ( !template_v2pdu ) {
            Logger_log( LOGGER_PRIORITY_WARNING,
                "send_trap: failed to convert v1->v2 template PDU\n" );
        }
    }

    /*
     * Check whether we're ignoring authFail traps
     */
    if ( template_v1pdu ) {
        if ( template_v1pdu->trapType == PRIOT_TRAP_AUTHFAIL && trap_enableauthentraps == SNMP_AUTHENTICATED_TRAPS_DISABLED ) {
            Api_freePdu( template_v1pdu );
            Api_freePdu( template_v2pdu );
            return 0;
        }

        /*
     * Ensure that the v1 trap PDU includes the local IP address
     */
        pdu_in_addr_t = ( in_addr_t* )template_v1pdu->agentAddr;
        v1trapaddress = DefaultStore_getString( DsStore_APPLICATION_ID,
            DsAgentString_TRAP_ADDR );
        if ( v1trapaddress != NULL ) {
            /* "v1trapaddress" was specified in config, try to resolve it */
            res = System_getHostByNameV4( v1trapaddress, pdu_in_addr_t );
        }
        if ( v1trapaddress == NULL || res < 0 ) {
            /* "v1trapaddress" was not specified in config or the resolution failed,
            * try any local address */
            *pdu_in_addr_t = System_getMyAddress();
        }
    }

    if ( template_v2pdu ) {
        /* A context name was provided, so copy it and its length to the v2 pdu
     * template. */
        if ( context != NULL ) {
            template_v2pdu->contextName = strdup( context );
            template_v2pdu->contextNameLen = strlen( context );
        }
    }

    /*
     *  Now loop through the list of trap trap_sinks
     *   and call the trap callback routines,
     *   providing an appropriately formatted PDU in each case
     */
    for ( sink = trap_sinks; sink; sink = sink->next ) {

        if ( template_v2pdu ) {
            template_v2pdu->command = sink->pdutype;
            Trap_sendTrapToSess( sink->sesp, template_v2pdu );
        }
    }
    if ( template_v1pdu )
        Callback_call( CallbackMajor_APPLICATION,
            PriotdCallback_SEND_TRAP1, template_v1pdu );
    if ( template_v2pdu )
        Callback_call( CallbackMajor_APPLICATION,
            PriotdCallback_SEND_TRAP2, template_v2pdu );
    Api_freePdu( template_v1pdu );
    Api_freePdu( template_v2pdu );
    return 0;
}

void Trap_sendEnterpriseTrapVars( int trap,
    int specific,
    const oid* enterprise, int enterprise_length,
    VariableList* vars )
{
    Trap_sendTraps( trap, specific,
        enterprise, enterprise_length,
        vars, NULL, 0 );
    return;
}

/**
 * Captures responses or the lack there of from INFORMs that were sent
 * 1) a response is received from an INFORM
 * 2) one isn't received and the retries/timeouts have failed
*/
int Trap_handleInformResponse( int op, Types_Session* session,
    int reqid, Types_Pdu* pdu,
    void* magic )
{
    /* XXX: possibly stats update */
    switch ( op ) {

    case API_CALLBACK_OP_RECEIVED_MESSAGE:
        Api_incrementStatistic( API_STAT_SNMPINPKTS );
        DEBUG_MSGTL( ( "trap", "received the inform response for reqid=%d\n",
            reqid ) );
        break;

    case API_CALLBACK_OP_TIMED_OUT:
        DEBUG_MSGTL( ( "trap",
            "received a timeout sending an inform for reqid=%d\n",
            reqid ) );
        break;

    case API_CALLBACK_OP_SEND_FAILED:
        DEBUG_MSGTL( ( "trap",
            "failed to send an inform for reqid=%d\n",
            reqid ) );
        break;

    default:
        DEBUG_MSGTL( ( "trap", "received op=%d for reqid=%d when trying to send an inform\n", op, reqid ) );
    }

    return 1;
}

/*
 * Trap_sendTrapToSess: sends a trap to a session but assumes that the
 * pdu is constructed correctly for the session type.
 */
void Trap_sendTrapToSess( Types_Session* sess, Types_Pdu* template_pdu )
{
    Types_Pdu* pdu;
    int result;

    if ( !sess || !template_pdu )
        return;

    DEBUG_MSGTL( ( "trap", "sending trap type=%d, version=%ld\n",
        template_pdu->command, sess->version ) );

    template_pdu->version = sess->version;
    pdu = Client_clonePdu( template_pdu );
    pdu->sessid = sess->sessid; /* AgentX only ? */

    if ( template_pdu->command == PRIOT_MSG_INFORM
        || template_pdu->command == AGENTX_MSG_NOTIFY ) {
        result = Api_asyncSend( sess, pdu, &Trap_handleInformResponse, NULL );

    } else {
        if ( ( sess->version == PRIOT_VERSION_3 ) && ( pdu->command == PRIOT_MSG_TRAP2 ) && ( sess->securityEngineIDLen == 0 ) ) {
            u_char tmp[ IMPL_SPRINT_MAX_LEN ];

            int len = V3_getEngineID( tmp, sizeof( tmp ) );
            pdu->securityEngineID = ( u_char* )Memory_memdup( tmp, len );
            pdu->securityEngineIDLen = len;
        }

        result = Api_send( sess, pdu );
    }

    if ( result == 0 ) {
        Api_sessPerror( "priotd: send_trap", sess );
        Api_freePdu( pdu );
    } else {
        Api_incrementStatistic( API_STAT_SNMPOUTTRAPS );
        Api_incrementStatistic( API_STAT_SNMPOUTPKTS );
    }
}

void Trap_sendTrapVars( int trap, int specific, VariableList* vars )
{
    if ( trap == PRIOT_TRAP_ENTERPRISESPECIFIC )
        Trap_sendEnterpriseTrapVars( trap, specific, trap_objidEnterprisetrap,
            ASN01_OID_LENGTH( trap_objidEnterprisetrap ), vars );
    else
        Trap_sendEnterpriseTrapVars( trap, specific, trap_trapVersionId,
            ASN01_OID_LENGTH( trap_trapVersionId ), vars );
}

/* Send a trap under a context */
void Trap_sendTrapVarsWithContext( int trap, int specific,
    VariableList* vars, const char* context )
{
    if ( trap == PRIOT_TRAP_ENTERPRISESPECIFIC )
        Trap_sendTraps( trap, specific, trap_objidEnterprisetrap,
            ASN01_OID_LENGTH( trap_objidEnterprisetrap ), vars,
            context, 0 );
    else
        Trap_sendTraps( trap, specific, trap_trapVersionId,
            ASN01_OID_LENGTH( trap_trapVersionId ), vars,
            context, 0 );
}

/**
 * Sends an SNMPv1 trap (or the SNMPv2 equivalent) to the list of
 * configured trap destinations (or "sinks"), using the provided
 * values for the generic trap type and specific trap value.
 *
 * This function eventually calls Trap_sendEnterpriseTrapVars.  If the
 * trap type is not set to PRIOT_TRAP_ENTERPRISESPECIFIC the enterprise
 * and enterprise_length paramater is set to the pre defined NETSNMP_SYSTEM_MIB
 * oid and length respectively.  If the trap type is set to
 * PRIOT_TRAP_ENTERPRISESPECIFIC the enterprise and enterprise_length
 * parameters are set to the pre-defined NETSNMP_NOTIFICATION_MIB oid and length
 * respectively.
 *
 * @param trap is the generic trap type.
 *
 * @param specific is the specific trap value.
 *
 * @return void
 *
 * @see Trap_sendEnterpriseTrapVars
 * @see Trap_sendV2trap
 */

void Trap_sendEasyTrap( int trap, int specific )
{
    Trap_sendTrapVars( trap, specific, NULL );
}

/**
 * Uses the supplied list of variable bindings to form an SNMPv2 trap,
 * which is sent to SNMPv2-capable sinks  on  the  configured  list.
 * An equivalent INFORM is sent to the configured list of inform sinks.
 * Sinks that can only handle SNMPv1 traps are skipped.
 *
 * This function eventually calls Trap_sendEnterpriseTrapVars.  If the
 * trap type is not set to PRIOT_TRAP_ENTERPRISESPECIFIC the enterprise
 * and enterprise_length paramater is set to the pre defined NETSNMP_SYSTEM_MIB
 * oid and length respectively.  If the trap type is set to
 * PRIOT_TRAP_ENTERPRISESPECIFIC the enterprise and enterprise_length
 * parameters are set to the pre-defined NETSNMP_NOTIFICATION_MIB oid and length
 * respectively.
 *
 * @param vars is used to supply list of variable bindings to form an SNMPv2
 *	trap.
 *
 * @return void
 *
 * @see Trap_sendEasyTrap
 * @see Trap_sendEnterpriseTrapVars
 */

void Trap_sendV2trap( VariableList* vars )
{
    Trap_sendTrapVars( -1, -1, vars );
}

/**
 * Similar to Trap_sendV2trap(), with the added ability to specify a context.  If
 * the last parameter is NULL, then this call is equivalent to Trap_sendV2trap().
 *
 * @param vars is used to supply the list of variable bindings for the trap.
 *
 * @param context is used to specify the context of the trap.
 *
 * @return void
 *
 * @see Trap_sendV2trap
 */
void Trap_sendV3trap( VariableList* vars, const char* context )
{
    Trap_sendTraps( -1, -1,
        trap_trapVersionId, ASN01_OID_LENGTH( trap_trapVersionId ),
        vars, context, 0 );
}

void Trap_sendTrapPdu( Types_Pdu* pdu )
{
    Trap_sendTrapVars( -1, -1, pdu->variables );
}

/*******************
     *
     * Config file handling
     *
     *******************/

void Trap_priotdParseConfigAuthtrap( const char* token, char* cptr )
{
    int i;

    i = atoi( cptr );
    if ( i == 0 ) {
        if ( strcmp( cptr, "enable" ) == 0 ) {
            i = SNMP_AUTHENTICATED_TRAPS_ENABLED;
        } else if ( strcmp( cptr, "disable" ) == 0 ) {
            i = SNMP_AUTHENTICATED_TRAPS_DISABLED;
        }
    }
    if ( i < 1 || i > 2 ) {
        ReadConfig_configPerror( "authtrapenable must be 1 or 2" );
    } else {
        if ( strcmp( token, "pauthtrapenable" ) == 0 ) {
            if ( trap_enableauthentrapsset < 0 ) {
                /*
                 * This is bogus (and shouldn't happen anyway) -- the value
                 * of snmpEnableAuthenTraps.0 is already configured
                 * read-only.
                 */
                Logger_log( LOGGER_PRIORITY_WARNING,
                    "ignoring attempted override of read-only snmpEnableAuthenTraps.0\n" );
                return;
            } else {
                trap_enableauthentrapsset++;
            }
        } else {
            if ( trap_enableauthentrapsset > 0 ) {
                /*
                 * This is bogus (and shouldn't happen anyway) -- we already
                 * read a persistent value of snmpEnableAuthenTraps.0, which
                 * we should ignore in favour of this one.
                 */
                Logger_log( LOGGER_PRIORITY_WARNING,
                    "ignoring attempted override of read-only snmpEnableAuthenTraps.0\n" );
                /*
                 * Fall through and copy in this value.
                 */
            }
            trap_enableauthentrapsset = -1;
        }
        trap_enableauthentraps = i;
    }
}

/*
 * this must be standardized somewhere, right?
 */
#define MAX_ARGS 128

static int _trap_traptype;

static void
_Trap_trapOptProc( int argc, char* const* argv, int opt )
{
    switch ( opt ) {
    case 'C':
        while ( *optarg ) {
            switch ( *optarg++ ) {
            case 'i':
                _trap_traptype = PRIOT_MSG_INFORM;
                break;
            default:
                ReadConfig_configPerror( "unknown argument passed to -C" );
                break;
            }
        }
        break;
    }
}

void Trap_priotdParseConfigTrapsess( const char* word, char* cptr )
{
    char *argv[ MAX_ARGS ], *cp = cptr;
    int argn, rc;
    Types_Session session, *ss;
    Transport_Transport* transport;
    size_t len;

    /*
     * inform or trap?  default to trap
     */
    _trap_traptype = PRIOT_MSG_TRAP2;

    /*
     * create the argv[] like array
     */
    argv[ 0 ] = strdup( "priotd-trapsess" ); /* bogus entry for getopt() */
    for ( argn = 1; cp && argn < MAX_ARGS; argn++ ) {
        char tmp[ IMPL_SPRINT_MAX_LEN ];

        cp = ReadConfig_copyNword( cp, tmp, IMPL_SPRINT_MAX_LEN );
        argv[ argn ] = strdup( tmp );
    }

    ParseArgs_parseArgs( argn, argv, &session, "C:", _Trap_trapOptProc,
        PARSEARGS_NOLOGGING | PARSEARGS_NOZERO );

    transport = Transport_openClient( "priottrap", session.peername );
    if ( transport == NULL ) {
        ReadConfig_configPerror( "priotd: failed to parse this line." );
        return;
    }
    if ( ( rc = Api_sessConfigAndOpenTransport( &session, transport ) )
        != ErrorCode_SUCCESS ) {
        session.s_snmp_errno = rc;
        session.s_errno = 0;
        return;
    }
    ss = Api_add( &session, transport, NULL, NULL );
    for ( ; argn > 0; argn-- ) {
        free( argv[ argn - 1 ] );
    }

    if ( !ss ) {
        ReadConfig_configPerror( "priotd: failed to parse this line or the remote trap receiver is down.  Possible cause:" );
        Api_sessPerror( "priotd: Trap_priotdParseConfigTrapsess()", &session );
        return;
    }

    /*
     * If this is an SNMPv3 TRAP session, then the agent is
     *   the authoritative engine, so set the engineID accordingly
     */
    if ( ss->version == PRIOT_VERSION_3 && _trap_traptype != PRIOT_MSG_INFORM && ss->securityEngineIDLen == 0 ) {
        u_char tmp[ IMPL_SPRINT_MAX_LEN ];

        len = V3_getEngineID( tmp, sizeof( tmp ) );
        ss->securityEngineID = ( u_char* )Memory_memdup( tmp, len );
        ss->securityEngineIDLen = len;
    }

    Trap_addTrapSession( ss, _trap_traptype, ( _trap_traptype == PRIOT_MSG_INFORM ), ss->version );
}
