#ifndef VACM_H
#define VACM_H

#include "Generals.h"
#include "Types.h"

#define VACM_SUCCESS         0
#define VACM_NOSECNAME       1
#define VACM_NOGROUP         2
#define VACM_NOACCESS        3
#define VACM_NOVIEW          4
#define VACM_NOTINVIEW       5
#define VACM_NOSUCHCONTEXT   6
#define VACM_SUBTREE_UNKNOWN 7

#define VACM_SECURITYMODEL	 1
#define VACM_SECURITYNAME	 2
#define VACM_SECURITYGROUP	 3
#define VACM_SECURITYSTORAGE 4
#define VACM_SECURITYSTATUS	 5

#define VACM_ACCESSPREFIX	1
#define VACM_ACCESSMODEL	2
#define VACM_ACCESSLEVEL	3
#define VACM_ACCESSMATCH	4
#define VACM_ACCESSREAD	    5
#define VACM_ACCESSWRITE	6
#define VACM_ACCESSNOTIFY	7
#define VACM_ACCESSSTORAGE	8
#define VACM_ACCESSSTATUS	9

#define VACM_VACMVIEWSPINLOCK 1
#define VACM_VIEWNAME	      2
#define VACM_VIEWSUBTREE	  3
#define VACM_VIEWMASK	      4
#define VACM_VIEWTYPE	      5
#define VACM_VIEWSTORAGE	  6
#define VACM_VIEWSTATUS	      7

#define VACM_MAX_STRING      32
#define VACM_VACMSTRINGLEN   34      /* VACM_MAX_STRING + 2 */

struct Vacm_GroupEntry_s {
    int             securityModel;
    char            securityName[VACM_VACMSTRINGLEN];
    char            groupName[VACM_VACMSTRINGLEN];
    int             storageType;
    int             status;

    u_long          bitMask;
    struct Vacm_GroupEntry_s *reserved;
    struct Vacm_GroupEntry_s *next;
};

#define CONTEXT_MATCH_EXACT  1
#define CONTEXT_MATCH_PREFIX 2

/* VIEW ENUMS ---------------------------------------- */

/* SNMPD usage: get/set/send-notification views */
#define VACM_VIEW_READ     0
#define VACM_VIEW_WRITE    1
#define VACM_VIEW_NOTIFY   2

/* SNMPTRAPD usage: log execute and net-access (forward) usage */
#define VACM_VIEW_LOG      3
#define VACM_VIEW_EXECUTE  4
#define VACM_VIEW_NET      5

/* VIEW BIT MASK VALUES-------------------------------- */

/* SNMPD usage: get/set/send-notification views */
#define VACM_VIEW_READ_BIT      (1 << VACM_VIEW_READ)
#define VACM_VIEW_WRITE_BIT     (1 << VACM_VIEW_WRITE)
#define VACM_VIEW_NOTIFY_BIT    (1 << VACM_VIEW_NOTIFY)

/* SNMPTRAPD usage: log execute and net-access (forward) usage */
#define VACM_VIEW_LOG_BIT      (1 << VACM_VIEW_LOG)
#define VACM_VIEW_EXECUTE_BIT  (1 << VACM_VIEW_EXECUTE)
#define VACM_VIEW_NET_BIT      (1 << VACM_VIEW_NET)

#define VACM_VIEW_NO_BITS      0

/* Maximum number of views in the view array */
#define VACM_MAX_VIEWS     8

#define VACM_VIEW_ENUM_NAME "vacmviews"

void init_vacm(void);

struct Vacm_AccessEntry_s {
    char            groupName[VACM_VACMSTRINGLEN];
    char            contextPrefix[VACM_VACMSTRINGLEN];
    int             securityModel;
    int             securityLevel;
    int             contextMatch;
    char            views[VACM_MAX_VIEWS][VACM_VACMSTRINGLEN];
    int             storageType;
    int             status;

    u_long          bitMask;
    struct Vacm_AccessEntry_s *reserved;
    struct Vacm_AccessEntry_s *next;
};

struct Vacm_ViewEntry_s {
    char            viewName[VACM_VACMSTRINGLEN];
    oid            viewSubtree[TYPES_MAX_OID_LEN];
    size_t          viewSubtreeLen;
    u_char          viewMask[VACM_VACMSTRINGLEN];
    size_t          viewMaskLen;
    int             viewType;
    int             viewStorageType;
    int             viewStatus;

    u_long          bitMask;

    struct Vacm_ViewEntry_s *reserved;
    struct Vacm_ViewEntry_s *next;
};


void            Vacm_destroyViewEntry(const char *, oid *, size_t);

void            Vacm_destroyAllViewEntries(void);

#define VACM_MODE_FIND                0
#define VACM_MODE_IGNORE_MASK         1
#define VACM_MODE_CHECK_SUBTREE       2

struct Vacm_ViewEntry_s * Vacm_getViewEntry(const char *, oid *, size_t, int);
/*
 * Returns a pointer to the viewEntry with the
 * same viewName and viewSubtree
 * Returns NULL if that entry does not exist.
 */


void Vacm_initVacm();

int Vacm_checkSubtree(const char *, oid *, size_t);

/*
 * Check to see if everything within a subtree is in view, not in view,
 * or possibly both.
 *
 * Returns:
 *   VACM_SUCCESS          The OID is included in the view.
 *   VACM_NOTINVIEW        If no entry in the view list includes the
 *                         provided OID, or the OID is explicitly excluded
 *                         from the view.
 *   VACM_SUBTREE_UNKNOWN  The entire subtree has both allowed and
 *                         disallowed portions.
 */


void Vacm_scanViewInit(void);
/*
 * Initialized the scan routines so that they will begin at the
 * beginning of the list of viewEntries.
 *
 */



struct Vacm_ViewEntry_s * Vacm_scanViewNext(void);
/*
 * Returns a pointer to the next viewEntry.
 * These entries are returned in no particular order,
 * but if N entries exist, N calls to view_scanNext() will
 * return all N entries once.
 * Returns NULL if all entries have been returned.
 * view_scanInit() starts the scan over.
 */


struct Vacm_ViewEntry_s * Vacm_createViewEntry(const char *, oid *, size_t);
/*
 * Creates a viewEntry with the given index
 * and returns a pointer to it.
 * The status of this entry is created as invalid.
 */


void  Vacm_destroyGroupEntry(int, const char *);

void  Vacm_destroyAllGroupEntries(void);

struct Vacm_GroupEntry_s * Vacm_createGroupEntry(int, const char *);

struct Vacm_GroupEntry_s * Vacm_getGroupEntry(int, const char *);

void            Vacm_scanGroupInit(void);

struct Vacm_GroupEntry_s * Vacm_scanGroupNext(void);


void            Vacm_destroyAccessEntry(const char *, const char *, int, int);

void            Vacm_destroyAllAccessEntries(void);

struct Vacm_AccessEntry_s * Vacm_createAccessEntry(const char *,
                                                const char *, int, int);

struct Vacm_AccessEntry_s * Vacm_getAccessEntry(const char *,
                                                const char *, int, int);

void            Vacm_scanAccessInit(void);

struct Vacm_AccessEntry_s * Vacm_scanAccessNext(void);


int             Vacm_isConfigured(void);

void            Vacm_save(const char *token, const char *type);
void            Vacm_saveView(struct Vacm_ViewEntry_s *view,
                               const char *token, const char *type);
void            Vacm_saveAccess(struct Vacm_AccessEntry_s *access_entry,
                                 const char *token, const char *type);
void            Vacm_saveAuthAccess(struct Vacm_AccessEntry_s *access_entry,
                                 const char *token, const char *type, int authtype);
void            Vacm_saveGroup(struct Vacm_GroupEntry_s *group_entry,
                                const char *token, const char *type);


void            Vacm_parseConfigView(const char *token, const char *line);

void            Vacm_parseConfigGroup(const char *token,
                                        const char *line);

void            Vacm_parseConfigAccess(const char *token,
                                         const char *line);

void            Vacm_parseConfigAuthAccess(const char *token,
                                              const char *line);


int             Vacm_storeVacm(int majorID, int minorID, void *serverarg,
                           void *clientarg);


struct Vacm_ViewEntry_s * Vacm_viewGet(struct Vacm_ViewEntry_s *head,
                                        const char *viewName,
                                        oid * viewSubtree,
                                        size_t viewSubtreeLen, int mode);


#endif // VACM_H
