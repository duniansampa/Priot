#include "DataList.h"

#include "Tools.h"
#include "Debug.h"
#include "Logger.h"
#include "DefaultStore.h"
#include "Priot.h"
#include "ReadConfig.h"
#include "Assert.h"

/** @defgroup data_list generic linked-list data handling with a string as a key.
 * @ingroup library
 * @{
*/

/** frees the data and a name at a given data_list node.
 * Note that this doesn't free the node itself.
 * @param node the node for which the data should be freed
 */


 void DataList_free(DataList_DataList *node)
{
    DataList_FuncFree *beer;
    if (!node)
        return;

    beer = node->freeFunc;
    if (beer)
        (beer) (node->data);
    TOOLS_FREE(node->name);
}

/** frees all data and nodes in a list.
 * @param head the top node of the list to be freed.
 */

void DataList_freeAll(DataList_DataList *head)
{
    DataList_DataList *tmpptr;
    for (; head;) {
        DataList_free(head);
        tmpptr = head;
        head = head->next;
        TOOLS_FREE(tmpptr);
    }
}

/** adds creates a data_list node given a name, data and a free function ptr.
 * @param name the name of the node to cache the data.
 * @param data the data to be stored under that name
 * @param beer A function that can free the data pointer (in the future)
 * @return a newly created data_list node which can be given to the DataList_add function.
 */
DataList_DataList * DataList_create(const char *name, void *data, DataList_FuncFree * beer)
{
    DataList_DataList *node;

    if (!name)
        return NULL;
    node = TOOLS_MALLOC_TYPEDEF(DataList_DataList);
    if (!node)
        return NULL;
    node->name = strdup(name);
    if (!node->name) {
        free(node);
        return NULL;
    }
    node->data = data;
    node->freeFunc = beer;
    return node;
}

/** adds data to a datalist
 * @param head a pointer to the head node of a data_list
 * @param node a node to stash in the data_list
 */
void DataList_addNode(DataList_DataList **head, DataList_DataList *node)
{
    DataList_DataList *ptr;

    Assert_assert(NULL != head);
    Assert_assert(NULL != node);
    Assert_assert(NULL != node->name);

    DEBUG_MSGTL(("data_list","adding key '%s'\n", node->name));

    if (!*head) {
        *head = node;
        return;
    }

    if (0 == strcmp(node->name, (*head)->name)) {
        Assert_assert(!"list key == is unique"); /* always fail */
        Logger_log(LOGGER_PRIORITY_WARNING,
                 "WARNING: adding duplicate key '%s' to data list\n",
                 node->name);
    }

    for (ptr = *head; ptr->next != NULL; ptr = ptr->next) {
        Assert_assert(NULL != ptr->name);
        if (0 == strcmp(node->name, ptr->name)) {
            Assert_assert(!"list key == is unique"); /* always fail */
            Logger_log(LOGGER_PRIORITY_WARNING,
                     "WARNING: adding duplicate key '%s' to data list\n",
                     node->name);
        }
    }

    Assert_assert(NULL != ptr);
    if (ptr)                    /* should always be true */
        ptr->next = node;
}

/** adds data to a datalist
 * @note DataList_addNode is preferred
 * @param head a pointer to the head node of a data_list
 * @param node a node to stash in the data_list
 */
/**  */
void DataList_add(DataList_DataList **head, DataList_DataList *node)
{
    DataList_addNode(head, node);
}

/** adds data to a datalist
 * @param head a pointer to the head node of a data_list
 * @param name the name of the node to cache the data.
 * @param data the data to be stored under that name
 * @param beer A function that can free the data pointer (in the future)
 * @return a newly created data_list node which was inserted in the list
 */
DataList_DataList * DataList_addData(DataList_DataList **head, const char *name,
                                     void *data, DataList_FuncFree * beer)
{
    DataList_DataList *node;
    if (!name) {
        Logger_log(LOGGER_PRIORITY_ERR,"no name provided.");
        return NULL;
    }
    node = DataList_create(name, data, beer);
    if(NULL == node) {
        Logger_log(LOGGER_PRIORITY_ERR,"could not allocate memory for node.");
        return NULL;
    }

    DataList_add(head, node);

    return node;
}

/** returns a data_list node's data for a given name within a data_list
 * @param head the head node of a data_list
 * @param name the name to find
 * @return a pointer to the data cached at that node
 */
void * DataList_get(DataList_DataList *head, const char *name)
{
    if (!name)
        return NULL;
    for (; head; head = head->next)
        if (head->name && strcmp(head->name, name) == 0)
            break;
    if (head)
        return head->data;
    return NULL;
}

/** returns a data_list node for a given name within a data_list
 * @param head the head node of a data_list
 * @param name the name to find
 * @return a pointer to the data_list node
 */
DataList_DataList * DataList_getNode(DataList_DataList *head, const char *name)
{
    if (!name)
        return NULL;
    for (; head; head = head->next)
        if (head->name && strcmp(head->name, name) == 0)
            break;
    if (head)
        return head;
    return NULL;
}

/** Removes a named node from a data_list (and frees it)
 * @param realhead a pointer to the head node of a data_list
 * @param name the name to find and remove
 * @return 0 on successful find-and-delete, 1 otherwise.
 */
int DataList_removeNode(DataList_DataList **realhead, const char *name)
{
    DataList_DataList *head, *prev;
    if (!name)
        return 1;
    for (head = *realhead, prev = NULL; head;
         prev = head, head = head->next) {
        if (head->name && strcmp(head->name, name) == 0) {
            if (prev)
                prev->next = head->next;
            else
                *realhead = head->next;
            DataList_free(head);
            free(head);
            return 0;
        }
    }
    return 1;
}

/** used to store registered save/parse handlers (specifically, parsing info) */
static DataList_DataList * dataList_saveHead;

/** registers to store a data_list set of data at persistent storage time
 *
 * @param datalist the data to be saved
 * @param type the name of the application to save the data as.  If left NULL the default application name that was registered during the init_snmp call will be used (recommended).
 * @param token the unique token identifier string to use as the first word in the persistent file line.
 * @param savePtr a function pointer which will be called to save the rest of the data to a buffer.
 * @param readPtr a function pointer which can read the remainder of a saved line and return the application specific void * pointer.
 * @param freePtr a function pointer which will be passed to the data node for freeing it in the future when/if the list/node is cleaned up or destroyed.
 */

void DataList_registerSave(DataList_DataList **dataList,
                           const char *type, const char *token,
                           DataList_FuncSave *savePtr,
                           DataList_FuncRead *readPtr,
                           DataList_FuncFree *freePtr)
{
    DataList_SaveInfo *info;

    if (!savePtr && !readPtr)
        return;

    info = TOOLS_MALLOC_TYPEDEF(DataList_SaveInfo);

    if (!info) {
        Logger_log(LOGGER_PRIORITY_ERR, "couldn't malloc a DataList_SaveInfo typedef");
        return;
    }

    info->dataList = dataList;
    info->token = token;
    info->type = type;
    if (!info->type) {
        info->type = DefaultStore_getString(DEFAULTSTORE_STORAGE::LIBRARY_ID, DEFAULTSTORE_LIB_STRING::APPTYPE);
    }

    /* function which will save the data */
    info->savePtr = savePtr;
    if (savePtr)
        Callback_registerCallback(CALLBACK_LIBRARY, CALLBACK_STORE_DATA,
                               DataList_saveAllDataCallback, info);

    /* function which will read the data back in */
    info->readPtr = readPtr;
    if (readPtr) {
        /** @todo netsnmp_register_save_list should handle the same token name being saved from different types? */
        DataList_add(&dataList_saveHead, DataList_create(token, info, NULL));

        ReadConfig_registerConfigHandler(type, token, DataList_readDataCallback,
                                NULL /* XXX */, NULL);
    }

    info->freePtr = freePtr;
}


/** intended to be registerd as a callback operation.
 * It should be registered using:
 *
 * snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA, netsnmp_save_all_data_callback, INFO_POINTER);
 *
 * where INFO_POINTER is a pointer to a DataList_SaveInfo object containing apporpriate registration information
 */
int DataList_saveAllDataCallback(int major, int minor, void *serverarg, void *clientarg) {

    DataList_SaveInfo *info = (DataList_SaveInfo *)clientarg;

    if (!clientarg) {
        Logger_log(LOGGER_PRIORITY_WARNING, "DataList_saveAllDataCallback called with no passed data");
        return PRIOT_ERR_NOERROR;
    }

    DataList_saveAll(*(info->dataList), info->type, info->token, info->savePtr);

    return PRIOT_ERR_NOERROR;
}

/** intended to be called as a callback during persistent save operations.
 * See the DataList_saveAllDataCallback for where this is typically used. */
int DataList_saveAll(DataList_DataList *head,
                      const char *type, const char *token,
                      DataList_FuncSave * savePtr)
{
    char buf[TOOLS_MAXBUF], *cp;

    for (; head; head = head->next) {
        if (head->name) {
            /* save begining of line */
            snprintf(buf, sizeof(buf), "%s ", token);
            cp = buf + strlen(buf);
            cp = ReadConfig_saveOctetString(cp, (u_char*)head->name, strlen(head->name));
            *cp++ = ' ';

            /* call registered function to save the rest */
            if (!(savePtr)(cp, sizeof(buf) - strlen(buf), head->data)) {
                ReadConfig_store(type, buf);
            }
        }
    }
    return PRIOT_ERR_NOERROR;
}

/** intended to be registerd as a .conf parser
 * It should be registered using:
 *
 * register_app_config_handler("token", netsnmp_read_data_callback, XXX)
 *
 * where INFO_POINTER is a pointer to a netsnmp_data_list_saveinfo object
 * containing apporpriate registration information
 * @todo make netsnmp_read_data_callback deal with a free routine
 */
void DataList_readDataCallback(const char *token, char *line) {
    DataList_SaveInfo *info;
    char *dataname = NULL;
    size_t dataname_len;
    void *data = NULL;

    /* find the stashed information about what we're parsing */
    info = (DataList_SaveInfo *) DataList_get(dataList_saveHead, token);
    if (!info) {
        Logger_log(LOGGER_PRIORITY_WARNING, "DataList_readDataCallback called without previously registered subparser");
        return;
    }

    /* read in the token */
    line =
        ReadConfig_readData(ASN01_OCTET_STR, line,
                              &dataname, &dataname_len);

    if (!line || !dataname)
        return;

    /* call the sub-parser to read the rest */
    data = (info->readPtr)(line, strlen(line));

    if (!data) {
        free(dataname);
        return;
    }

    /* add to the datalist */
    DataList_add(info->dataList, DataList_create(dataname, data, info->freePtr));

    return;
}
