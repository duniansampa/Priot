#ifndef FDEVENTMANAGER_H
#define FDEVENTMANAGER_H

#include "LargeFdSet.h"

#define NUM_EXTERNAL_FDS 32
#define FD_REGISTERED_OK                 0
#define FD_REGISTRATION_FAILED          -2
#define FD_UNREGISTERED_OK               0
#define FD_NO_SUCH_REGISTRATION         -1

/* Since the inception of netsnmp_external_event_info and
 * netsnmp_dispatch_external_events, there is no longer a need for the data
 * below to be globally visible.  We will leave it global for now for
 * compatibility purposes. */
extern int      fdEventManager_externalReadfd[NUM_EXTERNAL_FDS],   fdEventManager_externalReadfdlen;
extern int      fdEventManager_externalWritefd[NUM_EXTERNAL_FDS],  fdEventManager_externalWritefdlen;
extern int      fdEventManager_externalExceptfd[NUM_EXTERNAL_FDS], fdEventManager_externalExceptfdlen;

extern void     (*fdEventManager_externalReadfdFT[NUM_EXTERNAL_FDS])   (int, void *);
extern void     (*fdEventManager_externalWritefdFT[NUM_EXTERNAL_FDS])  (int, void *);
extern void     (*fdEventManager_externalExceptfdFT[NUM_EXTERNAL_FDS]) (int, void *);

extern void    *fdEventManager_externalReadfdData[NUM_EXTERNAL_FDS];
extern void    *fdEventManager_externalWritefdData[NUM_EXTERNAL_FDS];
extern void    *fdEventManager_externalExceptfdData[NUM_EXTERNAL_FDS];

/* Here are the key functions of this unit.  Use register_xfd to register
 * a callback to be called when there is x activity on the register fd.
 * x can be read, write, or except (for exception).  When registering,
 * you can pass in a pointer to some data that you have allocated that
 * you would like to have back when the callback is called. */
int             FdEventManager_registerReadfd(int, void (*func)(int, void *),   void *);
int             FdEventManager_registerWritefd(int, void (*func)(int, void *),  void *);
int             FdEventManager_registerExceptfd(int, void (*func)(int, void *), void *);

/* Unregisters a given fd for events */
int             FdEventManager_unregisterReadfd(int);
int             FdEventManager_unregisterWritefd(int);
int             FdEventManager_unregisterExceptfd(int);

/*
 * External Event Info
 *
 * Description:
 *   Call this function to add an external event fds to your read, write,
 *   exception fds that your application already has.  When this function
 *   returns, your fd_sets will be ready for select().  It returns the
 *   biggest fd in the fd_sets so far.
 *
 * Input Parameters: None
 *
 * Output Parameters: None
 *
 * In/Out Parameters:
 *   numfds - The biggest fd so far.  On exit to this function, numfds
 *            could of changed since we pass out the new biggest fd.
 *   readfds - Set of read FDs that we are monitoring.  This function
 *             can modify this set to have more FDs that we are monitoring.
 *   writefds - Set of write FDs that we are monitoring.  This function
 *             can modify this set to have more FDs that we are monitoring.
 *   exceptfds - Set of exception FDs that we are monitoring.  This function
 *             can modify this set to have more FDs that we are monitoring.
 *
 * Return Value: None
 *
 * Side Effects: None
 */

void FdEventManager_externalEventInfo(int *numfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds);


void FdEventManager_externalEventInfo2(int *numfds,
                                  Types_LargeFdSet *readfds,
                                  Types_LargeFdSet *writefds,
                                  Types_LargeFdSet *exceptfds);

/*
 * Dispatch External Events
 *
 * Description:
 *   Call this function after select returns with pending events.  If any of
 *   them were NETSNMP external events, the registered callback will be called.
 *   The corresponding fd_set will have the FD cleared after the event is
 *   dispatched.
 *
 * Input Parameters: None
 *
 * Output Parameters: None
 *
 * In/Out Parameters:
 *   count - Number of FDs that have activity.  In this function, we decrement
 *           count as we dispatch an event.
 *   readfds - Set of read FDs that have activity
 *   writefds - Set of write FDs that have activity
 *   exceptfds - Set of exception FDs that have activity
 *
 * Return Value: None
 *
 * Side Effects: None
 */

void FdEventManager_dispatchExternalEvents(int *count, fd_set *readfds, fd_set *writefds, fd_set *exceptfds);

void FdEventManager_dispatchExternalEvents2(int *count,
                                       Types_LargeFdSet *readfds,
                                       Types_LargeFdSet *writefds,
                                       Types_LargeFdSet *exceptfds);

#endif // FDEVENTMANAGER_H
