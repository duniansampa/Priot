#ifndef ALARM_H
#define ALARM_H

#include "Generals.h"

typedef void( Alarm_CallbackFT )( unsigned int clientreg, void* clientarg );

/*
 * alarm flags
 */
#define ALARM_SA_REPEAT 0x01 /* keep repeating every X seconds */
#define ALARM_SA_FIRED 0x10 /* Being processed in run_alarms */

struct Alarm_s {
    /** Alarm interval. Zero if single-shot. */
    struct timeval t;
    unsigned int flags;
    unsigned int clientreg;
    /** Last time the alarm fired [monotonic clock]. */
    struct timeval t_lastM;
    /** Next time the alarm will fire [monotonic clock]. */
    struct timeval t_nextM;
    void* clientarg;
    Alarm_CallbackFT* thecallback;
    struct Alarm_s* next;
};

/*
 * the ones you should need
 */

void Alarm_unregister( unsigned int clientreg );
void Alarm_unregisterAll( void );

unsigned int Alarm_register( unsigned int when, unsigned int flags,
    Alarm_CallbackFT* thecallback, void* clientarg );

unsigned int Alarm_registerHr( struct timeval t, unsigned int flags,
    Alarm_CallbackFT* cb, void* cd );

int Alarm_reset( unsigned int clientreg );

/*
 * the ones you shouldn't
 */
void Alarm_initAlarm( void );

int Alarm_initAlarmPostConfig( int majorid, int minorid, void* serverarg,
    void* clientarg );

void Alarm_saUpdateEntry( struct Alarm_s* alrm );

struct Alarm_s* Alarm_saFindNext( void );

void Alarm_runAlarms( void );

void Alarm_handler( int a );
void Alarm_setAnAlarm( void );
int Alarm_getNextAlarmTime( struct timeval* alarm_tm, const struct timeval* now );
int Alarm_getNextAlarmDelayTime( struct timeval* delta );
struct Alarm_s* Alarm_saFindSpecific( unsigned int clientreg );

#endif // ALARM_H
