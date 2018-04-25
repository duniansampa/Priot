#ifndef NOTIFICATION_LOG_H
#define NOTIFICATION_LOG_H

#include "AgentHandler.h"

/*
 * function declarations 
 */
void init_notification_log(void);
void shutdown_notification_log(void);

void log_notification(Types_Pdu *pdu, Transport_Transport *transport);


#endif                          /* NOTIFICATION_LOG_H */
