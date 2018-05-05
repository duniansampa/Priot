#ifndef DATALIST_H
#define DATALIST_H

#include "Generals.h"
#include "Callback.h"


typedef void    (DataList_FuncFree) (void *);
typedef int     (DataList_FuncSave) (char *buf, size_t buf_len, void *);
typedef void *  (DataList_FuncRead) (char *buf, size_t buf_len);

/** @struct netsnmp_data_list_s
* used to iterate through lists of data
*/
typedef struct DataList_DataList_s {
    struct DataList_DataList_s *next;
    char           *name;
   /** The pointer to the data passed on. */
    void           *data;
    /** must know how to free DataList_DataList->data */
    DataList_FuncFree *freeFunc;
} DataList_DataList;

typedef struct DataList_SaveInfo_s {
   DataList_DataList **dataList;
   const char *type;
   const char *token;
   DataList_FuncSave *savePtr;
   DataList_FuncRead *readPtr;
   DataList_FuncFree *freePtr;
} DataList_SaveInfo;


DataList_DataList * DataList_create(const char *, void *, DataList_FuncFree* );

void DataList_addNode(DataList_DataList **head, DataList_DataList *node);

DataList_DataList * DataList_addData(DataList_DataList **head, const char *name, void *data,
                                     DataList_FuncFree * beer);

void * DataList_get(DataList_DataList *head, const char *node);


void DataList_free(DataList_DataList *head);    /* single */

void DataList_freeAll(DataList_DataList *head);        /* multiple */

int DataList_removeNode(DataList_DataList **realHead, const char *name);

DataList_DataList * DataList_getNode(DataList_DataList *head, const char *name);

/** depreciated: use netsnmp_data_list_add_node() */
void  DataList_add(DataList_DataList **head, DataList_DataList *node);


void DataList_registerSave(DataList_DataList **dataList,
                           const char *type, const char *token,
                           DataList_FuncSave *savePtr,
                           DataList_FuncRead *readPtr,
                           DataList_FuncFree *freePtr);

int DataList_saveAll(DataList_DataList *head,
                      const char *type, const char *token,
                      DataList_FuncSave * savePtr);

Callback_CallbackFT DataList_saveAllDataCallback;

void DataList_readDataCallback(const char *token, char *line);

#endif // DATALIST_H
