#ifndef BaseIncludes_H_
#define BaseIncludes_H_

#include "Settings.h"

//! \brief These are the default C++ includes.
#include <stdio.h>          // printf
#include <sys/types.h>      // mkdir, rmdir
#include <sys/stat.h>       // mkdir
#include <unistd.h>         // rmdir
#include <errno.h>          // errno
#include <dirent.h>         // DIR
#include <string.h>         // string
#include <pthread.h>        //threads
#include <semaphore.h>      //semaphore
#include <sched.h>          //
#include <errno.h>          //errno
#include <stdarg.h>         //var_start, var_arg
#include <stdlib.h>         //
#include <limits.h>         //limits
#include <termios.h>		// struct termios, tcgetattr, cfsetispeed, cfsetospeed, tcsetattr, IHFLOW, OHFLOW, IXOFF, IXON
#include <fcntl.h>		    //fcntl
#include <math.h>			//modf
#include <float.h>          //float.h
#include <signal.h>         //signal
#include <ctype.h>          //isspace, toupper


//#define GENERALS_NO_64BIT_SUPPORT

typedef unsigned char       uchar;
typedef unsigned short int  ushort;
typedef unsigned int        uint;
typedef unsigned long int   ulong;
typedef uchar               ubyte;
typedef char                byte;

#ifndef __cplusplus
#include <stdbool.h>
#endif
typedef unsigned long       oid;

#ifdef GENERALS_NO_64BIT_SUPPORT
    typedef uint                uint64;
    typedef long                int64;
#else
    typedef unsigned long long  uint64;
    typedef long long           int64;
#endif

#endif // BaseIncludes_H_
