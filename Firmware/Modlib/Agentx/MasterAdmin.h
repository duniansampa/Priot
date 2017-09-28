#ifndef MASTERADMIN_H
#define MASTERADMIN_H

#include "Types.h"

int
MasterAdmin_handleMasterAgentxPacket( int,
                                      Types_Session *,
                                      int,
                                      Types_Pdu *,
                                      void * );

int
MasterAdmin_closeAgentxSession( Types_Session * session,
                                int sessid );

int
MasterAdmin_closeAgentxSession( Types_Session * session,
                                int sessid );

#endif // MASTERADMIN_H
