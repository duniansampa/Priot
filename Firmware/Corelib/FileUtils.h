#ifndef FILEUTILS_H
#define FILEUTILS_H

#include "DataList.h"

/*------------------------------------------------------------------
 *
 * structures
 *
 */
typedef struct FileUtils_File_s {

    /** file name */
    char                   *name;

    /** file descriptor for the file */
    int                     fd;

    /** filesystem flags */
    int                     fsFlags;

    /** open/create mode */
    mode_t                  mode;

    /** netsnmp flags */
    u_int                   nsFlags;

    /** file stats */
    struct stat            *stats;

    /*
     * future expansion
     */
    DataList_DataList      *extras;

} FileUtils_File;



/*------------------------------------------------------------------
 *
 * Prototypes
 *
 */
FileUtils_File * FileUtils_new(const char *name, int fsFlags, mode_t mode, u_int nsFlags);

FileUtils_File * FileUtils_create(void);

FileUtils_File * FileUtils_fill(FileUtils_File * filei, const char* name, int fsFlags, mode_t mode, u_int nsFlags);

int FileUtils_release(FileUtils_File * filei);

int FileUtils_open(FileUtils_File * filei);
int FileUtils_close(FileUtils_File * filei);

/** support FileUtils_File containers */
int FileUtils_compareName(FileUtils_File *lhs, FileUtils_File *rhs);

void FileUtils_containerFree(FileUtils_File *file, void *context);



/*------------------------------------------------------------------
 *
 * flags
 *
 */
#define FILEUTILS_FILE_NO_AUTOCLOSE                         0x00000001
#define FILEUTILS_FILE_STATS                                0x00000002
#define FILEUTILS_FILE_AUTO_OPEN                            0x00000004

/*------------------------------------------------------------------
 *
 * macros
 *
 */
#define FILEUTILS_NS_FI_AUTOCLOSE(x) (0 == (x & FILEUTILS_FILE_NO_AUTOCLOSE))
#define FILEUTILS_H_NS_FI_(x) (0 == (x & PRIOT_FILE_))


#endif // FILEUTILS_H
