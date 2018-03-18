#ifndef DIRUTILS_H
#define DIRUTILS_H

#include "Container.h"

/*
 * filter function; return 1 to include file, 0 to exclude
 */
#define DIRUTILS_DIR_EXCLUDE 0
#define DIRUTILS_DIR_INCLUDE 1

typedef int (DirUtils_FuncFilter)(const void *text, void *ctx);


/*------------------------------------------------------------------
 *
 * Prototypes
 */
Container_Container * DirUtils_readSome(Container_Container *userContainer,
                                        const char *dirname,
                                        DirUtils_FuncFilter *filter,
                                        void *filterCtx, u_int flags);

#define DirUtils_read(c,d,f) DirUtils_readSome(c,d,NULL,NULL,f);

void DirUtils_free(Container_Container *c);



/*------------------------------------------------------------------
 *
 * flags
 */
#define DIRUTILS_DIR_RECURSE                           0x0001
#define DIRUTILS_DIR_RELATIVE_PATH                     0x0002
#define DIRUTILS_DIR_SORTED                            0x0004

/** don't return null if dir empty */
#define DIRUTILS_DIR_EMPTY_OK                          0x0008
/** store netsnmp_file instead of filenames */
#define DIRUTILS_DIR_NSFILE                            0x0010
/** load stats in netsnmp_file */
#define DIRUTILS_DIR_NSFILE_STATS                      0x0020

#endif // DIRUTILS_H
