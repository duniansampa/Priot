#include "CallbackDomain.h"
#include "System/Util/Utilities.h"
#include "System/Util/Trace.h"
#include "Impl.h"
#include "Client.h"
#include "Priot.h"

static Transport_TransportList *_callbackDomain_trlist = NULL;

static int      _callbackDomain_callbackCount = 0;

typedef struct CallbackHack_s {
    void           *orig_transport_data;
    Types_Pdu    *pdu;
} CallbackHack;

typedef struct CallbackQueue_s {
    int             callback_num;
    CallbackPass *item;
    struct CallbackQueue_s *next, *prev;
} CallbackQueue;

CallbackQueue *callbackDomain_thequeue;

static Transport_Transport *
_CallbackDomain_findTransportFromCallbackNum(int num)
{
    static Transport_TransportList *ptr;
    for (ptr = _callbackDomain_trlist; ptr; ptr = ptr->next)
        if (((CallbackInfo *) ptr->transport->data)->
            callback_num == num)
            return ptr->transport;
    return NULL;
}

static void
_CallbackDomain_debugPdu(const char *ourstring, Types_Pdu *pdu)
{
    VariableList *vb;
    int             i = 1;
    DEBUG_MSGTL((ourstring,
                "PDU: command = %d, errstat = %ld, errindex = %ld\n",
                pdu->command, pdu->errstat, pdu->errindex));
    for (vb = pdu->variables; vb; vb = vb->next) {
        DEBUG_MSGTL((ourstring, "  var %d:", i++));
        DEBUG_MSGVAR((ourstring, vb));
        DEBUG_MSG((ourstring, "\n"));
    }
}

void
CallbackDomain_pushQueue(int num, CallbackPass *item)
{
    CallbackQueue *newitem = MEMORY_MALLOC_TYPEDEF(CallbackQueue);
    CallbackQueue *ptr;

    if (newitem == NULL)
        return;
    newitem->callback_num = num;
    newitem->item = item;
    if (callbackDomain_thequeue) {
        for (ptr = callbackDomain_thequeue; ptr && ptr->next; ptr = ptr->next) {
        }
        ptr->next = newitem;
        newitem->prev = ptr;
    } else {
        callbackDomain_thequeue = newitem;
    }
    DEBUG_IF("dumpSendCallbackTransport") {
        _CallbackDomain_debugPdu("dumpSendCallbackTransport", item->pdu);
    }
}

CallbackPass *
CallbackDomain_popQueue(int num)
{
    CallbackPass *cp;
    CallbackQueue *ptr;

    for (ptr = callbackDomain_thequeue; ptr; ptr = ptr->next) {
        if (ptr->callback_num == num) {
            if (ptr->prev) {
                ptr->prev->next = ptr->next;
            } else {
                callbackDomain_thequeue = ptr->next;
            }
            if (ptr->next) {
                ptr->next->prev = ptr->prev;
            }
            cp = ptr->item;
            MEMORY_FREE(ptr);
            DEBUG_IF("dumpRecvCallbackTransport") {
                _CallbackDomain_debugPdu("dumpRecvCallbackTransport",
                                   cp->pdu);
            }
            return cp;
        }
    }
    return NULL;
}

/*
 * Return a string representing the address in data, or else the "far end"
 * address if data is NULL.
 */

char *
CallbackDomain_fmtaddr(Transport_Transport *t, void *data, int len)
{
    char buf[IMPL_SPRINT_MAX_LEN];
    CallbackInfo *mystuff;

    if (!t)
        return strdup("callback: unknown");

    mystuff = (CallbackInfo *) t->data;

    if (!mystuff)
        return strdup("callback: unknown");

    snprintf(buf, IMPL_SPRINT_MAX_LEN, "callback: %d on fd %d",
             mystuff->callback_num, mystuff->pipefds[0]);
    return strdup(buf);
}



/*
 * You can write something into opaque that will subsequently get passed back
 * to your send function if you like.  For instance, you might want to
 * remember where a PDU came from, so that you can send a reply there...
 */

int
CallbackDomain_recv(Transport_Transport *t, void *buf, int size,
              void **opaque, int *olength)
{
    int rc = -1;
    char newbuf[1];
    CallbackInfo *mystuff = (CallbackInfo *) t->data;

    DEBUG_MSGTL(("transportCallback", "hook_recv enter\n"));

    while (rc < 0) {
    rc = read(mystuff->pipefds[0], newbuf, 1);
    if (rc < 0 && errno != EINTR) {
        break;
    }
    }
    if (rc > 0)
        memset(buf, 0, rc);

    if (mystuff->linkedto) {
        /*
         * we're the client.  We don't need to do anything.
         */
    } else {
        /*
         * malloc the space here, but it's filled in by
         * snmp_callback_created_pdu() below
         */
        int            *returnnum = (int *) calloc(1, sizeof(int));
        *opaque = returnnum;
        *olength = sizeof(int);
    }
    DEBUG_MSGTL(("transportCallback", "hook_recv exit\n"));
    return rc;
}



int
CallbackDomain_send(Transport_Transport *t, void *buf, int size,
              void **opaque, int *olength)
{
    int from, rc = -1;
    CallbackInfo *mystuff = (CallbackInfo *) t->data;
    CallbackPass *cp;

    /*
     * extract the pdu from the hacked buffer
     */
    Transport_Transport *other_side;
    CallbackHack  *ch = (CallbackHack *) * opaque;
    Types_Pdu    *pdu = ch->pdu;
    *opaque = ch->orig_transport_data;
    MEMORY_FREE(ch);

    DEBUG_MSGTL(("transportCallback", "hook_send enter\n"));

    cp = MEMORY_MALLOC_TYPEDEF(CallbackPass);
    if (!cp)
        return -1;

    cp->pdu = Client_clonePdu(pdu);
    if (cp->pdu->transportData) {
        /*
         * not needed and not properly freed later
         */
        MEMORY_FREE(cp->pdu->transportData);
    }

    if (cp->pdu->flags & PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE)
        cp->pdu->flags ^= PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE;

    /*
     * push the sent pdu onto the stack
     */
    /*
     * AND send a bogus byte to the remote callback receiver's pipe
     */
    if (mystuff->linkedto) {
        /*
         * we're the client, send it to the parent
         */
        cp->return_transport_num = mystuff->callback_num;

        other_side = _CallbackDomain_findTransportFromCallbackNum(mystuff->linkedto);
        if (!other_side) {
            Api_freePdu(cp->pdu);
            MEMORY_FREE(cp);
            return -1;
        }

    while (rc < 0) {

        rc = write(((CallbackInfo *)other_side->data)->pipefds[1],
               " ", 1);
        if (rc < 0 && errno != EINTR) {
        break;
        }
    }
        CallbackDomain_pushQueue(mystuff->linkedto, cp);
        /*
         * we don't need the transport data any more
         */
        MEMORY_FREE(*opaque);
    } else {
        /*
         * we're the server, send it to the person that sent us the request
         */
        from = **((int **) opaque);
        /*
         * we don't need the transport data any more
         */
        MEMORY_FREE(*opaque);
        other_side = _CallbackDomain_findTransportFromCallbackNum(from);
        if (!other_side) {
            Api_freePdu(cp->pdu);
            MEMORY_FREE(cp);
            return -1;
        }
    while (rc < 0) {

        rc = write(((CallbackInfo *)other_side->data)->pipefds[1],
               " ", 1);
        if (rc < 0 && errno != EINTR) {
        break;
        }
    }
        CallbackDomain_pushQueue(from, cp);
    }

    DEBUG_MSGTL(("transportCallback", "hook_send exit\n"));
    return 0;
}



int
CallbackDomain_close(Transport_Transport *t)
{
    int             rc;
    CallbackInfo *mystuff = (CallbackInfo *) t->data;
    DEBUG_MSGTL(("transportCallback", "hook_close enter\n"));


    rc  = close(mystuff->pipefds[0]);
    rc |= close(mystuff->pipefds[1]);

    rc |= Transport_removeFromList(&_callbackDomain_trlist, t);

    DEBUG_MSGTL(("transportCallback", "hook_close exit\n"));
    return rc;
}



int
CallbackDomain_accept(Transport_Transport *t)
{
    DEBUG_MSGTL(("transportCallback", "hook_accept enter\n"));
    DEBUG_MSGTL(("transportCallback", "hook_accept exit\n"));
    return -1;
}



/*
 * Open a Callback-domain transport for SNMP.  Local is TRUE if addr
 * is the local address to bind to (i.e. this is a server-type
 * session); otherwise addr is the remote address to send things to
 * (and we make up a temporary name for the local end of the
 * connection).
 */

Transport_Transport *
CallbackDomain_transport(int to)
{

    Transport_Transport *t = NULL;
    CallbackInfo *mydata;
    int             rc;

    /*
     * transport
     */
    t = MEMORY_MALLOC_TYPEDEF(Transport_Transport);
    if (!t)
        return NULL;

    /*
     * our stuff
     */
    mydata = MEMORY_MALLOC_TYPEDEF(CallbackInfo);
    if (!mydata) {
        MEMORY_FREE(t);
        return NULL;
    }
    mydata->linkedto = to;
    mydata->callback_num = ++_callbackDomain_callbackCount;
    mydata->data = NULL;
    t->data = mydata;


    rc = pipe(mydata->pipefds);
    t->sock = mydata->pipefds[0];

    if (rc) {
        MEMORY_FREE(mydata);
        MEMORY_FREE(t);
        return NULL;
    }

    t->f_recv    = CallbackDomain_recv;
    t->f_send    = CallbackDomain_send;
    t->f_close   = CallbackDomain_close;
    t->f_accept  = CallbackDomain_accept;
    t->f_fmtaddr = CallbackDomain_fmtaddr;

    Transport_addToList(&_callbackDomain_trlist, t);

    if (to)
        DEBUG_MSGTL(("transportCallback", "initialized %d linked to %d\n",
                    mydata->callback_num, to));
    else
        DEBUG_MSGTL(("transportCallback",
                    "initialized master listening on %d\n",
                    mydata->callback_num));
    return t;
}

int
CallbackDomain_hookParse(Types_Session * sp,
                            Types_Pdu *pdu,
                            u_char * packetptr, size_t len)
{
    if (PRIOT_MSG_RESPONSE == pdu->command ||
        PRIOT_MSG_REPORT == pdu->command)
        pdu->flags |= PRIOT_UCD_MSG_FLAG_RESPONSE_PDU;
    else
        pdu->flags &= (~PRIOT_UCD_MSG_FLAG_RESPONSE_PDU);

    return PRIOT_ERR_NOERROR;
}

int
CallbackDomain_hookBuild(Types_Session * sp,
                            Types_Pdu *pdu, u_char * ptk, size_t * len)
{
    /*
     * very gross hack, as this is passed later to the transport_send
     * function
     */
    CallbackHack  *ch = MEMORY_MALLOC_TYPEDEF(CallbackHack);
    if (ch == NULL)
        return -1;
    DEBUG_MSGTL(("transportCallback", "hook_build enter\n"));
    ch->pdu = pdu;
    ch->orig_transport_data = pdu->transportData;
    pdu->transportData = ch;
    switch (pdu->command) {
    case PRIOT_MSG_GETBULK:
        if (pdu->max_repetitions < 0) {
            sp->s_snmp_errno = ErrorCode_BAD_REPETITIONS;
            return -1;
        }
        if (pdu->non_repeaters < 0) {
            sp->s_snmp_errno = ErrorCode_BAD_REPEATERS;
            return -1;
        }
        break;
    case PRIOT_MSG_RESPONSE:
    case PRIOT_MSG_TRAP:
    case PRIOT_MSG_TRAP2:
        pdu->flags &= (~PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE);
        /*
         * Fallthrough
         */
    default:
        if (pdu->errstat == API_DEFAULT_ERRSTAT)
            pdu->errstat = 0;
        if (pdu->errindex == API_DEFAULT_ERRINDEX)
            pdu->errindex = 0;
        break;
    }

    /*
     * Copy missing values from session defaults
     */
    switch (pdu->version) {
    case PRIOT_VERSION_3:
        if (pdu->securityNameLen == 0) {
      pdu->securityName = (char *)malloc(sp->securityNameLen);
            if (pdu->securityName == NULL) {
                sp->s_snmp_errno = ErrorCode_MALLOC;
                return -1;
            }
            memmove(pdu->securityName,
                     sp->securityName, sp->securityNameLen);
            pdu->securityNameLen = sp->securityNameLen;
        }
        if (pdu->securityModel == -1)
            pdu->securityModel = sp->securityModel;
        if (pdu->securityLevel == 0)
            pdu->securityLevel = sp->securityLevel;
        /* WHAT ELSE ?? */
    }
    ptk[0] = 0;
    *len = 1;
    DEBUG_MSGTL(("transportCallback", "hook_build exit\n"));
    return 1;
}

int
CallbackDomain_checkPacket(u_char * pkt, size_t len)
{
    return 1;
}

Types_Pdu    *
CallbackDomain_createPdu(Transport_Transport *transport,
                            void *opaque, size_t olength)
{
    Types_Pdu    *pdu;
    CallbackPass *cp =
        CallbackDomain_popQueue(((CallbackInfo *) transport->data)->
                           callback_num);
    if (!cp)
        return NULL;
    pdu = cp->pdu;
    pdu->transportData = opaque;
    pdu->transportDataLength = olength;
    if (opaque)                 /* if created, we're the server */
        *((int *) opaque) = cp->return_transport_num;
    MEMORY_FREE(cp);
    return pdu;
}

Types_Session *
CallbackDomain_open(int attach_to,
                      int (*return_func) (int op,
                                          Types_Session * session,
                                          int reqid, Types_Pdu *pdu,
                                          void *magic),
                      int (*fpre_parse) (Types_Session *,
                                         struct Transport_Transport_s *,
                                         void *, int),
                      int (*fpost_parse) (Types_Session *, Types_Pdu *,
                                          int))
{
    Types_Session callback_sess, *callback_ss;
    Transport_Transport *callback_tr;

    callback_tr = CallbackDomain_transport(attach_to);
    Api_sessInit(&callback_sess);
    callback_sess.callback = return_func;
    if (attach_to) {
        /*
         * client
         */
        /*
         * trysess.community = (u_char *) callback_ss;
         */
    } else {
        callback_sess.isAuthoritative = API_SESS_AUTHORITATIVE;
    }
    callback_sess.remote_port = 0;
    callback_sess.retries = 0;
    callback_sess.timeout = 30000000;
    callback_sess.version = API_DEFAULT_VERSION;       /* (mostly) bogus */
    callback_ss = Api_addFull(&callback_sess, callback_tr,
                                fpre_parse,
                                CallbackDomain_hookParse, fpost_parse,
                                CallbackDomain_hookBuild,
                                NULL,
                                CallbackDomain_checkPacket,
                                CallbackDomain_createPdu);
    if (callback_ss)
        callback_ss->local_port =
            ((CallbackInfo *) callback_tr->data)->callback_num;
    return callback_ss;
}



void
CallbackDomain_clearCallbackList(void)
{

    Transport_TransportList *list = _callbackDomain_trlist, *next = NULL;
    Transport_Transport *tr = NULL;

    DEBUG_MSGTL(("callbackClear", "called CallbackDomain_clearCallbackList()\n"));
    while (list != NULL) {
    next = list->next;
    tr = list->transport;

    if (tr != NULL) {
        tr->f_close(tr);
        Transport_removeFromList(&_callbackDomain_trlist, tr);
        Transport_free(tr);
    }
    list = next;
    }
    _callbackDomain_trlist = NULL;

}

