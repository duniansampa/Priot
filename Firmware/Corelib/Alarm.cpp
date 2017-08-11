#include "Alarm.h"

#include "Tools.h"
#include "Debug.h"
#include "DefaultStore.h"
#include "Assert.h"
#include "Callback.h"

#include <sys/time.h>


static struct Alarm_alarm_s * alarm_thealarms = NULL;
static int alarm_startAlarms = 0;
static unsigned int alarm_regnum = 1;

int Alarm_initAlarmPostConfig(int majorid, int minorid, void *serverarg,
                       void *clientarg)
{
    alarm_startAlarms = 1;
    Alarm_setAnAlarm();
    return API_ERR_SUCCESS;
}

void Alarm_initAlarm(void)
{
    alarm_startAlarms = 0;
    Callback_registerCallback(CALLBACK_LIBRARY,
                           CALLBACK_POST_READ_CONFIG,
                           Alarm_initAlarmPostConfig, NULL);
}

void Alarm_saUpdateEntry(struct Alarm_alarm_s *a)
{
    if (!timerisset(&a->t_lastM)) {
        /*
         * First call of Alarm_saUpdateEntry() for alarm a: set t_lastM and t_nextM.
         */
        Tools_getMonotonicClock(&a->t_lastM);
        TOOLS_TIMERADD(&a->t_lastM, &a->t, &a->t_nextM);
    } else if (!timerisset(&a->t_nextM)) {
        /*
         * We've been called but not reset for the next call.
         */
        if (a->flags & ALARM_SA_REPEAT) {
            if (timerisset(&a->t)) {
                TOOLS_TIMERADD(&a->t_lastM, &a->t, &a->t_nextM);
            } else {
                DEBUG_MSGTL(("snmp_alarm",
                            "update_entry: illegal interval specified\n"));
                Alarm_unregister(a->clientreg);
            }
        } else {
            /*
             * Single time call, remove it.
             */
            Alarm_unregister(a->clientreg);
        }
    }
}

/**
 * This function removes the callback function from a list of registered
 * alarms, unregistering the alarm.
 *
 * @param clientreg is a unique unsigned integer representing a registered
 *	alarm which the client wants to unregister.
 *
 * @return void
 *
 * @see Alarm_register
 * @see Alarm_registerHr
 * @see Alarm_unregisterAll
 */
void Alarm_unregister(unsigned int clientreg)
{
    struct Alarm_alarm_s *sa_ptr, **prevNext = &alarm_thealarms;

    for (sa_ptr = alarm_thealarms;
         sa_ptr != NULL && sa_ptr->clientreg != clientreg;
         sa_ptr = sa_ptr->next) {
        prevNext = &(sa_ptr->next);
    }

    if (sa_ptr != NULL) {
        *prevNext = sa_ptr->next;
        DEBUG_MSGTL(("snmp_alarm", "unregistered alarm %d\n",
            sa_ptr->clientreg));
        /*
         * Note: do not free the clientarg, it's the client's responsibility
         */
        free(sa_ptr);
    } else {
        DEBUG_MSGTL(("snmp_alarm", "no alarm %d to unregister\n", clientreg));
    }
}

/**
 * This function unregisters all alarms currently stored.
 *
 * @return void
 *
 * @see Alarm_register
 * @see Alarm_register_hr
 * @see Alarm_unregister
 */
void Alarm_unregisterAll(void)
{
  struct Alarm_alarm_s *sa_ptr, *sa_tmp;

  for (sa_ptr = alarm_thealarms; sa_ptr != NULL; sa_ptr = sa_tmp) {
    sa_tmp = sa_ptr->next;
    free(sa_ptr);
  }
  DEBUG_MSGTL(("snmp_alarm", "ALL alarms unregistered\n"));
  alarm_thealarms = NULL;
}

struct Alarm_alarm_s * Alarm_saFindNext(void)
{
    struct Alarm_alarm_s *a, *lowest = NULL;

    for (a = alarm_thealarms; a != NULL; a = a->next)
        if (!(a->flags & ALARM_SA_FIRED)
            && (lowest == NULL || timercmp(&a->t_nextM, &lowest->t_nextM, <)))
            lowest = a;

    return lowest;
}

struct Alarm_alarm_s * Alarm_saFindSpecific(unsigned int clientreg)
{
    struct Alarm_alarm_s *sa_ptr;
    for (sa_ptr = alarm_thealarms; sa_ptr != NULL; sa_ptr = sa_ptr->next) {
        if (sa_ptr->clientreg == clientreg) {
            return sa_ptr;
        }
    }
    return NULL;
}

void Alarm_runAlarms(void)
{
    struct Alarm_alarm_s *a;
    unsigned int    clientreg;
    struct timeval  t_now;

    /*
     * Loop through everything we have repeatedly looking for the next thing to
     * call until all events are finally in the future again.
     */

    while ((a = Alarm_saFindNext()) != NULL) {
        Tools_getMonotonicClock(&t_now);

        if (timercmp(&a->t_nextM, &t_now, >))
            return;

        clientreg = a->clientreg;
        a->flags |= ALARM_SA_FIRED;
        DEBUG_MSGTL(("snmp_alarm", "run alarm %d\n", clientreg));
        (*(a->thecallback)) (clientreg, a->clientarg);
        DEBUG_MSGTL(("snmp_alarm", "alarm %d completed\n", clientreg));

        a = Alarm_saFindSpecific(clientreg);
        if (a) {
            a->t_lastM = t_now;
            timerclear(&a->t_nextM);
            a->flags &= ~ALARM_SA_FIRED;
            Alarm_saUpdateEntry(a);
        } else {
            DEBUG_MSGTL(("snmp_alarm", "alarm %d deleted itself\n",
                        clientreg));
        }
    }
}



void Alarm_handler(int a)
{
    Alarm_runAlarms();
    Alarm_setAnAlarm();
}



/**
 * Look up the time at which the next alarm will fire.
 *
 * @param[out] alarm_tm Time at which the next alarm will fire.
 * @param[in] now Earliest time that should be written into *alarm_tm.
 *
 * @return Zero if no alarms are scheduled; non-zero 'clientreg' value
 *   identifying the first alarm that will fire if one or more alarms are
 *   scheduled.
 */
int Alarm_getNextAlarmTime(struct timeval *alarm_tm, const struct timeval *now)
{
    struct Alarm_alarm_s *sa_ptr;

    sa_ptr = Alarm_saFindNext();

    if (sa_ptr) {
        Assert_assert(alarm_tm);
        Assert_assert(timerisset(&sa_ptr->t_nextM));
        if (timercmp(&sa_ptr->t_nextM, now, >))
            *alarm_tm = sa_ptr->t_nextM;
        else
            *alarm_tm = *now;
        return sa_ptr->clientreg;
    } else {
        return 0;
    }
}

/**
 * Get the time until the next alarm will fire.
 *
 * @param[out] delta Time until the next alarm.
 *
 * @return Zero if no alarms are scheduled; non-zero 'clientreg' value
 *   identifying the first alarm that will fire if one or more alarms are
 *   scheduled.
 */
int Alarm_getNextAlarmDelayTime(struct timeval *delta)
{
    struct timeval t_now, alarm_tm;
    int res;

    Tools_getMonotonicClock(&t_now);
    res = Alarm_getNextAlarmTime(&alarm_tm, &t_now);
    if (res)
        TOOLS_TIMERSUB(&alarm_tm, &t_now, delta);
    return res;
}


void Alarm_setAnAlarm(void)
{
    struct timeval  delta;
    int    nextalarm = Alarm_getNextAlarmDelayTime(&delta);

    /*
     * We don't use signals if they asked us nicely not to.  It's expected
     * they'll check the next alarm time and do their own calling of
     * Alarm_runAlarms().
     */

    if (nextalarm && !DefaultStore_getBoolean(DEFAULTSTORE_STORAGE::LIBRARY_ID,
                    DEFAULTSTORE_LIB_BOOLEAN::ALARM_DONT_USE_SIG)) {
        struct itimerval it;

        it.it_value = delta;
        timerclear(&it.it_interval);

        signal(SIGALRM, Alarm_handler);
        setitimer(ITIMER_REAL, &it, NULL);
        DEBUG_MSGTL(("snmp_alarm", "schedule alarm %d in %ld.%03ld seconds\n",
                    nextalarm, (long) delta.tv_sec, (long)(delta.tv_usec / 1000)));


    } else {
        DEBUG_MSGTL(("snmp_alarm", "no alarms found to schedule\n"));
    }
}


/**
 * This function registers function callbacks to occur at a specific time
 * in the future.
 *
 * @param when is an unsigned integer specifying when the callback function
 *             will be called in seconds.
 *
 * @param flags is an unsigned integer that specifies how frequent the callback
 *	function is called in seconds.  Should be ALARM_SA_REPEAT or 0.  If
 *	flags  is  set with ALARM_SA_REPEAT, then the registered callback function
 *	will be called every ALARM_SA_REPEAT seconds.  If flags is 0 then the
 *	function will only be called once and then removed from the
 *	registered alarm list.
 *
 * @param thecallback is a pointer Alarm_CallbackFUNC which is the callback
 *	function being stored and registered.
 *
 * @param clientarg is a void pointer used by the callback function.  This
 *	pointer is assigned to Alarm_alarm_s->clientarg and passed into the
 *	callback function for the client's specific needs.
 *
 * @return Returns a unique unsigned integer(which is also passed as the first
 *	argument of each callback), which can then be used to remove the
 *	callback from the list at a later point in the future using the
 *	Alarm_unregister() function.  If memory could not be allocated
 *	for the snmp_alarm struct 0 is returned.
 *
 * @see Alarm_unregister
 * @see Alarm_registerHr
 * @see Alarm_unregisterAll
 */
unsigned int Alarm_register(unsigned int when, unsigned int flags,
                    Alarm_CallbackFT * thecallback, void *clientarg)
{
    struct timeval  t;

    if (0 == when) {
        t.tv_sec = 0;
        t.tv_usec = 1;
    } else {
        t.tv_sec = when;
        t.tv_usec = 0;
    }

    return Alarm_registerHr(t, flags, thecallback, clientarg);
}


/**
 * This function offers finer granularity as to when the callback
 * function is called by making use of t->tv_usec value forming the
 * "when" aspect of Alarm_register().
 *
 * @param t is a timeval structure used to specify when the callback
 *	function(alarm) will be called.  Adds the ability to specify
 *	microseconds.  t.tv_sec and t.tv_usec are assigned
 *	to Alarm_alarm_s->tv_sec and Alarm_alarm_s->tv_usec respectively internally.
 *	The Alarm_register function only assigns seconds(it's when
 *	argument).
 *
 * @param flags is an unsigned integer that specifies how frequent the callback
 *	function is called in seconds.  Should be ALARM_SA_REPEAT or NULL.  If
 *	flags  is  set with ALARM_SA_REPEAT, then the registered callback function
 *	will be called every ALARM_SA_REPEAT seconds.  If flags is NULL then the
 *	function will only be called once and then removed from the
 *	registered alarm list.
 *
 * @param cb is a pointer Alarm_CallbackFUNC which is the callback
 *	function being stored and registered.
 *
 * @param cd is a void pointer used by the callback function.  This
 *	pointer is assigned to Alarm_alarm_s->clientarg and passed into the
 *	callback function for the client's specific needs.
 *
 * @return Returns a unique unsigned integer(which is also passed as the first
 *	argument of each callback), which can then be used to remove the
 *	callback from the list at a later point in the future using the
 *	Alarm_unregister() function.  If memory could not be allocated
 *	for the Alarm_alarm_s struct 0 is returned.
 *
 * @see Alarm_register
 * @see Alarm_unregister
 * @see Alarm_unregisterAll
 */
unsigned int Alarm_registerHr(struct timeval t, unsigned int flags,
                       Alarm_CallbackFT * cb, void *cd)
{
    struct Alarm_alarm_s **s = NULL;

    for (s = &(alarm_thealarms); *s != NULL; s = &((*s)->next));

    *s = TOOLS_MALLOC_STRUCT(Alarm_alarm_s);
    if (*s == NULL) {
        return 0;
    }

    (*s)->t = t;
    (*s)->flags = flags;
    (*s)->clientarg = cd;
    (*s)->thecallback = cb;
    (*s)->clientreg = alarm_regnum++;
    (*s)->next = NULL;

    Alarm_saUpdateEntry(*s);

    DEBUG_MSGTL(("snmp_alarm",
                "registered alarm %d, t = %ld.%03ld, flags=0x%02x\n",
                (*s)->clientreg, (long) (*s)->t.tv_sec, (long)((*s)->t.tv_usec / 1000),
                (*s)->flags));

    if (alarm_startAlarms) {
        Alarm_setAnAlarm();
    }

    return (*s)->clientreg;
}

/**
 * This function resets an existing alarm.
 *
 * @param clientreg is a unique unsigned integer representing a registered
 *	alarm which the client wants to unregister.
 *
 * @return 0 on success, -1 if the alarm was not found
 *
 * @see Alarm_register
 * @see Alarm_registerHr
 * @see Alarm_unregister
 */
int Alarm_reset(unsigned int clientreg)
{
    struct Alarm_alarm_s *a;
    struct timeval  t_now;
    if ((a = Alarm_saFindSpecific(clientreg)) != NULL) {
        Tools_getMonotonicClock(&t_now);
        a->t_lastM.tv_sec = t_now.tv_sec;
        a->t_lastM.tv_usec = t_now.tv_usec;
        a->t_nextM.tv_sec = 0;
        a->t_nextM.tv_usec = 0;
        TOOLS_TIMERADD(&t_now, &a->t, &a->t_nextM);
        return 0;
    }
    DEBUG_MSGTL(("snmp_alarm_reset", "alarm %d not found\n",
                clientreg));
    return -1;
}
/**  @} */
