#ifndef FILE_H_
#define FILE_H_

/** \file File.h
 *  \brief  File is a object type that identifies a FILE and contains the information
 *          needed to control FILE, for example: name, descriptor, mode, stats, etc.
 *
 *  This contains the prototypes for the File and eventually
 *  any macros, constants, or global variables you will need.
 *
 *  \author Dunian Coutinho Sampa (duniansampa)
 *  \bug    No known bugs.
 */

#include "System/Containers/Map.h"

/** flags
 */
#define FileFlags_NO_AUTOCLOSE 0x00000001
#define FileFlags_STATS 0x00000002
#define FileFlags_AUTO_OPEN 0x00000004

/** macros
 */
#define FILE_IS_AUTOCLOSE( x ) ( 0 == ( x & FileFlags_NO_AUTOCLOSE ) )

typedef struct File_s {

    /** file name */
    char* name;

    /** file descriptor for the file */
    int fileDescriptor;

    /** filesystem flags */
    int filesystemFlags;

    /** open/create mode */
    mode_t mode;

    /** Priot flags */
    u_int priotFlags;

    /** file stats */
    struct stat* stats;

} File;

/** \brief  allocate a File structure
 *
 * This routine should be used instead of allocating on the stack,
 * for future compatability.
 */
File* File_create( void );

/** \brief  open file and get stats
 *
 * \param   name - file name
 * \param   filesystemFlags - filesystem flags passed to open()
 * \param   mode - optional filesystem open modes passed to open()
 * \param   priotFlags - priot flags
 */
File* File_new( const char* name, int filesystemFlags, mode_t mode, u_int priotFlags );

/** \brief  fill core members in a File_s structure
 *
 * \param   filei - structure to fill; if NULL, a new one will be allocated
 * \param   name - file name
 * \param   filesystemFlags - filesystem flags passed to open()
 * \param   mode - optional filesystem open modes passed to open()
 * \param   priotFlags - priot flags
 */
File* File_fill( File* filei, const char* name, int filesystemFlags, mode_t mode, u_int priotFlags );

/** \brief  closes the file if auto close flag is true and releases a file structure.
 *
 *  \retval see close() man page
 */
int File_release( File* filei );

/** \brief  opens the file associated with a File_s structure
 *
 *  \retval -1   - error opening file
 *  \retval >=0  - file descriptor for opened file
 */
int File_open( File* filei );

/** \brief  closes the file associated with a netsnmp_file structure
 *
 *  \retval  0  - success
 *  \retval -1  - error
 */
int File_close( File* filei );

/** \brief  compares two files by their names.
 *          returns 0 if lhs->name == rhs->name.
*/
int File_compareFilesByName( File* lhs, File* rhs );

/** \brief  it works the same as the File_release function.
 *          It is to be used with the container.
 */
void File_free( File* file, void* context );

#endif // FILE_H_
