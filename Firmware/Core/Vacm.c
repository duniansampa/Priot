#include "Vacm.h"
#include "System/Containers/MapList.h"
#include "TextualConvention.h"
#include "ReadConfig.h"
#include "System/Util/Utilities.h"
#include "Priot.h"
#include "System/Util/DefaultStore.h"
#include "Api.h"
#include "System/Util/Trace.h"


/*
 * vacm.c
 *
 * SNMPv3 View-based Access Control Model
 */

static struct Vacm_ViewEntry_s *  _vacm_viewList = NULL,   *_vacm_viewScanPtr = NULL;
static struct Vacm_AccessEntry_s *_vacm_accessList = NULL, *_vacm_accessScanPtr = NULL;
static struct Vacm_GroupEntry_s * _vacm_groupList = NULL,  *_vacm_groupScanPtr = NULL;

/*
 * Macro to extend view masks with 1 bits when shorter than subtree lengths
 * REF: vacmViewTreeFamilyMask [RFC3415], snmpNotifyFilterMask [RFC3413]
 */

#define VIEW_MASK(viewPtr, idx, mask) \
    ((idx >= viewPtr->viewMaskLen) ? mask : (viewPtr->viewMask[idx] & mask))

/**
 * Initilizes the VACM code.
 * Specifically:
 *  - adds a set of enums mapping view numbers to human readable names
 */
void
Vacm_initVacm(void)
{
    /* views for access via get/set/send-notifications */
    MapList_addPair(VACM_VIEW_ENUM_NAME, strdup("read"),
                         VACM_VIEW_READ);
    MapList_addPair(VACM_VIEW_ENUM_NAME, strdup("write"),
                         VACM_VIEW_WRITE);
    MapList_addPair(VACM_VIEW_ENUM_NAME, strdup("notify"),
                         VACM_VIEW_NOTIFY);

    /* views for permissions when receiving notifications */
    MapList_addPair(VACM_VIEW_ENUM_NAME, strdup("log"),
                         VACM_VIEW_LOG);
    MapList_addPair(VACM_VIEW_ENUM_NAME, strdup("execute"),
                         VACM_VIEW_EXECUTE);
    MapList_addPair(VACM_VIEW_ENUM_NAME, strdup("net"),
                         VACM_VIEW_NET);
}

void
Vacm_save(const char *token, const char *type)
{
    struct Vacm_ViewEntry_s *vptr;
    struct Vacm_AccessEntry_s *aptr;
    struct Vacm_GroupEntry_s *gptr;
    int i;

    for (vptr = _vacm_viewList; vptr != NULL; vptr = vptr->next) {
        if (vptr->viewStorageType == TC_ST_NONVOLATILE)
            Vacm_saveView(vptr, token, type);
    }

    for (aptr = _vacm_accessList; aptr != NULL; aptr = aptr->next) {
        if (aptr->storageType == TC_ST_NONVOLATILE) {
            /* Store the standard views (if set) */
            if ( aptr->views[VACM_VIEW_READ  ][0] ||
                 aptr->views[VACM_VIEW_WRITE ][0] ||
                 aptr->views[VACM_VIEW_NOTIFY][0] )
                Vacm_saveAccess(aptr, token, type);
            /* Store any other (valid) access views */
            for ( i=VACM_VIEW_NOTIFY+1; i<VACM_MAX_VIEWS; i++ ) {
                if ( aptr->views[i][0] )
                    Vacm_saveAuthAccess(aptr, token, type, i);
            }
        }
    }

    for (gptr = _vacm_groupList; gptr != NULL; gptr = gptr->next) {
        if (gptr->storageType == TC_ST_NONVOLATILE)
            Vacm_saveGroup(gptr, token, type);
    }
}

/*
 * Vacm_saveView(): saves a view entry to the persistent cache
 */
void
Vacm_saveView(struct Vacm_ViewEntry_s *view, const char *token,
               const char *type)
{
    char            line[4096];
    char           *cptr;

    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "%s%s %d %d %d ", token, "View",
            view->viewStatus, view->viewStorageType, view->viewType);
    line[ sizeof(line)-1 ] = 0;
    cptr = &line[strlen(line)]; /* the NULL */

    cptr =
        ReadConfig_saveOctetString(cptr, (u_char *) view->viewName + 1,
                                      view->viewName[0]);
    *cptr++ = ' ';
    cptr =
        ReadConfig_saveObjid(cptr, view->viewSubtree+1,
                                     view->viewSubtreeLen-1);
    *cptr++ = ' ';
    cptr = ReadConfig_saveOctetString(cptr, (u_char *) view->viewMask,
                                         view->viewMaskLen);

    ReadConfig_store(type, line);
}

void
Vacm_parseConfigView(const char *token, const char *line)
{
    struct Vacm_ViewEntry_s view;
    struct Vacm_ViewEntry_s *vptr;
    char           *viewName = (char *) &view.viewName;
    oid            *viewSubtree = (oid *) & view.viewSubtree;
    u_char         *viewMask;
    size_t          len;

    view.viewStatus = atoi(line);
    line = ReadConfig_skipTokenConst(line);
    view.viewStorageType = atoi(line);
    line = ReadConfig_skipTokenConst(line);
    view.viewType = atoi(line);
    line = ReadConfig_skipTokenConst(line);
    len = sizeof(view.viewName);
    line =
        ReadConfig_readOctetString(line, (u_char **) & viewName, &len);
    view.viewSubtreeLen = TYPES_MAX_OID_LEN;
    line =
        ReadConfig_readObjidConst(line, (oid **) & viewSubtree,
                               &view.viewSubtreeLen);

    vptr =
        Vacm_createViewEntry(view.viewName, view.viewSubtree,
                             view.viewSubtreeLen);
    if (!vptr) {
        return;
    }

    vptr->viewStatus = view.viewStatus;
    vptr->viewStorageType = view.viewStorageType;
    vptr->viewType = view.viewType;
    viewMask = vptr->viewMask;
    vptr->viewMaskLen = sizeof(vptr->viewMask);
    line =
        ReadConfig_readOctetString(line, &viewMask, &vptr->viewMaskLen);
}

/*
 * Vacm_saveAccess(): saves an access entry to the persistent cache
 */
void
Vacm_saveAccess(struct Vacm_AccessEntry_s *access_entry, const char *token,
                 const char *type)
{
    char            line[4096];
    char           *cptr;

    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "%s%s %d %d %d %d %d ",
            token, "Access", access_entry->status,
            access_entry->storageType, access_entry->securityModel,
            access_entry->securityLevel, access_entry->contextMatch);
    line[ sizeof(line)-1 ] = 0;
    cptr = &line[strlen(line)]; /* the NULL */
    cptr =
        ReadConfig_saveOctetString(cptr,
                                      (u_char *) access_entry->groupName + 1,
                                      access_entry->groupName[0] + 1);
    *cptr++ = ' ';
    cptr =
        ReadConfig_saveOctetString(cptr,
                                      (u_char *) access_entry->contextPrefix + 1,
                                      access_entry->contextPrefix[0] + 1);

    *cptr++ = ' ';
    cptr = ReadConfig_saveOctetString(cptr, (u_char *) access_entry->views[VACM_VIEW_READ],
                                         strlen(access_entry->views[VACM_VIEW_READ]) + 1);
    *cptr++ = ' ';
    cptr =
        ReadConfig_saveOctetString(cptr, (u_char *) access_entry->views[VACM_VIEW_WRITE],
                                      strlen(access_entry->views[VACM_VIEW_WRITE]) + 1);
    *cptr++ = ' ';
    cptr =
        ReadConfig_saveOctetString(cptr, (u_char *) access_entry->views[VACM_VIEW_NOTIFY],
                                      strlen(access_entry->views[VACM_VIEW_NOTIFY]) + 1);

    ReadConfig_store(type, line);
}

void
Vacm_saveAuthAccess(struct Vacm_AccessEntry_s *access_entry,
                      const char *token, const char *type, int authtype)
{
    char            line[4096];
    char           *cptr;

    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "%s%s %d %d %d %d %d ",
            token, "AuthAccess", access_entry->status,
            access_entry->storageType, access_entry->securityModel,
            access_entry->securityLevel, access_entry->contextMatch);
    line[ sizeof(line)-1 ] = 0;
    cptr = &line[strlen(line)]; /* the NULL */
    cptr =
        ReadConfig_saveOctetString(cptr,
                                      (u_char *) access_entry->groupName + 1,
                                      access_entry->groupName[0] + 1);
    *cptr++ = ' ';
    cptr =
        ReadConfig_saveOctetString(cptr,
                                      (u_char *) access_entry->contextPrefix + 1,
                                      access_entry->contextPrefix[0] + 1);

    snprintf(cptr, sizeof(line)-(cptr-line), " %d ", authtype);
    while ( *cptr )
        cptr++;

    *cptr++ = ' ';
    cptr = ReadConfig_saveOctetString(cptr,
                               (u_char *)access_entry->views[authtype],
                                  strlen(access_entry->views[authtype]) + 1);

    ReadConfig_store(type, line);
}

char * Vacm_parseConfigAccessCommon(struct Vacm_AccessEntry_s **aptr,
                                 const char *line)
{
    struct Vacm_AccessEntry_s access;
    char           *cPrefix = (char *) &access.contextPrefix;
    char           *gName   = (char *) &access.groupName;
    size_t          len;

    access.status = atoi(line);
    line = ReadConfig_skipTokenConst(line);
    access.storageType = atoi(line);
    line = ReadConfig_skipTokenConst(line);
    access.securityModel = atoi(line);
    line = ReadConfig_skipTokenConst(line);
    access.securityLevel = atoi(line);
    line = ReadConfig_skipTokenConst(line);
    access.contextMatch = atoi(line);
    line = ReadConfig_skipTokenConst(line);
    len  = sizeof(access.groupName);
    line = ReadConfig_readOctetString(line, (u_char **) &gName,   &len);
    len  = sizeof(access.contextPrefix);
    line = ReadConfig_readOctetString(line, (u_char **) &cPrefix, &len);

    *aptr = Vacm_getAccessEntry(access.groupName,
                                  access.contextPrefix,
                                  access.securityModel,
                                  access.securityLevel);
    if (!*aptr)
        *aptr = Vacm_createAccessEntry(access.groupName,
                                  access.contextPrefix,
                                  access.securityModel,
                                  access.securityLevel);
    if (!*aptr)
        return NULL;

    (*aptr)->status = access.status;
    (*aptr)->storageType   = access.storageType;
    (*aptr)->securityModel = access.securityModel;
    (*aptr)->securityLevel = access.securityLevel;
    (*aptr)->contextMatch  = access.contextMatch;
    return UTILITIES_REMOVE_CONST(char *, line);
}

void
Vacm_parseConfigAccess(const char *token, const char *line)
{
    struct Vacm_AccessEntry_s *aptr;
    char           *readView, *writeView, *notifyView;
    size_t          len;

    line = Vacm_parseConfigAccessCommon(&aptr, line);
    if (!line)
        return;

    readView = (char *) aptr->views[VACM_VIEW_READ];
    len = sizeof(aptr->views[VACM_VIEW_READ]);
    line =
        ReadConfig_readOctetString(line, (u_char **) & readView, &len);
    writeView = (char *) aptr->views[VACM_VIEW_WRITE];
    len = sizeof(aptr->views[VACM_VIEW_WRITE]);
    line =
        ReadConfig_readOctetString(line, (u_char **) & writeView, &len);
    notifyView = (char *) aptr->views[VACM_VIEW_NOTIFY];
    len = sizeof(aptr->views[VACM_VIEW_NOTIFY]);
    line =
        ReadConfig_readOctetString(line, (u_char **) & notifyView,
                                      &len);
}

void
Vacm_parseConfigAuthAccess(const char *token, const char *line)
{
    struct Vacm_AccessEntry_s *aptr;
    int             authtype;
    char           *view;
    size_t          len;

    line = Vacm_parseConfigAccessCommon(&aptr, line);
    if (!line)
        return;

    authtype = atoi(line);
    line = ReadConfig_skipTokenConst(line);

    view = (char *) aptr->views[authtype];
    len  = sizeof(aptr->views[authtype]);
    line = ReadConfig_readOctetString(line, (u_char **) & view, &len);
}

/*
 * vacm_save_group(): saves a group entry to the persistent cache
 */
void
Vacm_saveGroup(struct Vacm_GroupEntry_s *group_entry, const char *token,
                const char *type)
{
    char            line[4096];
    char           *cptr;

    memset(line, 0, sizeof(line));
    snprintf(line, sizeof(line), "%s%s %d %d %d ",
            token, "Group", group_entry->status,
            group_entry->storageType, group_entry->securityModel);
    line[ sizeof(line)-1 ] = 0;
    cptr = &line[strlen(line)]; /* the NULL */

    cptr =
        ReadConfig_saveOctetString(cptr,
                                      (u_char *) group_entry->securityName + 1,
                                      group_entry->securityName[0] + 1);
    *cptr++ = ' ';
    cptr = ReadConfig_saveOctetString(cptr, (u_char *) group_entry->groupName,
                                         strlen(group_entry->groupName) + 1);

    ReadConfig_store(type, line);
}

void
Vacm_parseConfigGroup(const char *token, const char *line)
{
    struct Vacm_GroupEntry_s group;
    struct Vacm_GroupEntry_s *gptr;
    char           *securityName = (char *) &group.securityName;
    char           *groupName;
    size_t          len;

    group.status = atoi(line);
    line = ReadConfig_skipTokenConst(line);
    group.storageType = atoi(line);
    line = ReadConfig_skipTokenConst(line);
    group.securityModel = atoi(line);
    line = ReadConfig_skipTokenConst(line);
    len = sizeof(group.securityName);
    line =
        ReadConfig_readOctetString(line, (u_char **) & securityName,
                                      &len);

    gptr = Vacm_createGroupEntry(group.securityModel, group.securityName);
    if (!gptr)
        return;

    gptr->status = group.status;
    gptr->storageType = group.storageType;
    groupName = (char *) gptr->groupName;
    len = sizeof(group.groupName);
    line =
        ReadConfig_readOctetString(line, (u_char **) & groupName, &len);
}

struct Vacm_ViewEntry_s *
Vacm_viewGet(struct Vacm_ViewEntry_s *head, const char *viewName,
                  oid * viewSubtree, size_t viewSubtreeLen, int mode)
{
    struct Vacm_ViewEntry_s *vp, *vpret = NULL;
    char            view[VACM_VACMSTRINGLEN];
    int             found, glen;
    int count=0;

    glen = (int) strlen(viewName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    view[0] = glen;
    strcpy(view + 1, viewName);
    for (vp = head; vp; vp = vp->next) {
        if (!memcmp(view, vp->viewName, glen + 1)
            && viewSubtreeLen >= (vp->viewSubtreeLen - 1)) {
            int             mask = 0x80;
            unsigned int    oidpos, maskpos = 0;
            found = 1;

            for (oidpos = 0;
                 found && oidpos < vp->viewSubtreeLen - 1;
                 oidpos++) {
                if (mode==VACM_MODE_IGNORE_MASK || (VIEW_MASK(vp, maskpos, mask) != 0)) {
                    if (viewSubtree[oidpos] !=
                        vp->viewSubtree[oidpos + 1])
                        found = 0;
                }
                if (mask == 1) {
                    mask = 0x80;
                    maskpos++;
                } else
                    mask >>= 1;
            }

            if (found) {
                /*
                 * match successful, keep this node if its longer than
                 * the previous or (equal and lexicographically greater
                 * than the previous).
                 */
                count++;
                if (mode == VACM_MODE_CHECK_SUBTREE) {
                    vpret = vp;
                } else if (vpret == NULL
                           || vp->viewSubtreeLen > vpret->viewSubtreeLen
                           || (vp->viewSubtreeLen == vpret->viewSubtreeLen
                               && Api_oidCompare(vp->viewSubtree + 1,
                                                   vp->viewSubtreeLen - 1,
                                                   vpret->viewSubtree + 1,
                                                   vpret->viewSubtreeLen - 1) >
                               0)) {
                    vpret = vp;
                }
            }
        }
    }
    DEBUG_MSGTL(("vacm:getView", ", %s\n", (vpret) ? "found" : "none"));
    if (mode == VACM_MODE_CHECK_SUBTREE && count > 1) {
        return NULL;
    }
    return vpret;
}

/*******************************************************************o-o******
 * vacm_checkSubtree
 *
 * Check to see if everything within a subtree is in view, not in view,
 * or possibly both.
 *
 * Parameters:
 *   *viewName           - Name of view to check
 *   *viewSubtree        - OID of subtree
 *    viewSubtreeLen     - length of subtree OID
 *
 * Returns:
 *   VACM_SUCCESS          The OID is included in the view.
 *   VACM_NOTINVIEW        If no entry in the view list includes the
 *                         provided OID, or the OID is explicitly excluded
 *                         from the view.
 *   VACM_SUBTREE_UNKNOWN  The entire subtree has both allowed and disallowed
 *                         portions.
 */
int
Vacm_viewSubtreeCheck(struct Vacm_ViewEntry_s *head, const char *viewName,
                           oid * viewSubtree, size_t viewSubtreeLen)
{
    struct Vacm_ViewEntry_s *vp, *vpShorter = NULL, *vpLonger = NULL;
    char            view[VACM_VACMSTRINGLEN];
    int             found, glen;

    glen = (int) strlen(viewName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return VACM_NOTINVIEW;
    view[0] = glen;
    strcpy(view + 1, viewName);
    DEBUG_MSGTL(("9:vacm:checkSubtree", "view %s\n", viewName));
    for (vp = head; vp; vp = vp->next) {
        if (!memcmp(view, vp->viewName, glen + 1)) {
            /*
             * If the subtree defined in the view is shorter than or equal
             * to the subtree we are comparing, then it might envelop the
             * subtree we are comparing against.
             */
            if (viewSubtreeLen >= (vp->viewSubtreeLen - 1)) {
                int             mask = 0x80;
                unsigned int    oidpos, maskpos = 0;
                found = 1;

                /*
                 * check the mask
                 */
                for (oidpos = 0;
                     found && oidpos < vp->viewSubtreeLen - 1;
                     oidpos++) {
                    if (VIEW_MASK(vp, maskpos, mask) != 0) {
                        if (viewSubtree[oidpos] !=
                            vp->viewSubtree[oidpos + 1])
                            found = 0;
                    }
                    if (mask == 1) {
                        mask = 0x80;
                        maskpos++;
                    } else
                        mask >>= 1;
                }

                if (found) {
                    /*
                     * match successful, keep this node if it's longer than
                     * the previous or (equal and lexicographically greater
                     * than the previous).
                     */
                    DEBUG_MSGTL(("9:vacm:checkSubtree", " %s matched?\n", vp->viewName));

                    if (vpShorter == NULL
                        || vp->viewSubtreeLen > vpShorter->viewSubtreeLen
                        || (vp->viewSubtreeLen == vpShorter->viewSubtreeLen
                           && Api_oidCompare(vp->viewSubtree + 1,
                                               vp->viewSubtreeLen - 1,
                                               vpShorter->viewSubtree + 1,
                                               vpShorter->viewSubtreeLen - 1) >
                                   0)) {
                        vpShorter = vp;
                    }
                }
            }
            /*
             * If the subtree defined in the view is longer than the
             * subtree we are comparing, then it might ambiguate our
             * response.
             */
            else {
                int             mask = 0x80;
                unsigned int    oidpos, maskpos = 0;
                found = 1;

                /*
                 * check the mask up to the length of the provided subtree
                 */
                for (oidpos = 0;
                     found && oidpos < viewSubtreeLen;
                     oidpos++) {
                    if (VIEW_MASK(vp, maskpos, mask) != 0) {
                        if (viewSubtree[oidpos] !=
                            vp->viewSubtree[oidpos + 1])
                            found = 0;
                    }
                    if (mask == 1) {
                        mask = 0x80;
                        maskpos++;
                    } else
                        mask >>= 1;
                }

                if (found) {
                    /*
                     * match successful.  If we already found a match
                     * with a different view type, then parts of the subtree
                     * are included and others are excluded, so return UNKNOWN.
                     */
                    DEBUG_MSGTL(("9:vacm:checkSubtree", " %s matched?\n", vp->viewName));
                    if (vpLonger != NULL
                        && (vpLonger->viewType != vp->viewType)) {
                        DEBUG_MSGTL(("vacm:checkSubtree", ", %s\n", "unknown"));
                        return VACM_SUBTREE_UNKNOWN;
                    }
                    else if (vpLonger == NULL) {
                        vpLonger = vp;
                    }
                }
            }
        }
    }
    DEBUG_MSGTL(("9:vacm:checkSubtree", " %s matched\n", viewName));

    /*
     * If we found a matching view subtree with a longer OID than the provided
     * OID, check to see if its type is consistent with any matching view
     * subtree we may have found with a shorter OID than the provided OID.
     *
     * The view type of the longer OID is inconsistent with the shorter OID in
     * either of these two cases:
     *  1) No matching shorter OID was found and the view type of the longer
     *     OID is INCLUDE.
     *  2) A matching shorter ID was found and its view type doesn't match
     *     the view type of the longer OID.
     */
    if (vpLonger != NULL) {
        if ((!vpShorter && vpLonger->viewType != PRIOT_VIEW_EXCLUDED)
            || (vpShorter && vpLonger->viewType != vpShorter->viewType)) {
            DEBUG_MSGTL(("vacm:checkSubtree", ", %s\n", "unknown"));
            return VACM_SUBTREE_UNKNOWN;
        }
    }

    if (vpShorter && vpShorter->viewType != PRIOT_VIEW_EXCLUDED) {
        DEBUG_MSGTL(("vacm:checkSubtree", ", %s\n", "included"));
        return VACM_SUCCESS;
    }

    DEBUG_MSGTL(("vacm:checkSubtree", ", %s\n", "excluded"));
    return VACM_NOTINVIEW;
}

void
Vacm_scanViewInit(void)
{
    _vacm_viewScanPtr = _vacm_viewList;
}

struct Vacm_ViewEntry_s *
Vacm_scanViewNext(void)
{
    struct Vacm_ViewEntry_s *returnval = _vacm_viewScanPtr;
    if (_vacm_viewScanPtr)
        _vacm_viewScanPtr = _vacm_viewScanPtr->next;
    return returnval;
}

struct Vacm_ViewEntry_s *
Vacm_viewCreate(struct Vacm_ViewEntry_s **head, const char *viewName,
                     oid * viewSubtree, size_t viewSubtreeLen)
{
    struct Vacm_ViewEntry_s *vp, *lp, *op = NULL;
    int             cmp, cmp2, glen;

    glen = (int) strlen(viewName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    vp = (struct Vacm_ViewEntry_s *) calloc(1,
                                          sizeof(struct Vacm_ViewEntry_s));
    if (vp == NULL)
        return NULL;
    vp->reserved =
        (struct Vacm_ViewEntry_s *) calloc(1, sizeof(struct Vacm_ViewEntry_s));
    if (vp->reserved == NULL) {
        free(vp);
        return NULL;
    }

    vp->viewName[0] = glen;
    strcpy(vp->viewName + 1, viewName);
    vp->viewSubtree[0] = viewSubtreeLen;
    memcpy(vp->viewSubtree + 1, viewSubtree, viewSubtreeLen * sizeof(oid));
    vp->viewSubtreeLen = viewSubtreeLen + 1;

    lp = *head;
    while (lp) {
        cmp = memcmp(lp->viewName, vp->viewName, glen + 1);
        cmp2 = Api_oidCompare(lp->viewSubtree, lp->viewSubtreeLen,
                                vp->viewSubtree, vp->viewSubtreeLen);
        if (cmp == 0 && cmp2 > 0)
            break;
        if (cmp > 0)
            break;
        op = lp;
        lp = lp->next;
    }
    vp->next = lp;
    if (op)
        op->next = vp;
    else
        *head = vp;
    return vp;
}

void
Vacm_viewDestroy(struct Vacm_ViewEntry_s **head, const char *viewName,
                      oid * viewSubtree, size_t viewSubtreeLen)
{
    struct Vacm_ViewEntry_s *vp, *lastvp = NULL;

    if ((*head) && !strcmp((*head)->viewName + 1, viewName)
        && (*head)->viewSubtreeLen == viewSubtreeLen
        && !memcmp((char *) (*head)->viewSubtree, (char *) viewSubtree,
                   viewSubtreeLen * sizeof(oid))) {
        vp = (*head);
        (*head) = (*head)->next;
    } else {
        for (vp = (*head); vp; vp = vp->next) {
            if (!strcmp(vp->viewName + 1, viewName)
                && vp->viewSubtreeLen == viewSubtreeLen
                && !memcmp((char *) vp->viewSubtree, (char *) viewSubtree,
                           viewSubtreeLen * sizeof(oid)))
                break;
            lastvp = vp;
        }
        if (!vp || !lastvp)
            return;
        lastvp->next = vp->next;
    }
    if (vp->reserved)
        free(vp->reserved);
    free(vp);
    return;
}

void
Vacm_viewClear(struct Vacm_ViewEntry_s **head)
{
    struct Vacm_ViewEntry_s *vp;
    while ((vp = (*head))) {
        (*head) = vp->next;
        if (vp->reserved)
            free(vp->reserved);
        free(vp);
    }
}

struct Vacm_GroupEntry_s *
Vacm_getGroupEntry(int securityModel, const char *securityName)
{
    struct Vacm_GroupEntry_s *vp;
    char            secname[VACM_VACMSTRINGLEN];
    int             glen;

    glen = (int) strlen(securityName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    secname[0] = glen;
    strcpy(secname + 1, securityName);

    for (vp = _vacm_groupList; vp; vp = vp->next) {
        if ((securityModel == vp->securityModel
             || vp->securityModel == PRIOT_SEC_MODEL_ANY)
            && !memcmp(vp->securityName, secname, glen + 1))
            return vp;
    }
    return NULL;
}

void
Vacm_scanGroupInit(void)
{
    _vacm_groupScanPtr = _vacm_groupList;
}

struct Vacm_GroupEntry_s *
Vacm_scanGroupNext(void)
{
    struct Vacm_GroupEntry_s *returnval = _vacm_groupScanPtr;
    if (_vacm_groupScanPtr)
        _vacm_groupScanPtr = _vacm_groupScanPtr->next;
    return returnval;
}

struct Vacm_GroupEntry_s *
Vacm_createGroupEntry(int securityModel, const char *securityName)
{
    struct Vacm_GroupEntry_s *gp, *lg, *og;
    int             cmp, glen;

    glen = (int) strlen(securityName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    gp = (struct Vacm_GroupEntry_s *) calloc(1,
                                           sizeof(struct Vacm_GroupEntry_s));
    if (gp == NULL)
        return NULL;
    gp->reserved =
        (struct Vacm_GroupEntry_s *) calloc(1,
                                          sizeof(struct Vacm_GroupEntry_s));
    if (gp->reserved == NULL) {
        free(gp);
        return NULL;
    }

    gp->securityModel = securityModel;
    gp->securityName[0] = glen;
    strcpy(gp->securityName + 1, securityName);

    lg = _vacm_groupList;
    og = NULL;
    while (lg) {
        if (lg->securityModel > securityModel)
            break;
        if (lg->securityModel == securityModel &&
            (cmp =
             memcmp(lg->securityName, gp->securityName, glen + 1)) > 0)
            break;
        /*
         * if (lg->securityModel == securityModel && cmp == 0) abort();
         */
        og = lg;
        lg = lg->next;
    }
    gp->next = lg;
    if (og == NULL)
        _vacm_groupList = gp;
    else
        og->next = gp;
    return gp;
}

void
Vacm_destroyGroupEntry(int securityModel, const char *securityName)
{
    struct Vacm_GroupEntry_s *vp, *lastvp = NULL;

    if (_vacm_groupList && _vacm_groupList->securityModel == securityModel
        && !strcmp(_vacm_groupList->securityName + 1, securityName)) {
        vp = _vacm_groupList;
        _vacm_groupList = _vacm_groupList->next;
    } else {
        for (vp = _vacm_groupList; vp; vp = vp->next) {
            if (vp->securityModel == securityModel
                && !strcmp(vp->securityName + 1, securityName))
                break;
            lastvp = vp;
        }
        if (!vp || !lastvp)
            return;
        lastvp->next = vp->next;
    }
    if (vp->reserved)
        free(vp->reserved);
    free(vp);
    return;
}

void
Vacm_destroyAllGroupEntries(void)
{
    struct Vacm_GroupEntry_s *gp;
    while ((gp = _vacm_groupList)) {
        _vacm_groupList = gp->next;
        if (gp->reserved)
            free(gp->reserved);
        free(gp);
    }
}

struct Vacm_AccessEntry_s *
Vacm_chooseBest( struct Vacm_AccessEntry_s *current,
                   struct Vacm_AccessEntry_s *candidate)
{
    /*
     * RFC 3415: vacmAccessTable:
     *    2) if this set has [more than] one member, ...
     *       it comes down to deciding how to weight the
     *       preferences between ContextPrefixes,
     *       SecurityModels, and SecurityLevels
     */
    if (( !current ) ||
        /* a) if the subset of entries with securityModel
         *    matching the securityModel in the message is
         *    not empty, then discard the rest
         */
        (  current->securityModel == PRIOT_SEC_MODEL_ANY &&
         candidate->securityModel != PRIOT_SEC_MODEL_ANY ) ||
        /* b) if the subset of entries with vacmAccessContextPrefix
         *    matching the contextName in the message is
         *    not empty, then discard the rest
         */
        (  current->contextMatch  == CONTEXT_MATCH_PREFIX &&
         candidate->contextMatch  == CONTEXT_MATCH_EXACT ) ||
        /* c) discard all entries with ContextPrefixes shorter
         *    than the longest one remaining in the set
         */
        (  current->contextMatch  == CONTEXT_MATCH_PREFIX &&
           current->contextPrefix[0] < candidate->contextPrefix[0] ) ||
        /* d) select the entry with the highest securityLevel
         */
        (  current->securityLevel < candidate->securityLevel )) {

        return candidate;
    }

    return current;
}

struct Vacm_AccessEntry_s *
Vacm_getAccessEntry(const char *groupName,
                    const char *contextPrefix,
                    int securityModel, int securityLevel)
{
    struct Vacm_AccessEntry_s *vp, *best=NULL;
    char            group[VACM_VACMSTRINGLEN];
    char            context[VACM_VACMSTRINGLEN];
    int             glen, clen;

    glen = (int) strlen(groupName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    clen = (int) strlen(contextPrefix);
    if (clen < 0 || clen > VACM_MAX_STRING)
        return NULL;

    group[0] = glen;
    strcpy(group + 1, groupName);
    context[0] = clen;
    strcpy(context + 1, contextPrefix);
    for (vp = _vacm_accessList; vp; vp = vp->next) {
        if ((securityModel == vp->securityModel
             || vp->securityModel == PRIOT_SEC_MODEL_ANY)
            && securityLevel >= vp->securityLevel
            && !memcmp(vp->groupName, group, glen + 1)
            &&
            ((vp->contextMatch == CONTEXT_MATCH_EXACT
              && clen == vp->contextPrefix[0]
              && (memcmp(vp->contextPrefix, context, clen + 1) == 0))
             || (vp->contextMatch == CONTEXT_MATCH_PREFIX
                 && clen >= vp->contextPrefix[0]
                 && (memcmp(vp->contextPrefix + 1, context + 1,
                            vp->contextPrefix[0]) == 0))))
            best = Vacm_chooseBest( best, vp );
    }
    return best;
}

void
Vacm_scanAccessInit(void)
{
    _vacm_accessScanPtr = _vacm_accessList;
}

struct Vacm_AccessEntry_s *
Vacm_scanAccessNext(void)
{
    struct Vacm_AccessEntry_s *returnval = _vacm_accessScanPtr;
    if (_vacm_accessScanPtr)
        _vacm_accessScanPtr = _vacm_accessScanPtr->next;
    return returnval;
}

struct Vacm_AccessEntry_s *
Vacm_createAccessEntry(const char *groupName,
                       const char *contextPrefix,
                       int securityModel, int securityLevel)
{
    struct Vacm_AccessEntry_s *vp, *lp, *op = NULL;
    int             cmp, glen, clen;

    glen = (int) strlen(groupName);
    if (glen < 0 || glen > VACM_MAX_STRING)
        return NULL;
    clen = (int) strlen(contextPrefix);
    if (clen < 0 || clen > VACM_MAX_STRING)
        return NULL;
    vp = (struct Vacm_AccessEntry_s *) calloc(1,
                                            sizeof(struct
                                                   Vacm_AccessEntry_s));
    if (vp == NULL)
        return NULL;
    vp->reserved =
        (struct Vacm_AccessEntry_s *) calloc(1,
                                           sizeof(struct
                                                  Vacm_AccessEntry_s));
    if (vp->reserved == NULL) {
        free(vp);
        return NULL;
    }

    vp->securityModel = securityModel;
    vp->securityLevel = securityLevel;
    vp->groupName[0] = glen;
    strcpy(vp->groupName + 1, groupName);
    vp->contextPrefix[0] = clen;
    strcpy(vp->contextPrefix + 1, contextPrefix);

    lp = _vacm_accessList;
    while (lp) {
        cmp = memcmp(lp->groupName, vp->groupName, glen + 1);
        if (cmp > 0)
            break;
        if (cmp < 0)
            goto next;
        cmp = memcmp(lp->contextPrefix, vp->contextPrefix, clen + 1);
        if (cmp > 0)
            break;
        if (cmp < 0)
            goto next;
        if (lp->securityModel > securityModel)
            break;
        if (lp->securityModel < securityModel)
            goto next;
        if (lp->securityLevel > securityLevel)
            break;
      next:
        op = lp;
        lp = lp->next;
    }
    vp->next = lp;
    if (op == NULL)
        _vacm_accessList = vp;
    else
        op->next = vp;
    return vp;
}

void
Vacm_destroyAccessEntry(const char *groupName,
                        const char *contextPrefix,
                        int securityModel, int securityLevel)
{
    struct Vacm_AccessEntry_s *vp, *lastvp = NULL;

    if (_vacm_accessList && _vacm_accessList->securityModel == securityModel
        && _vacm_accessList->securityLevel == securityLevel
        && !strcmp(_vacm_accessList->groupName + 1, groupName)
        && !strcmp(_vacm_accessList->contextPrefix + 1, contextPrefix)) {
        vp = _vacm_accessList;
        _vacm_accessList = _vacm_accessList->next;
    } else {
        for (vp = _vacm_accessList; vp; vp = vp->next) {
            if (vp->securityModel == securityModel
                && vp->securityLevel == securityLevel
                && !strcmp(vp->groupName + 1, groupName)
                && !strcmp(vp->contextPrefix + 1, contextPrefix))
                break;
            lastvp = vp;
        }
        if (!vp || !lastvp)
            return;
        lastvp->next = vp->next;
    }
    if (vp->reserved)
        free(vp->reserved);
    free(vp);
    return;
}

void
Vacm_destroyAllAccessEntries(void)
{
    struct Vacm_AccessEntry_s *ap;
    while ((ap = _vacm_accessList)) {
        _vacm_accessList = ap->next;
        if (ap->reserved)
            free(ap->reserved);
        free(ap);
    }
}

int Vacm_storeVacm(int majorID, int minorID, void *serverarg, void *clientarg)
{
    /*
     * figure out our application name
     */
    char           *appname = (char *) clientarg;
    if (appname == NULL) {
        appname = DefaultStore_getString(DsStore_LIBRARY_ID,
                    DsStr_APPTYPE);
    }

    /*
     * save the VACM MIB
     */
    Vacm_save("vacm", appname);
    return ErrorCode_SUCCESS;
}

/*
 * returns 1 if vacm has *any* (non-built-in) configuration entries,
 * regardless of whether or not there is enough to make a decision,
 * else return 0
 */
int
Vacm_isConfigured(void)
{
    if (_vacm_accessList == NULL && _vacm_groupList == NULL) {
        return 0;
    }
    return 1;
}

/*
 * backwards compatability
 */
struct Vacm_ViewEntry_s *
Vacm_getViewEntry(const char *viewName,
                  oid * viewSubtree, size_t viewSubtreeLen, int mode)
{
    return Vacm_viewGet( _vacm_viewList, viewName, viewSubtree, viewSubtreeLen,
                             mode);
}

int
Vacm_checkSubtree(const char *viewName,
                  oid * viewSubtree, size_t viewSubtreeLen)
{
    return Vacm_viewSubtreeCheck( _vacm_viewList, viewName, viewSubtree,
                                       viewSubtreeLen);
}

struct Vacm_ViewEntry_s *
Vacm_createViewEntry(const char *viewName,
                     oid * viewSubtree, size_t viewSubtreeLen)
{
    return Vacm_viewCreate( &_vacm_viewList, viewName, viewSubtree,
                                viewSubtreeLen);
}

void
Vacm_destroyViewEntry(const char *viewName,
                      oid * viewSubtree, size_t viewSubtreeLen)
{
    Vacm_viewDestroy( &_vacm_viewList, viewName, viewSubtree, viewSubtreeLen);
}

void
Vacm_destroyAllViewEntries(void)
{
    Vacm_viewClear( &_vacm_viewList );
}
