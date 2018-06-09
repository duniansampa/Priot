#ifndef IOT_ENGINETIME_H
#define IOT_ENGINETIME_H

#include "Generals.h"

/** ============================[ Macros ============================ */

/*
 * undefine to enable time synchronization only on authenticatedFlag packets
 */
#define engineSYNC_OPT 1

/** the size of engine time list: _engineTime_list */
#define engineLIST_SIZE 23

/**
 * Macros for streamlined engineId existence checks --
 *
 *      e       is char *engineId,
 *      e_l     is u_int engineIdLength.
 *
 *
 *  engineIS_ENGINE_KNOWN(e, e_l)
 *      Returns:
 *              TRUE    If engineId is recoreded in the EngineId List;
 *              FALSE   Otherwise.
 *
 *  engineENSURE_ENGINE_RECORD(e, e_l)
 *      Adds the given engineId to the EngineId List if it does not exist
 *              already.  engineId is added with a <engineTime, engineBoots>
 *              tuple of <0,0>.  ALWAYS succeeds -- except in case of a
 *              fatal internal error.
 *      Returns:
 *              ErrorCode_SUCCESS On success;
 *              ErrorCode_GENERR  Otherwise.
 *
 *  engineMAKE_NEW_ENGINE_RECORD(e, e_l)
 *      Returns:
 *              ErrorCode_SUCCESS If engineId already exists in the EngineId List;
 *              ErrorCode_GENERR  Otherwise -and- invokes engineENSURE_ENGINE_RECORD()
 *                                      to add an entry to the EngineId List.
 *
 * @note  Requres the following declaration in modules calling engineIS_ENGINE_KNOWN():
 *        static u_int    dummy_etime, dummy_eboot;
 */
#define engineIS_ENGINE_KNOWN( e, e_l )                \
    ( ( EngineTime_get( e, e_l,                        \
            &_usm_dummyEboot, &_usm_dummyEtime, TRUE ) \
          == ErrorCode_SUCCESS )                       \
            ? TRUE                                     \
            : FALSE )

#define engineENSURE_ENGINE_RECORD( e, e_l )                         \
    ( ( EngineTime_set( e, e_l, 0, 0, FALSE ) == ErrorCode_SUCCESS ) \
            ? ErrorCode_SUCCESS                                      \
            : ErrorCode_GENERR )

#define engineMAKE_NEW_ENGINE_RECORD( e, e_l )    \
    ( ( engineIS_ENGINE_KNOWN( e, e_l ) == TRUE ) \
            ? ErrorCode_SUCCESS                   \
            : ( engineENSURE_ENGINE_RECORD( e, e_l ), ErrorCode_GENERR ) )

/** ============================[ Types ]================== */

typedef struct EngineTime_s {
    /** a value that uniquely identifies the PRIOT engine */
    u_char* engineId;

    /** the length of engineId */
    u_int engineIdLength;

    /** provides an indication of time at that PRIOT engine*/
    u_int engineTime;

    u_int engineBoot;

    /*
     * Time & boots values received from last authenticatedFlag
     * message within the previous time window.
     */
    time_t lastReceivedEngineTime;

    /*
     * Timestamp made when engineTime/engineBoots was last
     *  updated.  Measured in seconds.
     */
    u_int authenticatedFlag;

    struct EngineTime_s* next;
} Enginetime_t, *Enginetime_p;

/** =============================[ Functions Prototypes ]================== */

/**
 * @brief EngineTime_get
 *        Lookup engineId and return the recorded values for the
 *        <engineTime, engineBoot> tuple adjusted to reflect the estimated time
 *        at the engine in question.
 *
 * @note What if timediff wraps?  >shrug<
 *       Then: you need to increment the boots value.  Now.  Detecting
 *       this is another matter.
 *
 * @note special case: if engineID is NULL or if engineID_len is 0 then
 *       the time tuple is returned immediately as zero.
 *
 * @param engineId -
 * @param engineIdLength -
 * @param engineBoot -
 * @param engineTime -
 * @param authenticatedFlag -
 *
 * @returns ErrorCode_SUCCESS : on success -- when a record for engineId is found.
 *          ErrorCode_GENERR  : Otherwise.
 */
int EngineTime_get( const u_char* engineId, u_int engineIdLength, u_int* engineBoot,
    u_int* engineTime, u_int authenticatedFlag );

/**
 * @brief EngineTime_getEx
 *        Lookup engineId and return the recorded values for the
 *        <engineTime, engineBoot> tuple adjusted to reflect the estimated time
 *        at the engine in question.
 *
 * @note Special case: if engineId is NULL or if engineIdLength is 0 then
 *       the time tuple is returned immediately as zero.
 *
 * @note What if timediff wraps?  >shrug<
 *       Then: you need to increment the boots value.  Now.  Detecting
 *       this is another matter.
 *
 * @param engineId -
 * @param engineIdLength -
 * @param engineBoot -
 * @param engineTime -
 * @param lastEngineTime -
 * @param authenticatedFlag -
 *
 * @returns ErrorCode_SUCCESS : Success -- when a record for engineID is found.
 *          ErrorCode_GENERR  : Otherwise.
 */
int EngineTime_getEx( u_char* engineId,
    u_int engineIdLength,
    u_int* engineBoot,
    u_int* engineTime,
    u_int* lastEngineTime,
    u_int authenticatedFlag );

/**
 * @brief EngineTime_set
 *        Lookup engineId and store the given <engineTime, engineBoot> tuple
 *        and then stamp the record with a consistent source of local time.
 *        If the engineId record does not exist, create one.
 *
 * @note Special case: engineId is NULL or engineIdLength is 0 defines an engineID
 *       that is "always set."
 *
 * @note "Current time within the local engine" == time(NULL)...
 *
 * @param engineId -
 * @param engineIdLength -
 * @param engineBoot -
 * @param engineTime -
 * @param authenticatedFlag -
 *
 * @returns ErrorCode_SUCCESS : on success.
 *          ErrorCode_GENERR  : otherwise.
 */
int EngineTime_set( const u_char* engineId, u_int engineIdLength,
    u_int engineBoot, u_int engineTime, u_int authenticatedFlag );

/**
 * @brief EngineTime_searchInList
 *        Searches _engineTime_list for an entry with engineId.
 *
 * @note ASSUMES that no engineId will have more than one record in the list.
 *
 * @param engineId -
 * @param engineIdLength -
 * @return Pointer to a _engineTime_list record with engineId <engineId>  -OR- NULL if no record exists.
 */
Enginetime_p EngineTime_searchInList( const u_char* engineId, u_int engineIdLength );

/**
 * @brief EngineTime_hashEngineId
 *        Uses a cheap hash to build an index into the _engineTime_list.  Method is
 *        to hash the engineId, then split the hash into u_int's and add them up
 *        and modulo the size of the list.
 *
 * @param engineId -
 * @param engineIdLength -
 * @returns               >0 : _engineTime_list index for this engineId.
 *          ErrorCode_GENERR : in case of error.
 */
int EngineTime_hashEngineId( const u_char* engineId, u_int engineIdLength );

/**
 * @brief EngineTime_clear
 *        frees all of the memory used by entries in the _engineTime_list
 */
void EngineTime_clear( void );

/**
 * @brief LcdTime_freeEnginetime
 *        frees all of the memory used by entry engineId in the _engineTime_list
 *
 * @param engineId -
 * @param engineIdLength -
 */
void EngineTime_free( unsigned char* engineId, size_t engineIdLength );

#endif // IOT_ENGINETIME_H
