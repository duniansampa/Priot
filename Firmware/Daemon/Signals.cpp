#include "Signals.h"

Signals::Signals()
{

}
int  Signals::RECONFIG = 0;

void Signals::registerOne(int sig, void (* handle)(int) ){

    signal(sig, handle);
}

void Signals::registerAll(){

    DEBUGMSGTL(("signal", "registering SIGTERM signal handler\n"));
    this->registerOne(SIGTERM, Signals::shutDown);
    DEBUGMSGTL(("signal", "registering SIGINT signal handler\n"));
    this->registerOne(SIGINT, Signals::shutDown);
    this->registerOne(SIGHUP, SIG_IGN);   // do not terminate on early SIGHUP //
    DEBUGMSGTL(("signal", "registering SIGUSR1 signal handler\n"));
    this->registerOne(SIGUSR1, Signals::dump);
    DEBUGMSGTL(("signal", "registering SIGPIPE signal handler\n"));
    this->registerOne(SIGPIPE, SIG_IGN);   // 'Inline' failure of wayward readers //
    this->registerOne(SIGXFSZ, Signals::catchRandomSignal);
}

void Signals::shutDown(int a)
{
    //netsnmp_running = 0;
}

void Signals::reconfig(int a)
{
    Signals::RECONFIG = 1;
    signal(SIGHUP, Signals::reconfig);
}


extern  void     dump_registry(void);

void Signals::dump(int a)
{
    //dump_registry();
    signal(SIGUSR1, Signals::dump);
}

void Signals::catchRandomSignal(int a)
{
    /* Disable all logs and log the error via syslog */
//    snmp_disable_log();

//    snmp_enable_syslog();

//    snmp_log(LOG_ERR, "Exiting on signal %d\n", a);

//    snmp_disable_syslog();

    exit(1);
}
