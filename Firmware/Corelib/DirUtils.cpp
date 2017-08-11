#include "DirUtils.h"
#include "Debug.h"
#include "Tools.h"
#include "Logger.h"
#include "FileUtils.h"
#include "Strlcpy.h"


static int DirUtils_insertNsfile( Container_Container *c, const char *name, struct stat *stats, u_int flags);

/*
 * read file names in a directory, with an optional filter
 */
Container_Container * DirUtils_readSome(Container_Container *userContainer,
                                      const char *dirname,
                                      DirUtils_FuncFilter *filter,
                                      void *filterCtx, u_int flags)
{
    DIR                 *dir;
    Container_Container *container = userContainer;
    struct dirent       *file;
    char                 path[TOOLS_MAXPATH];
    size_t               dirname_len;
    int                  rc;
    struct stat          statbuf;
    FileUtils_File         nsFileTmp;

    if ((flags & DIRUTILS_DIR_RELATIVE_PATH) && (flags & DIRUTILS_DIR_RECURSE)) {
        DEBUG_MSGTL(("directory:container",
                    "no support for relative path with recursion\n"));
        return NULL;
    }

    DEBUG_MSGTL(("directory:container", "reading %s\n", dirname));

    /*
     * create the container, if needed
     */
    if (NULL == container) {
        if (flags & DIRUTILS_DIR_NSFILE) {
            container = Container_find("nsfileDirectoryContainer:"
                                               "binaryArray");
            if (container) {
                container->compare = (Container_FuncCompare*) FileUtils_compareName;
                container->freeItem = (Container_FuncObjFunc *) FileUtils_containerFree;
            }
        }
        else
            container = Container_find("directoryContainer:cstring");
        if (NULL == container)
            return NULL;
        container->containerName = strdup(dirname);
        /** default to unsorted */
        if (! (flags & DIRUTILS_DIR_SORTED))
            CONTAINER_SET_OPTIONS(container, CONTAINER_KEY_UNSORTED, rc);
    }

    dir = opendir(dirname);
    if (NULL == dir) {
        DEBUG_MSGTL(("directory:container", "  not a dir\n"));
        if (container != userContainer)
            DirUtils_free(container);
        return NULL;
    }

    /** copy dirname into path */
    if (flags & DIRUTILS_DIR_RELATIVE_PATH)
        dirname_len = 0;
    else {
        dirname_len = strlen(dirname);
        Strlcpy_strlcpy(path, dirname, sizeof(path));
        if ((dirname_len + 2) > sizeof(path)) {
            /** not enough room for files */
            closedir(dir);
            if (container != userContainer)
                DirUtils_free(container);
            return NULL;
        }
        path[dirname_len] = '/';
        path[++dirname_len] = '\0';
    }

    /** iterate over dir */
    while ((file = readdir(dir))) {

        if ((file->d_name == NULL) || (file->d_name[0] == 0))
            continue;

        /** skip '.' and '..' */
        if ((file->d_name[0] == '.') &&
            ((file->d_name[1] == 0) ||
             ((file->d_name[1] == '.') && ((file->d_name[2] == 0)))))
            continue;

        Strlcpy_strlcpy(&path[dirname_len], file->d_name, sizeof(path) - dirname_len);
        if (NULL != filter) {
            if (flags & DIRUTILS_DIR_NSFILE_STATS) {
                /** use local vars for now */
                if (stat(path, &statbuf) != 0) {
                    Logger_log(LOGGER_PRIORITY_ERR, "could not stat %s\n", file->d_name);
                    break;
                }
                nsFileTmp.stats = &statbuf;
                nsFileTmp.name = path;
                rc = (*filter)(&nsFileTmp, filterCtx);
            }
            else
                rc = (*filter)(path, filterCtx);
            if (0 == rc) {
                DEBUG_MSGTL(("directory:container:filtered", "%s\n",
                            file->d_name));
                continue;
            }
        }

        DEBUG_MSGTL(("directory:container:found", "%s\n", path));
        if ((flags & DIRUTILS_DIR_RECURSE) && (file->d_type == DT_DIR)

            ) {
            /** xxx add the dir as well? not for now.. maybe another flag? */
            DirUtils_read(container, path, flags);
        }
        else if (flags & DIRUTILS_DIR_NSFILE) {
            if (DirUtils_insertNsfile( container, file->d_name,
                                filter ? &statbuf : NULL, flags ) < 0)
                break;
        }
        else {
            char *dup = strdup(path);
            if (NULL == dup) {
                Logger_log(LOGGER_PRIORITY_ERR, "strdup failed while building directory container\n");
                break;
            }
            rc = CONTAINER_INSERT(container, dup);
            if (-1 == rc ) {
                DEBUG_MSGTL(("directory:container", "  err adding %s\n", path));
                free(dup);
            }
        }
    } /* while */

    closedir(dir);

    rc = CONTAINER_SIZE(container);
    DEBUG_MSGTL(("directory:container", "  container now has %d items\n", rc));
    if ((0 == rc) && !(flags & DIRUTILS_DIR_EMPTY_OK)) {
        DirUtils_free(container);
        return NULL;
    }

    return container;
}

void DirUtils_free(Container_Container *container)
{
    CONTAINER_FREE_ALL(container, NULL);
    CONTAINER_FREE(container);
}

static int DirUtils_insertNsfile( Container_Container *c, const char *name, struct stat *stats,
                u_int flags)
{
    int           rc;
    FileUtils_File *nsFile = FileUtils_new(name, 0, 0, 0);
    if (NULL == nsFile) {
        Logger_log(LOGGER_PRIORITY_ERR, "error creating ns_file\n");
        return -1;
    }

    if (flags & DIRUTILS_DIR_NSFILE_STATS) {
        nsFile->stats = (struct stat*)calloc(1,sizeof(*(nsFile->stats)));
        if (NULL == nsFile->stats) {
            Logger_log(LOGGER_PRIORITY_ERR, "error creating stats for ns_file\n");
            FileUtils_release(nsFile);
            return -1;
        }

        /** use stats from earlier if we have them */
        if (stats) {
            memcpy(nsFile->stats, stats, sizeof(*stats));
        } else if (stat(nsFile->name, nsFile->stats) < 0) {
            Logger_log(LOGGER_PRIORITY_ERR, "stat() failed for ns_file\n");
            FileUtils_release(nsFile);
            return -1;
        }
    }

    rc = CONTAINER_INSERT(c, nsFile);
    if (-1 == rc ) {
        DEBUG_MSGTL(("directory:container", "  err adding %s\n", name));
        FileUtils_release(nsFile);
    }

    return 0;
}
