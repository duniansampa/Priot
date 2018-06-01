#include "Session.h"
#include "Api.h"
#include "Client.h"

void Session_sessInit(Types_Session * session){
    Api_sessInit(session);
}

Types_Session * Session_open(Types_Session * session){
    return Api_open(session);
}


int Session_close(Types_Session * session){
    return Api_close(session);
}

int Session_closeSessions(void){
    return Api_closeSessions();
}

int Session_send(Types_Session * session, Types_Pdu * pdu){
    return Api_send(session, pdu);
}


int Session_asyncSend(Types_Session * session , Types_Pdu * pdu , Types_CallbackFT callback, void * cb_data){
    return Api_asyncSend(session, pdu, callback, cb_data );
}

void Session_read(fd_set * fdset){
    Api_read(fdset);
}


void Session_read2(LargeFdSet_t * fdset){
    Api_read2(fdset);
}


int Session_synchResponse(Types_Session *ss, Types_Pdu * pdu, Types_Pdu ** response){
    return Client_synchResponse(ss, pdu, response);
}


int Session_selectInfo(int * numfds, fd_set * fdset, struct timeval * timeout, int * block){
    return Api_selectInfo(numfds, fdset, timeout, block);
}


int Session_selectInfo2(int *numfds, LargeFdSet_t *fdset, struct timeval *timeout, int *block){
    return Api_selectInfo2(numfds, fdset, timeout, block);
}


int Session_sessSelectInfoFlags(void *sessp, int *numfds, fd_set *fdset, struct timeval *timeout, int *block, int flags){
    return Api_sessSelectInfoFlags(sessp, numfds, fdset, timeout, block, flags);
}

int Session_sessSelectInfo2Flags(void *sessp, int *numfds, LargeFdSet_t * fdset, struct timeval *timeout, int *block, int flags){
    return Api_sessSelectInfo2Flags(sessp, numfds, fdset, timeout, block, flags);
}


void Session_timeout(void){
    Api_timeout();
}


void *Session_sessOpen(Types_Session * pss){
    return Api_sessOpen(pss);
}

void * Session_sessPointer(Types_Session * session){
    return Api_sessPointer(session);
}

Types_Session *Session_sessSession(void * sessp){
    return Api_sessSession(sessp);
}

Types_Session *Session_sessSessionLookup(void * sessp){
    return Api_sessSessionLookup(sessp);
}

int Session_sessSend(void * sessp, Types_Pdu * pdu){
    return Api_sessSend(sessp, pdu);
}

int Session_sessAsyncSend(void *sessp, Types_Pdu *pdu, Types_CallbackFT callback, void *cb_data){
    return Api_sessAsyncSend(sessp, pdu, callback, cb_data);
}

int Session_sessSelectInfo(void * sessp, int * numfds, fd_set * fdset, struct timeval * timeout, int * block){

    return Api_sessSelectInfo(sessp, numfds, fdset, timeout, block);
}

int Session_sessSelectInfo2(void * sessp, int * numfds, LargeFdSet_t * fdset, struct timeval * timeout, int * block){
    return Api_sessSelectInfo2(sessp, numfds,fdset, timeout, block);
}

int Session_sessRead(void * sessp, fd_set * fdset){
    return Api_sessRead(sessp, fdset);
}

int Session_sessRead2(void * sessp, LargeFdSet_t * fdset){
    return Api_sessRead2(sessp, fdset);
}

void Session_sessTimeout(void * sessp){
    return Api_sessTimeout(sessp);
}

int Session_sessClose(void * sessp){
    return Api_sessClose(sessp);
}


int Session_sessSynchResponse(void * sessp, Types_Pdu * pdu, Types_Pdu ** response){
    return Client_sessSynchResponse(sessp, pdu, response);
}
