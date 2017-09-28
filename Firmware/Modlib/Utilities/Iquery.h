#ifndef IQUERY_H
#define IQUERY_H

#include "Types.h"

void
Iquery_initIquery(void);

Types_Session *
Iquery_userSession( char* secName );

Types_Session *
Iquery_communitySession( char* community,
                         int version );

Types_Session *
Iquery_pduSession( Types_Pdu* pdu) ;

Types_Session *
Iquery_session( char    * secName,
                int       version,
                int       secModel,
                int       secLevel,
                u_char  * engineID,
                size_t    engIDLen );

#endif // IQUERY_H
