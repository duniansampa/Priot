#include "LcdTime.h"
#include "System/Util/Utilities.h"
#include "System/Util/Debug.h"
#include "Scapi.h"
#include "Usm.h"
#include "V3.h"
#include "Api.h"
/*
 * Global static hashlist to contain Enginetime entries.
 *
 * New records are prepended to the appropriate list at the hash index.
 */
static LcdTime_EnginetimePTR _lcdTime_etimelist[ LCDTIME_ETIMELIST_SIZE ];

/*******************************************************************-o-******
 * LcdTime_getEnginetime
 *
 * Parameters:
 *	*engineID
 *	 engineID_len
 *	*engineboot
 *	*engine_time
 *
 * Returns:
 *	ErrorCode_SUCCESS		Success -- when a record for engineID is found.
 *	ErrorCode_GENERR		Otherwise.
 *
 *
 * Lookup engineID and return the recorded values for the
 * <engine_time, engineboot> tuple adjusted to reflect the estimated time
 * at the engine in question.
 *
 * Special case: if engineID is NULL or if engineID_len is 0 then
 * the time tuple is returned immediately as zero.
 *
 * XXX	What if timediff wraps?  >shrug<
 * XXX  Then: you need to increment the boots value.  Now.  Detecting
 *            this is another matter.
 */

int LcdTime_getEnginetime(const u_char * engineID,
                   u_int engineID_len,
                   u_int * engineboot,
                   u_int * engine_time, u_int authenticated)
{
    int             rval = ErrorCode_SUCCESS;
    int             timediff = 0;
    LcdTime_EnginetimePTR      e = NULL;



    /*
     * Sanity check.
     */
    if (!engine_time || !engineboot) {
        UTILITIES_QUIT_FUN(ErrorCode_GENERR, goto_getEnginetimeQuit);
    }


    /*
     * Compute estimated current engine_time tuple at engineID if
     * a record is cached for it.
     */
    *engine_time = *engineboot = 0;

    if (!engineID || (engineID_len <= 0)) {
        UTILITIES_QUIT_FUN(ErrorCode_GENERR, goto_getEnginetimeQuit);
    }

    if (!(e = LcdTime_searchEnginetimeList(engineID, engineID_len))) {
        UTILITIES_QUIT_FUN(ErrorCode_GENERR, goto_getEnginetimeQuit);
    }

    if (!authenticated || e->authenticatedFlag) {
        *engine_time = e->engineTime;
        *engineboot = e->engineBoot;

       timediff = (int) (V3_localEngineTime() - e->lastReceivedEngineTime);
    }

    if (timediff > (int) (UTILITIES_ENGINETIME_MAX - *engine_time)) {
        *engine_time = (timediff - (UTILITIES_ENGINETIME_MAX - *engine_time));

        /*
         * FIX -- move this check up... should not change anything
         * * if engineboot is already locked.  ???
         */
        if (*engineboot < UTILITIES_ENGINEBOOT_MAX) {
            *engineboot += 1;
        }

    } else {
        *engine_time += timediff;
    }

    DEBUG_MSGTL(("LcdTime_getEnginetime", "engineID "));
    DEBUG_MSGHEX(("LcdTime_getEnginetime", engineID, engineID_len));
    DEBUG_MSG(("LcdTime_getEnginetime", ": boots=%d, time=%d\n", *engineboot,
              *engine_time));

goto_getEnginetimeQuit:
    return rval;

}

/*******************************************************************-o-******
 * get_enginetime
 *
 * Parameters:
 *	*engineID
 *	 engineID_len
 *	*engineboot
 *	*engine_time
 *
 * Returns:
 *	ErrorCode_SUCCESS		Success -- when a record for engineID is found.
 *	ErrorCode_GENERR		Otherwise.
 *
 *
 * Lookup engineID and return the recorded values for the
 * <engine_time, engineboot> tuple adjusted to reflect the estimated time
 * at the engine in question.
 *
 * Special case: if engineID is NULL or if engineID_len is 0 then
 * the time tuple is returned immediately as zero.
 *
 * XXX	What if timediff wraps?  >shrug<
 * XXX  Then: you need to increment the boots value.  Now.  Detecting
 *            this is another matter.
 */
int LcdTime_getEnginetimeEx(u_char * engineID,
                  u_int engineID_len,
                  u_int * engineboot,
                  u_int * engine_time,
                  u_int * last_engine_time, u_int authenticated)
{
    int             rval = ErrorCode_SUCCESS;
    int             timediff = 0;
    LcdTime_EnginetimePTR e = NULL;



    /*
     * Sanity check.
     */
    if (!engine_time || !engineboot || !last_engine_time) {
        UTILITIES_QUIT_FUN(ErrorCode_GENERR, goto_getEnginetimeExQuit);
    }


    /*
     * Compute estimated current engine_time tuple at engineID if
     * a record is cached for it.
     */
    *last_engine_time = *engine_time = *engineboot = 0;

    if (!engineID || (engineID_len <= 0)) {
        UTILITIES_QUIT_FUN(ErrorCode_GENERR, goto_getEnginetimeExQuit);
    }

    if (!(e = LcdTime_searchEnginetimeList(engineID, engineID_len))) {
        UTILITIES_QUIT_FUN(ErrorCode_GENERR, goto_getEnginetimeExQuit);
    }

    if (!authenticated || e->authenticatedFlag) {
        *last_engine_time = *engine_time = e->engineTime;
        *engineboot = e->engineBoot;

       timediff = (int) (V3_localEngineTime() - e->lastReceivedEngineTime);

    }

    if (timediff > (int) (UTILITIES_ENGINETIME_MAX - *engine_time)) {
        *engine_time = (timediff - (UTILITIES_ENGINETIME_MAX - *engine_time));

        /*
         * FIX -- move this check up... should not change anything
         * * if engineboot is already locked.  ???
         */
        if (*engineboot < UTILITIES_ENGINEBOOT_MAX) {
            *engineboot += 1;
        }

    } else {
        *engine_time += timediff;
    }

    DEBUG_MSGTL(("LcdTime_getEnginetimeEx", "engineID "));
    DEBUG_MSGHEX(("LcdTime_getEnginetimeEx", engineID, engineID_len));
    DEBUG_MSG(("LcdTime_getEnginetimeEx", ": boots=%d, time=%d\n",
              *engineboot, *engine_time));

goto_getEnginetimeExQuit:
    return rval;

}                               /* end get_enginetime_ex() */


void LcdTime_freeEnginetime(unsigned char *engineID, size_t engineID_len)
{
    LcdTime_EnginetimePTR      e = NULL;
    int             rval = 0;

    rval = LcdTime_hashEngineID(engineID, engineID_len);
    if (rval < 0)
    return;

    e = _lcdTime_etimelist[rval];

    while (e != NULL) {
        _lcdTime_etimelist[rval] = e->next;
        MEMORY_FREE(e->engineID);
        MEMORY_FREE(e);
        e = _lcdTime_etimelist[rval];
    }

}


/*******************************************************************-o-****
**
 * free_etimelist
 *
 * Parameters:
 *   None
 *
 * Returns:
 *   void
 *
 *
 * Free all of the memory used by entries in the etimelist.
 *
 */
void LcdTime_freeEtimelist(void)
{
     int index = 0;
     LcdTime_EnginetimePTR e = NULL;
     LcdTime_EnginetimePTR nextE = NULL;

     for( ; index < LCDTIME_ETIMELIST_SIZE; ++index)
     {
           e = _lcdTime_etimelist[index];

           while(e != NULL)
           {
                 nextE = e->next;
                 MEMORY_FREE(e->engineID);
                 MEMORY_FREE(e);
                 e = nextE;
           }

           _lcdTime_etimelist[index] = NULL;
     }
     return;
}


/*******************************************************************-o-******
 * set_enginetime
 *
 * Parameters:
 *	*engineID
 *	 engineID_len
 *	 engineboot
 *	 engine_time
 *
 * Returns:
 *	ErrorCode_SUCCESS		Success.
 *	ErrorCode_GENERR		Otherwise.
 *
 *
 * Lookup engineID and store the given <engine_time, engineboot> tuple
 * and then stamp the record with a consistent source of local time.
 * If the engineID record does not exist, create one.
 *
 * Special case: engineID is NULL or engineID_len is 0 defines an engineID
 * that is "always set."
 *
 * XXX	"Current time within the local engine" == time(NULL)...
 */
int LcdTime_setEnginetime(const u_char * engineID,
                          u_int engineID_len,
                          u_int engineboot, u_int engine_time, u_int authenticated)
{
    int             rval = ErrorCode_SUCCESS, iindex;
    LcdTime_EnginetimePTR e = NULL;



    /*
     * Sanity check.
     */
    if (!engineID || (engineID_len <= 0)) {
        return rval;
    }


    /*
     * Store the given <engine_time, engineboot> tuple in the record
     * for engineID.  Create a new record if necessary.
     */
    if (!(e = LcdTime_searchEnginetimeList(engineID, engineID_len))) {
        if ((iindex = LcdTime_hashEngineID(engineID, engineID_len)) < 0) {
           UTILITIES_QUIT_FUN(ErrorCode_GENERR, goto_setEnginetimeQuit);
        }

        e = (LcdTime_EnginetimePTR) calloc(1, sizeof(*e));

        e->next = _lcdTime_etimelist[iindex];
        _lcdTime_etimelist[iindex] = e;

        e->engineID = (u_char *) calloc(1, engineID_len);
        memcpy(e->engineID, engineID, engineID_len);

        e->engineID_len = engineID_len;
    }

    if (authenticated || !e->authenticatedFlag) {
        e->authenticatedFlag = authenticated;

        e->engineTime = engine_time;
        e->engineBoot = engineboot;
        e->lastReceivedEngineTime = V3_localEngineTime();
    }

    e = NULL;                   /* Indicates a successful update. */

    DEBUG_MSGTL(("LcdTime_setEnginetime", "engineID "));
    DEBUG_MSGHEX(("LcdTime_setEnginetime", engineID, engineID_len));
    DEBUG_MSG(("LcdTime_setEnginetime", ": boots=%d, time=%d\n", engineboot,
              engine_time));

goto_setEnginetimeQuit:
    MEMORY_FREE(e);

    return rval;

}                               /* end set_enginetime() */



/*******************************************************************-o-******
 * search_enginetime_list
 *
 * Parameters:
 *	*engineID
 *	 engineID_len
 *
 * Returns:
 *	Pointer to a etimelist record with engineID <engineID>  -OR-
 *	NULL if no record exists.
 *
 *
 * Search etimelist for an entry with engineID.
 *
 * ASSUMES that no engineID will have more than one record in the list.
 */

LcdTime_EnginetimePTR LcdTime_searchEnginetimeList(const u_char * engineID, u_int engineID_len)
{
    int             rval = ErrorCode_SUCCESS;
    LcdTime_EnginetimePTR e = NULL;


    /*
     * Sanity check.
     */
    if (!engineID || (engineID_len <= 0)) {
        UTILITIES_QUIT_FUN(ErrorCode_GENERR, goto_searchEnginetimeListQuit);
    }


    /*
     * Find the entry for engineID if there be one.
     */
    rval = LcdTime_hashEngineID(engineID, engineID_len);
    if (rval < 0) {
        UTILITIES_QUIT_FUN(ErrorCode_GENERR, goto_searchEnginetimeListQuit);
    }
    e = _lcdTime_etimelist[rval];

    for ( /*EMPTY*/; e; e = e->next) {
        if ((engineID_len == e->engineID_len)
            && !memcmp(e->engineID, engineID, engineID_len)) {
            break;
        }
    }


goto_searchEnginetimeListQuit:
    return e;

}                               /* end search_enginetime_list() */

/*******************************************************************-o-******
 * hash_engineID
 *
 * Parameters:
 *	*engineID
 *	 engineID_len
 *
 * Returns:
 *	>0			etimelist index for this engineID.
 *	ErrorCode_GENERR		Error.
 *
 *
 * Use a cheap hash to build an index into the etimelist.  Method is
 * to hash the engineID, then split the hash into u_int's and add them up
 * and modulo the size of the list.
 *
 */

int LcdTime_hashEngineID(const u_char * engineID, u_int engineID_len)
{
    int             rval = ErrorCode_GENERR;
    size_t          buf_len = UTILITIES_MAX_BUFFER;
    u_int           additive = 0;
    u_char         *bufp, buf[UTILITIES_MAX_BUFFER];
    void           *context = NULL;



    /*
     * Sanity check.
     */
    if (!engineID || (engineID_len <= 0)) {
        UTILITIES_QUIT_FUN(ErrorCode_GENERR, goto_hashEngineIDQuit);
    }


    /*
     * Hash engineID into a list index.
     */
    rval = Scapi_hash(usm_hMACMD5AuthProtocol,
                   sizeof(usm_hMACMD5AuthProtocol) / sizeof(oid),
                   engineID, engineID_len, buf, &buf_len);
    if (rval == ErrorCode_SC_NOT_CONFIGURED) {
        /* fall back to sha1 */
        rval = Scapi_hash(usm_hMACSHA1AuthProtocol,
                   sizeof(usm_hMACSHA1AuthProtocol) / sizeof(oid),
                   engineID, engineID_len, buf, &buf_len);
    }

    UTILITIES_QUIT_FUN(rval, goto_hashEngineIDQuit);

    for (bufp = buf; (bufp - buf) < (int) buf_len; bufp += 4) {
        additive += (u_int) * bufp;
    }

goto_hashEngineIDQuit:
    MEMORY_FREE(context);
    memset(buf, 0, UTILITIES_MAX_BUFFER);

    return (rval < 0) ? rval : (int)(additive % LCDTIME_ETIMELIST_SIZE);

}                               /* end hash_engineID() */

