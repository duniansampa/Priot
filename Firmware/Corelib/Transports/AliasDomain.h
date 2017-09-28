#ifndef ALIASDOMAIN_H
#define ALIASDOMAIN_H

#include "Generals.h"
#include "../Transport.h"

/*
 * Simple aliases for complex transport strings that can be specified
 * via the snmp.conf file and the 'alias' token.
 */

#define TRANSPORT_DOMAIN_ALIAS_IP		1,3,6,1,2,1,100,1,5
extern oid aliasDomain_priotALIASDomain[];

/*
 * "Constructor" for transport domain object.
 */

void AliasDomain_ctor(void);

#endif // ALIASDOMAIN_H
