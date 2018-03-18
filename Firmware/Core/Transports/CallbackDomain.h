#ifndef CALLBACKDOMAIN_H
#define CALLBACKDOMAIN_H


#include "Types.h"
#include "Transport.h"

typedef struct CallbackPass_s {
    int             return_transport_num;
    Types_Pdu    *pdu;
    struct CallbackPass_s *next;
} CallbackPass;

typedef struct CallbackInfo_s {
    int             linkedto;
    void           *parent_data;
    CallbackPass *data;
    int             callback_num;
    int             pipefds[2];
} CallbackInfo;

Transport_Transport * CallbackDomain_transport(int);

int
CallbackDomain_hookParse(Types_Session * sp,
                                            Types_Pdu *pdu,
                                            u_char * packetptr,
                                            size_t len);
int
CallbackDomain_hookBuild(Types_Session * sp,
                                            Types_Pdu *pdu,
                                            u_char * ptk, size_t * len);
int
CallbackDomain_checkPacket(u_char * pkt, size_t len);

Types_Pdu    *
CallbackDomain_createPdu(Transport_Transport *transport,
                                            void *opaque, size_t olength);
Types_Session *
CallbackDomain_open( int attach_to,
                   int (*return_func) (int op,
                                       Types_Session
                                       * session,
                                       int reqid,
                                       Types_Pdu
                                       *pdu,
                                       void *magic),
                   int (*fpre_parse) (Types_Session
                                      *,
                                      struct
                                      Transport_Transport_s
                                      *, void *, int),
                   int (*fpost_parse) (Types_Session
                                       *,
                                       Types_Pdu *,
                                       int));

void
CallbackDomain_clearCallbackList(void);


#endif // CALLBACKDOMAIN_H
