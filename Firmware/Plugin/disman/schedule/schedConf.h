#ifndef SCHEDCONF_H
#define SCHEDCONF_H

#include "System/Util/Callback.h"

/*
 * function declarations 
 */
void            init_schedConf(void);

void            parse_sched_periodic(const char *, char *);
void            parse_sched_timed(   const char *, char *);
void            parse_schedTable(    const char *, char *);
Callback_f    store_schedTable;


#endif                          /* SCHEDCONF_H */
