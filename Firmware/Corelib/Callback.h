#ifndef CALLBACK_H
#define CALLBACK_H
#include "Generals.h"

#define CALLBACK_MAX_CALLBACK_IDS    2
#define CALLBACK_MAX_CALLBACK_SUBIDS 16

/*
 * Callback Major Types
 */
#define CALLBACK_LIBRARY     0
#define CALLBACK_APPLICATION 1

/*
 * CALLBACK_LIBRARY minor types
 */
#define CALLBACK_POST_READ_CONFIG	        0
#define CALLBACK_STORE_DATA	                1
#define CALLBACK_SHUTDOWN		            2
#define CALLBACK_POST_PREMIB_READ_CONFIG	3
#define CALLBACK_LOGGING			        4
#define CALLBACK_SESSION_INIT		        5
#define CALLBACK_PRE_READ_CONFIG	        7
#define CALLBACK_PRE_PREMIB_READ_CONFIG	    8


/*
 * Callback priority (lower priority numbers called first(
 */
#define CALLBACK_HIGHEST_PRIORITY      -1024
#define CALLBACK_DEFAULT_PRIORITY       0
#define CALLBACK_LOWEST_PRIORITY        1024


typedef int     (Callback_CallbackFT) (int majorID, int minorID,
                                       void *serverarg, void *clientarg);

struct Callback_GenCallback_s {
    Callback_CallbackFT   *sc_callback;
    void           *sc_client_arg;
    int             priority;
    struct Callback_GenCallback_s *next;
};

/*
 * function prototypes
 */

void Callback_initCallbacks(void);


int  Callback_registerCallback2(int major, int minor,
                                          Callback_CallbackFT * new_callback,
                                          void *arg, int priority);

int  Callback_registerCallback(int major, int minor,
                                       Callback_CallbackFT * new_callback, void *arg);

int  Callback_callCallbacks(int major, int minor,
                                    void *caller_arg);

int  Callback_available(int major, int minor);      /* is >1 available */

int  Callback_countCallbacks(int major, int minor); /* ret the number registered */

int  Callback_unregisterCallback(int major, int minor,
                                         Callback_CallbackFT * new_callback,
                                         void *arg, int matchargs);

void  Callback_clearCallback (void);
int   Callback_clearClientArg(void *, int i, int j);

struct Callback_GenCallback_s * Callback_list(int major, int minor);

#endif // CALLBACK_H
