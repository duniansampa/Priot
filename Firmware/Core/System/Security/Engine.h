#ifndef IOT_ENGINE_H
#define IOT_ENGINE_H

#include "Generals.h"

/** ============================[ Macros ]============================ */

/**
 * undefine to enable time synchronization only on authenticatedFlag packets
 */
#define engineSYNC_OPT 1

/** the size of engine time list: _engine_list */
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
    ( ( Engine_get( e, e_l,                            \
            &_usm_dummyEboot, &_usm_dummyEtime, TRUE ) \
          == ErrorCode_SUCCESS )                       \
            ? TRUE                                     \
            : FALSE )

#define engineENSURE_ENGINE_RECORD( e, e_l )                     \
    ( ( Engine_set( e, e_l, 0, 0, FALSE ) == ErrorCode_SUCCESS ) \
            ? ErrorCode_SUCCESS                                  \
            : ErrorCode_GENERR )

#define engineMAKE_NEW_ENGINE_RECORD( e, e_l )    \
    ( ( engineIS_ENGINE_KNOWN( e, e_l ) == TRUE ) \
            ? ErrorCode_SUCCESS                   \
            : ( engineENSURE_ENGINE_RECORD( e, e_l ), ErrorCode_GENERR ) )

/** ============================[ Types ]================== */

typedef struct Engine_s {

    /** uniquely and unambiguously identifies an PRIOT engine. */
    u_char* engineId;

    /** the length of engineId */
    u_int engineIdLength;

    /**
     * a count of the number of times the PRIOT engine has
     * re-booted/re-initialized since engineId was last configured;
     */
    u_int engineBoot;

    /** the number of seconds since the engineBoots counter was last incremented */
    u_int engineTime;

    /**
     * records the highest value of engineTime that was received by the
     * non-authoritative PRIOT engine from the authoritative PRIOT engine
     * and is used to eliminate the possibility of replaying messages
     * that would prevent the non-authoritative PRIOT engine's notion of
     * the engineTime from advancing
     */
    time_t lastReceivedEngineTime;

    /*
     * Timestamp made when engineTime/engineBoots was last
     *  updated.  Measured in seconds.
     */
    u_int authenticatedFlag;

    struct Engine_s* next;
} Engine_t, *Engine_p;

/** =============================[ Functions Prototypes ]================== */

/**
 * @brief Engine_get
 *        Lookup engineId and return the recorded values for the
 *        <engineTime, engineBoot> tuple adjusted to reflect the estimated time
 *        at the engine in question.
 *
 * @note What if timediff wraps?  >shrug<
 *       Then: you need to increment the boots value.  Now.  Detecting
 *       this is another matter.
 *
 * @note special case: if engineId is NULL or if engineIdLength is 0 then
 *       the time tuple is returned immediately as zero.
 *
 * @param engineId - uniquely and unambiguously identifies an PRIOT engine
 * @param engineIdLength - the length of engineId
 * @param engineBoot - the engine boot information
 * @param engineTime - the engine time information
 * @param authenticatedFlag -
 *
 * @returns ErrorCode_SUCCESS : on success -- when a record for engineId is found.
 *          ErrorCode_GENERR  : Otherwise.
 */
int Engine_get( const u_char* engineId, u_int engineIdLength, u_int* engineBoot,
    u_int* engineTime, u_int authenticatedFlag );

/**
 * @brief Engine_getEx
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
 * @param engineId - uniquely and unambiguously identifies an PRIOT engine
 * @param engineIdLength - the length of engineId
 * @param engineBoot - the engine boot information
 * @param engineTime - the engine time information
 * @param authenticatedFlag -
 *
 * @returns ErrorCode_SUCCESS : Success -- when a record for engineID is found.
 *          ErrorCode_GENERR  : Otherwise.
 */
int Engine_getEx( u_char* engineId,
    u_int engineIdLength,
    u_int* engineBoot,
    u_int* engineTime,
    u_int* lastEngineTime,
    u_int authenticatedFlag );

/**
 * @brief Engine_set
 *        Lookup engineId and store the given <engineTime, engineBoot> tuple
 *        and then stamp the record with a consistent source of local time.
 *        If the engineId record does not exist, create one.
 *
 * @note Special case: engineId is NULL or engineIdLength is 0 defines an engineID
 *       that is "always set."
 *
 * @note "Current time within the local engine" == time(NULL)...
 *
 * @param engineId - uniquely and unambiguously identifies an PRIOT engine
 * @param engineIdLength - the length of engineId
 * @param engineBoot - the engine boot information
 * @param engineTime - the engine time information
 * @param authenticatedFlag -
 *
 * @returns ErrorCode_SUCCESS : on success.
 *          ErrorCode_GENERR  : otherwise.
 */
int Engine_set( const u_char* engineId, u_int engineIdLength,
    u_int engineBoot, u_int engineTime, u_int authenticatedFlag );

/**
 * @brief Engine_searchInList
 *        Searches _engine_list for an entry with engineId.
 *
 * @note ASSUMES that no engineId will have more than one record in the list.
 *
 * @param engineId -
 * @param engineIdLength - the length of engineId
 * @return Pointer to a _engine_list record with engineId <engineId>  -OR- NULL if no record exists.
 */
Engine_p Engine_searchInList( const u_char* engineId, u_int engineIdLength );

/**
 * @brief Engine_hashEngineId
 *        Uses a cheap hash to build an index into the _engine_list.  Method is
 *        to hash the engineId, then split the hash into u_int's and add them up
 *        and modulo the size of the list.
 *
 * @param engineId - uniquely and unambiguously identifies an PRIOT engine
 * @param engineIdLength - the length of engineId
 * @returns               >0 : _engine_list index for this engineId.
 *          ErrorCode_GENERR : in case of error.
 */
int Engine_hashEngineId( const u_char* engineId, u_int engineIdLength );

/**
 * @brief Engine_clear
 *        frees all of the memory used by entries in the _engine_list
 */
void Engine_clear( void );

/**
 * @brief Engine_free
 *        frees all of the memory used by entry engineId in the _engine_list
 *
 * @param engineId - uniquely and unambiguously identifies an PRIOT engine
 * @param engineIdLength - the length of engineId
 */
void Engine_free( unsigned char* engineId, size_t engineIdLength );

#endif // IOT_ENGINE_H
