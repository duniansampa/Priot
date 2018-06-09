#ifndef SESSION_H
#define SESSION_H

#include "Generals.h"
#include "System/Dispatcher/LargeFdSet.h"
#include "Types.h"
/**
 *  Library API routines concerned with specifying and using SNMP "sessions"
 *    including sending and receiving requests.
 */

void Session_sessInit( Types_Session* );

/*
 * Types_Session *Session_open(session)
 *      Types_Session *session;
 *
 * Sets up the session with the snmp_session information provided
 * by the user.  Then opens and binds the necessary UDP port.
 * A handle to the created session is returned (this is different than
 * the pointer passed to Session_open()).  On any error, NULL is returned
 * and snmp_errno is set to the appropriate error code.
 */

Types_Session* Session_open( Types_Session* );

/*
 * int snmp_close(session)
 *     Types_Session *session;
 *
 * Close the input session.  Frees all data allocated for the session,
 * dequeues any pending requests, and closes any sockets allocated for
 * the session.  Returns 0 on error, 1 otherwise.
 *
 * Session_closeSessions() does the same thing for all open sessions
 */

int Session_close( Types_Session* );

int Session_closeSessions( void );

/*
 * int Session_send(session, pdu)
 *     Types_Session *session;
 *     Types_Pdu      *pdu;
 *
 * Sends the input pdu on the session after calling snmp_build to create
 * a serialized packet.  If necessary, set some of the pdu data from the
 * session defaults.  Add a request corresponding to this pdu to the list
 * of outstanding requests on this session, then send the pdu.
 * Returns the request id of the generated packet if applicable, otherwise 1.
 * On any error, 0 is returned.
 * The pdu is freed by Session_send() unless a failure occured.
 */

int Session_send( Types_Session*, Types_Pdu* );

/*
 * int Session_asyncSend(session, pdu, callback, cb_data)
 *     Types_Session *session;
 *     Types_Pdu      *pdu;
 *     Types_CallbackFT callback;
 *     void   *cb_data;
 *
 * Sends the input pdu on the session after calling snmp_build to create
 * a serialized packet.  If necessary, set some of the pdu data from the
 * session defaults.  Add a request corresponding to this pdu to the list
 * of outstanding requests on this session and store callback and data,
 * then send the pdu.
 * Returns the request id of the generated packet if applicable, otherwise 1.
 * On any error, 0 is returned.
 * The pdu is freed by Session_send() unless a failure occured.
 */

int Session_asyncSend( Types_Session*, Types_Pdu*,
    Types_CallbackFT, void* );

/*
 * void Session_read(fdset)
 *     fd_set  *fdset;
 *
 * Checks to see if any of the fd's set in the fdset belong to
 * snmp.  Each socket with it's fd set has a packet read from it
 * and snmp_parse is called on the packet received.  The resulting pdu
 * is passed to the callback routine for that session.  If the callback
 * routine returns successfully, the pdu and it's request are deleted.
 */

void Session_read( fd_set* );

/*
 * Session_read2() is similar to Session_read(), but accepts a pointer to a
 * large file descriptor set instead of a pointer to a regular file
 * descriptor set.
 */

void Session_read2( LargeFdSet_t* );

int Session_synchResponse( Types_Session*, Types_Pdu*,
    Types_Pdu** );

/*
 * int Session_selectInfo(numfds, fdset, timeout, block)
 * int *numfds;
 * fd_set   *fdset;
 * struct timeval *timeout;
 * int *block;
 *
 * Returns info about what snmp requires from a select statement.
 * numfds is the number of fds in the list that are significant.
 * All file descriptors opened for SNMP are OR'd into the fdset.
 * If activity occurs on any of these file descriptors, Session_read
 * should be called with that file descriptor set.
 *
 * The timeout is the latest time that SNMP can wait for a timeout.  The
 * select should be done with the minimum time between timeout and any other
 * timeouts necessary.  This should be checked upon each invocation of select.
 * If a timeout is received, Session_timeout should be called to check if the
 * timeout was for SNMP.  (Session_timeout is idempotent)
 *
 * Block is 1 if the select is requested to block indefinitely, rather than
 * time out.  If block is input as 1, the timeout value will be treated as
 * undefined, but it must be available for setting in Session_selectInfo.  On
 * return, if block is true, the value of timeout will be undefined.
 *
 * Session_selectInfo returns the number of open sockets.  (i.e. The number
 * of sessions open)
 */

int Session_selectInfo( int*, fd_set*, struct timeval*,
    int* );

/*
 * Session_selectInfo2() is similar to Session_selectInfo(), but accepts a
 * pointer to a large file descriptor set instead of a pointer to a
 * regular file descriptor set.
 */

int Session_selectInfo2( int*, LargeFdSet_t*,
    struct timeval*, int* );

#define SESSION_SELECT_NOFLAGS 0x00
#define SESSION_SELECT_NOALARMS 0x01

int Session_sessSelectInfoFlags( void*, int*, fd_set*,
    struct timeval*, int*, int );
int Session_sessSelectInfo2Flags( void*, int*,
    LargeFdSet_t*,
    struct timeval*, int*, int );

/*
 * void Session_timeout();
 *
 * Session_timeout should be called whenever the timeout from Session_selectInfo
 * expires, but it is idempotent, so Session_timeout can be polled (probably a
 * cpu expensive proposition).  Session_timeout checks to see if any of the
 * sessions have an outstanding request that has timed out.  If it finds one
 * (or more), and that pdu has more retries available, a new packet is formed
 * from the pdu and is resent.  If there are no more retries available, the
 * callback for the session is used to alert the user of the timeout.
 */

void Session_timeout( void );

/*
 * single session API.
 *
 * These functions perform similar actions as snmp_XX functions,
 * but operate on a single session only.
 *
 * Synopsis:

 void * sessp;
 Types_Session session, *ss;
 Types_Pdu *pdu, *response;

 Session_sessInit(&session);
 session.retries = ...
 session.remote_port = ...
 sessp = Session_sessOpen(&session);
 ss = Session_sessSession(sessp);
 if (ss == NULL)
 exit(1);
 ...
 if (ss->community) free(ss->community);
 ss->community = strdup(gateway);
 ss->community_len = strlen(gateway);
 ...
 Session_sessSynchResponse(sessp, pdu, &response);
 ...
 Session_sessClose(sessp);

 * See also:
 * Session_sessSynchResponse, in snmp_client.h.

 * Notes:
 *  1. Invoke Session_sessSession after Session_sessOpen.
 *  2. Session_sessSession return value is an opaque pointer.
 *  3. Do NOT free memory returned by Session_sessSession.
 *  4. Replace Session_send(ss,pdu) with Session_sessSend(sessp,pdu)
 */

void* Session_sessOpen( Types_Session* );

void* Session_sessPointer( Types_Session* );

Types_Session* Session_sessSession( void* );

Types_Session* Session_sessSessionLookup( void* );

/*
 * use return value from Session_sessOpen as void * parameter
 */

int Session_sessSend( void*, Types_Pdu* );

int Session_sessAsyncSend( void*, Types_Pdu*,
    Types_CallbackFT, void* );

int Session_sessSelectInfo( void*, int*, fd_set*,
    struct timeval*, int* );

int Session_sessSelectInfo2( void*, int*, LargeFdSet_t*,
    struct timeval*, int* );
/*
 * Returns 0 if success, -1 if fail.
 */

int Session_sessRead( void*, fd_set* );
/*
 * Similar to Session_sessRead(), but accepts a pointer to a large file
 * descriptor set instead of a pointer to a file descriptor set.
 */

int Session_sessRead2( void*,
    LargeFdSet_t* );

void Session_sessTimeout( void* );

int Session_sessClose( void* );

int Session_sessSynchResponse( void*, Types_Pdu*,
    Types_Pdu** );

#endif // SESSION_H
