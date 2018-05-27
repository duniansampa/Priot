/*
 * swinst_rpm.c:
 *     hrSWInstalledTable data access:
 */

#include "swinst.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "TextualConvention.h"
#include "siglog/data_access/swinst.h"
#include <fcntl.h>
#include <rpm/header.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmlegacy.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmts.h>

/*
    * Location of RPM package directory.
    * Used for:
    *    - reporting hrSWInstalledLast* objects
    *    - detecting when the cached contents are out of date.
    */

char pkg_directory[ UTILITIES_MAX_PATH ];

/* ---------------------------------------------------------------------
 */
void netsnmp_swinst_arch_init( void )
{
    char* rpmdbpath = NULL;
    const char* dbpath;
    struct stat stat_buf;

    rpmReadConfigFiles( NULL, NULL );
    rpmdbpath = rpmGetPath( "%{_dbpath}", NULL );
    dbpath = rpmdbpath;

    snprintf( pkg_directory, UTILITIES_MAX_PATH, "%s/Packages", dbpath );
    MEMORY_FREE( rpmdbpath );
    dbpath = NULL;
    if ( -1 == stat( pkg_directory, &stat_buf ) ) {
        Logger_log( LOGGER_PRIORITY_ERR, "Can't find directory of RPM packages\n" );
        pkg_directory[ 0 ] = '\0';
    }
}

void netsnmp_swinst_arch_shutdown( void )
{
    /* Nothing to do */
    return;
}

/* ---------------------------------------------------------------------
 */
int netsnmp_swinst_arch_load( Container_Container* container, u_int flags )
{
    rpmts ts;

    rpmdbMatchIterator mi;
    Header h;
    char *n, *v, *r, *g;
    int32_t* t;
    time_t install_time;
    size_t date_len;
    int i = 1;
    netsnmp_swinst_entry* entry;

    ts = rpmtsCreate();
    rpmtsSetVSFlags( ts, ( _RPMVSF_NOSIGNATURES | _RPMVSF_NODIGESTS ) );

    mi = rpmtsInitIterator( ts, RPMDBI_PACKAGES, NULL, 0 );
    if ( mi == NULL )
        LOGGER_LOGONCE( ( LOGGER_PRIORITY_ERR, "rpmdbOpen() failed\n" ) );

    while ( NULL != ( h = rpmdbNextIterator( mi ) ) ) {
        const u_char* dt;
        entry = netsnmp_swinst_entry_create( i++ );
        if ( NULL == entry )
            continue; /* error already logged by function */
        CONTAINER_INSERT( container, entry );

        h = headerLink( h );
        headerGetEntry( h, RPMTAG_NAME, NULL, ( void** )&n, NULL );
        headerGetEntry( h, RPMTAG_VERSION, NULL, ( void** )&v, NULL );
        headerGetEntry( h, RPMTAG_RELEASE, NULL, ( void** )&r, NULL );
        headerGetEntry( h, RPMTAG_GROUP, NULL, ( void** )&g, NULL );
        headerGetEntry( h, RPMTAG_INSTALLTIME, NULL, ( void** )&t, NULL );

        entry->swName_len = snprintf( entry->swName, sizeof( entry->swName ),
            "%s-%s-%s", n, v, r );
        if ( entry->swName_len > sizeof( entry->swName ) )
            entry->swName_len = sizeof( entry->swName );
        entry->swType = ( NULL != strstr( g, "System Environment" ) )
            ? 2 /* operatingSystem */
            : 4; /*  application    */

        install_time = *t;
        dt = Tc_dateNTime( &install_time, &date_len );
        if ( date_len != 8 && date_len != 11 ) {
            Logger_log( LOGGER_PRIORITY_ERR, "Bogus length from Tc_dateNTime for %s", entry->swName );
            entry->swDate_len = 0;
        } else {
            entry->swDate_len = date_len;
            memcpy( entry->swDate, dt, entry->swDate_len );
        }

        headerFree( h );
    }
    rpmdbFreeIterator( mi );
    rpmtsFree( ts );

    DEBUG_MSGTL( ( "swinst:load:arch", "loaded %d entries\n",
        ( int )CONTAINER_SIZE( container ) ) );

    return 0;
}
