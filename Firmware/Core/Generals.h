#ifndef BaseIncludes_H_
#define BaseIncludes_H_

#include "Config.h"

/** @brief These are the default C++ includes.
 */
#include <ctype.h> /** isspace, toupper */
#include <dirent.h> /** DIR */
#include <errno.h> /** errno */
#include <errno.h> /** errno */
#include <fcntl.h> /** fcntl */
#include <float.h> /** float.h */
#include <limits.h> /** limits */
#include <math.h> /** modf */
#include <pthread.h> /** threads */
#include <sched.h>
#include <semaphore.h> /** semaphore */
#include <signal.h> /** signal */
#include <stdarg.h> /** var_start, var_arg */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h> /** printf */
#include <stdlib.h>
#include <string.h> /** string */
#include <sys/stat.h> /** mkdir */
#include <sys/types.h> /** mkdir, rmdir */
#include <termios.h> /** struct termios, tcgetattr, cfsetispeed, cfsetospeed, tcsetattr, IHFLOW, OHFLOW, IXOFF, IXON */
#include <unistd.h> /** rmdir */

/** #define GENERALS_NO_64BIT_SUPPORT */

typedef uint32_t oid;
typedef uint8_t byte;

#endif
