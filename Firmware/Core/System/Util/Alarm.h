#ifndef IOT_ALARM_H
#define IOT_ALARM_H

#include "Generals.h"

/** callback function type */
typedef void( AlarmCallback_f )( unsigned int alarmId, void* callbackFuncArg );

/** alarm flag */

/** the callback function will only be called once and then removed from the
    registered alarm list */
#define AlarmFlag_NO_REPEAT 0x0

/** keep repeating every X seconds */
#define AlarmFlag_REPEAT 0x01

/** Being processed in Alarm_runAlarms */
#define AlarmFlag_FIRED 0x10

typedef struct Alarm_s {
    /** Alarm interval. Zero if single-shot. */
    struct timeval timeInterval;

    /** alarm flag. Can be AlarmFlag_REPEAT or AlarmFlag_NO_REPEAT (0) */
    unsigned int alarmFlag;

    /** a unique unsigned integer(which is also passed as the first
      * argument of each callback)
      */
    unsigned int alarmId;

    /** Last time the alarm fired [monotonic clock]. */
    struct timeval lastTime;

    /** Next time the alarm will fire [monotonic clock]. */
    struct timeval nextTime;

    /** void pointer to Argument used by the callback function */
    void* callbackFuncArg;

    /** the pointer to the callback function */
    AlarmCallback_f* callbackFunc;

} Alarm;

/** Initializes the alarms. */
void Alarm_initAlarm( void );

/*
 * Loop through everything we have repeatedly looking for the next thing to
 * call until all events are finally in the future again.
 */
void Alarm_runAlarms( void );

/**
 * This function registers function callbacks to occur at a specific time
 * in the future.
 *
 * @param seconds - is an unsigned integer specifying when the callback function
 *                  will be called in seconds.
 *
 * @param alarmFlag - is an unsigned integer that specifies how frequent the callback
 *                    function is called in seconds.  Should be AlarmFlag_REPEAT or
 *                    AlarmFlag_NO_REPEAT (0).  If alarmFlag  is  set with AlarmFlag_REPEAT,
 *                    then the registered callback function will be called every AlarmFlag_REPEAT
 *                    seconds.  If alarmFlag is AlarmFlag_NO_REPEAT (0) then the function will only be
 *                    called once and then removed from the registered alarm list.
 *
 * @param callbackFunc - is a pointer AlarmCallback_f which is the callback
 *                       function being stored and registered.
 *
 * @param callbackFuncArg - is a void pointer used by the callback function.  This
 *                          pointer is assigned to Alarm_s->callbackFuncArg and passed into the
 *                          callback function for the user's specific needs.
 *
 * @return Returns a unique unsigned integer( alarmId - which is also passed as the first
 *         argument of each callback), which can then be used to remove the
 *         callback from the list at a later point in the future using the
 *         Alarm_unregister() function.  If memory could not be allocated
 *         for the Alarm_s struct 0 is returned.
 *
 * \see Alarm_unregister
 * \see Alarm_registerHr
 * \see Alarm_unregisterAll
 */
unsigned int Alarm_register( unsigned int seconds, unsigned int alarmFlag,
    AlarmCallback_f* callbackFunc, void* callbackFuncArg );

/**
 * This function removes the callback function from a list of registered
 * alarms, unregistering the alarm.
 *
 * @param alarmId - is a unique unsigned integer representing a registered
 *                  alarm which the user wants to unregister.
 *
 * @return void
 *
 * \see Alarm_register
 * \see Alarm_registerHr
 * \see Alarm_unregisterAll
 */
void Alarm_unregister( unsigned int alarmId );

/**
 * This function unregisters all alarms currently stored.
 *
 * @return void
 *
 * \see Alarm_register
 * \see Alarm_register_hr
 * \see Alarm_unregister
 */
void Alarm_unregisterAll( void );

/**
 * This function resets an existing alarm.
 *
 * @param alarmId - is a unique unsigned integer representing a registered
 *                  alarm which the client wants to unregister.
 *
 * @return 0 on success, -1 if the alarm was not found
 *
 * \see Alarm_register
 * \see Alarm_registerHr
 * \see Alarm_unregister
 */
int Alarm_reset( unsigned int alarmId );

/** It finds the alarm with the shortest time (nextTime) in the list,
 *  that is, the alarm whose time (nextTime) will end soon.
 *
 *  @return the alarm element or NULL
 */
Alarm* Alarm_findNext( void );

/**
 * Look up the time at which the next alarm will fire.
 *
 *  @param[out] alarmNextTime - Time at which the next alarm will fire.
 *  @param[in] now - Earliest time that should be written into *alarmNextTime.
 *
 *  @return Zero if no alarms are scheduled; non-zero 'alarmId' value identifying
 *          the first alarm that will fire if one or more alarms are scheduled.
 */
int Alarm_getNextAlarmTime( struct timeval* alarmNextTime, const struct timeval* now );

/** @brief  returns a alarm element for a given alarmId within a list
 *
 *  @param   alarmId - the alarmId
 *  @return  a pointer to the alarm element or NULL
 */
Alarm* Alarm_findSpecific( unsigned int alarmId );

#endif // IOT_ALARM_H
