#ifndef DEFAULT_STORE_H
#define DEFAULT_STORE_H

#include "Generals.h"

#include "DataType.h"
#include "DsLib.h"


/**  The purpose of the default storage is three-fold:

       1)     To create a global storage space without creating a
              whole  bunch  of globally accessible variables or a
              whole bunch of access functions to work  with  more
              privately restricted variables.

       2)     To provide a single location where the thread lock-
              ing needs to be implemented. At the  time  of  this
              writing,  however,  thread  locking  is  not yet in
              place.

       3)     To reduce the number of cross dependencies  between
              code  pieces that may or may not be linked together
              in the long run. This provides for a  single  loca-
              tion  in which configuration data, for example, can
              be stored for a separate section of code  that  may
              not be linked in to the application in question.

       The functions defined here implement these goals.

       Currently, three data types are supported: booleans, _i32e-
       gers, and strings. Each of these data types have  separate
       storage  spaces.  In  addition, the storage space for each
       data type is divided further  by  the  application  level.
       Currently,  there  are  two  storage  spaces. The first is
       reserved for  the  PRIOT library  itself.  The  second  is
       _i32ended  for  use  in applications and is not modified or
       checked by the library, and, therefore, this is the  space
       usable by you.
 */

#define DEFAULTSTORE_MAX_IDS            3
#define DEFAULTSTORE_MAX_SUBIDS         48  /* needs to be a multiple of 8 */

#define DEFAULTSTORE_PRIOT_VERSION_3    3   /* real */

/*
 * These definitions correspond with the "which" argument to the API,
 * when the storeid argument is DS_LIBRARY_ID
 */

//Storage
struct DSStorage_s{
    int LIBRARY_ID;
    int APPLICATION_ID;
    int TOKEN_ID;
};

extern const struct DSStorage_s    DSSTORAGE;




int        DefaultStore_setBoolean(int storeid, int which, int value);
int        DefaultStore_toggleBoolean(int storeid, int which);
int        DefaultStore_getBoolean(int storeid, int which);
int        DefaultStore_setInt(int storeid, int which, int value);
int        DefaultStore_getInt(int storeid, int which);
int        DefaultStore_setString(int storeid, int which,  const char *value);
char  *      DefaultStore_getString(int storeid, int which);
int        DefaultStore_setVoid(int storeid, int which, void *value);
void *      DefaultStore_getVoid(int storeid, int which);
int        DefaultStore_parseBoolean(char * line);
void        DefaultStore_handleConfig(const char * token, char * line);
int        DefaultStore_registerConfig(uchar type, const char * storename,  const char * token,  int storeid, int which);
int        DefaultStore_registerPremib(uchar type, const char * storename,  const char * token, int storeid, int which);
void        DefaultStore_shutdown(void);



#endif // DEFAULT_STORE_H
