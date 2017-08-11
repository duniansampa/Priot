#ifndef ENUM_H
#define ENUM_H

struct Enum_EnumList_s {
    struct Enum_EnumList_s *next;
    int             value;
    char           *label;
};

#define ENUM_SE_MAX_IDS 5
#define ENUM_SE_MAX_SUBIDS 32        /* needs to be a multiple of 8 */

/*
 * begin storage definitions
 */
/*
 * These definitions correspond with the "storid" argument to the API
 */
#define ENUM_SE_LIBRARY_ID     0
#define ENUM_SE_MIB_ID         1
#define ENUM_SE_APPLICATION_ID 2
#define ENUM_SE_ASSIGNED_ID    3

/*
 * library specific enum locations
 */

/*
 * error codes
 */
#define ENUM_SE_OK            0
#define ENUM_SE_NOMEM         1
#define ENUM_SE_ALREADY_THERE 2
#define ENUM_SE_DNE           -2

int  Enum_initEnum(const char *type);

struct Enum_EnumList_s * Enum_seFindList(unsigned int major,
                                    unsigned int minor);

struct Enum_EnumList_s * Enum_seFindSlist(const char *listname);

int  Enum_seStoreInList(struct Enum_EnumList_s *,
                           unsigned int major, unsigned int minor);

int  Enum_seFindValue(unsigned int major, unsigned int minor,
                              const char *label);

int  Enum_seFindFreeValue(unsigned int major, unsigned int minor);
char * Enum_seFindLabel(unsigned int major, unsigned int minor,
                              int value);
/**
 * Add the pair (label, value) to the list (major, minor). Transfers
 * ownership of the memory pointed to by label to the list:
 * Enum_clearEnum() deallocates that memory.
 */
int Enum_seAddPair(unsigned int major, unsigned int minor,
                            char *label, int value);

/*
 * finds a list of enums in a list of enum structs associated by a name.
 */
/*
 * find a list, and then operate on that list
 *   ( direct methods further below if you already have the list pointer)
 */

char * Enum_seFindLabelInSlist(const char *listname,
                                       int value);

int  Enum_seFindValueInSlist(const char *listname,
                                       const char *label);

int  Enum_seFindFreeValueInSlist(const char *listname);
/**
 * Add the pair (label, value) to the slist with name listname. Transfers
 * ownership of the memory pointed to by label to the list:
 * Enum_clearEnum() deallocates that memory.
 */

int  Enum_seAddPairToSlist(const char *listname, char *label,
                                     int value);

/*
 * operates directly on a possibly external list
 */
char * Enum_seFindLabelInList(struct Enum_EnumList_s *list,
                                      int value);

int   Enum_seFindValueInList(struct Enum_EnumList_s *list,
                                      const char *label);

int   Enum_seFindFreeValueInList(struct Enum_EnumList_s *list);

int   Enum_seAddPairToList(struct Enum_EnumList_s **list,
                                    char *label, int value);

/*
 * Persistent enumeration lists
 */
void Enum_seStoreEnumList(struct Enum_EnumList_s *new_list,
                                   const char *token, const char *type);

void  Enum_seStoreList(unsigned int major, unsigned int minor,
                              const char *type);

void  Enum_seClearSlist(const char *listname);

void  Enum_seStoreSlist(const char *listname, const char *type);

int   Enum_seStoreSlistCallback(int majorID, int minorID,
                                       void *serverargs, void *clientargs);

void  Enum_seReadConf(const char *word, char *cptr);

/**
 * Deallocate the memory allocated by Enum_initEnum(): remove all key/value
 * pairs stored by Enum_seAdd_*() calls.
 */

void  Enum_clearEnum(void);

#endif // ENUM_H
