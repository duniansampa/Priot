#ifndef SUBAGENT_H
#define SUBAGENT_H

#include "Types.h"
#include "System/Util/Callback.h"
#include "System/Util/Alarm.h"

int
Subagent_init(void);

int
Subagent_handleAgentxPacket( int,
                             Types_Session *, int,
                             Types_Pdu *, void *);
Callback_f
Subagent_registerCallback;


AlarmCallback_f
Subagent_checkSession;

#endif // SUBAGENT_H
