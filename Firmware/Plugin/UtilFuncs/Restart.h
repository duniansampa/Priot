#ifndef RESTART_H
#define RESTART_H

#include "Types.h"
#include "Vars.h"

extern char ** restart_argvrestartp;

extern char * restart_argvrestartname;

extern char * restart_argvrestart;

void      Restart_doit(int);

WriteMethodFT Restart_hook;

#endif // RESTART_H
