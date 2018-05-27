/** \file File.c
 *  @brief  File is a object type that identifies a FILE and contains the information
 *          needed to control FILE, for example: name, descriptor, mode, stats, etc.
 *
 *  This is the implementation of the File.
 *  More details about its implementation,
 *  one should read these comments.
 *
 *  \author Dunian Coutinho Sampa (duniansampa)
 *  \bug    No known bugs.
 */

#include "File.h"
#include "System/Util/Assert.h"
#include "System/Containers/Map.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

File* File_create( void )
{
    File* filei = MEMORY_MALLOC_TYPEDEF( File );

    /*
     * 0 is a valid file descriptor, so init to -1
     */
    if ( NULL != filei )
        filei->fileDescriptor = -1;
    else {
        Logger_log( LOGGER_PRIORITY_WARNING, "failed to malloc FileUtils_File structure\n" );
    }

    return filei;
}

File* File_new( const char* name, int filesystemFlags, mode_t mode, u_int priotFlags )
{
    File* filei = File_fill( NULL, name, filesystemFlags, mode, 0 );
    if ( NULL == filei )
        return NULL;

    if ( priotFlags & FileFlags_STATS ) {
        filei->stats = ( struct stat* )calloc( 1, sizeof( *( filei->stats ) ) );
        if ( NULL == filei->stats )
            DEBUG_MSGT( ( "nsfile:new", "no memory for stats\n" ) );
        else if ( stat( name, filei->stats ) != 0 )
            DEBUG_MSGT( ( "nsfile:new", "error getting stats\n" ) );
    }

    if ( priotFlags & FileFlags_AUTO_OPEN )
        File_open( filei );

    return filei;
}

File* File_fill( File* filei, const char* name, int filesystemFlags, mode_t mode, u_int priotFlags )
{
    if ( NULL == filei ) {
        filei = File_create();
        if ( NULL == filei ) /* failure already logged */
            return NULL;
    }

    if ( NULL != name )
        filei->name = strdup( name );

    filei->filesystemFlags = filesystemFlags;
    filei->priotFlags = priotFlags;
    filei->mode = mode;

    return filei;
}

int File_release( File* filei )
{
    int rc = 0;

    if ( NULL == filei )
        return -1;

    if ( ( filei->fileDescriptor > 0 ) && FILE_IS_AUTOCLOSE( filei->priotFlags ) )
        rc = close( filei->fileDescriptor );

    if ( NULL != filei->name )
        free( filei->name ); /* no point in MEMORY_FREE */

    if ( NULL != filei->stats )
        free( filei->stats );

    MEMORY_FREE( filei );

    return rc;
}

int File_open( File* filei )
{
    /*
     * basic sanity checks
     */
    if ( ( NULL == filei ) || ( NULL == filei->name ) )
        return -1;

    /*
     * if file is already open, just return the fd.
     */
    if ( -1 != filei->fileDescriptor )
        return filei->fileDescriptor;

    /*
     * try to open the file, loging an error if we failed
     */
    if ( 0 == filei->mode )
        filei->fileDescriptor = open( filei->name, filei->filesystemFlags );
    else
        filei->fileDescriptor = open( filei->name, filei->filesystemFlags, filei->mode );

    if ( filei->fileDescriptor < 0 ) {
        DEBUG_MSGTL( ( "FileUtils_File", "error opening %s (%d)\n", filei->name, errno ) );
    }

    /*
     * return results
     */
    return filei->fileDescriptor;
}

int File_close( File* filei )
{
    int rc;

    /*
     * basic sanity checks
     */
    if ( ( NULL == filei ) || ( NULL != filei->name ) )
        return -1;

    /*
     * make sure it's not already closed
     */
    if ( -1 == filei->fileDescriptor ) {
        return 0;
    }

    /*
     * close the file, logging an error if we failed
     */
    rc = close( filei->fileDescriptor );
    if ( rc < 0 ) {
        DEBUG_MSGTL( ( "FileUtils_File", "error closing %s (%d)\n", filei->name, errno ) );
    } else
        filei->fileDescriptor = -1;

    return rc;
}

void File_free( File* file, void* context )
{
    File_release( file );
}

int File_compareFilesByName( File* lhs, File* rhs )
{
    Assert_assert( ( NULL != lhs ) && ( NULL != rhs ) );
    Assert_assert( ( NULL != lhs->name ) && ( NULL != rhs->name ) );

    return strcmp( lhs->name, rhs->name );
}
