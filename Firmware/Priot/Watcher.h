#ifndef WATCHER_H
#define WATCHER_H

#include "AgentHandler.h"

/** @ingroup watcher
 *  @{
 */

/*
 * if handler flag has this bit set, the timestamp will be
 * treated as a pointer to the timestamp. If this bit is
 * not set (the default), the timestamp is a struct timeval
 * that must be compared to the agent starttime.
 */
#define NETSNMP_WATCHER_DIRECT MIB_HANDLER_CUSTOM1

/** The size of the watched object is constant.
 *  @hideinitializer
 */
#define WATCHER_FIXED_SIZE     0x01
/** The maximum size of the watched object is stored in max_size.
 *  If WATCHER_SIZE_STRLEN is set then it is supposed that max_size + 1
 *  bytes could be stored in the buffer.
 *  @hideinitializer
 */
#define WATCHER_MAX_SIZE       0x02
/** If set then the variable data_size_p points to is supposed to hold the
 *  current size of the watched object and will be updated on writes.
 *  @hideinitializer
 *  @since Net-SNMP 5.5
 */
#define WATCHER_SIZE_IS_PTR    0x04
/** If set then data is suppposed to be a zero-terminated character array
 *  and both data_size and data_size_p are ignored. Additionally \\0 is a
 *  forbidden character in the data set.
 *  @hideinitializer
 *  @since Net-SNMP 5.5
 */
#define WATCHER_SIZE_STRLEN    0x08
/** If set then size is in units of object identifiers.
 *  This is useful if you have an OID and tracks the OID_LENGTH of it as
 *  opposed to it's size.
 *  @hideinitializer
 *  @since Net-SNMP 5.5.1
 */
#define WATCHER_SIZE_UNIT_OIDS 0x10

typedef struct WatcherInfo_s {
    void     *data;
    size_t    data_size;
    size_t    max_size;
    u_char    type;
    int       flags;
    size_t   *data_size_p;
} WatcherInfo;

/** @} */

int Watcher_registerWatchedInstance( HandlerRegistration *reginfo,
                                       WatcherInfo         *winfo);

int Watcher_registerWatchedInstance2(HandlerRegistration *reginfo,
                                       WatcherInfo         *winfo);

int Watcher_registerWatchedScalar(   HandlerRegistration *reginfo,
                                       WatcherInfo         *winfo);

int Watcher_registerWatchedScalar2(  HandlerRegistration *reginfo,
                                       WatcherInfo         *winfo);

int Watcher_registerWatchedTimestamp(HandlerRegistration *reginfo,
                                       markerT timestamp);

int Watcher_timestampRegister(MibHandler *whandler,
                                       HandlerRegistration *reginfo,
                                       markerT timestamp);

int Watcher_registerWatchedSpinlock(HandlerRegistration *reginfo,
                                      int *spinlock);

/*
 * Convenience registration calls
 */

int Watcher_registerUlongScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              NodeHandlerFT * subhandler);

int Watcher_registerReadOnlyUlongScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              NodeHandlerFT * subhandler);

int Watcher_registerLongScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              long * it,
                              NodeHandlerFT * subhandler);

int Watcher_registerReadOnlyLongScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              long * it,
                              NodeHandlerFT * subhandler);

int Watcher_registerIntScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              int * it,
                              NodeHandlerFT * subhandler);

int Watcher_registerReadOnlyIntScalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              int * it,
                              NodeHandlerFT * subhandler);

int Watcher_registerReadOnlyCounter32Scalar(const char *name,
                              const oid * reg_oid, size_t reg_oid_len,
                              u_long * it,
                              NodeHandlerFT * subhandler);

#define WATCHER_HANDLER_NAME "watcher"

MibHandler*
Watcher_getWatcherHandler(void);

WatcherInfo *
Watcher_initWatcherInfo(WatcherInfo *, void *, size_t, u_char, int);

WatcherInfo *
Watcher_initWatcherInfo6(WatcherInfo *,
               void *, size_t, u_char, int, size_t, size_t*);

WatcherInfo *
Watcher_createWatcherInfo(void *, size_t, u_char, int);

WatcherInfo *
Watcher_createWatcherInfo6(void *, size_t, u_char, int, size_t, size_t*);

WatcherInfo *
Watcher_cloneWatcherInfo(WatcherInfo *winfo);

void
Watcher_ownsWatcherInfo(MibHandler *handler);

NodeHandlerFT
Watcher_helperHandler;

MibHandler*
Watcher_getWatchedTimestampHandler(void);

NodeHandlerFT
Watcher_timestampHandler;

MibHandler*
Watcher_getWatchedSpinlockHandler(void);

NodeHandlerFT
Watcher_watchedSpinlockHandler;


#endif // WATCHER_H
