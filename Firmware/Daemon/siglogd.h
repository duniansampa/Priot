
#ifndef SIGLOGD_H
#define SIGLOGD_H

#include "includes.h"


extern char   **argvrestartp;
extern char    *argvrestart;
extern char    *argvrestartname;

//-------------------------------------
extern int      agent_role;
extern int      verbose;
extern int      (*sd_handlers[]) (int);
extern int      smux_listen_sd;

extern int      snmp_read_packet(int);

/*
 * config file parsing routines
 */
void            agentBoots_conf(char *, char *);

int main_aux(int argc, char *argv[]);



#endif //SIGLOGD_H
