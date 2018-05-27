#ifndef MTEOBJECTSCONF_H
#define MTEOBJECTSCONF_H

#include "System/Util/Callback.h"
/*
 * function declarations 
 */
void            init_mteObjectsConf(void);
void            parse_mteOTable(const char *, char *);
Callback_f    store_mteOTable;
Callback_f    clear_mteOTable;

#endif                          /* MTEOBJECTSCONF_H */
