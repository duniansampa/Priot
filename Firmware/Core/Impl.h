#ifndef IMPL_H
#define IMPL_H

#include "Generals.h"

#define IMPL_COMMUNITY_MAX_LEN	256

/*
 * Space for character representation of an object identifier
 */
#define IMPL_SPRINT_MAX_LEN		2560


#define IMPL_READ	    1
#define IMPL_WRITE	    0

#define IMPL_RESERVE1    0
#define IMPL_RESERVE2    1
#define IMPL_ACTION	     2
#define IMPL_COMMIT      3
#define IMPL_FREE        4
#define IMPL_UNDO        5
#define IMPL_FINISHED_SUCCESS    9
#define IMPL_FINISHED_FAILURE	10

/*
 * Access control statements for the agent
 */
#define IMPL_OLDAPI_RONLY	0x1     /* read access only */
#define IMPL_OLDAPI_RWRITE	0x2     /* read and write access (must have 0x2 bit set) */
#define IMPL_OLDAPI_NOACCESS 0x0000  /* no access for anybody */

#define IMPL_RONLY           IMPL_OLDAPI_RONLY
#define IMPL_RWRITE          IMPL_OLDAPI_RWRITE
#define IMPL_NOACCESS        IMPL_OLDAPI_NOACCESS

/*
 * changed to ERROR_MSG to eliminate conflict with other includes
 */
#define IMPL_ERROR_MSG(string)	Api_setDetail(string)

    /*
     * from snmp.c
     */
    //extern u_char   sid[];      /* size SID_MAX_LEN */


    /*
     * For calling secauth_build, FIRST_PASS is an indication that a new nonce
     * and lastTimeStamp should be recorded.  LAST_PASS is an indication that
     * the packet should be checksummed and encrypted if applicable, in
     * preparation for transmission.
     * 0 means do neither, FIRST_PASS | LAST_PASS means do both.
     * For secauth_parse, FIRST_PASS means decrypt the packet, otherwise leave it
     * alone.  LAST_PASS is ignored.
     */
#define IMPL_FIRST_PASS	1
#define	IMPL_LAST_PASS	2
    u_char         * Imp_comstrParse(u_char *, size_t *, u_char *, size_t *, long *);
    u_char         * Imp_comstrBuild(u_char *, size_t *, u_char *, size_t *, long *, size_t);

    int              Imp_hasAccess(u_char, int, int, int);

#endif // IMPL_H
