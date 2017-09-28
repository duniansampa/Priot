#ifndef STRUCT_H
#define STRUCT_H

#define STRMAX 1024
#define SHPROC 1
#define EXECPROC 2
#define PASSTHRU 3
#define PASSTHRU_PERSIST 4
#define MIBMAX 30

#include "Types.h"

struct Extensible_s {
    char            name[STRMAX];
    char            command[STRMAX];
    char            fixcmd[STRMAX];
    int             type;
    int             result;
    char            output[STRMAX];
    struct Extensible_s *next;
    oid             miboid[MIBMAX];
    size_t          miblen;
    int             mibpriority;
    Types_PidT   pid;
};

struct Myproc_s {
    char            name[STRMAX];
    char            fixcmd[STRMAX];
    int             min;
    int             max;
    struct Myproc_s *next;
};

/*
 * struct mibinfo
 * {
 * int numid;
 * unsigned long mibid[10];
 * char *name;
 * void (*handle) ();
 * };
 */

#endif // STRUCT_H
