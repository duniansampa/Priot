#ifndef DIRECTORY_H_
#define DIRECTORY_H_

#include "System/Containers/Container.h"

/** flags
*/
#define DirectoryFlags_RECURSE 0x0001
#define DirectoryFlags_RELATIVE_PATH 0x0002
#define DirectoryFlags_SORTED 0x0004
/** don't return null if dir empty */
#define DirectoryFlags_EMPTY_OK 0x0008
/** store File_s instead of fileNames */
#define DirectoryFlags_PRIOT_FILE 0x0010
/** load stats in File_s */
#define DirectoryFlags_PRIOT_FILE_STATS 0x0020

/** filters
*/
#define DirectoryFilter_EXCLUDE 0
#define DirectoryFilter_INCLUDE 1

#define Directory_read( c, d, f ) Directory_readSome( c, d, NULL, NULL, f );

/** filter function; return 1 to include file, 0 to exclude */
typedef int( DirectoryFilter_f )( const void* text, void* ctx );

/** \brief Reads file names in a directory, with an optional filter
 */
Container_Container* Directory_readSome( Container_Container* userContainer,
    const char* dirName,
    DirectoryFilter_f* filterFunction,
    void* filterCtx, u_int directoryFlags );

/** \brief  clear all containers
 */
void Directory_free( Container_Container* c );

#endif // DIRECTORY_H_
