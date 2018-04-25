#ifndef MTEEVENTCONF_H
#define MTEEVENTCONF_H

#include "Callback.h"

/*
 * function declarations 
 */
void            init_mteEventConf(void);

void            parse_notificationEvent(const char *, char *);
void            parse_setEvent(         const char *, char *);

void            parse_mteETable(   const char *, char *);
void            parse_mteENotTable(const char *, char *);
void            parse_mteESetTable(const char *, char *);
Callback_CallbackFT    store_mteETable;
Callback_CallbackFT    clear_mteETable;

#endif                          /* MTEEVENTCONF_H */
