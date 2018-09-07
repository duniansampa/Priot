/*
 * disk_hw.c
 */

#include "disk_hw.h"
#include "AgentReadConfig.h"
#include "AgentRegistry.h"
#include "Impl.h"
#include "PluginSettings.h"
#include "ReadConfig.h"
#include "VarStruct.h"
#include "siglog/agent/hardware/fsys.h"
#include "struct.h"
#include "utilities/header_simple_table.h"

/*
 *  * config file parsing routines
 *   */
static void disk_free_config( void );
static void disk_parse_config( const char*, char* );
static void disk_parse_config_all( const char*, char* );

static netsnmp_fsys_info** _expand_disk_array( char* cptr );

#define MAX_INT_32 0x7fffffff
#define MAX_UINT_32 0xffffffff

int numdisks;
int allDisksIncluded = 0;
int maxdisks = 0;
netsnmp_fsys_info** disks = NULL;

struct Variable2_s extensible_disk_variables[] = {
    { MIBINDEX, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { MIBINDEX } },
    { ERRORNAME, asnOCTET_STR, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { ERRORNAME } },
    { DISKDEVICE, asnOCTET_STR, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKDEVICE } },
    { DISKMINIMUM, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKMINIMUM } },
    { DISKMINPERCENT, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKMINPERCENT } },
    { DISKTOTAL, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKTOTAL } },
    { DISKAVAIL, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKAVAIL } },
    { DISKUSED, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKUSED } },
    { DISKPERCENT, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKPERCENT } },
    { DISKPERCENTNODE, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKPERCENTNODE } },
    { ERRORFLAG, asnINTEGER, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { ERRORFLAG } },
    { ERRORMSG, asnOCTET_STR, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { ERRORMSG } },
    { DISKTOTALLOW, asnUNSIGNED, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKTOTALLOW } },
    { DISKTOTALHIGH, asnUNSIGNED, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKTOTALHIGH } },
    { DISKAVAILLOW, asnUNSIGNED, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKAVAILLOW } },
    { DISKAVAILHIGH, asnUNSIGNED, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKAVAILHIGH } },
    { DISKUSEDLOW, asnUNSIGNED, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKUSEDLOW } },
    { DISKUSEDHIGH, asnUNSIGNED, IMPL_OLDAPI_RONLY,
        var_extensible_disk, 1, { DISKUSEDHIGH } },
};

/*
 * Define the OID pointer to the top of the mib tree that we're
 * registering underneath 
 */
oid disk_variables_oid[] = { NETSNMP_UCDAVIS_MIB, NETSNMP_DISKMIBNUM, 1 };

void init_disk_hw( void )
{
    /*
   * register ourselves with the agent to handle our mib tree 
   */
    REGISTER_MIB( "ucd-snmp/disk", extensible_disk_variables, Variable2_s,
        disk_variables_oid );

    AgentReadConfig_priotdRegisterConfigHandler( "disk", disk_parse_config,
        disk_free_config,
        "path [ minspace | minpercent% ]" );
    AgentReadConfig_priotdRegisterConfigHandler( "includeAllDisks", disk_parse_config_all,
        disk_free_config,
        "minpercent%" );
    allDisksIncluded = 0;
}

static void
disk_free_config( void )
{
    netsnmp_fsys_info* entry;

    for ( entry = netsnmp_fsys_get_first();
          entry != NULL;
          entry = netsnmp_fsys_get_next( entry ) ) {

        entry->minspace = -1;
        entry->minpercent = -1;
        entry->flags &= ~NETSNMP_FS_FLAG_UCD;
    }
    if ( disks ) {
        free( disks );
        disks = NULL;
        maxdisks = numdisks = 0;
    }
    allDisksIncluded = 0;
}

static void
disk_parse_config( const char* token, char* cptr )
{
    char path[ STRMAX ];
    int minpercent;
    int minspace;
    netsnmp_fsys_info* entry;

    /*
   * Ensure there is space for the new entry
   */
    if ( numdisks == maxdisks ) {
        if ( !_expand_disk_array( cptr ) )
            return;
    }

    /*
   * read disk path (eg, /1 or /usr) 
   */
    ReadConfig_copyNword( cptr, path, sizeof( path ) );
    cptr = ReadConfig_skipNotWhite( cptr );
    cptr = ReadConfig_skipWhite( cptr );

    /*
   * read optional minimum disk usage spec 
   */
    if ( cptr != NULL ) {
        if ( strchr( cptr, '%' ) == NULL ) {
            minspace = atoi( cptr );
            minpercent = -1;
        } else {
            minspace = -1;
            minpercent = atoi( cptr );
        }
    } else {
        minspace = NETSNMP_DEFDISKMINIMUMSPACE;
        minpercent = -1;
    }

    /*
   * check if the disk already exists, if so then modify its
   * parameters. if it does not exist then add it
   */
    entry = netsnmp_fsys_by_path( path, NETSNMP_FS_FIND_CREATE );
    if ( entry ) {
        entry->minspace = minspace;
        entry->minpercent = minpercent;
        entry->flags |= NETSNMP_FS_FLAG_UCD;
        disks[ numdisks++ ] = entry;
    }
}

static void
disk_parse_config_all( const char* token, char* cptr )
{
    int minpercent = DISKMINPERCENT;
    netsnmp_fsys_info* entry;

    /*
   * read the minimum disk usage percent
   */
    if ( cptr != NULL ) {
        if ( strchr( cptr, '%' ) != NULL ) {
            minpercent = atoi( cptr );
        }
    }
    /*
   * if we have already seen the "includeAllDisks" directive
   * then search for the disk in the "disks" array and modify
   * the values. if we havent seen the "includeAllDisks"
   * directive then include this disk
   */
    if ( allDisksIncluded ) {
        ReadConfig_configPerror( "includeAllDisks already specified." );
        ReadConfig_error( "\tignoring: includeAllDisks %s", cptr );
    } else {

        netsnmp_fsys_load( NULL, NULL ); /* Prime the fsys H/W module */
        for ( entry = netsnmp_fsys_get_first();
              entry != NULL;
              entry = netsnmp_fsys_get_next( entry ) ) {

            if ( !( entry->flags & NETSNMP_FS_FLAG_ACTIVE ) )
                continue;
            entry->minspace = -1;
            entry->minpercent = minpercent;
            entry->flags |= NETSNMP_FS_FLAG_UCD;
            /*
           * Ensure there is space for the new entry
           */
            if ( numdisks == maxdisks ) {
                if ( !_expand_disk_array( entry->device ) )
                    return;
            }
            disks[ numdisks++ ] = entry;
        }
        allDisksIncluded = 1;
    }
}

static int _percent( unsigned long long value, unsigned long long total )
{
    float v = value, t = total, pct;

    /* avoid division by zero */
    if ( total == 0 )
        return 0;

    pct = ( v * 100 ) / t; /* Calculate percentage using floating point
                           arithmetic, to avoid overflow errors */
    pct += 0.5; /* rounding */
    return ( int )pct;
}

static netsnmp_fsys_info**
_expand_disk_array( char* cptr )
{

    if ( maxdisks == 0 )
        maxdisks = 50;
    else
        maxdisks *= 2;

    disks = ( netsnmp_fsys_info** )realloc( disks, maxdisks * sizeof( netsnmp_fsys_info* ) );
    if ( !disks ) {
        ReadConfig_configPerror( "malloc failed for new disk allocation." );
        ReadConfig_error( "\tignoring: %s", cptr );
        return NULL;
    }

    if ( maxdisks == 50 )
        memset( disks, 0, maxdisks * sizeof( netsnmp_fsys_info* ) );
    else
        memset( disks + maxdisks / 2, 0, maxdisks / 2 * sizeof( netsnmp_fsys_info* ) );

    return disks;
}

/*
 * var_extensible_disk(...
 * Arguments:
 * vp     IN      - pointer to variable entry that points here
 * name    IN/OUT  - IN/name requested, OUT/name found
 * length  IN/OUT  - length of IN/OUT oid's 
 * exact   IN      - TRUE if an exact match was requested
 * var_len OUT     - length of variable or 0 if function returned
 * write_method
 * 
 */
u_char*
var_extensible_disk( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact,
    size_t* var_len, WriteMethodFT** write_method )
{
    int disknum = 0;
    netsnmp_fsys_info* entry;
    unsigned long long val;
    static long long_ret;
    static char errmsg[ 300 ];
    Cache* cache;

    /* Update the fsys H/W module */
    cache = netsnmp_fsys_get_cache();
    CacheHandler_checkAndReload( cache );

tryAgain:
    if ( header_simple_table( vp, name, length, exact, var_len, write_method, numdisks ) )
        return ( NULL );
    disknum = name[ *length - 1 ] - 1;
    entry = disks[ disknum ];
    if ( !entry ) {
        if ( !exact || !( entry->flags & NETSNMP_FS_FLAG_UCD ) )
            goto tryAgain;
        return NULL;
    }

    switch ( vp->magic ) {
    case MIBINDEX:
        long_ret = disknum + 1;
        return ( ( u_char* )( &long_ret ) );
    case ERRORNAME: /* DISKPATH */
        *var_len = strlen( entry->path );
        return ( ( u_char* )entry->path );
    case DISKDEVICE:
        *var_len = strlen( entry->device );
        return ( ( u_char* )entry->device );
    case DISKMINIMUM:
        long_ret = entry->minspace;
        return ( ( u_char* )( &long_ret ) );
    case DISKMINPERCENT:
        long_ret = entry->minpercent;
        return ( ( u_char* )( &long_ret ) );

    case DISKTOTAL:
        val = netsnmp_fsys_size_ull( entry );
        if ( val > MAX_INT_32 )
            long_ret = MAX_INT_32;
        else
            long_ret = ( long )val;
        return ( ( u_char* )( &long_ret ) );
    case DISKTOTALLOW:
        long_ret = netsnmp_fsys_size_ull( entry ) & MAX_UINT_32;
        return ( ( u_char* )( &long_ret ) );
    case DISKTOTALHIGH:
        long_ret = netsnmp_fsys_size_ull( entry ) >> 32;
        return ( ( u_char* )( &long_ret ) );

    case DISKAVAIL:
        val = netsnmp_fsys_avail_ull( entry );
        if ( val > MAX_INT_32 )
            long_ret = MAX_INT_32;
        else
            long_ret = ( long )val;
        return ( ( u_char* )( &long_ret ) );
    case DISKAVAILLOW:
        long_ret = netsnmp_fsys_avail_ull( entry ) & MAX_UINT_32;
        return ( ( u_char* )( &long_ret ) );
    case DISKAVAILHIGH:
        long_ret = netsnmp_fsys_avail_ull( entry ) >> 32;
        return ( ( u_char* )( &long_ret ) );

    case DISKUSED:
        val = netsnmp_fsys_used_ull( entry );
        if ( val > MAX_INT_32 )
            long_ret = MAX_INT_32;
        else
            long_ret = ( long )val;
        return ( ( u_char* )( &long_ret ) );
    case DISKUSEDLOW:
        long_ret = netsnmp_fsys_used_ull( entry ) & MAX_UINT_32;
        return ( ( u_char* )( &long_ret ) );
    case DISKUSEDHIGH:
        long_ret = netsnmp_fsys_used_ull( entry ) >> 32;
        return ( ( u_char* )( &long_ret ) );

    case DISKPERCENT:
        long_ret = _percent( entry->used, entry->size );
        return ( ( u_char* )( &long_ret ) );

    case DISKPERCENTNODE:
        long_ret = _percent( entry->inums_total - entry->inums_avail, entry->inums_total );
        return ( ( u_char* )( &long_ret ) );

    case ERRORFLAG:
        long_ret = 0;
        val = netsnmp_fsys_avail_ull( entry );
        if ( ( entry->minspace >= 0 ) && ( val < entry->minspace ) )
            long_ret = 1;
        else if ( ( entry->minpercent >= 0 ) && ( _percent( entry->avail, entry->size ) < entry->minpercent ) )
            long_ret = 1;
        return ( ( u_char* )( &long_ret ) );

    case ERRORMSG:
        errmsg[ 0 ] = 0;
        val = netsnmp_fsys_avail_ull( entry );
        if ( ( entry->minspace >= 0 ) && ( val < entry->minspace ) )
            snprintf( errmsg, sizeof( errmsg ),
                "%s: less than %d free (= %d)",
                entry->path, entry->minspace,
                ( int )val );
        else if ( ( entry->minpercent >= 0 ) && ( _percent( entry->avail, entry->size ) < entry->minpercent ) )
            snprintf( errmsg, sizeof( errmsg ),
                "%s: less than %d%% free (= %d%%)",
                entry->path, entry->minpercent,
                _percent( entry->avail, entry->size ) );
        errmsg[ sizeof( errmsg ) - 1 ] = 0;
        *var_len = strlen( errmsg );
        return ( ( u_char* )( errmsg ) );
    }
    return NULL;
}
