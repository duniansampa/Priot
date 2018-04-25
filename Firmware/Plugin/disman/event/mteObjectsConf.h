#ifndef MTEOBJECTSCONF_H
#define MTEOBJECTSCONF_H

#include "Callback.h"
/*
 * function declarations 
 */
void            init_mteObjectsConf(void);
void            parse_mteOTable(const char *, char *);
Callback_CallbackFT    store_mteOTable;
Callback_CallbackFT    clear_mteOTable;

#endif                          /* MTEOBJECTSCONF_H */
