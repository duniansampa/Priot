/*
 *  Host Resources MIB - network device group implementation - hr_network.c
 *
 */

#include "hr_network.h"
#include "AgentRegistry.h"
#include "Impl.h"
#include "host_res.h"
#include "mibII/interfaces.h"
#include "siglog/data_access/interface.h"
#include <net/if.h>

#include "PluginSettings.h"

/*********************
	 *
	 *  Kernel & interface information,
	 *   and internal forward declarations
	 *
	 *********************/

void Init_HR_Network( void );
int Get_Next_HR_Network( void );
void Save_HR_Network_Info( void );

const char* describe_networkIF( int );
int network_status( int );
int network_errors( int );
int header_hrnet( struct Variable_s*, oid*, size_t*, int,
    size_t*, WriteMethodFT** );

#define HRN_MONOTONICALLY_INCREASING

/*********************
	 *
	 *  Initialisation & common implementation functions
	 *
	 *********************/

#define HRNET_IFINDEX 1

struct Variable4_s hrnet_variables[] = {
    { HRNET_IFINDEX, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
        var_hrnet, 2, { 1, 1 } }
};
oid hrnet_variables_oid[] = { 1, 3, 6, 1, 2, 1, 25, 3, 4 };

void init_hr_network( void )
{
    init_device[ HRDEV_NETWORK ] = Init_HR_Network;
    next_device[ HRDEV_NETWORK ] = Get_Next_HR_Network;
    save_device[ HRDEV_NETWORK ] = Save_HR_Network_Info;
    dev_idx_inc[ HRDEV_NETWORK ] = 1;

    device_descr[ HRDEV_NETWORK ] = describe_networkIF;
    device_status[ HRDEV_NETWORK ] = network_status;
    device_errors[ HRDEV_NETWORK ] = network_errors;

    REGISTER_MIB( "host/hr_network", hrnet_variables, Variable4_s,
        hrnet_variables_oid );
}

/*
 * header_hrnet(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */

int header_hrnet( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
#define HRNET_ENTRY_NAME_LENGTH 11
    oid newname[ ASN01_MAX_OID_LEN ];
    int net_idx;
    int result;
    int LowIndex = -1;

    DEBUG_MSGTL( ( "host/hr_network", "var_hrnet: " ) );
    DEBUG_MSGOID( ( "host/hr_network", name, *length ) );
    DEBUG_MSG( ( "host/hr_network", " %d\n", exact ) );

    memcpy( ( char* )newname, ( char* )vp->name, vp->namelen * sizeof( oid ) );
    /*
     * Find "next" net entry 
     */

    Init_HR_Network();
    for ( ;; ) {
        net_idx = Get_Next_HR_Network();
        if ( net_idx == -1 )
            break;
        newname[ HRNET_ENTRY_NAME_LENGTH ] = net_idx;
        result = Api_oidCompare( name, *length, newname, vp->namelen + 1 );
        if ( exact && ( result == 0 ) ) {
            LowIndex = net_idx;
            break;
        }
        if ( !exact && ( result < 0 ) && ( LowIndex == -1 || net_idx < LowIndex ) ) {
            LowIndex = net_idx;
            break;
        }
    }

    if ( LowIndex == -1 ) {
        DEBUG_MSGTL( ( "host/hr_network", "... index out of range\n" ) );
        return ( MATCH_FAILED );
    }

    newname[ HRNET_ENTRY_NAME_LENGTH ] = LowIndex;
    memcpy( ( char* )name, ( char* )newname,
        ( vp->namelen + 1 ) * sizeof( oid ) );
    *length = vp->namelen + 1;
    *write_method = ( WriteMethodFT* )0;
    *var_len = sizeof( long ); /* default to 'long' results */

    DEBUG_MSGTL( ( "host/hr_network", "... get net stats " ) );
    DEBUG_MSGOID( ( "host/hr_network", name, *length ) );
    DEBUG_MSG( ( "host/hr_network", "\n" ) );
    return LowIndex;
}

/*********************
	 *
	 *  System specific implementation functions
	 *
	 *********************/

u_char*
var_hrnet( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
    int net_idx;

    net_idx = header_hrnet( vp, name, length, exact, var_len, write_method );
    if ( net_idx == MATCH_FAILED )
        return NULL;

    switch ( vp->magic ) {
    case HRNET_IFINDEX:
        vars_longReturn = net_idx & ( ( 1 << HRDEV_TYPE_SHIFT ) - 1 );
        return ( u_char* )&vars_longReturn;
    default:
        DEBUG_MSGTL( ( "snmpd", "unknown sub-id %d in var_hrnet\n",
            vp->magic ) );
    }
    return NULL;
}

/*********************
	 *
	 *  Internal implementation functions
	 *
	 *********************/

static char HRN_name[ 16 ];
static netsnmp_interface_entry* HRN_ifnet;
#define M_Interface_Scan_Next( a, b, c, d ) Interface_Scan_NextInt( a, b, c, d )

static char HRN_savedName[ 16 ];
static u_short HRN_savedFlags;
static int HRN_savedErrors;

void Init_HR_Network( void )
{
    Interface_Scan_Init();
}

int Get_Next_HR_Network( void )
{
    int HRN_index;
    if ( M_Interface_Scan_Next( &HRN_index, HRN_name, &HRN_ifnet, NULL ) == 0 )
        HRN_index = -1;

    if ( -1 == HRN_index )
        return HRN_index;

    /*
     * If the index is greater than the shift registry space,
     * this will overrun into the next device type block,
     * potentially resulting in duplicate index values
     * which may cause the agent to crash.
     *   To avoid this, we silently drop interfaces greater
     * than the shift registry size can handle.
     */
    if ( HRN_index > ( 1 << HRDEV_TYPE_SHIFT ) )
        return -1;

    return ( HRDEV_NETWORK << HRDEV_TYPE_SHIFT ) + HRN_index;
}

void Save_HR_Network_Info( void )
{
    strcpy( HRN_savedName, HRN_name );
    HRN_savedFlags = HRN_ifnet->os_flags;
    HRN_savedErrors = HRN_ifnet->stats.ierrors + HRN_ifnet->stats.oerrors;
}

const char*
describe_networkIF( int idx )
{
    static char string[ 1024 ];

    snprintf( string, sizeof( string ) - 1, "network interface %s", HRN_savedName );
    string[ sizeof( string ) - 1 ] = 0;
    return string;
}

int network_status( int idx )
{

    if ( HRN_savedFlags & IFF_UP )
        return 2; /* running */
    else
        return 5; /* down */
}

int network_errors( int idx )
{
    return HRN_savedErrors;
}
