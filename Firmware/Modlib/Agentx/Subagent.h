#ifndef SUBAGENT_H
#define SUBAGENT_H

#include "Types.h"
#include "Callback.h"
#include "Alarm.h"

int
Subagent_init(void);

int
Subagent_handleAgentxPacket( int,
                             Types_Session *, int,
                             Types_Pdu *, void *);
Callback_CallbackFT
Subagent_registerCallback;


Alarm_CallbackFT
Subagent_checkSession;

#endif // SUBAGENT_H
