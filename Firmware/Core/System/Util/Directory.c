#include "Directory.h"
#include "File.h"
#include "System/String.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"

static int _Directory_insertPriotFile( Container_Container* c, const char* name, struct stat* stats, u_int flags );

Container_Container* Directory_readSome( Container_Container* userContainer,
    const char* dirName,
    DirectoryFilter_f* filter,
    void* filterCtx, u_int flags )
{
    DIR* dir;
    Container_Container* container = userContainer;
    struct dirent* file;
    char path[ UTILITIES_MAX_PATH ];
    size_t dirNameLen;
    int rc;
    struct stat statbuf;
    File nsFileTmp;

    if ( ( flags & DirectoryFlags_RELATIVE_PATH ) && ( flags & DirectoryFlags_RECURSE ) ) {
        DEBUG_MSGTL( ( "directory:container",
            "no support for relative path with recursion\n" ) );
        return NULL;
    }

    DEBUG_MSGTL( ( "directory:container", "reading %s\n", dirName ) );

    /*
     * create the container, if needed
     */
    if ( NULL == container ) {
        if ( flags & DirectoryFlags_PRIOT_FILE ) {
            container = Container_find( "nsfileDirectoryContainer:"
                                        "binaryArray" );
            if ( container ) {
                container->compare = ( Container_FuncCompare* )File_compareFilesByName;
                container->freeItem = ( Container_FuncObjFunc* )File_free;
            }
        } else
            container = Container_find( "directoryContainer:cstring" );
        if ( NULL == container )
            return NULL;
        container->containerName = strdup( dirName );
        /** default to unsorted */
        if ( !( flags & DirectoryFlags_SORTED ) )
            CONTAINER_SET_OPTIONS( container, CONTAINER_KEY_UNSORTED, rc );
    }

    dir = opendir( dirName );
    if ( NULL == dir ) {
        DEBUG_MSGTL( ( "directory:container", "  not a dir\n" ) );
        if ( container != userContainer )
            Directory_free( container );
        return NULL;
    }

    /** copy dirname into path */
    if ( flags & DirectoryFlags_RELATIVE_PATH )
        dirNameLen = 0;
    else {
        dirNameLen = strlen( dirName );
        String_copyTruncate( path, dirName, sizeof( path ) );
        if ( ( dirNameLen + 2 ) > sizeof( path ) ) {
            /** not enough room for files */
            closedir( dir );
            if ( container != userContainer )
                Directory_free( container );
            return NULL;
        }
        path[ dirNameLen ] = '/';
        path[ ++dirNameLen ] = '\0';
    }

    /** iterate over dir */
    while ( ( file = readdir( dir ) ) ) {

        if ( ( file->d_name == NULL ) || ( file->d_name[ 0 ] == 0 ) )
            continue;

        /** skip '.' and '..' */
        if ( ( file->d_name[ 0 ] == '.' ) && ( ( file->d_name[ 1 ] == 0 ) || ( ( file->d_name[ 1 ] == '.' ) && ( ( file->d_name[ 2 ] == 0 ) ) ) ) )
            continue;

        String_copyTruncate( &path[ dirNameLen ], file->d_name, sizeof( path ) - dirNameLen );
        if ( NULL != filter ) {
            if ( flags & DirectoryFlags_PRIOT_FILE_STATS ) {
                /** use local vars for now */
                if ( stat( path, &statbuf ) != 0 ) {
                    Logger_log( LOGGER_PRIORITY_ERR, "could not stat %s\n", file->d_name );
                    break;
                }
                nsFileTmp.stats = &statbuf;
                nsFileTmp.name = path;
                rc = ( *filter )( &nsFileTmp, filterCtx );
            } else
                rc = ( *filter )( path, filterCtx );
            if ( 0 == rc ) {
                DEBUG_MSGTL( ( "directory:container:filtered", "%s\n",
                    file->d_name ) );
                continue;
            }
        }

        DEBUG_MSGTL( ( "directory:container:found", "%s\n", path ) );
        if ( ( flags & DirectoryFlags_RECURSE ) && ( file->d_type == DT_DIR )

                 ) {
            /** xxx add the dir as well? not for now.. maybe another flag? */
            Directory_read( container, path, flags );
        } else if ( flags & DirectoryFlags_PRIOT_FILE ) {
            if ( _Directory_insertPriotFile( container, file->d_name,
                     filter ? &statbuf : NULL, flags )
                < 0 )
                break;
        } else {
            char* dup = strdup( path );
            if ( NULL == dup ) {
                Logger_log( LOGGER_PRIORITY_ERR, "strdup failed while building directory container\n" );
                break;
            }
            rc = CONTAINER_INSERT( container, dup );
            if ( -1 == rc ) {
                DEBUG_MSGTL( ( "directory:container", "  err adding %s\n", path ) );
                free( dup );
            }
        }
    } /* while */

    closedir( dir );

    rc = CONTAINER_SIZE( container );
    DEBUG_MSGTL( ( "directory:container", "  container now has %d items\n", rc ) );
    if ( ( 0 == rc ) && !( flags & DirectoryFlags_EMPTY_OK ) ) {
        Directory_free( container );
        return NULL;
    }

    return container;
}

void Directory_free( Container_Container* container )
{
    CONTAINER_FREE_ALL( container, NULL );
    CONTAINER_FREE( container );
}

static int _Directory_insertPriotFile( Container_Container* c, const char* name, struct stat* stats,
    u_int flags )
{
    int rc;
    File* nsFile = File_new( name, 0, 0, 0 );
    if ( NULL == nsFile ) {
        Logger_log( LOGGER_PRIORITY_ERR, "error creating ns_file\n" );
        return -1;
    }

    if ( flags & DirectoryFlags_PRIOT_FILE_STATS ) {
        nsFile->stats = ( struct stat* )calloc( 1, sizeof( *( nsFile->stats ) ) );
        if ( NULL == nsFile->stats ) {
            Logger_log( LOGGER_PRIORITY_ERR, "error creating stats for ns_file\n" );
            File_release( nsFile );
            return -1;
        }

        /** use stats from earlier if we have them */
        if ( stats ) {
            memcpy( nsFile->stats, stats, sizeof( *stats ) );
        } else if ( stat( nsFile->name, nsFile->stats ) < 0 ) {
            Logger_log( LOGGER_PRIORITY_ERR, "stat() failed for ns_file\n" );
            File_release( nsFile );
            return -1;
        }
    }

    rc = CONTAINER_INSERT( c, nsFile );
    if ( -1 == rc ) {
        DEBUG_MSGTL( ( "directory:container", "  err adding %s\n", name ) );
        File_release( nsFile );
    }

    return 0;
}
