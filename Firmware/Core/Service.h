#ifndef SERVICE_H
#define SERVICE_H

#include "Generals.h"

/* Default port handling */

int Service_registerDefaultDomain(const char* application, const char* domain);

const char* Service_lookupDefaultDomain(const char* application);

const char* const * Service_lookupDefaultDomains(const char* application);

void Service_clearDefaultDomain(void);

int Service_registerDefaultTarget(const char* application, const char* domain,
                const char* target);

const char * Service_lookupDefaultTarget(const char* application, const char* domain);

void Service_clearDefaultTarget(void);

void Service_registerServiceHandlers(void);

#endif // SERVICE_H
