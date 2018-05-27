#include "FdEventManager.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"


int     fdEventManager_externalReadfd[NUM_EXTERNAL_FDS],   fdEventManager_externalReadfdlen   = 0;
int     fdEventManager_externalWritefd[NUM_EXTERNAL_FDS],  fdEventManager_externalWritefdlen  = 0;
int     fdEventManager_externalExceptfd[NUM_EXTERNAL_FDS], fdEventManager_externalExceptfdlen = 0;
void  (*fdEventManager_externalReadfdFT[NUM_EXTERNAL_FDS]) (int, void *);
void  (*fdEventManager_externalWritefdFT[NUM_EXTERNAL_FDS]) (int, void *);
void  (*fdEventManager_externalExceptfdFT[NUM_EXTERNAL_FDS]) (int, void *);
void   *fdEventManager_externalReadfdData[NUM_EXTERNAL_FDS];
void   *fdEventManager_externalWritefdData[NUM_EXTERNAL_FDS];
void   *fdEventManager_externalExceptfdData[NUM_EXTERNAL_FDS];

static int _externalFdUnregistered;

/*
 * Register a given fd for read events.  Call callback when events
 * are received.
 */
int
FdEventManager_registerReadfd(int fd, void (*func) (int, void *), void *data)
{
    if (fdEventManager_externalReadfdlen < NUM_EXTERNAL_FDS) {
        fdEventManager_externalReadfd[fdEventManager_externalReadfdlen] = fd;
        fdEventManager_externalReadfdFT[fdEventManager_externalReadfdlen] = func;
        fdEventManager_externalReadfdData[fdEventManager_externalReadfdlen] = data;
        fdEventManager_externalReadfdlen++;
        DEBUG_MSGTL(("fd_event_manager:register_readfd", "registered fd %d\n", fd));
        return FD_REGISTERED_OK;
    } else {
        Logger_log(LOGGER_PRIORITY_CRIT, "register_readfd: too many file descriptors\n");
        return FD_REGISTRATION_FAILED;
    }
}

/*
 * Register a given fd for write events.  Call callback when events
 * are received.
 */
int
FdEventManager_registerWritefd(int fd, void (*func) (int, void *), void *data)
{
    if (fdEventManager_externalWritefdlen < NUM_EXTERNAL_FDS) {
        fdEventManager_externalWritefd[fdEventManager_externalWritefdlen] = fd;
        fdEventManager_externalWritefdFT[fdEventManager_externalWritefdlen] = func;
        fdEventManager_externalWritefdData[fdEventManager_externalWritefdlen] = data;
        fdEventManager_externalWritefdlen++;
        DEBUG_MSGTL(("fd_event_manager:register_writefd", "registered fd %d\n", fd));
        return FD_REGISTERED_OK;
    } else {
        Logger_log(LOGGER_PRIORITY_CRIT,
                 "register_writefd: too many file descriptors\n");
        return FD_REGISTRATION_FAILED;
    }
}

/*
 * Register a given fd for exception events.  Call callback when events
 * are received.
 */
int
FdEventManager_registerExceptfd(int fd, void (*func) (int, void *), void *data)
{
    if (fdEventManager_externalExceptfdlen < NUM_EXTERNAL_FDS) {
        fdEventManager_externalExceptfd[fdEventManager_externalExceptfdlen] = fd;
        fdEventManager_externalExceptfdFT[fdEventManager_externalExceptfdlen] = func;
        fdEventManager_externalExceptfdData[fdEventManager_externalExceptfdlen] = data;
        fdEventManager_externalExceptfdlen++;
        DEBUG_MSGTL(("fd_event_manager:register_exceptfd", "registered fd %d\n", fd));
        return FD_REGISTERED_OK;
    } else {
        Logger_log(LOGGER_PRIORITY_CRIT,
                 "register_exceptfd: too many file descriptors\n");
        return FD_REGISTRATION_FAILED;
    }
}

/*
 * Unregister a given fd for read events.
 */
int
FdEventManager_unregisterReadfd(int fd)
{
    int             i, j;

    for (i = 0; i < fdEventManager_externalReadfdlen; i++) {
        if (fdEventManager_externalReadfd[i] == fd) {
            fdEventManager_externalReadfdlen--;
            for (j = i; j < fdEventManager_externalReadfdlen; j++) {
                fdEventManager_externalReadfd[j] = fdEventManager_externalReadfd[j + 1];
                fdEventManager_externalReadfdFT[j] = fdEventManager_externalReadfdFT[j + 1];
                fdEventManager_externalReadfdData[j] = fdEventManager_externalReadfdData[j + 1];
            }
            DEBUG_MSGTL(("fd_event_manager:unregister_readfd", "unregistered fd %d\n", fd));
            _externalFdUnregistered = 1;
            return FD_UNREGISTERED_OK;
        }
    }
    return FD_NO_SUCH_REGISTRATION;
}

/*
 * Unregister a given fd for read events.
 */
int
FdEventManager_unregisterWritefd(int fd)
{
    int             i, j;

    for (i = 0; i < fdEventManager_externalWritefdlen; i++) {
        if (fdEventManager_externalWritefd[i] == fd) {
            fdEventManager_externalWritefdlen--;
            for (j = i; j < fdEventManager_externalWritefdlen; j++) {
                fdEventManager_externalWritefd[j] = fdEventManager_externalWritefd[j + 1];
                fdEventManager_externalWritefdFT[j] = fdEventManager_externalWritefdFT[j + 1];
                fdEventManager_externalWritefdData[j] = fdEventManager_externalWritefdData[j + 1];
            }
            DEBUG_MSGTL(("fd_event_manager:unregister_writefd", "unregistered fd %d\n", fd));
            _externalFdUnregistered = 1;
            return FD_UNREGISTERED_OK;
        }
    }
    return FD_NO_SUCH_REGISTRATION;
}

/*
 * Unregister a given fd for exception events.
 */
int
FdEventManager_unregisterExceptfd(int fd)
{
    int             i, j;

    for (i = 0; i < fdEventManager_externalExceptfdlen; i++) {
        if (fdEventManager_externalExceptfd[i] == fd) {
            fdEventManager_externalExceptfdlen--;
            for (j = i; j < fdEventManager_externalExceptfdlen; j++) {
                fdEventManager_externalExceptfd[j] = fdEventManager_externalExceptfd[j + 1];
                fdEventManager_externalExceptfdFT[j] = fdEventManager_externalExceptfdFT[j + 1];
                fdEventManager_externalExceptfdData[j] = fdEventManager_externalExceptfdData[j + 1];
            }
            DEBUG_MSGTL(("fd_event_manager:unregister_exceptfd", "unregistered fd %d\n",
                        fd));
            _externalFdUnregistered = 1;
            return FD_UNREGISTERED_OK;
        }
    }
    return FD_NO_SUCH_REGISTRATION;
}

/*
 * NET-SNMP External Event Info
 */
void FdEventManager_externalEventInfo(int *numfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds)
{
  Types_LargeFdSet lreadfds;
  Types_LargeFdSet lwritefds;
  Types_LargeFdSet lexceptfds;

  LargeFdSet_init(&lreadfds, FD_SETSIZE);
  LargeFdSet_init(&lwritefds, FD_SETSIZE);
  LargeFdSet_init(&lexceptfds, FD_SETSIZE);

  LargeFdSet_copyFdSetToLargeFdSet(&lreadfds, readfds);
  LargeFdSet_copyFdSetToLargeFdSet(&lwritefds, writefds);
  LargeFdSet_copyFdSetToLargeFdSet(&lexceptfds, exceptfds);

  FdEventManager_externalEventInfo2(numfds, &lreadfds, &lwritefds, &lexceptfds);

  if (LargeFdSet_copyLargeFdSetToFdSet(readfds, &lreadfds) < 0
      || LargeFdSet_copyLargeFdSetToFdSet(writefds, &lwritefds) < 0
      || LargeFdSet_copyLargeFdSetToFdSet(exceptfds, &lexceptfds) < 0)
  {
    Logger_log(LOGGER_PRIORITY_ERR,
         "Use netsnmp_external_event_info2() for processing"
         " large file descriptors\n");
  }

  LargeFdSet_cleanup(&lreadfds);
  LargeFdSet_cleanup(&lwritefds);
  LargeFdSet_cleanup(&lexceptfds);
}

void FdEventManager_externalEventInfo2(int *numfds,
                                  Types_LargeFdSet *readfds,
                                  Types_LargeFdSet *writefds,
                                  Types_LargeFdSet *exceptfds)
{
  int i;

  _externalFdUnregistered = 0;

  for (i = 0; i < fdEventManager_externalReadfdlen; i++) {
    LARGEFDSET_FD_SET(fdEventManager_externalReadfd[i], readfds);
    if (fdEventManager_externalReadfd[i] >= *numfds)
      *numfds = fdEventManager_externalReadfd[i] + 1;
  }
  for (i = 0; i < fdEventManager_externalWritefdlen; i++) {
    LARGEFDSET_FD_SET(fdEventManager_externalWritefd[i], writefds);
    if (fdEventManager_externalWritefd[i] >= *numfds)
      *numfds = fdEventManager_externalWritefd[i] + 1;
  }
  for (i = 0; i < fdEventManager_externalExceptfdlen; i++) {
    LARGEFDSET_FD_SET(fdEventManager_externalExceptfd[i], exceptfds);
    if (fdEventManager_externalExceptfd[i] >= *numfds)
      *numfds = fdEventManager_externalExceptfd[i] + 1;
  }
}

/*
 * NET-SNMP Dispatch External Events
 */
void FdEventManager_dispatchExternalEvents(int *count, fd_set *readfds, fd_set *writefds, fd_set *exceptfds)
{
  Types_LargeFdSet lreadfds;
  Types_LargeFdSet lwritefds;
  Types_LargeFdSet lexceptfds;

  LargeFdSet_init(&lreadfds, FD_SETSIZE);
  LargeFdSet_init(&lwritefds, FD_SETSIZE);
  LargeFdSet_init(&lexceptfds, FD_SETSIZE);

  LargeFdSet_copyFdSetToLargeFdSet(&lreadfds, readfds);
  LargeFdSet_copyFdSetToLargeFdSet(&lwritefds, writefds);
  LargeFdSet_copyFdSetToLargeFdSet(&lexceptfds, exceptfds);

  FdEventManager_dispatchExternalEvents2(count, &lreadfds, &lwritefds, &lexceptfds);

  if (LargeFdSet_copyLargeFdSetToFdSet(readfds, &lreadfds) < 0
      || LargeFdSet_copyLargeFdSetToFdSet(writefds,  &lwritefds) < 0
      || LargeFdSet_copyLargeFdSetToFdSet(exceptfds, &lexceptfds) < 0)
  {
    Logger_log(LOGGER_PRIORITY_ERR,
         "Use netsnmp_dispatch_external_events2() for processing"
         " large file descriptors\n");
  }

  LargeFdSet_cleanup(&lreadfds);
  LargeFdSet_cleanup(&lwritefds);
  LargeFdSet_cleanup(&lexceptfds);
}

void FdEventManager_dispatchExternalEvents2(int *count,
                                       Types_LargeFdSet *readfds,
                                       Types_LargeFdSet *writefds,
                                       Types_LargeFdSet *exceptfds)
{
  int i;
  for (i = 0;
       *count && (i < fdEventManager_externalReadfdlen) && !_externalFdUnregistered; i++) {
      if (LARGEFDSET_FD_ISSET(fdEventManager_externalReadfd[i], readfds)) {
          DEBUG_MSGTL(("fd_event_manager:netsnmp_dispatch_external_events",
                     "readfd[%d] = %d\n", i, fdEventManager_externalReadfd[i]));
          fdEventManager_externalReadfdFT[i] (fdEventManager_externalReadfd[i],
                                  fdEventManager_externalReadfdData[i]);
          LARGEFDSET_FD_CLR(fdEventManager_externalReadfd[i], readfds);
          (*count)--;
      }
  }
  for (i = 0;
       *count && (i < fdEventManager_externalWritefdlen) && !_externalFdUnregistered; i++) {
      if (LARGEFDSET_FD_ISSET(fdEventManager_externalWritefd[i], writefds)) {
          DEBUG_MSGTL(("fd_event_manager:netsnmp_dispatch_external_events",
                     "writefd[%d] = %d\n", i, fdEventManager_externalWritefd[i]));
          fdEventManager_externalWritefdFT[i] (fdEventManager_externalWritefd[i],
                                   fdEventManager_externalWritefdData[i]);
          LARGEFDSET_FD_CLR(fdEventManager_externalWritefd[i], writefds);
          (*count)--;
      }
  }
  for (i = 0;
       *count && (i < fdEventManager_externalExceptfdlen) && !_externalFdUnregistered; i++) {
      if (LARGEFDSET_FD_ISSET(fdEventManager_externalExceptfd[i], exceptfds)) {
          DEBUG_MSGTL(("fd_event_manager:netsnmp_dispatch_external_events",
                     "exceptfd[%d] = %d\n", i, fdEventManager_externalExceptfd[i]));
          fdEventManager_externalExceptfdFT[i] (fdEventManager_externalExceptfd[i],
                                    fdEventManager_externalExceptfdData[i]);
          LARGEFDSET_FD_CLR(fdEventManager_externalExceptfd[i], exceptfds);
          (*count)--;
      }
  }
}

