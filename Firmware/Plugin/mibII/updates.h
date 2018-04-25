#ifndef UPDATES_H
#define UPDATES_H

#include "AgentHandler.h"
/**
 * Create a handler_registration that checks *set to determine how to proceede,
 * if *set is less than 0 then the object is nonwriteable, otherwise, *set is
 * set to 1 in the commit phase. All other parameters are as in
 * netsnmp_create_handler_registration.
 */

extern HandlerRegistration*
netsnmp_create_update_handler_registration(const char* name,
                                           const oid* id, size_t idlen,
                                           int mode, int* set);

#endif
