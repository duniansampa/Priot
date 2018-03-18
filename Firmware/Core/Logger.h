#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

#define  LOGGER_PRIORITY_EMERG       0       /* system is unusable */
#define  LOGGER_PRIORITY_ALERT       1       /* action must be taken immediately */
#define  LOGGER_PRIORITY_CRIT        2       /* critical conditions */
#define  LOGGER_PRIORITY_ERR         3       /* error conditions */
#define  LOGGER_PRIORITY_WARNING     4       /* warning conditions */
#define  LOGGER_PRIORITY_NOTICE      5       /* normal but significant condition */
#define  LOGGER_PRIORITY_INFO        6       /* informational */
#define  LOGGER_PRIORITY_DEBUG       7       /* debug-level messages */


#define  LOGGER_LOG_TO_STDOUT	1
#define  LOGGER_LOG_TO_STDERR	2
#define  LOGGER_LOG_TO_FILE		3
#define  LOGGER_LOG_TO_SYSLOG	4
#define  LOGGER_LOG_TO_CALLBACK	5
#define  LOGGER_LOG_TO_NONE		6


#define LOGGER_DEFAULT_LOG_ID "priot"


#define  LOGGER_LOGONCE(x)      \
    do {                        \
        static char logged = 0; \
        if (!logged) {          \
            logged = 1;         \
            Logger_log x ;      \
        }                       \
    } while(0)


typedef struct Logger_LogHandler_s Logger_LogHandler;

typedef int (Logger_FunctionHandler)(Logger_LogHandler*, int, const char *);


struct Logger_LogMessage {
    int             priority;
    const char     *msg;
};


struct Logger_LogHandler_s {
    int	enabled;
    int	priority;
    int	pri_max;
    int	type;
    const char *token;		/* Also used for filename */

    Logger_FunctionHandler	*handler;

    int    imagic;		/* E.g. file descriptor, syslog facility */
    void   *magic;		/* E.g. Callback function */

    Logger_LogHandler	*next, *prev;
};


//==== public functions ======//
void Logger_initLogger(void);
void Logger_disableSyslog(void);
void Logger_disableFilelog(void);
void Logger_disableStderrlog(void);
void Logger_disableCalllog(void);
void Logger_enableSyslog(void);
void Logger_enableSyslogIdent(const char *ident, const int facility);
void Logger_enableFilelog(Logger_LogHandler *logh, int dont_zero_log);
void Logger_enableStderrlog(void);
void Logger_enableCalllog(void);
int  Logger_stderrlogStatus(void);
void Logger_setLineBuffering(FILE *stream);
int  Logger_logOptions(char *optarg, int argc, char *const *argv);
void Logger_logOptionsUsage(const char *lead, FILE * outf);
int  Logger_addLoghandler( Logger_LogHandler *logh );
int  Logger_removeLoghandler( Logger_LogHandler *logh );
int  Logger_enableLoghandler( const char *token );
int  Logger_disableLoghandler( const char *token );
void Logger_enableThisLoghandler(Logger_LogHandler *logh);
void Logger_disableThisLoghandler(Logger_LogHandler *logh);
void Logger_loggingRestart(void);
char * Logger_logSyslogname(const char *pstr);
Logger_LogHandler  * Logger_getLogHandlerHead(void);
Logger_LogHandler * Logger_registerLoghandler( int type, int priority );
Logger_LogHandler * Logger_findLoghandler( const char *token );



//==== private functions ======//
void Logger_parseConfigLogOption(const char *token, char *cptr);
void Logger_shutdownLogger(void);
int  Logger_decodePriority( char **optarg, int *pri_max );
int  Logger_decodeFacility( char *optarg );
int  Logger_isDoLogging(void);
//static char * Logger_sprintfStamp(time_t * now, char *sbuf);
void Logger_disableSyslogEntry(Logger_LogHandler *logh);
void Logger_disableLog(void);
void Logger_disableFilelogEntry(Logger_LogHandler *logh);
void Logger_enableSyslogIdent(const char *ident, const int facility);
void Logger_logPerror(const char *s);
void Logger_enableFilelog2(const char *logfilename, int dont_zero_log);
int  Logger_logHandlerStdouterr( Logger_LogHandler * logh, int pri, const char *str);
int  Logger_logHandlerSyslog(  Logger_LogHandler * logh, int pri, const char *str);
int  Logger_logHandlerFile(Logger_LogHandler * logh, int pri, const char *str);
int  Logger_logHandlerCallback(Logger_LogHandler * logh, int pri, const char *str);
int  Logger_logHandlerNull( Logger_LogHandler * logh, int pri, const char *str);
void Logger_logString(int priority, const char *str);
int  Logger_vlog(int priority, const char *format, va_list ap);
int  Logger_log(int priority, const char *format, ...);
void Logger_logString(int priority, const char *str);
Logger_LogHandler * Logger_findLoghandler( const char *token );
Logger_LogHandler * Logger_registerLoghandler( int type, int priority );

#endif // LOGGER_H
