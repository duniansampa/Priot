#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H

#include "AgentHandler.h"

/*
 * The multiplexer helper
 */

/** @name multiplexer
 *  @{ */

/** @struct MibHandlerMethods
 *  Defines the subhandlers to be called by the multiplexer helper
 */
typedef struct MibHandlerMethods_s {
   /** called when a GET request is received */
    MibHandler* get_handler;
   /** called when a GETNEXT request is received */
    MibHandler* getnext_handler;
   /** called when a GETBULK request is received */
    MibHandler* getbulk_handler;
   /** called when a SET request is received */
    MibHandler* set_handler;
} MibHandlerMethods;

/** @} */

MibHandler*
Multiplexer_getMultiplexerHandler(MibHandlerMethods *);

NodeHandlerFT Multiplexer_helperHandler;


#endif // MULTIPLEXER_H
