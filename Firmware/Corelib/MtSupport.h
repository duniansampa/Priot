#ifndef MTSUPPORT_H
#define MTSUPPORT_H

/*
 * mt_support.h - multi-thread resource locking support declarations
 */

/*
 * Lock group identifiers
 */

#define MTSUPPORT_LIBRARY_ID      0
#define MTSUPPORT_APPLICATION_ID  1
#define MTSUPPORT_TOKEN_ID        2

#define MTSUPPORT_MAX_IDS         3    /* one greater than last from above */
#define MSUPPORTT_MAX_SUBIDS      10


/*
 * Lock resource identifiers for library resources
 */

#define MTSUPPORT_LIB_NONE        0
#define MTSUPPORT_LIB_SESSION     1
#define MTSUPPORT_LIB_REQUESTID   2
#define MTSUPPORT_LIB_MESSAGEID   3
#define MTSUPPORT_LIB_SESSIONID   4
#define MTSUPPORT_LIB_TRANSID     5

#define MTSUPPORT_LIB_MAXIMUM     6    /* must be one greater than the last one */


#define MtSupport_resInit() do {} while (0)
#define MtSupport_resLock(x,y) do {} while (0)
#define MtSupport_resUnlock(x,y) do {} while (0)
#define MtSupport_resDestroyMutex(x,y) do {} while (0)

#endif // MTSUPPORT_H
