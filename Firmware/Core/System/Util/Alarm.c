#include "Alarm.h"
#include "System/Util/Callback.h"
#include "System/Util/DefaultStore.h"
#include "System/Containers/Map.h"
#include "System/Util/Assert.h"
#include "System/Util/Trace.h"
#include "System/Util/Utilities.h"
#include <sys/time.h>

/** ============================[ Private Variables ]================== */

/** the map to store the all the alarms */
static Map* _alarm_theAlarms = NULL;

/** If _alarm_startAlarms == 0, the alarm is not initialized.
 *  If _alarm_startAlarms == 1, the alarm is not initialized.
 */
static int _alarm_startAlarms = 0;

/** Alarms counter. */
static unsigned int _alarm_alarmIdNumber = 1;

/** ===================[ Private Functions Prototypes ]================== */

/**
 * This function offers finer granularity as to when the callback
 * function is called by making use of t->tv_usec value forming the
 * "seconds" parameter aspect of Alarm_register().
 *
 * @param time -  is a timeval structure used to specify when the callback
 *                function (alarm) will be called.  Adds the ability to specify
 *                microseconds.  time.tv_sec and time.tv_usec are assigned
 *                to Alarm_s->*Time.tv_sec and Alarm_s->*Time.tv_usec respectively internally.
 *                The Alarm_register function only assigns seconds(it's seconds argument).
 *
 * @param alarmFlag - is an unsigned integer that specifies how frequent the callback
 *                    function is called in seconds.  Should be AlarmFlag_REPEAT or 0.
 *                    If alarmFlag is  set with AlarmFlag_REPEAT, then the registered callback
 *                    function will be called every AlarmFlag_REPEAT seconds.  If alarmFlag is 0 then the
 *                    function will only be called once and then removed from the registered alarm list.
 *
 * @param callbackFunc - is a pointer AlarmCallback_f which is the callback
 *                       function being stored and registered.
 *
 * @param callbackFuncArg - is a void pointer used by the callback function.  This
 *                          pointer is assigned to Alarm_s->callbackFuncArg and passed into the
 *                          callback function for the user's specific needs.
 *
 * @return Returns a unique unsigned integer ( alarmId - which is also passed as the first
 *         argument of each callback), which can then be used to remove the
 *         callback from the list at a later point in the future using the
 *         Alarm_unregister() function.  If memory could not be allocated
 *         for the Alarm_s struct 0 is returned.
 *
 * \see Alarm_register
 * \see Alarm_unregister
 * \see Alarm_unregisterAll
 */
static unsigned int _Alarm_register( struct timeval time, unsigned int alarmFlag,
    AlarmCallback_f* callbackFunc, void* callbackFuncArg );

/**
 *  Updates the lastTime and/or nextTime of the alarm.
 *  If lastTime == 0, updates the lastTime for now. And updates
 *  nextTime, using lastTime and timeInterval. If nextTime = 0
 *  and alarmFlag is AlarmFlag_REPEAT and timeInterval is equal to 0,
 *  then it removes the alarm because there can be no alarm with timeInterval == 0
 *  in this condition. If the alarmFlag flag is AlarmFlag_NO_REPEAT, then it removes the alarm.
 *
 * @param alarm - an alarm entry
 *
 * \see Alarm_register
 * \see Alarm_registerHr
 * \see Alarm_unregister
 */
static void _Alarm_updateEntry( Alarm* alarm );

/** The callback function */
static int _Alarm_initAlarmPostConfig( int majorid, int minorid, void* serverarg,
    void* callbackFuncArg );

/** Registers the Alarm_handler function as handler for the SIGALRM signal.
 *  Sets the timer by calling the setitimer function.
 *  When time is out the function Alarm_handler will be called.
 */
static void _Alarm_setAnAlarm( void );

/** It is called when the SIGALRM signal is emitted.
 *  When the time expires this signal is emitted.
 */
static void _Alarm_handler( int a );

/** Used to free the field value of the map */
static void _Alarm_freeFunction( void* value );

/**
 * Get the time until the next alarm will fire.
 *
 * @param[out] delta -  Time until the next alarm.
 *
 * @return Zero if no alarms are scheduled; non-zero 'alarmId' value identifying
 *         the first alarm that will fire if one or more alarms are scheduled.
 */
static int _Alarm_getNextAlarmDelayTime( struct timeval* delta );

/** =============================[ Public Functions ]================== */

unsigned int Alarm_register( unsigned int seconds, unsigned int alarmFlag,
    AlarmCallback_f* callbackFunc, void* callbackFuncArg )
{
    struct timeval time;

    if ( 0 == seconds ) {
        time.tv_sec = 0;
        time.tv_usec = 1;
    } else {
        time.tv_sec = seconds;
        time.tv_usec = 0;
    }

    return _Alarm_register( time, alarmFlag, callbackFunc, callbackFuncArg );
}

void Alarm_initAlarm( void )
{
    _alarm_startAlarms = 0;
    Callback_register( CallbackMajor_LIBRARY, CallbackMinor_POST_READ_CONFIG,
        _Alarm_initAlarmPostConfig, NULL );
}

void Alarm_unregister( unsigned int alarmId )
{
    int r = Map_erase( &_alarm_theAlarms, Convert_int32ToString( alarmId ) );

    if ( r == 0 ) {
        DEBUG_MSGTL( ( "Alarm", "unregistered alarm %d\n", alarmId ) );
    } else {
        DEBUG_MSGTL( ( "Alarm", "no alarm %d to unregister\n", alarmId ) );
    }
}

void Alarm_unregisterAll( void )
{

    Map_clear( _alarm_theAlarms );
    _alarm_theAlarms = NULL;
    DEBUG_MSGTL( ( "Alarm", "ALL alarms unregistered\n" ) );
}

Alarm* Alarm_findNext( void )
{
    Map* a;
    Alarm* lowest = NULL;

    for ( a = _alarm_theAlarms; a != NULL; a = a->next ) {
        Alarm* value = ( Alarm* )a->value;
        if ( !( value->alarmFlag & AlarmFlag_FIRED ) && ( lowest == NULL || timercmp( &value->nextTime, &lowest->nextTime, < ) ) )
            lowest = ( Alarm* )a->value;
    }

    return lowest;
}

Alarm* Alarm_findSpecific( unsigned int alarmId )
{
    return Map_at( _alarm_theAlarms, Convert_int32ToString( alarmId ) );
}

void Alarm_runAlarms( void )
{
    struct Alarm_s* a;
    unsigned int alarmId;
    struct timeval t_now;

    /*
     * Loop through everything we have repeatedly looking for the next thing to
     * call until all events are finally in the future again.
     */

    while ( ( a = Alarm_findNext() ) != NULL ) {
        Time_getMonotonicClock( &t_now );

        if ( timercmp( &a->nextTime, &t_now, > ) )
            return;

        alarmId = a->alarmId;
        a->alarmFlag |= AlarmFlag_FIRED;
        DEBUG_MSGTL( ( "Alarm", "run alarm %d\n", alarmId ) );
        ( *( a->callbackFunc ) )( alarmId, a->callbackFuncArg );
        DEBUG_MSGTL( ( "Alarm", "alarm %d completed\n", alarmId ) );

        a = Alarm_findSpecific( alarmId );
        if ( a ) {
            a->lastTime = t_now;
            timerclear( &a->nextTime );
            a->alarmFlag &= ~AlarmFlag_FIRED;
            _Alarm_updateEntry( a );
        } else {
            DEBUG_MSGTL( ( "Alarm", "alarm %d deleted itself\n", alarmId ) );
        }
    }
}

int Alarm_getNextAlarmTime( struct timeval* alarmNextTime, const struct timeval* now )
{
    Alarm* sa_ptr;

    sa_ptr = Alarm_findNext();

    if ( sa_ptr ) {
        Assert_assert( alarmNextTime );
        Assert_assert( timerisset( &sa_ptr->nextTime ) );
        if ( timercmp( &sa_ptr->nextTime, now, > ) )
            *alarmNextTime = sa_ptr->nextTime;
        else
            *alarmNextTime = *now;
        return sa_ptr->alarmId;
    } else {
        return 0;
    }
}

int Alarm_reset( unsigned int alarmId )
{
    struct Alarm_s* a;
    struct timeval nowTime;
    if ( ( a = Alarm_findSpecific( alarmId ) ) != NULL ) {
        Time_getMonotonicClock( &nowTime );
        a->lastTime.tv_sec = nowTime.tv_sec;
        a->lastTime.tv_usec = nowTime.tv_usec;
        a->nextTime.tv_sec = 0;
        a->nextTime.tv_usec = 0;
        TIME_ADD_TIME( &nowTime, &a->timeInterval, &a->nextTime );
        return 0;
    }
    DEBUG_MSGTL( ( "AlarmReset", "alarm %d not found\n", alarmId ) );
    return -1;
}

/** =============================[ Private Functions ]================== */

static unsigned int _Alarm_register( struct timeval time, unsigned int alarmFlag,
    AlarmCallback_f* callbackFunc, void* callbackFuncArg )
{
    Alarm* newAlarm = NULL;

    newAlarm = MEMORY_MALLOC_TYPEDEF( Alarm );

    if ( newAlarm == NULL )
        return 0;

    newAlarm->timeInterval = time;
    newAlarm->alarmFlag = alarmFlag;
    newAlarm->callbackFuncArg = callbackFuncArg;
    newAlarm->callbackFunc = callbackFunc;
    newAlarm->alarmId = _alarm_alarmIdNumber++;

    Map_emplace( &_alarm_theAlarms, Convert_int32ToString( newAlarm->alarmId ), newAlarm, _Alarm_freeFunction );

    _Alarm_updateEntry( newAlarm );

    DEBUG_MSGTL( ( "priotAlarm", "registered alarm %d, t = %ld.%03ld, flags=0x%02x\n",
        newAlarm->alarmId, ( long )newAlarm->timeInterval.tv_sec,
        ( long )( newAlarm->timeInterval.tv_usec / 1000 ), newAlarm->alarmFlag ) );

    if ( _alarm_startAlarms ) {
        _Alarm_setAnAlarm();
    }

    return newAlarm->alarmId;
}

static int _Alarm_initAlarmPostConfig( int majorid, int minorid, void* serverarg,
    void* clientarg )
{
    _alarm_startAlarms = 1;
    _Alarm_setAnAlarm();
    return ErrorCode_SUCCESS;
}

static void _Alarm_updateEntry( Alarm* alarm )
{
    if ( !timerisset( &alarm->lastTime ) ) {
        /** It just came in here because lastTime == 0.
          * First call of Alarm_updateEntry() for alarm a: set lastTime and nextTime.
          * updates the lastTime for now. And updates nextTime, using lastTime and timeInterval.
          */
        Time_getMonotonicClock( &alarm->lastTime );
        TIME_ADD_TIME( &alarm->lastTime, &alarm->timeInterval, &alarm->nextTime );
    } else if ( !timerisset( &alarm->nextTime ) ) {
        /** It just came in here because lastTime != 0 and nextTime == 0 .
         *  We've been called but not reset for the next call.
         */
        if ( alarm->alarmFlag & AlarmFlag_REPEAT ) {
            if ( timerisset( &alarm->timeInterval ) ) {
                TIME_ADD_TIME( &alarm->lastTime, &alarm->timeInterval, &alarm->nextTime );
            } else {

                /** If nextTime = 0 and alarmFlag is AlarmFlag_REPEAT and timeInterval is equal to 0,
                 *  then it removes the alarm because there can be no alarm with timeInterval == 0
                 *  in this condition.
                 */
                DEBUG_MSGTL( ( "Alarm", "updateEntry: illegal interval specified\n" ) );
                Alarm_unregister( alarm->alarmId );
            }
        } else {
            /**
             * Single time call, remove it.
             * If the alarmFlag flag is AlarmFlag_NO_REPEAT (0), then it removes the alarm.
             */
            Alarm_unregister( alarm->alarmId );
        }
    }
}

static void _Alarm_setAnAlarm( void )
{
    struct timeval delta;
    int nextAlarm = _Alarm_getNextAlarmDelayTime( &delta );

    /**
    * We don't use signals if they asked us nicely not to.  It's expected
    * they'll check the next alarm time and do their own calling of
    * Alarm_runAlarms().
    */

    if ( nextAlarm && !DefaultStore_getBoolean( DsStore_LIBRARY_ID, DsBool_ALARM_DONT_USE_SIGNAL ) ) {
        struct itimerval it;

        it.it_value = delta;
        timerclear( &it.it_interval );

        signal( SIGALRM, _Alarm_handler );
        setitimer( ITIMER_REAL, &it, NULL );

        DEBUG_MSGTL( ( "Alarm", "schedule alarm %d in %ld.%03ld seconds\n",
            nextAlarm, ( long )delta.tv_sec, ( long )( delta.tv_usec / 1000 ) ) );

    } else {
        DEBUG_MSGTL( ( "Alarm", "no alarms found to schedule\n" ) );
    }
}

static void _Alarm_handler( int a )
{
    Alarm_runAlarms();
    _Alarm_setAnAlarm();
}

static void _Alarm_freeFunction( void* value )
{
    /** value == callbackFuncArg
     *  Note: do not free the callbackFuncArg, it's the user's responsibility
     */
}

static int _Alarm_getNextAlarmDelayTime( struct timeval* delta )
{
    struct timeval t_now, alarm_tm;
    int res;

    Time_getMonotonicClock( &t_now );
    res = Alarm_getNextAlarmTime( &alarm_tm, &t_now );
    if ( res )
        TIME_SUB_TIME( &alarm_tm, &t_now, delta );
    return res;
}
