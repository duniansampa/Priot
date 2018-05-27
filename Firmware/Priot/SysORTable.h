#ifndef AGENTSYSORTABLE_H
#define AGENTSYSORTABLE_H

#include "AgentCallbacks.h"
#include "System/Util/Callback.h"
#include "Generals.h"
#include "Types.h"

struct SysORTable_s {
    char* OR_descr;
    oid* OR_oid;
    size_t OR_oidlen;
    Types_Session* OR_sess;
    u_long OR_uptime;
};

struct RegisterSysORParameters_s {
    char* descr;
    oid* name;
    size_t namelen;
};

#define SYS_ORTABLE_REGISTERED_OK 0
#define SYS_ORTABLE_REGISTRATION_FAILED -1
#define SYS_ORTABLE_UNREGISTERED_OK 0
#define SYS_ORTABLE_NO_SUCH_REGISTRATION -1

#define REGISTER_SYSOR_TABLE( theoid, len, descr )                              \
    do {                                                                        \
        struct SysORTable_s t;                                                  \
        t.OR_descr = UTILITIES_REMOVE_CONST( char*, descr );                    \
        t.OR_oid = theoid;                                                      \
        t.OR_oidlen = len;                                                      \
        t.OR_sess = NULL;                                                       \
        t.OR_uptime = 0;                                                        \
        Callback_call( CallbackMajor_APPLICATION, PriotdCallback_REQ_REG_SYSOR, \
            &t );                                                               \
    } while ( 0 );

#define REGISTER_SYSOR_ENTRY( theoid, descr ) \
    REGISTER_SYSOR_TABLE( theoid, sizeof( theoid ) / sizeof( oid ), descr )

#define UNREGISTER_SYSOR_TABLE( theoid, len )     \
    do {                                          \
        struct SysORTable_s t;                    \
        t.OR_descr = NULL;                        \
        t.OR_oid = theoid;                        \
        t.OR_oidlen = len;                        \
        t.OR_sess = NULL;                         \
        t.OR_uptime = 0;                          \
        Callback_call( CallbackMajor_APPLICATION, \
            PriotdCallback_REQ_UNREG_SYSOR, &t ); \
    } while ( 0 );

#define UNREGISTER_SYSOR_ENTRY( theoid ) \
    UNREGISTER_SYSOR_TABLE( theoid, sizeof( theoid ) / sizeof( oid ) )

#define UNREGISTER_SYSOR_SESS( sess )                  \
    Callback_callCallbacks( CallbackMajor_APPLICATION, \
        PriotdCallback_REQ_UNREG_SYSOR_SESS, sess );

void SysORTable_initAgentSysORTable( void );

void SysORTable_shutdownAgentSysORTable( void );

void SysORTable_foreach( void ( *f )( const struct SysORTable_s*, void* ), void* c );

int SysORTable_registerSysORTable( oid* oidin, size_t oidlen, const char* descr );

int SysORTable_unregisterSysORTable( oid* oidin, size_t oidlen );

int SysORTable_registerSysORTableSess( oid* oidin,
    size_t oidlen,
    const char* descr,
    Types_Session* ss );

int SysORTable_unregisterSysORTableSess( oid* oidin,
    size_t oidlen,
    Types_Session* ss );

void SysORTable_unregisterSysORTableBySession( Types_Session* ss );

#endif // AGENTSYSORTABLE_H
