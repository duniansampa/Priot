#include "proc.h"
#include "AgentHandler.h"
#include "AgentReadConfig.h"
#include "AgentRegistry.h"
#include "Impl.h"
#include "System/Util/Logger.h"
#include "PluginSettings.h"
#include "ReadConfig.h"
#include "VarStruct.h"
#include "siglog/data_access/swrun.h"
#include "struct.h"
#include "util_funcs.h"
#include "utilities/header_simple_table.h"

static struct myproc* get_proc_instance( struct myproc*, oid );
struct myproc* procwatch = NULL;
static struct extensible fixproc;
int numprocs = 0;

void init_proc( void )
{

    /*
     * define the structure we're going to ask the agent to register our
     * information at 
     */
    struct Variable2_s extensible_proc_variables[] = {
        { MIBINDEX, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
            var_extensible_proc, 1, { MIBINDEX } },
        { ERRORNAME, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_proc, 1, { ERRORNAME } },
        { PROCMIN, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
            var_extensible_proc, 1, { PROCMIN } },
        { PROCMAX, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
            var_extensible_proc, 1, { PROCMAX } },
        { PROCCOUNT, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
            var_extensible_proc, 1, { PROCCOUNT } },
        { ERRORFLAG, ASN01_INTEGER, IMPL_OLDAPI_RONLY,
            var_extensible_proc, 1, { ERRORFLAG } },
        { ERRORMSG, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_proc, 1, { ERRORMSG } },
        { ERRORFIX, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
            var_extensible_proc, 1, { ERRORFIX } },
        { ERRORFIXCMD, ASN01_OCTET_STR, IMPL_OLDAPI_RONLY,
            var_extensible_proc, 1, { ERRORFIXCMD } }
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid proc_variables_oid[] = { NETSNMP_UCDAVIS_MIB, NETSNMP_PROCMIBNUM, 1 };

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB( "ucd-snmp/proc", extensible_proc_variables, Variable2_s,
        proc_variables_oid );

    AgentReadConfig_priotdRegisterConfigHandler( "proc", proc_parse_config,
        proc_free_config,
        "process-name [max-num] [min-num]" );
    AgentReadConfig_priotdRegisterConfigHandler( "procfix", procfix_parse_config, NULL,
        "process-name program [arguments...]" );
}

/*
 * Define priotd.conf reading routines first.  They get called
 * automatically by the invocation of a macro in the proc.h file. 
 */

void proc_free_config( void )
{
    struct myproc *ptmp, *ptmp2;

    for ( ptmp = procwatch; ptmp != NULL; ) {
        ptmp2 = ptmp;
        ptmp = ptmp->next;
        free( ptmp2 );
    }
    procwatch = NULL;
    numprocs = 0;
}

/*
 * find a give entry in the linked list associated with a proc name 
 */
static struct myproc*
get_proc_by_name( char* name )
{
    struct myproc* ptmp;

    if ( name == NULL )
        return NULL;

    for ( ptmp = procwatch; ptmp != NULL && strcmp( ptmp->name, name ) != 0;
          ptmp = ptmp->next )
        ;
    return ptmp;
}

void procfix_parse_config( const char* token, char* cptr )
{
    char tmpname[ STRMAX ];
    struct myproc* procp;

    /*
     * don't allow two entries with the same name 
     */
    cptr = ReadConfig_copyNword( cptr, tmpname, sizeof( tmpname ) );
    if ( ( procp = get_proc_by_name( tmpname ) ) == NULL ) {
        ReadConfig_configPerror( "No proc entry registered for this proc name yet." );
        return;
    }

    if ( strlen( cptr ) > sizeof( procp->fixcmd ) ) {
        ReadConfig_configPerror( "fix command too long." );
        return;
    }

    strcpy( procp->fixcmd, cptr );
}

void proc_parse_config( const char* token, char* cptr )
{
    char tmpname[ STRMAX ];
    struct myproc** procp = &procwatch;

    /*
     * don't allow two entries with the same name 
     */
    ReadConfig_copyNword( cptr, tmpname, sizeof( tmpname ) );
    if ( get_proc_by_name( tmpname ) != NULL ) {
        ReadConfig_configPerror( "Already have an entry for this process." );
        return;
    }

    /*
     * skip past used ones 
     */
    while ( *procp != NULL )
        procp = &( ( *procp )->next );

    ( *procp ) = ( struct myproc* )calloc( 1, sizeof( struct myproc ) );
    if ( *procp == NULL )
        return; /* memory alloc error */
    numprocs++;
    /*
     * not blank and not a comment 
     */
    ReadConfig_copyNword( cptr, ( *procp )->name, sizeof( ( *procp )->name ) );
    cptr = ReadConfig_skipNotWhite( cptr );
    if ( ( cptr = ReadConfig_skipWhite( cptr ) ) ) {
        ( *procp )->max = atoi( cptr );
        cptr = ReadConfig_skipNotWhite( cptr );
        if ( ( cptr = ReadConfig_skipWhite( cptr ) ) )
            ( *procp )->min = atoi( cptr );
        else
            ( *procp )->min = 0;
    } else {
        /* Default to asssume that we require at least one
         *  such process to be running, but no upper limit */
        ( *procp )->max = 0;
        ( *procp )->min = 1;
        /* This frees "proc <procname> 0 0" to monitor
         * processes that should _not_ be running. */
    }

    DEBUG_MSGTL( ( "ucd-snmp/proc", "Read:  %s (%d) (%d)\n",
        ( *procp )->name, ( *procp )->max, ( *procp )->min ) );
}

/*
 * The routine that handles everything 
 */

u_char*
var_extensible_proc( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact,
    size_t* var_len, WriteMethodFT** write_method )
{

    struct myproc* proc;
    static long long_ret;
    static char errmsg[ 300 ];

    if ( header_simple_table( vp, name, length, exact, var_len, write_method, numprocs ) )
        return ( NULL );

    if ( ( proc = get_proc_instance( procwatch, name[ *length - 1 ] ) ) ) {
        switch ( vp->magic ) {
        case MIBINDEX:
            long_ret = name[ *length - 1 ];
            return ( ( u_char* )( &long_ret ) );
        case ERRORNAME: /* process name to check for */
            *var_len = strlen( proc->name );
            return ( ( u_char* )( proc->name ) );
        case PROCMIN:
            long_ret = proc->min;
            return ( ( u_char* )( &long_ret ) );
        case PROCMAX:
            long_ret = proc->max;
            return ( ( u_char* )( &long_ret ) );
        case PROCCOUNT:
            long_ret = sh_count_procs( proc->name );
            return ( ( u_char* )( &long_ret ) );
        case ERRORFLAG:
            long_ret = sh_count_procs( proc->name );
            if ( long_ret >= 0 &&
                /* Too few processes running */
                ( ( proc->min && long_ret < proc->min ) ||
                     /* Too many processes running */
                     ( proc->max && long_ret > proc->max ) ||
                     /* Processes running that shouldn't be */
                     ( proc->min == 0 && proc->max == 0 && long_ret > 0 ) ) ) {
                long_ret = 1;
            } else {
                long_ret = 0;
            }
            return ( ( u_char* )( &long_ret ) );
        case ERRORMSG:
            long_ret = sh_count_procs( proc->name );
            if ( long_ret < 0 ) {
                errmsg[ 0 ] = 0; /* catch out of mem errors return 0 count */
            } else if ( proc->min && long_ret < proc->min ) {
                if ( long_ret > 0 )
                    snprintf( errmsg, sizeof( errmsg ),
                        "Too few %s running (# = %d)",
                        proc->name, ( int )long_ret );
                else
                    snprintf( errmsg, sizeof( errmsg ),
                        "No %s process running", proc->name );
            } else if ( proc->max && long_ret > proc->max ) {
                snprintf( errmsg, sizeof( errmsg ),
                    "Too many %s running (# = %d)",
                    proc->name, ( int )long_ret );
            } else if ( proc->min == 0 && proc->max == 0 && long_ret > 0 ) {
                snprintf( errmsg, sizeof( errmsg ),
                    "%s process should not be running.", proc->name );
            } else {
                errmsg[ 0 ] = 0;
            }
            errmsg[ sizeof( errmsg ) - 1 ] = 0;
            *var_len = strlen( errmsg );
            return ( ( u_char* )errmsg );
        case ERRORFIX:
            *write_method = fixProcError;
            vars_longReturn = fixproc.result;
            return ( ( u_char* )&vars_longReturn );
        case ERRORFIXCMD:
            if ( proc->fixcmd ) {
                *var_len = strlen( proc->fixcmd );
                return ( u_char* )proc->fixcmd;
            }
            errmsg[ 0 ] = 0;
            *var_len = 0;
            return ( ( u_char* )errmsg );
        }
        return NULL;
    }
    return NULL;
}

int fixProcError( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{

    struct myproc* proc;
    long tmp = 0;

    if ( ( proc = get_proc_instance( procwatch, name[ 10 ] ) ) ) {
        if ( var_val_type != ASN01_INTEGER ) {
            Logger_log( LOGGER_PRIORITY_ERR, "Wrong type != int\n" );
            return PRIOT_ERR_WRONGTYPE;
        }
        tmp = *( ( long* )var_val );
        if ( tmp == 1 && action == IMPL_COMMIT ) {
            if ( proc->fixcmd[ 0 ] ) {
                strcpy( fixproc.command, proc->fixcmd );
                exec_command( &fixproc );
            }
        }
        return PRIOT_ERR_NOERROR;
    }
    return PRIOT_ERR_WRONGTYPE;
}

static struct myproc*
get_proc_instance( struct myproc* proc, oid inst )
{
    int i;

    if ( proc == NULL )
        return ( NULL );
    for ( i = 1; ( i != ( int )inst ) && ( proc != NULL ); i++ )
        proc = proc->next;
    return ( proc );
}

int sh_count_procs( char* procname )
{
    return swrun_count_processes_by_name( procname );
}
