#ifndef LCDTIME_H
#define LCDTIME_H

#include "Generals.h"



/*
 * undefine to enable time synchronization only on authenticated packets
 */

#define LCDTIME_SYNC_OPT 1

/*
 * Macros and definitions.
 */
#define LCDTIME_ETIMELIST_SIZE	23



typedef struct LcdTime_Enginetime_s {
    u_char         *engineID;
    u_int           engineID_len;

    u_int           engineTime;
    u_int           engineBoot;
    /*
     * Time & boots values received from last authenticated
     * *   message within the previous time window.
     */

    time_t          lastReceivedEngineTime;
    /*
     * Timestamp made when engineTime/engineBoots was last
     * *   updated.  Measured in seconds.
     */

    u_int           authenticatedFlag;

    struct LcdTime_Enginetime_s *next;
} LcdTime_Enginetime   , *LcdTime_EnginetimePTR;




/*
 * Macros for streamlined engineID existence checks --
 *
 *      e       is char *engineID,
 *      e_l     is u_int engineID_len.
 *
 *
 *  ISENGINEKNOWN(e, e_l)
 *      Returns:
 *              TRUE    If engineID is recoreded in the EngineID List;
 *              FALSE   Otherwise.
 *
 *  ENSURE_ENGINE_RECORD(e, e_l)
 *      Adds the given engineID to the EngineID List if it does not exist
 *              already.  engineID is added with a <enginetime, engineboots>
 *              tuple of <0,0>.  ALWAYS succeeds -- except in case of a
 *              fatal internal error.
 *      Returns:
 *              ErrorCode_SUCCESS On success;
 *              ErrorCode_GENERR  Otherwise.
 *
 *  MAKENEW_ENGINE_RECORD(e, e_l)
 *      Returns:
 *              ErrorCode_SUCCESS If engineID already exists in the EngineID List;
 *              ErrorCode_GENERR  Otherwise -and- invokes ENSURE_ENGINE_RECORD()
 *                                      to add an entry to the EngineID List.
 *
 * XXX  Requres the following declaration in modules calling ISENGINEKNOWN():
 *      static u_int    dummy_etime, dummy_eboot;
 */
#define LCDTIME_ISENGINEKNOWN(e, e_l)                       \
( (LcdTime_getEnginetime(e, e_l,                            \
    &usm_dummyEboot, &usm_dummyEtime, TRUE) == ErrorCode_SUCCESS)	\
    ? TRUE                                                  \
    : FALSE )

#define LCDTIME_ENSURE_ENGINE_RECORD(e, e_l)				\
( (LcdTime_setEnginetime(e, e_l, 0, 0, FALSE) == ErrorCode_SUCCESS)	\
    ? ErrorCode_SUCCESS                                       \
    : ErrorCode_GENERR )

#define LCDTIME_MAKENEW_ENGINE_RECORD(e, e_l)				\
( (LCDTIME_ISENGINEKNOWN(e, e_l) == TRUE)                   \
    ? ErrorCode_SUCCESS                                       \
    : (LCDTIME_ENSURE_ENGINE_RECORD(e, e_l), ErrorCode_GENERR) )



/*
 * Prototypes.
 */
int LcdTime_getEnginetime(const u_char * engineID, u_int engineID_len,
                               u_int * engine_boot,
                               u_int * engine_time,
                               u_int authenticated);

int LcdTime_getEnginetimeEx(u_char * engineID,
                              u_int engineID_len,
                              u_int * engine_boot,
                              u_int * engine_time,
                              u_int * last_engine_time,
                              u_int authenticated);

int LcdTime_setEnginetime(const u_char * engineID, u_int engineID_len,
                           u_int engine_boot, u_int engine_time,
                           u_int authenticated);







LcdTime_EnginetimePTR LcdTime_searchEnginetimeList(const u_char * engineID, u_int engineID_len);

int LcdTime_hashEngineID(const u_char * engineID, u_int engineID_len);

void LcdTime_dumpEtimelistEntry(LcdTime_EnginetimePTR e, int count);
void LcdTime_dumpEtimelist(void);
void LcdTime_freeEtimelist(void);
void LcdTime_freeEnginetime(unsigned char *engineID, size_t engineID_len);


#endif // LCDTIME_H
