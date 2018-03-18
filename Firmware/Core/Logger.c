#include "Logger.h"

#include <sys/syslog.h>
#include "Debug.h"
#include "DefaultStore.h"
#include "Tools.h"
#include "ReadConfig.h"
#include "Strlcpy.h"
#include "Assert.h"
#include "Callback.h"

#include <ctype.h>

#define LOGLENGTH 1024

/*
 * logger_logHandlerHead:  A list of all log handlers, in increasing order of priority
 * logh_priorities:  'Indexes' into this list, by priority
 */
Logger_LogHandler * logger_logHandlerHead = NULL;
Logger_LogHandler * logger_logHandlerPriorities[LOGGER_PRIORITY_DEBUG + 1 ];

static int  _logger_logHandlerEnabled = 0;


static char _logger_syslogname[64] = LOGGER_DEFAULT_LOG_ID;


void Logger_disableThisLoghandler(Logger_LogHandler *logh)
{
    if (!logh || (0 == logh->enabled))
        return;
    logh->enabled = 0;
    --_logger_logHandlerEnabled;
    Assert_assert(_logger_logHandlerEnabled >= 0);
}

void Logger_enableThisLoghandler(Logger_LogHandler *logh)
{
    if (!logh || (0 != logh->enabled))
        return;
    logh->enabled = 1;
    ++_logger_logHandlerEnabled;
}


void Logger_parseConfigLogOption(const char *token, char *cptr)
{
  int my_argc = 0 ;
  char **my_argv = NULL;

  Logger_logOptions( cptr, my_argc, my_argv );
}

void Logger_initLogger(void)
{

    DefaultStore_registerPremib(DATATYPE_BOOLEAN, "priot", "logTimestamp",
                                DsStorage_LIBRARY_ID, DsBool_LOG_TIMESTAMP);

    ReadConfig_registerPrenetMibHandler("priot", "logOption",
                                    Logger_parseConfigLogOption, NULL, "string");

}

void Logger_shutdownLogger(void)
{
   Logger_disableLog();
   while(NULL != logger_logHandlerHead)
      Logger_removeLoghandler( logger_logHandlerHead );
}


/* Set line buffering mode for a stream. */
void Logger_setLineBuffering(FILE *stream)
{
    /* See also the C89 or C99 standard for more information about setvbuf(). */
    setvbuf(stream, NULL, _IOLBF, BUFSIZ);
}

/*
* Decodes log priority.
* @param optarg - IN - priority to decode, "0" or "0-7"
*                 OUT - points to last character after the decoded priority
* @param pri_max - OUT - maximum priority (i.e. 0x7 from "0-7")
*/
int Logger_decodePriority( char **optarg, int *pri_max )
{
    int pri_low = LOG_DEBUG;

    if (*optarg == NULL)
        return -1;

    switch (**optarg) {
    case '0':
    case '!':
        pri_low = LOGGER_PRIORITY_EMERG;
        break;
    case '1':
    case 'a':
    case 'A':
        pri_low = LOGGER_PRIORITY_ALERT;
        break;
    case '2':
    case 'c':
    case 'C':
        pri_low = LOGGER_PRIORITY_CRIT;
        break;
    case '3':
    case 'e':
    case 'E':
        pri_low = LOGGER_PRIORITY_ERR;
        break;
    case '4':
    case 'w':
    case 'W':
        pri_low = LOGGER_PRIORITY_WARNING;
        break;
    case '5':
    case 'n':
    case 'N':
        pri_low = LOGGER_PRIORITY_NOTICE;
        break;
    case '6':
    case 'i':
    case 'I':
        pri_low = LOGGER_PRIORITY_INFO;
        break;
    case '7':
    case 'd':
    case 'D':
        pri_low = LOGGER_PRIORITY_DEBUG;
        break;
    default:
        fprintf(stderr, "invalid priority: %c\n",**optarg);
        return -1;
    }
    *optarg = *optarg+1;

    if (pri_max && **optarg=='-') {
        *optarg = *optarg + 1; /* skip '-' */
        *pri_max = Logger_decodePriority( optarg, NULL );
        if (*pri_max == -1) return -1;
        if (pri_low < *pri_max) {
            int tmp = pri_low;
            pri_low = *pri_max;
            *pri_max = tmp;
        }

    }
    return pri_low;
}

int Logger_decodeFacility( char *optarg )
{
    if (optarg == NULL)
        return -1;

    switch (*optarg) {
    case 'd':
    case 'D':
        return LOG_DAEMON;
    case 'u':
    case 'U':
        return LOG_USER;
    case '0':
        return LOG_LOCAL0;
    case '1':
        return LOG_LOCAL1;
    case '2':
        return LOG_LOCAL2;
    case '3':
        return LOG_LOCAL3;
    case '4':
        return LOG_LOCAL4;
    case '5':
        return LOG_LOCAL5;
    case '6':
        return LOG_LOCAL6;
    case '7':
        return LOG_LOCAL7;
    default:
        fprintf(stderr, "invalid syslog facility: %c\n",*optarg);
        return -1;
    }
}


int Logger_logOptions(char *optarg, int argc, char *const *argv)
{
    char           *cp = optarg;
    /*
     * Hmmm... this doesn't seem to work.
     * The main agent 'getopt' handling assumes
     *   that the -L option takes an argument,
     *   and objects if this is missing.
     * Trying to differentiate between
     *   new-style "-Lx", and old-style "-L xx"
     *   is likely to be a major headache.
     */
    char            missing_opt = 'e';	/* old -L is new -Le */
    int             priority = LOGGER_PRIORITY_DEBUG;
    int             pri_max  = LOGGER_PRIORITY_EMERG;
    int             inc_optind = 0;
    Logger_LogHandler *logh;

    DEBUG_MSGT(("logging:options", "optarg: '%s', argc %d, argv '%s'\n",
               optarg, argc, argv ? argv[0] : "NULL"));
    optarg++;
    if (!*cp)
        cp = &missing_opt;

    /*
     * Support '... -Lx=value ....' syntax
     */
    if (*optarg == '=') {
        optarg++;
    }
    /*
     * and '.... "-Lx value" ....'  (*with* the quotes)
     */
    while (*optarg && isspace((unsigned char)(*optarg))) {
        optarg++;
    }
    /*
     * Finally, handle ".... -Lx value ...." syntax
     *   (*without* surrounding quotes)
     */
    if ((!*optarg) && (NULL != argv)) {
        /*
         * We've run off the end of the argument
         *  so move on to the next.
         * But we might not actually need it, so don't
     *  increment optind just yet!
         */
        optarg = argv[optind];
        inc_optind = 1;
    }

    DEBUG_MSGT(("logging:options", "*cp: '%c'\n", *cp));
    switch (*cp) {
    /*
     * Log to Standard Error
     */
    case 'E':
        priority = Logger_decodePriority( &optarg, &pri_max );
        if (priority == -1)  return -1;
        if (inc_optind)
            optind++;
        /* Fallthrough */
    case 'e':
        logh = Logger_registerLoghandler(LOGGER_LOG_TO_STDERR, priority);
        if (logh) {
            Logger_setLineBuffering(stderr);
            logh->pri_max = pri_max;
            logh->token   = strdup("stderr");
        }
        break;

        /*
     * Log to Standard Output
     */
    case 'O':
        priority = Logger_decodePriority( &optarg, &pri_max );
        if (priority == -1)  return -1;
        if (inc_optind)
            optind++;
        /* Fallthrough */
    case 'o':
        logh = Logger_registerLoghandler(LOGGER_LOG_TO_STDERR, priority);
        if (logh) {
            Logger_setLineBuffering(stdout);
            logh->pri_max = pri_max;
            logh->token   = strdup("stdout");
            logh->imagic  = 1;	    /* stdout, not stderr */
        }
        break;

        /*
         * Log to a named file
         */
    case 'F':
        priority = Logger_decodePriority( &optarg, &pri_max );
        if (priority == -1 || !argv)  return -1;
        optarg = argv[++optind];
        /* Fallthrough */
    case 'f':
        if (inc_optind)
            optind++;
        if (!optarg) {
            fprintf(stderr, "Missing log file\n");
            return -1;
        }
        logh = Logger_registerLoghandler(LOGGER_LOG_TO_FILE, priority);
        if (logh) {
            logh->pri_max = pri_max;
            logh->token   = strdup(optarg);
            Logger_enableFilelog(logh,
                                   DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                                                          DsBool_APPEND_LOGFILES));
        }
        break;
        /*
         * Log to syslog
         */
    case 'S':
        priority = Logger_decodePriority( &optarg, &pri_max );
        if (priority == -1 || !argv)  return -1;
        if (!optarg[0]) {
            /* The command line argument with priority does not contain log
                 * facility. The facility must be in next argument then. */
            optind++;
            if (optind < argc)
                optarg = argv[optind];
        }
        /* Fallthrough */
    case 's':
        if (inc_optind)
            optind++;
        if (!optarg) {
            fprintf(stderr, "Missing syslog facility\n");
            return -1;
        }
        logh = Logger_registerLoghandler(LOGGER_LOG_TO_SYSLOG, priority);
        if (logh) {
            int facility = Logger_decodeFacility(optarg);
            if (facility == -1)  return -1;
            logh->pri_max = pri_max;
            logh->token   = strdup(Logger_logSyslogname(NULL));
            logh->magic   = (void *)(intptr_t)facility;
            Logger_enableSyslogIdent(Logger_logSyslogname(NULL), facility);
        }
        break;

        /*
             * Don't log
             */
    case 'N':
        priority = Logger_decodePriority( &optarg, &pri_max );
        if (priority == -1)  return -1;
        if (inc_optind)
            optind++;
        /* Fallthrough */
    case 'n':
        /*
         * disable all logs to clean them up (close files, etc),
         * remove all log handlers, then register a null handler.
         */
        Logger_disableLog();
        while(NULL != logger_logHandlerHead)
            Logger_removeLoghandler( logger_logHandlerHead );
        logh = Logger_registerLoghandler(LOGGER_LOG_TO_NONE, priority);
        if (logh) {
            logh->pri_max = pri_max;
        }
        break;

    default:
        fprintf(stderr, "Unknown logging option passed to -L: %c.\n", *cp);
        return -1;
    }
    return 0;
}

char * Logger_logSyslogname(const char *pstr)
{
  if (pstr)
    Strlcpy_strlcpy (_logger_syslogname, pstr, sizeof(_logger_syslogname));

  return _logger_syslogname;
}

void Logger_logOptionsUsage(const char *lead, FILE * outf)
{
    const char *pri1_msg = " for level 'pri' and above";
    const char *pri2_msg = " for levels 'p1' to 'p2'";
    fprintf(outf, "%se:           log to standard error\n", lead);
    fprintf(outf, "%so:           log to standard output\n", lead);
    fprintf(outf, "%sn:           don't log at all\n", lead);
    fprintf(outf, "%sf file:      log to the specified file\n", lead);
    fprintf(outf, "%ss facility:  log to syslog (via the specified facility)\n", lead);
    fprintf(outf, "\n%s(variants)\n", lead);
    fprintf(outf, "%s[EON] pri:   log to standard error, output or /dev/null%s\n", lead, pri1_msg);
    fprintf(outf, "%s[EON] p1-p2: log to standard error, output or /dev/null%s\n", lead, pri2_msg);
    fprintf(outf, "%s[FS] pri token:    log to file/syslog%s\n", lead, pri1_msg);
    fprintf(outf, "%s[FS] p1-p2 token:  log to file/syslog%s\n", lead, pri2_msg);
}

/**
 * Is logging done?
 *
 * @return Returns 0 if logging is off, 1 when it is done.
 *
 */
int Logger_isDoLogging(void)
{
    return (_logger_logHandlerEnabled > 0);
}

static char * _Logger_sprintfStamp(time_t * now, char *sbuf)
{
    time_t          Now;
    struct tm      *tm;

    if (now == NULL) {
        now = &Now;
        time(now);
    }
    tm = localtime(now);
    sprintf(sbuf, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d ",
            tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
            tm->tm_hour, tm->tm_min, tm->tm_sec);
    return sbuf;
}

void Logger_disableSyslogEntry(Logger_LogHandler *logh)
{
    if (!logh || !logh->enabled || logh->type != LOGGER_LOG_TO_SYSLOG)
        return;
    closelog();
    logh->imagic  = 0;

    Logger_disableThisLoghandler(logh);
}

void Logger_disableSyslog(void)
{
    Logger_LogHandler *logh;

    for (logh = logger_logHandlerHead; logh; logh = logh->next)
        if (logh->enabled && logh->type == LOGGER_LOG_TO_SYSLOG)
            Logger_disableSyslogEntry(logh);
}


void Logger_disableFilelogEntry(Logger_LogHandler *logh)
{
    if (!logh /* || !logh->enabled */ || logh->type != LOGGER_LOG_TO_FILE)
        return;

    if (logh->magic) {
        fputs("\n", (FILE*)logh->magic);	/* XXX - why? */
        fclose((FILE*)logh->magic);
        logh->magic   = NULL;
    }

    Logger_disableThisLoghandler(logh);
}

void Logger_disableFilelog(void)
{
    Logger_LogHandler *logh;

    for (logh = logger_logHandlerHead; logh; logh = logh->next)
        if (logh->enabled && logh->type == LOGGER_LOG_TO_FILE)
            Logger_disableFilelogEntry(logh);
}

/*
 * returns that status of stderr logging
 *
 * @retval 0 : stderr logging disabled
 * @retval 1 : stderr logging enabled
 */
int Logger_stderrlogStatus(void)
{
    Logger_LogHandler *logh;

    for (logh = logger_logHandlerHead; logh; logh = logh->next)
        if (logh->enabled && (logh->type == LOGGER_LOG_TO_STDOUT ||
                              logh->type == LOGGER_LOG_TO_STDERR)) {
            return 1;
       }

    return 0;
}


void Logger_disableStderrlog(void)
{
    Logger_LogHandler *logh;

    for (logh = logger_logHandlerHead; logh; logh = logh->next)
        if (logh->enabled && (logh->type == LOGGER_LOG_TO_STDOUT ||
                              logh->type == LOGGER_LOG_TO_STDERR)) {
            Logger_disableThisLoghandler(logh);
    }
}

void Logger_disableCalllog(void)
{
    Logger_LogHandler *logh;

    for (logh = logger_logHandlerHead; logh; logh = logh->next)
        if (logh->enabled && logh->type == LOGGER_LOG_TO_CALLBACK) {
            Logger_disableThisLoghandler(logh);
    }
}


void Logger_disableLog(void)
{
    Logger_LogHandler *logh;

    for (logh = logger_logHandlerHead; logh; logh = logh->next) {
        if (logh->type == LOGGER_LOG_TO_SYSLOG)
            Logger_disableSyslogEntry(logh);
        if (logh->type == LOGGER_LOG_TO_FILE)
            Logger_disableFilelogEntry(logh);
        Logger_disableThisLoghandler(logh);
    }
}

/*
 * close and reopen all file based logs, to allow logfile
 * rotation.
 */
void Logger_loggingRestart(void)
{
    Logger_LogHandler *logh;
    int doneone = 0;

    for (logh = logger_logHandlerHead; logh; logh = logh->next) {
        if (0 == logh->enabled)
            continue;
        if (logh->type == LOGGER_LOG_TO_SYSLOG) {
            Logger_disableSyslogEntry(logh);
            Logger_enableSyslogIdent(logh->token,(int)(intptr_t)logh->magic);
            doneone = 1;
        }
        if (logh->type == LOGGER_LOG_TO_FILE && !doneone) {
            Logger_disableFilelogEntry(logh);
            /** hmm, don't zero status isn't saved.. i think it's
             * safer not to overwrite, in case a hup is just to
             * re-read config files...
             */
            Logger_enableFilelog(logh, 1);
        }
    }
}


void Logger_enableSyslog(void)
{
    Logger_enableSyslogIdent(Logger_logSyslogname(NULL), LOG_DAEMON);
}

void Logger_enableSyslogIdent(const char *ident, const int facility)
{
    Logger_LogHandler *logh;
    int                  found = 0;
    int                  enable = 1;
    void                *eventlog_h = NULL;

    Logger_disableSyslog();	/* ??? */
    openlog(Logger_logSyslogname(ident), LOG_CONS | LOG_PID, facility);


    for (logh = logger_logHandlerHead; logh; logh = logh->next)
        if (logh->type == LOGGER_LOG_TO_SYSLOG) {
            logh->magic   = (void*)eventlog_h;
            logh->imagic  = enable;	/* syslog open */
            if (logh->enabled && (0 == enable))
                Logger_disableThisLoghandler(logh);
            else if ((0 == logh->enabled) && enable)
                Logger_enableThisLoghandler(logh);
            found = 1;
    }

    if (!found) {
        logh = Logger_registerLoghandler(LOGGER_LOG_TO_SYSLOG, LOG_DEBUG );
        if (logh) {
            logh->magic    = (void*)eventlog_h;
            logh->token    = strdup(ident);
            logh->imagic   = enable;	/* syslog open */
            if (logh->enabled && (0 == enable))
                Logger_disableThisLoghandler(logh);
            else if ((0 == logh->enabled) && enable)
                Logger_enableThisLoghandler(logh);
        }
    }
}

void Logger_enableFilelog(Logger_LogHandler *logh, int dont_zero_log)
{
    FILE *logfile;

    if (!logh)
        return;

    if (!logh->magic) {
        logfile = fopen(logh->token, dont_zero_log ? "a" : "w");
        if (!logfile) {
            Logger_logPerror(logh->token);
            return;
        }
        logh->magic = (void*)logfile;
        Logger_setLineBuffering(logfile);
    }
    Logger_enableThisLoghandler(logh);
}

void Logger_enableFilelog2(const char *logfilename, int dont_zero_log)
{
    Logger_LogHandler *logh;

    /*
     * don't disable ALL filelogs whenever a new one is enabled.
     * this prevents '-Lf file' from working in priotd, as the
     * call to set up /var/log/priotd.log will disable the previous
     * log setup.
     * Logger_disableFilelog();
     */

    if (logfilename) {
        logh = Logger_findLoghandler( logfilename );
        if (!logh) {
            logh = Logger_registerLoghandler(LOGGER_LOG_TO_FILE, LOG_DEBUG );
            if (logh)
                logh->token = strdup(logfilename);
    }
        if (logh)
            Logger_enableFilelog(logh, dont_zero_log);
    } else {
        for (logh = logger_logHandlerHead; logh; logh = logh->next)
            if (logh->type == LOGGER_LOG_TO_FILE)
                Logger_enableFilelog(logh, dont_zero_log);
    }
}


/* used in the perl modules and ip-mib/ipv4InterfaceTable/ipv4InterfaceTable_subagent.c */
void Logger_enableStderrlog(void)
{
    Logger_LogHandler *logh;
    int                  found = 0;

    for (logh = logger_logHandlerHead; logh; logh = logh->next)
        if (logh->type == LOGGER_LOG_TO_STDOUT ||
            logh->type == LOGGER_LOG_TO_STDERR) {
            Logger_enableThisLoghandler(logh);
            found         = 1;
        }

    if (!found) {
        logh = Logger_registerLoghandler(LOGGER_LOG_TO_STDERR,
                                           LOG_DEBUG );
        if (logh)
            logh->token    = strdup("stderr");
    }
}

void Logger_enableCalllog(void)	/* XXX - or take a callback routine ??? */
{
    Logger_LogHandler *logh;
    int                  found = 0;

    for (logh = logger_logHandlerHead; logh; logh = logh->next)
        if (logh->type == LOGGER_LOG_TO_CALLBACK) {
            Logger_enableThisLoghandler(logh);
            found  = 1;
    }

    if (!found) {
        logh = Logger_registerLoghandler(LOGGER_LOG_TO_CALLBACK, LOG_DEBUG );
        if (logh)
            logh->token    = strdup("callback");
    }
}

Logger_LogHandler * Logger_findLoghandler( const char *token )
{
    Logger_LogHandler *logh;
    if (!token)
        return NULL;

    for (logh = logger_logHandlerHead; logh; logh = logh->next)
        if (logh->token && !strcmp( token, logh->token ))
            break;

    return logh;
}


int Logger_addLoghandler( Logger_LogHandler *logh )
{
    int i;
    Logger_LogHandler *logh2;

    if (!logh)
        return 0;

    /*
     * Find the appropriate point for the new entry...
     *   (logh2 will point to the entry immediately following)
     */
    for (logh2 = logger_logHandlerHead; logh2; logh2 = logh2->next)
        if ( logh2->priority >= logh->priority )
            break;

    /*
     * ... and link it into the main list.
     */
    if (logh2) {
        if (logh2->prev)
            logh2->prev->next = logh;
        else
            logger_logHandlerHead = logh;
        logh->next  = logh2;
        logh2->prev = logh;
    } else if (logger_logHandlerHead ) {
        /*
         * If logh2 is NULL, we're tagging on to the end
         */
        for (logh2 = logger_logHandlerHead; logh2->next; logh2 = logh2->next)
            ;
        logh2->next = logh;
    } else {
        logger_logHandlerHead = logh;
    }

    /*
     * Also tweak the relevant priority-'index' array.
     */
    for (i=LOGGER_PRIORITY_EMERG; i<=logh->priority; i++)
        if (!logger_logHandlerPriorities[i] ||
             logger_logHandlerPriorities[i]->priority >= logh->priority)
             logger_logHandlerPriorities[i] = logh;

    return 1;
}

Logger_LogHandler * Logger_registerLoghandler( int type, int priority )
{
    Logger_LogHandler *logh;

    logh = TOOLS_MALLOC_TYPEDEF(Logger_LogHandler);
    if (!logh)
        return NULL;

    DEBUG_MSGT(("logging:register", "registering log type %d with pri %d\n",
               type, priority));

    logh->type     = type;
    switch ( type ) {
    case LOGGER_LOG_TO_STDOUT:
        logh->imagic  = 1;
        /* fallthrough */
    case LOGGER_LOG_TO_STDERR:
        logh->handler = Logger_logHandlerStdouterr;
        break;
    case LOGGER_LOG_TO_FILE:
        logh->handler = Logger_logHandlerFile;
        logh->imagic  = 1;
        break;
    case LOGGER_LOG_TO_SYSLOG:
        logh->handler = Logger_logHandlerSyslog;
        break;
    case LOGGER_LOG_TO_CALLBACK:
        logh->handler = Logger_logHandlerCallback;
        break;
    case LOGGER_LOG_TO_NONE:
        logh->handler = Logger_logHandlerNull;
        break;
    default:
        free(logh);
        return NULL;
    }
    logh->priority = priority;
    Logger_enableThisLoghandler(logh);
    Logger_addLoghandler( logh );
    return logh;
}

int Logger_enableLoghandler( const char *token )
{
    Logger_LogHandler *logh;

    logh = Logger_findLoghandler( token );
    if (!logh)
        return 0;
    Logger_enableThisLoghandler(logh);
    return 1;
}

int Logger_disableLoghandler( const char *token )
{
    Logger_LogHandler *logh;

    logh = Logger_findLoghandler( token );
    if (!logh)
        return 0;
    Logger_disableThisLoghandler(logh);
    return 1;
}

int Logger_removeLoghandler( Logger_LogHandler *logh )
{
    int i;
    if (!logh)
        return 0;

    if (logh->prev)
        logh->prev->next = logh->next;
    else
        logger_logHandlerHead = logh->next;

    if (logh->next)
        logh->next->prev = logh->prev;

    for (i=LOG_EMERG; i<=logh->priority; i++)
        logger_logHandlerPriorities[i] = NULL;
    free((char *)logh->token);
    TOOLS_FREE(logh);

    return 1;
}

int Logger_logHandlerStdouterr( Logger_LogHandler * logh, int pri, const char *str)
{
    static int      newline = 1;	 /* MTCRITICAL_RESOURCE */
    const char     *newline_ptr;
    char            sbuf[40];

    if ( DefaultStore_getBoolean(DsStorage_LIBRARY_ID,
                               DsBool_LOG_TIMESTAMP) && newline) {
        _Logger_sprintfStamp(NULL, sbuf);
    } else {
        strcpy(sbuf, "");
    }
    /*
     * Remember whether or not the current line ends with a newline for the
     * next call of log_handler_stdouterr().
     */
    newline_ptr = strrchr(str, '\n');
    newline = newline_ptr && newline_ptr[1] == 0;

    if (logh->imagic)
       printf(         "%s%s", sbuf, str);
    else
       fprintf(stderr, "%s%s", sbuf, str);

    return 1;
}

int Logger_logHandlerSyslog(Logger_LogHandler * logh, int pri, const char *str)
{
    /*
     * XXX
     * We've got three items of information to work with:
     *     Is the syslog currently open?
     *     What ident string to use?
     *     What facility to log to?
     *
     * We've got two "magic" locations (imagic & magic) plus the token
     */
    if (!(logh->imagic)) {
        const char *ident    = logh->token;
        int   facility = (int)(intptr_t)logh->magic;
        if (!ident)
            ident = DefaultStore_getString(DsStorage_LIBRARY_ID, DsStr_APPTYPE);

        openlog(ident, LOG_CONS | LOG_PID, facility);
        logh->imagic = 1;
    }
    syslog( pri, "%s", str );
    return 1;
}

int Logger_logHandlerFile(Logger_LogHandler * logh, int pri, const char *str)
{
    FILE           *fhandle;
    char            sbuf[40];

    /*
     * We use imagic to save information about whether the next output
     * will start a new line, and thus might need a timestamp
     */
    if (DefaultStore_getBoolean(DsStorage_LIBRARY_ID, DsBool_LOG_TIMESTAMP) && logh->imagic) {
        _Logger_sprintfStamp(NULL, sbuf);
    } else {
        strcpy(sbuf, "");
    }

    /*
     * If we haven't already opened the file, then do so.
     * Save the filehandle pointer for next time.
     *
     * Note that this should still work, even if the file
     * is closed in the meantime (e.g. a regular "cleanup" sweep)
     */
    fhandle = (FILE*)logh->magic;
    if (!logh->magic) {
        fhandle = fopen(logh->token, "a+");
        if (!fhandle)
            return 0;
        logh->magic = (void*)fhandle;
    }
    fprintf(fhandle, "%s%s", sbuf, str);
    fflush(fhandle);
    logh->imagic = str[strlen(str) - 1] == '\n';
    return 1;
}

int Logger_logHandlerCallback(Logger_LogHandler * logh, int pri, const char *str)
{
    /*
     * XXX - perhaps replace 'priot_call_callbacks' processing
     *       with individual callback log_handlers ??
     */
    struct Logger_LogMessage slm;
    int  dodebug = Debug_getDoDebugging();

    slm.priority = pri;
    slm.msg = str;
    if (dodebug)            /* turn off debugging inside the callbacks else will loop */
        Debug_setDoDebugging(0);
    Callback_callCallbacks(CALLBACK_LIBRARY, CALLBACK_LOGGING, &slm);
    if (dodebug)
        Debug_setDoDebugging(dodebug);
    return 1;
}


int Logger_logHandlerNull( Logger_LogHandler * logh, int pri, const char *str)
{
    /*
     * Dummy log handler - just throw away the error completely
     * You probably don't really want to do this!
     */
    return 1;
}


void Logger_logString(int priority, const char *str)
{
    static int stderr_enabled = 0;
    static Logger_LogHandler lh = { 1, 0, 0, 0, "stderr", Logger_logHandlerStdouterr, 0, NULL,  NULL, NULL };

    Logger_LogHandler *logh;

    /*
     * We've got to be able to log messages *somewhere*!
     * If you don't want stderr logging, then enable something else.
     */
    if (0 == _logger_logHandlerEnabled) {
        if (!stderr_enabled) {
            ++stderr_enabled;
            Logger_setLineBuffering(stderr);
        }
        Logger_logHandlerStdouterr( &lh, priority, str );

        return;
    }
    else if (stderr_enabled) {
        stderr_enabled = 0;
        Logger_logHandlerStdouterr( &lh, LOGGER_PRIORITY_INFO, "Log handling defined - disabling stderr\n" );
    }


    /*
     * Start at the given priority, and work "upwards"....
     */
    logh = logger_logHandlerPriorities[priority];
    for ( ; logh; logh = logh->next ) {
        /*
         * ... but skipping any handlers with a "maximum priority"
         *     that we have already exceeded. And don't forget to
         *     ensure this logging is turned on (see Logger_disableStderrlog
         *     and its cohorts).
         */
        if (logh->enabled && (priority >= logh->pri_max))
            logh->handler( logh, priority, str );
    }
}


/**
 * This priot logging function allows variable argument list given the
 * specified priority, format and a populated va_list structure.
 * The default logfile this function writes to is /var/log/priotd.log.
 *
 * @param priority is an integer representing the type of message to be written
 *	to the priot log file.  The types are errors, warning, and information.
 *      - The error types are:
 *        - LOGGER_PRIORITY_EMERG       system is unusable
 *        - LOGGER_PRIORITY_ALERT       action must be taken immediately
 *        - LOGGER_PRIORITY_CRIT        critical conditions
 *        - LOGGER_PRIORITY_ERR         error conditions
 *      - The warning type is:
 *        - LOGGER_PRIORITY_WARNING     warning conditions
 *      - The information types are:
 *        - LOGGER_PRIORITY_NOTICE      normal but significant condition
 *        - LOGGER_PRIORITY_INFO        informational
 *        - LOGGER_PRIORITY_DEBUG       debug-level messages
 *
 * @param format is a pointer to a char representing the variable argument list
 *	format used.
 *
 * @param ap is a va_list type used to traverse the list of arguments.
 *
 * @return Returns 0 on success, -1 when the code could not format the log-
 *         string, -2 when dynamic memory could not be allocated if the length
 *         of the log buffer is greater then 1024 bytes.  For each of these
 *         errors a LOG_ERR messgae is written to the logfile.
 *
 * @see priot_log
 */
int Logger_vlog(int priority, const char *format, va_list ap)
{
    char            buffer[LOGLENGTH];
    int             length;
    char           *dynamic;
    va_list         aq;

    va_copy(aq, ap);
    length = vsnprintf(buffer, LOGLENGTH, format, ap);
    va_end(ap);

    if (length == 0) {
        return (0);             /* Empty string */
    }

    if (length == -1) {
        Logger_logString(LOGGER_PRIORITY_ERR, "Could not format log-string\n");
        return (-1);
    }

    if (length < LOGLENGTH) {
        Logger_logString(priority, buffer);
        return (0);
    }

    dynamic = (char *) malloc(length + 1);
    if (dynamic == NULL) {
        Logger_logString(LOGGER_PRIORITY_ERR, "Could not allocate memory for log-message\n");

        Logger_logString(priority, buffer);
        return (-2);
    }

    vsnprintf(dynamic, length + 1, format, aq);

    Logger_logString(priority, dynamic);

    free(dynamic);
    va_end(aq);
    return 0;
}


/**
 * This priot logging function allows variable argument list given the
 * specified format and priority.  Calls the Logger_vlog function.
 * The default logfile this function writes to is /var/log/priotd.log.
 *
 * @see _vlog
 */
int Logger_log(int priority, const char *format, ...)
{
    va_list         ap;
    int             ret;
    va_start(ap, format);
    ret = Logger_vlog(priority, format, ap);
    va_end(ap);
    return (ret);
}


/*
 * log a critical error.
 */
void Logger_logPerror(const char *s)
{
    char           *error = strerror(errno);
    if (s) {
        if (error)
            Logger_log(LOGGER_PRIORITY_ERR, "%s: %s\n", s, error);
        else
            Logger_log(LOGGER_PRIORITY_ERR, "%s: Error %d out-of-range\n", s, errno);
    } else {
        if (error)
            Logger_log(LOGGER_PRIORITY_ERR, "%s\n", error);
        else
            Logger_log(LOGGER_PRIORITY_ERR, "Error %d out-of-range\n", errno);
    }
}

/* external access to logger_logHandlerHead variable */
Logger_LogHandler  * Logger_getLogHandlerHead(void)
{
    return logger_logHandlerHead;
}
