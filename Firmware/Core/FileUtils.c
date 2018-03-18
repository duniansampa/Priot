#include "FileUtils.h"
#include "Tools.h"
#include "Logger.h"
#include "Debug.h"
#include "DataList.h"
#include "Assert.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>


/*------------------------------------------------------------------
 *
 * Prototypes
 *
 */
/*------------------------------------------------------------------
 *
 * Core Functions
 *
 */

/**
 * allocate a FileUtils_File structure
 *
 * This routine should be used instead of allocating on the stack,
 * for future compatability.
 */

FileUtils_File * FileUtils_create(void)
{
    FileUtils_File *filei = TOOLS_MALLOC_TYPEDEF(FileUtils_File);

    /*
     * 0 is a valid file descriptor, so init to -1
     */
    if (NULL != filei)
        filei->fd = -1;
    else {
        Logger_log(LOGGER_PRIORITY_WARNING,"failed to malloc FileUtils_File structure\n");
    }

    return filei;
}

/**
 * open file and get stats
 */
FileUtils_File * FileUtils_new(const char *name, int fsFlags, mode_t mode, u_int nsFlags)
{
    FileUtils_File *filei = FileUtils_fill(NULL, name, fsFlags, mode, 0);
    if (NULL == filei)
        return NULL;

    if (nsFlags & FILEUTILS_FILE_STATS) {
        filei->stats = (struct stat*)calloc(1, sizeof(*(filei->stats)));
        if (NULL == filei->stats)
            DEBUG_MSGT(("nsfile:new", "no memory for stats\n"));
        else if (stat(name, filei->stats) != 0)
            DEBUG_MSGT(("nsfile:new", "error getting stats\n"));
    }

    if (nsFlags & FILEUTILS_FILE_AUTO_OPEN)
        FileUtils_open(filei);

    return filei;
}


/**
 * fill core members in a FileUtils_File structure
 *
 * @param filei      structure to fill; if NULL, a new one will be allocated
 * @param name       file name
 * @param fs_flags   filesystem flags passed to open()
 * @param mode       optional filesystem open modes passed to open()
 * @param ns_flags   net-snmp flags
 */
FileUtils_File * FileUtils_fill(FileUtils_File * filei, const char* name, int fsFlags, mode_t mode, u_int nsFlags)
{
    if (NULL == filei) {
        filei = FileUtils_create();
        if (NULL == filei) /* failure already logged */
            return NULL;
    }

    if (NULL != name)
        filei->name = strdup(name);

    filei->fsFlags = fsFlags;
    filei->nsFlags = nsFlags;
    filei->mode = mode;

    return filei;
}

/**
 * release a FileUtils_File structure
 *
 * @retval  see close() man page
 */
int FileUtils_release(FileUtils_File * filei)
{
    int rc = 0;

    if (NULL == filei)
        return -1;

    if ((filei->fd > 0) && FILEUTILS_NS_FI_AUTOCLOSE(filei->nsFlags))
        rc = close(filei->fd);

    if (NULL != filei->name)
        free(filei->name); /* no point in SNMP_FREE */

    if (NULL != filei->extras)
        DataList_freeAll(filei->extras);

    if (NULL != filei->stats)
        free(filei->stats);

    TOOLS_FREE(filei);

    return rc;
}

/**
 * open the file associated with a FileUtils_File structure
 *
 * @retval -1  : error opening file
 * @retval >=0 : file descriptor for opened file
 */
int FileUtils_open(FileUtils_File * filei)
{
    /*
     * basic sanity checks
     */
    if ((NULL == filei) || (NULL == filei->name))
        return -1;

    /*
     * if file is already open, just return the fd.
     */
    if (-1 != filei->fd)
        return filei->fd;

    /*
     * try to open the file, loging an error if we failed
     */
    if (0 == filei->mode)
        filei->fd = open(filei->name, filei->fsFlags);
    else
        filei->fd = open(filei->name, filei->fsFlags, filei->mode);

    if (filei->fd < 0) {
        DEBUG_MSGTL(("FileUtils_File", "error opening %s (%d)\n", filei->name, errno));
    }

    /*
     * return results
     */
    return filei->fd;
}


/**
 * close the file associated with a netsnmp_file structure
 *
 * @retval  0 : success
 * @retval -1 : error
 */
int FileUtils_close(FileUtils_File * filei)
{
    int rc;

    /*
     * basic sanity checks
     */
    if ((NULL == filei) || (NULL != filei->name))
        return -1;

    /*
     * make sure it's not already closed
     */
    if (-1 == filei->fd) {
        return 0;
    }

    /*
     * close the file, logging an error if we failed
     */
    rc = close(filei->fd);
    if (rc < 0) {
        DEBUG_MSGTL(("FileUtils_File", "error closing %s (%d)\n", filei->name, errno));
    }
    else
        filei->fd = -1;

    return rc;
}

void FileUtils_containerFree(FileUtils_File *file, void *context)
{
    FileUtils_release(file);
}

int FileUtils_compareName(FileUtils_File *lhs, FileUtils_File *rhs)
{
    Assert_assert((NULL != lhs) && (NULL != rhs));
    Assert_assert((NULL != lhs->name) && (NULL != rhs->name));

    return strcmp(lhs->name, rhs->name);
}
