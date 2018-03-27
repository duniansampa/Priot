#include "Restart.h"
#include "DefaultStore.h"
#include "Logger.h"
#include "Impl.h"
#include "Api.h"

#include <unistd.h>



char **restart_argvrestartp;
char *restart_argvrestartname;
char *restart_argvrestart;


void Restart_doit(int a)
{
    char * name = DefaultStore_getString(DsStorage_LIBRARY_ID, DsStr_APPTYPE);
    Api_shutdown(name);

    /*  This signal handler may run with SIGALARM blocked.
     *  Since the signal mask is preserved accross execv(), we must
     *  make sure that SIGALARM is unblocked prior of execv'ing.
     *  Otherwise SIGALARM will be ignored in the next incarnation
     *  of snmpd, because the signal is blocked. And thus, the
     *  restart doesn't work anymore.
     *
     *  A quote from the sigprocmask() man page:
     *  The use of sigprocmask() is unspecified in a multithreaded process; see
     *  pthread_sigmask(3).
     */
    {
        sigset_t empty_set;

        sigemptyset(&empty_set);
        sigprocmask(SIG_SETMASK, &empty_set, NULL);
    }


    /*
     * do the exec
     */
    execv(restart_argvrestartname, restart_argvrestartp);
    Logger_logPerror(restart_argvrestartname);
}


int Restart_hook(int action,
             u_char * var_val,
             u_char var_val_type,
             size_t var_val_len,
             u_char * statP, oid * name, size_t name_len)
{

    long            tmp = 0;

    if (var_val_type != ASN01_INTEGER) {
        Logger_log(LOGGER_PRIORITY_NOTICE, "Wrong type != int\n");
        return PRIOT_ERR_WRONGTYPE;
    }
    tmp = *((long *) var_val);
    if (tmp == 1 && action == IMPL_COMMIT) {
        signal(SIGALRM, Restart_doit);
        alarm(NETSNMP_RESTARTSLEEP);
    }
    return PRIOT_ERR_NOERROR;
}
