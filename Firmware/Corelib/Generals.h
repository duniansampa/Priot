#ifndef BaseIncludes_H_
#define BaseIncludes_H_


//! \brief These are the default C++ includes.
#include <iostream>         // For collections
#include <stdio.h>          // printf
#include <iomanip>          // setfill, setw
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
#include <cmath>
#include <algorithm>        // std::sort
#include <float.h>          //float.h
#include <signal.h>         //signal


#include "Settings.h"

using namespace std;


//#define GENERALS_NO_64BIT_SUPPORT

typedef unsigned char       _u8;
typedef unsigned short      _u16;
typedef int                 _i32;
typedef unsigned long       _u32;
typedef char             	_s8;
typedef short               _s16;
typedef long                _s32;
typedef unsigned char       _ubyte;
typedef char                _sbyte;
typedef time_t              _time;
typedef bool                _bool;
typedef float               _float;
typedef _u32                _oid;

#ifdef GENERALS_NO_64BIT_SUPPORT
    typedef _u32                _u64;
    typedef _s32                _s64;
#else
    typedef unsigned long long  _u64;
    typedef long long           _s64;
#endif

#endif // BaseIncludes_H_
