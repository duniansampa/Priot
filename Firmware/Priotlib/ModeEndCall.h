#ifndef MODEENDCALL_H
#define MODEENDCALL_H

#include "AgentHandler.h"

#define MODEENDCALL_MODE_END_ALL_MODES -999

typedef struct ModeHandlerList_s {
   struct ModeHandlerList_s *next;
   int  mode;
   MibHandler* callback_handler;
} ModeHandlerList;

/*
 * The helper calls another handler after each mode has been
 * processed.
 */

/* public functions */
MibHandler*
ModeEndCall_getModeEndCallHandler( ModeHandlerList* endlist );

ModeHandlerList*
ModeEndCall_addModeCallback( ModeHandlerList* endlist,
                             int              mode,
                             MibHandler*      callbackh );

/* internal */
NodeHandlerFT ModeEndCall_helper;


#endif // MODEENDCALL_H
