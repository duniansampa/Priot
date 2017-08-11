#ifndef TC_H
#define TC_H

#include "Generals.h"


/*
 * Tc.h: Provide some standard #defines for Textual Convention
 * related value information
 */

int Tc_dateandtimeSetBufFromVars(u_char *buf, size_t *bufsize,
                                      u_short y, u_char mon, u_char d,
                                      u_char h, u_char min, u_char s,
                                      u_char deci_seconds,
                                      int utc_offset_direction,
                                      u_char utc_offset_hours,
                                      u_char utc_offset_minutes);

u_char  * Tc_dateNTime(const time_t *, size_t *);
time_t    Tc_ctimeToTimet(const char *);

/*
 * TrueValue
 */
#define TC_TV_TRUE  1
#define TC_TV_FALSE 2

/*
 * RowStatus
 */
#define TC_RS_NONEXISTENT          0
#define TC_RS_ACTIVE	            1
#define TC_RS_NOTINSERVICE	        2
#define TC_RS_NOTREADY	            3
#define TC_RS_CREATEANDGO	        4
#define TC_RS_CREATEANDWAIT	    5
#define TC_RS_DESTROY		        6

#define TC_RS_IS_GOING_ACTIVE( x ) ( x == TC_RS_CREATEANDGO || x == TC_RS_ACTIVE )
#define TC_RS_IS_ACTIVE( x ) ( x == TC_RS_ACTIVE )
#define TC_RS_IS_NOT_ACTIVE( x ) ( ! TC_RS_IS_GOING_ACTIVE(x) )

/*
 * StorageType
 */
#define TC_ST_NONE         0
#define TC_ST_OTHER	    1
#define TC_ST_VOLATILE	    2
#define TC_ST_NONVOLATILE	3
#define TC_ST_PERMANENT	4
#define TC_ST_READONLY	    5


char   Tc_checkRowstatusTransition(int old_val, int new_val);

char   Tc_checkRowstatusWithStoragetypeTransition(int old_val, int new_val, int old_storage);

char   Tc_checkStorageTransition(int old_val, int new_val);

#endif // TC_H
