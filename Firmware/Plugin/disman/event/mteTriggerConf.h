#ifndef MTETRIGGERCONF_H
#define MTETRIGGERCONF_H

#include "System/Util/Callback.h"

/*
 * function declarations 
 */
void            init_mteTriggerConf(void);
void            parse_mteMonitor( const char *, const char *);
void            parse_default_mteMonitors( const char *, const char *);
void            parse_linkUpDown_traps(const char *, const char *);

void            parse_mteTTable(  const char *, char *);
void            parse_mteTDTable( const char *, char *);
void            parse_mteTExTable(const char *, char *);
void            parse_mteTBlTable(const char *, char *);
void            parse_mteTThTable(const char *, char *);
void            parse_mteTriggerTable(const char *, char *);
Callback_f    store_mteTTable;
Callback_f    clear_mteTTable;

#endif                          /* MTETRIGGERCONF_H */
