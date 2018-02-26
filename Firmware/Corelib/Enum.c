#include "Enum.h"
#include "ReadConfig.h"
#include "Tools.h"
#include "DefaultStore.h"
#include "Assert.h"
#include "Api.h"

#include <stdlib.h>
#include <stdio.h>

struct Enum_EnumListStr_s {
    char           *name;
    struct Enum_EnumList_s *list;
    struct Enum_EnumListStr_s *next;
};

static struct Enum_EnumList_s ***_enum_enumLists;

unsigned int    emum_currentMajNum;
unsigned int    emum_currentMinNum;

static struct Enum_EnumListStr_s *_enum_sliststorage;

static void  _Enum_freeEnumList(struct Enum_EnumList_s *list);

int Enum_initEnum(const char *type)
{
    int  i;

    if (NULL != _enum_enumLists)
        return ENUM_SE_OK;

    _enum_enumLists = (struct Enum_EnumList_s ***)
        calloc(1, sizeof(struct Enum_EnumList_s **) * ENUM_SE_MAX_IDS);
    if (!_enum_enumLists)
        return ENUM_SE_NOMEM;
    emum_currentMajNum = ENUM_SE_MAX_IDS;

    for (i = 0; i < ENUM_SE_MAX_IDS; i++) {
        if (!_enum_enumLists[i])
            _enum_enumLists[i] = (struct Enum_EnumList_s **)
                calloc(1, sizeof(struct Enum_EnumList_s *) * ENUM_SE_MAX_SUBIDS);
        if (!_enum_enumLists[i])
            return ENUM_SE_NOMEM;
    }
    emum_currentMinNum = ENUM_SE_MAX_SUBIDS;

    ReadConfig_registerConfigHandler(type, "enum", Enum_seReadConf, NULL, NULL);
    return ENUM_SE_OK;
}

int Enum_seStoreInList(struct Enum_EnumList_s *new_list,
              unsigned int major, unsigned int minor)
{
    int             ret = ENUM_SE_OK;

    if (major > emum_currentMajNum || minor > emum_currentMinNum) {
        /*
         * XXX: realloc
         */
        return ENUM_SE_NOMEM;
    }
    Assert_assert(NULL != _enum_enumLists);

    if (_enum_enumLists[major][minor] != NULL)
        ret = ENUM_SE_ALREADY_THERE;

    _enum_enumLists[major][minor] = new_list;

    return ret;
}

void Enum_seReadConf(const char *word, char *cptr)
{
    int major, minor;
    int value;
    char *cp, *cp2;
    char e_name[BUFSIZ];
    char e_enum[  BUFSIZ];

    if (!cptr || *cptr=='\0')
        return;

    /*
     * Extract the first token
     *   (which should be the name of the list)
     */
    cp = ReadConfig_copyNword(cptr, e_name, sizeof(e_name));
    cp = ReadConfig_skipWhite(cp);
    if (!cp || *cp=='\0')
        return;


    /*
     * Add each remaining enumeration to the list,
     *   using the appropriate style interface
     */
    if (sscanf(e_name, "%d:%d", &major, &minor) == 2) {
        /*
         *  Numeric major/minor style
         */
        while (1) {
            cp = ReadConfig_copyNword(cp, e_enum, sizeof(e_enum));
            if (sscanf(e_enum, "%d:", &value) != 1) {
                break;
            }
            cp2 = e_enum;
            while (*(cp2++) != ':')
                ;
            Enum_seAddPair(major, minor, strdup(cp2), value);
            if (!cp)
                break;
        }
    } else {
        /*
         *  Named enumeration
         */
        while (1) {
            cp = ReadConfig_copyNword(cp, e_enum, sizeof(e_enum));
            if (sscanf(e_enum, "%d:", &value) != 1) {
                break;
            }
            cp2 = e_enum;
            while (*(cp2++) != ':')
                ;
            Enum_seAddPairToSlist(e_name, strdup(cp2), value);
            if (!cp)
                break;
        }
    }
}

void Enum_seStoreEnumList(struct Enum_EnumList_s *new_list,
                   const char *token, const char *type)
{
    struct Enum_EnumList_s *listp = new_list;
    char line[2048];
    char buf[512];
    int  len;

    snprintf(line, sizeof(line), "enum %s", token);
    while (listp) {
        snprintf(buf, sizeof(buf), " %d:%s", listp->value, listp->label);
        /*
         * Calculate the space left in the buffer.
         * If this is not sufficient to include the next enum,
         *   then save the line so far, and start again.
         */
    len = sizeof(line) - strlen(line);
    if ((int)strlen(buf) > len) {
        ReadConfig_store(type, line);
            snprintf(line, sizeof(line), "enum %s", token);
        len = sizeof(line) - strlen(line);
    }

    strncat(line, buf, len);
        listp = listp->next;
    }

    ReadConfig_store(type, line);
}

void Enum_seStoreList(unsigned int major, unsigned int minor, const char *type)
{
    char token[32];

    snprintf(token, sizeof(token), "%d:%d", major, minor);
    Enum_seStoreEnumList(Enum_seFindList(major, minor), token, type);
}

struct Enum_EnumList_s * Enum_seFindList(unsigned int major, unsigned int minor)
{
    if (major > emum_currentMajNum || minor > emum_currentMinNum)
        return NULL;
    Assert_assert(NULL != _enum_enumLists);

    return _enum_enumLists[major][minor];
}

int Enum_seFindValueInList(struct Enum_EnumList_s *list, const char *label)
{
    if (!list)
        return ENUM_SE_DNE;          /* XXX: um, no good solution here */
    while (list) {
        if (strcmp(list->label, label) == 0)
            return (list->value);
        list = list->next;
    }

    return ENUM_SE_DNE;              /* XXX: um, no good solution here */
}

int Enum_seFindFreeValueInList(struct Enum_EnumList_s *list)
{
    int max_value = 0;
    if (!list)
        return ENUM_SE_DNE;

    for (;list; list=list->next) {
        if (max_value < list->value)
            max_value = list->value;
    }
    return max_value+1;
}

int Enum_seFindValue(unsigned int major, unsigned int minor, const char *label)
{
    return Enum_seFindValueInList(Enum_seFindList(major, minor), label);
}

int Enum_seFindFreeValue(unsigned int major, unsigned int minor)
{
    return Enum_seFindFreeValueInList(Enum_seFindList(major, minor));
}

char * Enum_seFindLabelInList(struct Enum_EnumList_s *list, int value)
{
    if (!list)
        return NULL;
    while (list) {
        if (list->value == value)
            return (list->label);
        list = list->next;
    }
    return NULL;
}

char * Enum_seFindLabel(unsigned int major, unsigned int minor, int value)
{
    return Enum_seFindLabelInList(Enum_seFindList(major, minor), value);
}

int Enum_seAddPairToList(struct Enum_EnumList_s **list, char *label, int value)
{
    struct Enum_EnumList_s *lastnode = NULL, *tmp;

    if (!list)
        return ENUM_SE_DNE;

    tmp = *list;
    while (tmp) {
        if (tmp->value == value) {
            free(label);
            return (ENUM_SE_ALREADY_THERE);
        }
        lastnode = tmp;
        tmp = tmp->next;
    }

    if (lastnode) {
        lastnode->next = TOOLS_MALLOC_STRUCT(Enum_EnumList_s);
        lastnode = lastnode->next;
    } else {
        (*list) = TOOLS_MALLOC_STRUCT(Enum_EnumList_s);
        lastnode = (*list);
    }
    if (!lastnode) {
        free(label);
        return (ENUM_SE_NOMEM);
    }
    lastnode->label = label;
    lastnode->value = value;
    lastnode->next = NULL;
    return (ENUM_SE_OK);
}

int Enum_seAddPair(unsigned int major, unsigned int minor, char *label, int value)
{
    struct Enum_EnumList_s *list = Enum_seFindList(major, minor);
    int             created = (list) ? 1 : 0;
    int             ret = Enum_seAddPairToList(&list, label, value);
    if (!created)
        Enum_seStoreInList(list, major, minor);
    return ret;
}

/*
 * remember a list of enums based on a lookup name.
 */
static struct Enum_EnumList_s ** _Enum_seFindSlistPtr(const char *listname)
{
    struct Enum_EnumListStr_s *sptr;
    if (!listname)
        return NULL;

    for (sptr = _enum_sliststorage; sptr != NULL; sptr = sptr->next)
        if (sptr->name && strcmp(sptr->name, listname) == 0)
            return &sptr->list;

    return NULL;
}

struct Enum_EnumList_s * Enum_seFindSlist(const char *listname)
{
    struct Enum_EnumList_s **ptr = _Enum_seFindSlistPtr(listname);
    return ptr ? *ptr : NULL;
}

char * Enum_seFindLabelInSlist(const char *listname, int value)
{
    return (Enum_seFindLabelInList(Enum_seFindSlist(listname), value));
}

int Enum_seFindValueInSlist(const char *listname, const char *label)
{
    return (Enum_seFindValueInList(Enum_seFindSlist(listname), label));
}

int  Enum_seFindFreeValueInSlist(const char *listname)
{
    return ( Enum_seFindFreeValueInList( Enum_seFindSlist(listname)));
}

int Enum_seAddPairToSlist(const char *listname, char *label, int value)
{
    struct Enum_EnumList_s *list = Enum_seFindSlist(listname);
    int             created = (list) ? 1 : 0;
    int             ret = Enum_seAddPairToList(&list, label, value);

    if (!created) {
        struct Enum_EnumListStr_s *sptr =
            TOOLS_MALLOC_STRUCT(Enum_EnumListStr_s);
        if (!sptr) {
            _Enum_freeEnumList(list);
            return ENUM_SE_NOMEM;
        }
        sptr->next = _enum_sliststorage;
        sptr->name = strdup(listname);
        sptr->list = list;
        _enum_sliststorage = sptr;
    }
    return ret;
}

static void _Enum_freeEnumList(struct Enum_EnumList_s *list)
{
    struct Enum_EnumList_s *next;

    while (list) {
        next = list->next;
        TOOLS_FREE(list->label);
        TOOLS_FREE(list);
        list = next;
    }
}

void Enum_clearEnum(void)
{
    struct Enum_EnumListStr_s *sptr = _enum_sliststorage, *next = NULL;
    int i, j;

    while (sptr != NULL) {
    next = sptr->next;
    _Enum_freeEnumList(sptr->list);
    TOOLS_FREE(sptr->name);
    TOOLS_FREE(sptr);
    sptr = next;
    }
    _enum_sliststorage = NULL;

    if (_enum_enumLists) {
        for (i = 0; i < ENUM_SE_MAX_IDS; i++) {
            if (_enum_enumLists[i]) {
                for (j = 0; j < ENUM_SE_MAX_SUBIDS; j++) {
                    if (_enum_enumLists[i][j])
                        _Enum_freeEnumList(_enum_enumLists[i][j]);
                }
                TOOLS_FREE(_enum_enumLists[i]);
            }
        }
        TOOLS_FREE(_enum_enumLists);
    }
}

void Enum_seClearList(struct Enum_EnumList_s **list)
{
    struct Enum_EnumList_s *this_entry, *next_entry;

    if (!list)
        return;

    this_entry = *list;
    while (this_entry) {
        next_entry = this_entry->next;
        TOOLS_FREE(this_entry->label);
        TOOLS_FREE(this_entry);
        this_entry = next_entry;
    }
    *list = NULL;
    return;
}

void Enum_seStoreSlist(const char *listname, const char *type)
{
    struct Enum_EnumList_s *list = Enum_seFindSlist(listname);
    Enum_seStoreEnumList(list, listname, type);
}

int Enum_seStoreSlistCallback(int majorID, int minorID,
                        void *serverargs, void *clientargs)
{
    char *appname = DefaultStore_getString(DsStorage_LIBRARY_ID,
                                          DsStr_APPTYPE);
    Enum_seStoreSlist((char *)clientargs, appname);
    return ErrorCode_SUCCESS;
}

void Enum_seClearSlist(const char *listname)
{
    Enum_seClearList(_Enum_seFindSlistPtr(listname));
}

void Enum_seClearAlllists(void)
{
    struct Enum_EnumListStr_s *sptr = NULL;

    for (sptr = _enum_sliststorage; sptr != NULL; sptr = sptr->next)
        Enum_seClearList(&(sptr->list));
}
