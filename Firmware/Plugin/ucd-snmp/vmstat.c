
#include "vmstat.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "ScalarGroup.h"
#include "Vars.h"
#include "siglog/agent/hardware/cpu.h"

#include "PluginSettings.h"

FindVarMethodFT var_extensible_vmstat;

void init_vmstat( void )
{
    const oid vmstat_oid[] = { NETSNMP_UCDAVIS_MIB, 11 };

    DEBUG_MSGTL( ( "vmstat", "Initializing\n" ) );
    ScalarGroup_registerScalarGroup(
        AgentHandler_createHandlerRegistration( "vmstat", vmstat_handler,
            vmstat_oid, ASN01_OID_LENGTH( vmstat_oid ),
            HANDLER_CAN_RONLY ),
        MIBINDEX, CPUNUMCPUS );
}

int vmstat_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{
    oid obj;
    unsigned long long value = 0;
    char cp[ 300 ];
    netsnmp_cpu_info* info = netsnmp_cpu_get_byIdx( -1, 0 );

    switch ( reqinfo->mode ) {
    case MODE_GET:
        obj = requests->requestvb->name[ requests->requestvb->nameLength - 2 ];

        switch ( obj ) {
        case MIBINDEX: /* dummy value */
            Client_setVarTypedInteger( requests->requestvb, ASN01_INTEGER, 1 );
            break;

        case ERRORNAME: /* dummy name */
            sprintf( cp, "systemStats" );
            Client_setVarTypedValue( requests->requestvb, ASN01_OCTET_STR,
                cp, strlen( cp ) );
            break;

        /*
        case IOSENT:
            long_ret = vmstat(iosent);
            return ((u_char *) (&long_ret));
        case IORECEIVE:
            long_ret = vmstat(ioreceive);
            return ((u_char *) (&long_ret));
        case IORAWSENT:
            long_ret = vmstat(rawiosent);
            return ((u_char *) (&long_ret));
        case IORAWRECEIVE:
            long_ret = vmstat(rawioreceive);
            return ((u_char *) (&long_ret));
*/

        /*
         *  Raw CPU statistics
         *  Taken directly from the (overall) cpu_info structure.
         *
         *  XXX - Need some form of flag to skip objects that
         *        aren't supported on a given architecture.
         */
        case CPURAWUSER:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->user_ticks & 0xffffffff );
            break;
        case CPURAWNICE:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->nice_ticks & 0xffffffff );
            break;
        case CPURAWSYSTEM:
            /*
              * Some architecture have traditionally reported a
              *   combination of CPU statistics for this object.
              * The CPU HAL module uses 'sys2_ticks' for this,
              *   so use this value in preference to 'sys_ticks'
              *   if it has a non-zero value.
              */
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                ( info->sys2_ticks ? info->sys2_ticks : info->sys_ticks ) & 0xffffffff );
            break;
        case CPURAWIDLE:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->idle_ticks & 0xffffffff );
            break;
        case CPURAWWAIT:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->wait_ticks & 0xffffffff );
            break;
        case CPURAWKERNEL:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->kern_ticks & 0xffffffff );
            break;
        case CPURAWINTR:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->intrpt_ticks & 0xffffffff );
            break;
        case CPURAWSOFTIRQ:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->sirq_ticks & 0xffffffff );
            break;
        case CPURAWSTEAL:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->steal_ticks & 0xffffffff );
            break;
        case CPURAWGUEST:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->guest_ticks & 0xffffffff );
            break;
        case CPURAWGUESTNICE:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->guestnice_ticks & 0xffffffff );
            break;
        case CPUNUMCPUS:
            Client_setVarTypedInteger( requests->requestvb, ASN01_INTEGER,
                cpu_num & 0x7fffffff );
            break;

        /*
         *  'Cooked' CPU statistics
         *     Percentage usage of the specified statistic calculated
         *     over the period (1 min) that history is being kept for.
         *
         *   This is actually a change of behaviour for some architectures,
         *     but:
         *        a)  It ensures consistency across all systems
         *        a)  It matches the definition of the MIB objects
         *
         *   Note that this value will only be reported once the agent
         *     has a full minute's history collected.
         */
        case CPUUSER:
            if ( info->history && info->history[ 0 ].total_hist ) {
                value = ( info->user_ticks - info->history[ 0 ].user_hist ) * 100;
                if ( info->total_ticks - info->history[ 0 ].total_hist )
                    value /= ( info->total_ticks - info->history[ 0 ].total_hist );
                else
                    value = 0; /* or skip this entry */
                Client_setVarTypedInteger( requests->requestvb,
                    ASN01_INTEGER, value & 0x7fffffff );
            }
            break;
        case CPUSYSTEM:
            if ( info->history && info->history[ 0 ].total_hist ) {
                /* or sys2_ticks ??? */
                value = ( info->sys_ticks - info->history[ 0 ].sys_hist ) * 100;
                if ( info->total_ticks - info->history[ 0 ].total_hist )
                    value /= ( info->total_ticks - info->history[ 0 ].total_hist );
                else
                    value = 0; /* or skip this entry */
                Client_setVarTypedInteger( requests->requestvb,
                    ASN01_INTEGER, value & 0x7fffffff );
            }
            break;
        case CPUIDLE:
            if ( info->history && info->history[ 0 ].total_hist ) {
                value = ( info->idle_ticks - info->history[ 0 ].idle_hist ) * 100;
                if ( info->total_ticks - info->history[ 0 ].total_hist )
                    value /= ( info->total_ticks - info->history[ 0 ].total_hist );
                else
                    value = 0; /* or skip this entry */
                Client_setVarTypedInteger( requests->requestvb,
                    ASN01_INTEGER, value & 0x7fffffff );
            }
            break;

        /*
         * Similarly for the Interrupt and Context switch statistics
         *   (raw and per-second, calculated over the last minute)
         */
        case SYSRAWINTERRUPTS:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->nInterrupts & 0xffffffff );
            break;
        case SYSRAWCONTEXT:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->nCtxSwitches & 0xffffffff );
            break;
        case SYSINTERRUPTS:
            if ( info->history && info->history[ 0 ].total_hist ) {
                value = ( info->nInterrupts - info->history[ 0 ].intr_hist ) / 60;
                Client_setVarTypedInteger( requests->requestvb,
                    ASN01_INTEGER, value & 0x7fffffff );
            }
            break;
        case SYSCONTEXT:
            if ( info->history && info->history[ 0 ].total_hist ) {
                value = ( info->nCtxSwitches - info->history[ 0 ].ctx_hist ) / 60;
                Client_setVarTypedInteger( requests->requestvb,
                    ASN01_INTEGER, value & 0x7fffffff );
            }
            break;

        /*
         * Similarly for the Swap statistics...
         */
        case RAWSWAPIN:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->swapIn & 0xffffffff );
            break;
        case RAWSWAPOUT:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->swapOut & 0xffffffff );
            break;
        case SWAPIN:
            if ( info->history && info->history[ 0 ].total_hist ) {
                value = ( info->swapIn - info->history[ 0 ].swpi_hist ) / 60;
                /* ??? value *= PAGE_SIZE;  */
                Client_setVarTypedInteger( requests->requestvb,
                    ASN01_INTEGER, value & 0x7fffffff );
            }
            break;
        case SWAPOUT:
            if ( info->history && info->history[ 0 ].total_hist ) {
                value = ( info->swapOut - info->history[ 0 ].swpo_hist ) / 60;
                /* ??? value *= PAGE_SIZE;  */
                Client_setVarTypedInteger( requests->requestvb,
                    ASN01_INTEGER, value & 0x7fffffff );
            }
            break;

        /*
         * ... and the I/O statistics.
         */
        case IORAWSENT:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->pageOut & 0xffffffff );
            break;
        case IORAWRECEIVE:
            Client_setVarTypedInteger( requests->requestvb, ASN01_COUNTER,
                info->pageIn & 0xffffffff );
            break;
        case IOSENT:
            if ( info->history && info->history[ 0 ].total_hist ) {
                value = ( info->pageOut - info->history[ 0 ].pageo_hist ) / 60;
                Client_setVarTypedInteger( requests->requestvb,
                    ASN01_INTEGER, value & 0x7fffffff );
            }
            break;
        case IORECEIVE:
            if ( info->history && info->history[ 0 ].total_hist ) {
                value = ( info->pageIn - info->history[ 0 ].pagei_hist ) / 60;
                Client_setVarTypedInteger( requests->requestvb,
                    ASN01_INTEGER, value & 0x7fffffff );
            }
            break;

        default:
            /*
   XXX - The systemStats group is "holely", so walking it would
         trigger this message repeatedly.  We really need a form
         of the table column registration mechanism, that would
         work with scalar groups.
               Logger_log(LOGGER_PRIORITY_ERR,
                   "unknown object (%d) in vmstat_handler\n", (int)obj);
 */
            break;
        }
        break;

    default:
        Logger_log( LOGGER_PRIORITY_ERR,
            "unknown mode (%d) in vmstat_handler\n",
            reqinfo->mode );
        return PRIOT_ERR_GENERR;
    }

    return PRIOT_ERR_NOERROR;
}
