#include "SysORTable.h"
#include "Agent.h"
#include "System/Util/Trace.h"
#include "System/Util/Callback.h"
#include "AgentCallbacks.h"

typedef struct DataNode_s {
    struct SysORTable_s  data;
    struct DataNode_s*   next;
    struct DataNode_s*   prev;
}* DataNode;

static DataNode _sysORTable_table = NULL;

static void
_SysORTable_erase( DataNode entry )
{
    entry->data.OR_uptime = Agent_getAgentUptime();
    DEBUG_MSGTL(("agent/sysORTable", "UNREG_SYSOR %p\n", &entry->data));
    Callback_call(CallbackMajor_APPLICATION, PriotdCallback_UNREG_SYSOR, &entry->data);
    free(entry->data.OR_oid);
    free(entry->data.OR_descr);
    if (entry->next == entry)
        _sysORTable_table = NULL;
    else {
        entry->next->prev = entry->prev;
        entry->prev->next = entry->next;
        if (entry == _sysORTable_table)
            _sysORTable_table = entry->next;
    }
    free(entry);
}

void
SysORTable_foreach( void (*f)( const struct SysORTable_s*, void* ), void* c )
{
    DEBUG_MSGTL(("agent/sysORTable", "foreach(%p, %p)\n", f, c));
    if(_sysORTable_table) {
        DataNode run = _sysORTable_table;
        do {
            DataNode tmp = run;
            run = run->next;
            f(&tmp->data, c);
        } while(_sysORTable_table && run != _sysORTable_table);
    }
}

int
SysORTable_registerSysORTableSess( oid*          oidin,
                                   size_t         oidlen,
                                   const char*    descr,
                                   Types_Session* ss )
{
    DataNode entry;

    DEBUG_MSGTL(("agent/sysORTable", "registering: "));
    DEBUG_MSGOID(("agent/sysORTable", oidin, oidlen));
    DEBUG_MSG(("agent/sysORTable", ", session %p\n", ss));

    entry = (DataNode)calloc(1, sizeof(struct DataNode_s));
    if (entry == NULL) {
        DEBUG_MSGTL(("agent/sysORTable", "Failed to allocate new entry\n"));
        return SYS_ORTABLE_REGISTRATION_FAILED;
    }

    entry->data.OR_descr = strdup(descr);
    if (entry->data.OR_descr == NULL) {
        DEBUG_MSGTL(("agent/sysORTable", "Failed to allocate new sysORDescr\n"));
        free(entry);
        return SYS_ORTABLE_REGISTRATION_FAILED;
    }

    entry->data.OR_oid = (oid *) malloc(sizeof(oid) * oidlen);
    if (entry->data.OR_oid == NULL) {
        DEBUG_MSGTL(("agent/sysORTable", "Failed to allocate new sysORID\n"));
        free(entry->data.OR_descr);
        free(entry);
        return SYS_ORTABLE_REGISTRATION_FAILED;
    }

    memcpy(entry->data.OR_oid, oidin, sizeof(oid) * oidlen);
    entry->data.OR_oidlen = oidlen;
    entry->data.OR_sess = ss;

    if(_sysORTable_table) {
        entry->next = _sysORTable_table;
        entry->prev = _sysORTable_table->prev;
        _sysORTable_table->prev->next = entry;
        _sysORTable_table->prev = entry;
    } else
        _sysORTable_table = entry->next = entry->prev = entry;

    entry->data.OR_uptime = Agent_getAgentUptime();

    Callback_call(CallbackMajor_APPLICATION,
                        PriotdCallback_REG_SYSOR, &entry->data);

    return SYS_ORTABLE_REGISTERED_OK;
}

int
SysORTable_registerSysORTable( oid*       oidin,
                               size_t      oidlen,
                               const char* descr )
{
    return SysORTable_registerSysORTableSess(oidin, oidlen, descr, NULL);
}

int
SysORTable_unregisterSysORTableSess( oid*          oidin,
                                     size_t         oidlen,
                                     Types_Session* ss )
{
    int any_unregistered = 0;

    DEBUG_MSGTL(("agent/sysORTable", "sysORTable unregistering: "));
    DEBUG_MSGOID(("agent/sysORTable", oidin, oidlen));
    DEBUG_MSG(("agent/sysORTable", ", session %p\n", ss));

    if(_sysORTable_table) {
        DataNode run = _sysORTable_table;
        do {
            DataNode tmp = run;
            run = run->next;
            if (tmp->data.OR_sess == ss &&
                Api_oidCompare(oidin, oidlen,
                                 tmp->data.OR_oid, tmp->data.OR_oidlen) == 0) {
                _SysORTable_erase(tmp);
                any_unregistered = 1;
            }
        } while(_sysORTable_table && run != _sysORTable_table);
    }

    if (any_unregistered) {
        DEBUG_MSGTL(("agent/sysORTable", "unregistering successfull\n"));
        return SYS_ORTABLE_UNREGISTERED_OK;
    } else {
        DEBUG_MSGTL(("agent/sysORTable", "unregistering failed\n"));
        return SYS_ORTABLE_NO_SUCH_REGISTRATION;
    }
}


int
SysORTable_unregisterSysORTable( oid*  oidin,
                                 size_t oidlen )
{
    return SysORTable_unregisterSysORTableSess(oidin, oidlen, NULL);
}


void
SysORTable_unregisterSysORTableBySession( Types_Session * ss )
{
    DEBUG_MSGTL(("agent/sysORTable",
                "sysORTable unregistering session %p\n", ss));

   if(_sysORTable_table) {
        DataNode run = _sysORTable_table;
        do {
            DataNode tmp = run;
            run = run->next;
            if (((ss->flags & API_FLAGS_SUBSESSION) &&
                 tmp->data.OR_sess == ss) ||
                (!(ss->flags & API_FLAGS_SUBSESSION) && tmp->data.OR_sess &&
                 tmp->data.OR_sess->subsession == ss))
                _SysORTable_erase(tmp);
        } while(_sysORTable_table && run != _sysORTable_table);
    }

    DEBUG_MSGTL(("agent/sysORTable",
                "sysORTable unregistering session %p done\n", ss));
}

static int
_SysORTable_registerSysORCallback( int   majorID,
                                  int   minorID,
                                  void* serverarg,
                                  void* clientarg )
{
    struct SysORTable_s *parms = (struct SysORTable_s*) serverarg;

    return SysORTable_registerSysORTableSess(parms->OR_oid, parms->OR_oidlen,
                                    parms->OR_descr, parms->OR_sess);
}

static int
_SysORTable_unregisterSysORBySessionCallback( int   majorID,
                                             int   minorID,
                                             void* serverarg,
                                             void* clientarg )
{
    Types_Session *session = (Types_Session *) serverarg;

    SysORTable_unregisterSysORTableBySession(session);

    return 0;
}

static int
_SysORTable_unregisterSysORCallback( int   majorID,
                                    int   minorID,
                                    void* serverarg,
                                    void* clientarg )
{
    struct SysORTable_s *parms = (struct SysORTable_s *) serverarg;

    return SysORTable_unregisterSysORTableSess(parms->OR_oid,
                                      parms->OR_oidlen,
                                      parms->OR_sess);
}

void
SysORTable_initAgentSysORTable( void )
{
    DEBUG_MSGTL(("agent/sysORTable", "SysORTable_initAgentSysORTable\n"));

    Callback_register(CallbackMajor_APPLICATION,
                              PriotdCallback_REQ_REG_SYSOR,
                              _SysORTable_registerSysORCallback, NULL);
    Callback_register(CallbackMajor_APPLICATION,
                              PriotdCallback_REQ_UNREG_SYSOR,
                               _SysORTable_unregisterSysORCallback, NULL);
    Callback_register(CallbackMajor_APPLICATION,
                           PriotdCallback_REQ_UNREG_SYSOR_SESS,
                           _SysORTable_unregisterSysORBySessionCallback, NULL);
}

void
SysORTable_shutdownAgentSysORTable( void )
{
    DEBUG_MSGTL(("agent/sysORTable", "shutdown_sysORTable\n"));
    while(_sysORTable_table)
        _SysORTable_erase(_sysORTable_table);
}

