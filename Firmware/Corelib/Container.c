#include "Container.h"
#include "Factory.h"
#include "Debug.h"
#include "Logger.h"
#include "Tools.h"
#include "ContainerBinaryArray.h"
#include "ContainerListSsll.h"
#include "ContainerNull.h"
#include "Api.h"
#include "Assert.h"



/** @defgroup container container
 */

/*------------------------------------------------------------------
 */
static Container_Container * _container_containers = NULL;

typedef struct Container_Type_s {
   const char             *name;
   Factory_Factory        *factory;
   Container_FuncCompare  *compare;
} Container_Type;


/*------------------------------------------------------------------
 */
static void _Container_factoryFree(void *dat, void *context)
{
    Container_Type *data = (Container_Type *)dat;
    if (data == NULL)
        return;

    if (data->name != NULL) {
        DEBUG_MSGTL(("container", "  Container_factoryFree() called for %s\n", data->name));
        free(TOOLS_REMOVE_CONST(void *, data->name)); /* TOOLS_FREE wasted on object about to be freed */
    }

    free(data); /* SNMP_FREE wasted on param */
}

/*------------------------------------------------------------------
 */

/*------------------------------------------------------------------
 */
void Container_initList(void)
{
    if (NULL != _container_containers)
        return;

    /*
     * create a binary arry container to hold container
     * factories
     */
    _container_containers = ContainerBinaryArray_getBinaryArray();
    _container_containers->compare = Container_compareCstring;
    _container_containers->containerName = strdup("containerList");

    /*
     * register containers
     */
    ContainerBinaryArray_init();
    ContainerListSsll_init();
    ContainerNull_init();
    /*
     * default aliases for some containers
     */
    Container_register("tableContainer", Container_getFactory("binaryArray"));

    Container_register("linkedList",  Container_getFactory("sortedSinglyLinkedList"));

    Container_register("ssllContainer", Container_getFactory("sortedSinglyLinkedList"));

    Container_registerWithCompare  ("cstring", Container_getFactory("binaryArray"), Container_compareDirectCstring);

    Container_registerWithCompare ("string", Container_getFactory("binaryArray"), Container_compareCstring);

    Container_registerWithCompare ("stringBinaryArray", Container_getFactory("binaryArray"), Container_compareCstring);
}

void Container_freeList(void)
{
    DEBUG_MSGTL(("container", "Container_freeList() called\n"));

    if (_container_containers == NULL)
        return;

    /*
     * free memory used by each factory entry
     */
    CONTAINER_FOR_EACH(_container_containers, ((Container_FuncObjFunc *)_Container_factoryFree), NULL);

    /*
     * free factory container
     */
    CONTAINER_FREE(_container_containers);

    _container_containers = NULL;
}

int Container_registerWithCompare(const char* name, Factory_Factory *f,  Container_FuncCompare  *c)
{
    Container_Type *ct, tmp;

    if (NULL == _container_containers)
        return -1;

    tmp.name = TOOLS_REMOVE_CONST(char *, name);
    ct = (Container_Type *)CONTAINER_FIND(_container_containers, &tmp);
    if (NULL!=ct) {
        DEBUG_MSGT(("containerRegistry",
                   "replacing previous container factory\n"));
        ct->factory = f;
    }else {
        ct = TOOLS_MALLOC_TYPEDEF(Container_Type);
        if (NULL == ct)
            return -1;
        ct->name = strdup(name);
        ct->factory = f;
        ct->compare = c;
        CONTAINER_INSERT(_container_containers, ct);
    }
    DEBUG_MSGT(("containerRegistry", "registered container factory %s (%s)\n",
               ct->name, f->product));

    return 0;
}

int Container_register(const char * name, Factory_Factory *f)
{
    return Container_registerWithCompare(name, f, NULL);
}

/*------------------------------------------------------------------
 */
Factory_Factory * Container_getFactory(const char *type)
{
    Container_Type ct, *found;

    if (NULL == _container_containers)
        return NULL;

    ct.name = type;
    found = (Container_Type *)CONTAINER_FIND(_container_containers, &ct);

    return found ? found->factory : NULL;
}

Factory_Factory * Container_findFactory(const char *typeList)
{
    Factory_Factory   *f = NULL;
    char              *list, *entry;
    char              *st = NULL;

    if (NULL==typeList)
        return NULL;

    list = strdup(typeList);
    entry = strtok_r(list, ":", &st);
    while(entry) {
        f = Container_getFactory(entry);
        if (NULL != f)
            break;
        entry = strtok_r(NULL, ":", &st);
    }

    free(list);
    return f;
}

/*------------------------------------------------------------------
 */
static Container_Type * _Container_getCt(const char *type)
{
    Container_Type ct;

    if (NULL == _container_containers)
        return NULL;

    ct.name = type;
    return (Container_Type *)CONTAINER_FIND(_container_containers, &ct);
}

static Container_Type * _Container_FindCt(const char *type_list)
{
    Container_Type    *ct = NULL;
    char              *list, *entry;
    char              *st = NULL;

    if (NULL==type_list)
        return NULL;

    list = strdup(type_list);
    entry = strtok_r(list, ":", &st);
    while(entry) {
        ct = _Container_getCt(entry);
        if (NULL != ct)
            break;
        entry = strtok_r(NULL, ":", &st);
    }

    free(list);
    return ct;
}



/*------------------------------------------------------------------
 */
Container_Container * Container_get(const char *type)
{
    Container_Container *c;
    Container_Type *ct = _Container_getCt(type);
    if (ct) {
        c = (Container_Container *)(ct->factory->produce());
        if (c && ct->compare)
            c->compare = ct->compare;
        return c;
    }

    return NULL;
}

/*------------------------------------------------------------------
 */
Container_Container * Container_find(const char *type)
{
    Container_Type *ct = _Container_FindCt(type);
    Container_Container *c = ct ? (Container_Container *)(ct->factory->produce()) : NULL;

    /*
     * provide default compare
     */
    if (c) {
        if (ct->compare)
            c->compare = ct->compare;
        else if (NULL == c->compare)
            c->compare = Container_compareIndex;
    }

    return c;
}

/*------------------------------------------------------------------
 */
void Container_addIndex(Container_Container *primary, Container_Container *newIndex)
{
    Container_Container *curr = primary;

    if((NULL == newIndex) || (NULL == primary)) {
        Logger_log(LOGGER_PRIORITY_ERR, "add index called with null pointer\n");
        return;
    }

    while(curr->next)
        curr = curr->next;

    curr->next = newIndex;
    newIndex->prev = curr;
}

/*------------------------------------------------------------------
 * These functions should EXACTLY match the inline version in
 * container.h. If you change one, change them both.
 */
int CONTAINER_INSERT_HELPER(Container_Container* x, const void* k)
{
    while(x && x->insertFilter && x->insertFilter(x,k) == 1)
        x = x->next;
    if(x) {
        int rc = x->insert(x,k);
        if(rc)
            Logger_log(LOGGER_PRIORITY_DEBUG,"error on subcontainer '%s' insert (%d)\n",
                     x->containerName ? x->containerName : "", rc);
        else {
            rc = CONTAINER_INSERT_HELPER(x->next, k);
            if(rc)
                x->remove(x,k);
        }
        return rc;
    }
    return 0;
}

/*------------------------------------------------------------------
 * These functions should EXACTLY match the inline version in
 * container.h. If you change one, change them both.
 */
int CONTAINER_INSERT(Container_Container* x, const void* k)
{
    /** start at first container */
    while(x->prev)
        x = x->prev;
    return CONTAINER_INSERT_HELPER(x, k);
}

/*------------------------------------------------------------------
 * These functions should EXACTLY match the inline version in
 * container.h. If you change one, change them both.
 */
int CONTAINER_REMOVE(Container_Container *x, const void *k)
{
    int rc2, rc = 0;

    /** start at last container */
    while(x->next)
        x = x->next;
    while(x) {
        rc2 = x->remove(x,k);
        /** ignore remove errors if there is a filter in place */
        if ((rc2) && (NULL == x->insertFilter)) {
            Logger_log(LOGGER_PRIORITY_ERR,"error on subcontainer '%s' remove (%d)\n",
                     x->containerName ? x->containerName : "", rc2);
            rc = rc2;
        }
        x = x->prev;

    }
    return rc;
}

/*------------------------------------------------------------------
 * These functions should EXACTLY match the function version in
 * container.c. If you change one, change them both.
 */
Container_Container * CONTAINER_DUP(Container_Container *x, void *ctx, u_int flags)
{
    if (NULL == x->duplicate) {
        Logger_log(LOGGER_PRIORITY_ERR, "container '%s' does not support duplicate\n",
                 x->containerName ? x->containerName : "");
        return NULL;
    }
    return x->duplicate(x, ctx, flags);
}

//------------------------------------------------------------------
// * These functions should EXACTLY match the inline version in
// * container.h. If you change one, change them both.

int CONTAINER_FREE(Container_Container * x){
   int  rc2, rc = 0;

    /** start at last container */
    while(x->next)
        x = x->next;
    while(x) {
        Container_Container *tmp;
        char *name;
        tmp = x->prev;
        name = x->containerName;
        x->containerName = NULL;
        rc2 = x->cfree(x);
        if (rc2) {
            Logger_log(LOGGER_PRIORITY_ERR,"error on subcontainer '%s' cfree (%d)\n",
                     name ? name : "", rc2);
            rc = rc2;
        }
        TOOLS_FREE(name);
        x = tmp;
    }
  return rc;
}

/*------------------------------------------------------------------
 * These functions should EXACTLY match the function version in
 * container.c. If you change one, change them both.
 */
/*
 * clear all containers. When clearing the *first* container, and
 * *only* the first container, call the function f for each item.
 * After calling this function, all containers should be empty.
 */
void CONTAINER_CLEAR(Container_Container *x, Container_FuncObjFunc *f, void *c)
{
    /** start at last container */
    while(x->next)
        x = x->next;
    while(x->prev) {
        x->clear(x, NULL, c);
        x = x->prev;
    }
    x->clear(x, f, c);
}

/*
 * clear all containers. When clearing the *first* container, and
 * *only* the first container, call the free_item function for each item.
 * After calling this function, all containers should be empty.
 */
void CONTAINER_FREE_ALL(Container_Container *x, void *c)
{
    CONTAINER_CLEAR(x, x->freeItem, c);
}

/*------------------------------------------------------------------
 * These functions should EXACTLY match the function version in
 * container.c. If you change one, change them both.
 */
/*
 * Find a sub-container with the given name
 */
Container_Container * CONTAINER_SUBCONTAINER_FIND(Container_Container *x, const char* name)
{
    if ((NULL == x) || (NULL == name))
        return NULL;

    /** start at first container */
    while(x->prev)
        x = x->prev;
    while(x) {
        if ((NULL != x->containerName) && (0 == strcmp(name,x->containerName)))
            break;
        x = x->next;
    }
    return x;
}

/*------------------------------------------------------------------
 */
void Container_init( Container_Container   *c,
                     Container_FuncRc      *init,
                     Container_FuncRc      *cfree,
                     Container_FuncSize    *size,
                     Container_FuncCompare *cmp,
                     Container_FuncOp      *ins,
                     Container_FuncOp      *rem,
                     Container_FuncRtn     *fnd)
{
    if (c == NULL)
        return;

    c->init = init;
    c->cfree = cfree;
    c->getSize = size;
    c->compare = cmp;
    c->insert = ins;
    c->remove = rem;
    c->find = fnd;
    c->freeItem = Container_simpleFree;
}

int Container_dataDup(Container_Container *dup, Container_Container *c)
{
    if (!dup || !c)
        return -1;

    if (c->containerName)
        dup->containerName = strdup(c->containerName);
    dup->compare = c->compare;
    dup->nCompare = c->nCompare;
    dup->release = c->release;
    dup->insertFilter = c->insertFilter;
    dup->freeItem = c->freeItem;
    dup->sync = c->sync;
    dup->flags = c->flags;

    return 0;
}

/*------------------------------------------------------------------
 *
 * simple comparison routines
 *
 */
int Container_compareIndex(const void *lhs, const void *rhs)
{
    int rc;
    Assert_assert((NULL != lhs) && (NULL != rhs));
    DEBUG_IF("compare:index") {
        DEBUG_MSGT(("compare:index", "compare "));
        DEBUG_MSGSUBOID(("compare:index", ((const Types_Index *) lhs)->oids,((const Types_Index *) lhs)->len));
        DEBUG_MSG(("compare:index", " to "));
        DEBUG_MSGSUBOID(("compare:index", ((const Types_Index *) rhs)->oids,((const Types_Index *) rhs)->len));
        DEBUG_MSG(("compare:index", "\n"));
    }
    rc = Api_oidCompare(((const Types_Index *) lhs)->oids,
                          ((const Types_Index *) lhs)->len,
                          ((const Types_Index *) rhs)->oids,
                          ((const Types_Index *) rhs)->len);
    DEBUG_MSGT(("compare:index", "result was %d\n", rc));
    return rc;
}

int Container_nCompareIndex(const void *lhs, const void *rhs)
{
    int rc;

    Assert_assert((NULL != lhs) && (NULL != rhs));

    DEBUG_IF("compare:index") {
        DEBUG_MSGT(("compare:index", "compare "));
        DEBUG_MSGSUBOID(("compare:index", ((const Types_Index *) lhs)->oids,((const Types_Index *) lhs)->len));
        DEBUG_MSG(("compare:index", " to "));
        DEBUG_MSGSUBOID(("compare:index", ((const Types_Index *) rhs)->oids, ((const Types_Index *) rhs)->len));
        DEBUG_MSG(("compare:index", "\n"));
    }
    rc = Api_oidNcompare(((const Types_Index *) lhs)->oids,
                           ((const Types_Index *) lhs)->len,
                           ((const Types_Index *) rhs)->oids,
                           ((const Types_Index *) rhs)->len,
                           ((const Types_Index *) rhs)->len);

    DEBUG_MSGT(("compare:index", "result was %d\n", rc));
    return rc;
}

int Container_compareCstring(const void * lhs, const void * rhs)
{
    return strcmp(((const Container_Type*)lhs)->name,
                  ((const Container_Type*)rhs)->name);
}

int Container_nCompareCstring(const void * lhs, const void * rhs)
{
    return strncmp(((const Container_Type*)lhs)->name,
                   ((const Container_Type*)rhs)->name,
                   strlen(((const Container_Type*)rhs)->name));
}

int Container_compareDirectCstring(const void * lhs, const void * rhs)
{
    return strcmp((const char*)lhs, (const char*)rhs);
}

/*
 * compare two memory buffers
 *
 * since snmp strings aren't NULL terminated, we can't use strcmp. So
 * compare up to the length of the smaller, and then use length to
 * break any ties.
 */
int Container_compareMem(const char * lhs, size_t lhs_len,
                    const char * rhs, size_t rhs_len)
{
    int rc, min = TOOLS_MIN(lhs_len, rhs_len);

    rc = memcmp(lhs, rhs, min);
    if((rc==0) && (lhs_len != rhs_len)) {
        if(lhs_len < rhs_len)
            rc = -1;
        else
            rc = 1;
    }

    return rc;
}

int Container_compareLong(const void * lhs, const void * rhs)
{
    typedef struct { long index; } dummy;

    const dummy *lhd = (const dummy*)lhs;
    const dummy *rhd = (const dummy*)rhs;

    if (lhd->index < rhd->index)
        return -1;
    else if (lhd->index > rhd->index)
        return 1;

    return 0;
}

int Container_compareUlong(const void * lhs, const void * rhs)
{
    typedef struct { u_long index; } dummy;

    const dummy *lhd = (const dummy*)lhs;
    const dummy *rhd = (const dummy*)rhs;

    if (lhd->index < rhd->index)
        return -1;
    else if (lhd->index > rhd->index)
        return 1;

    return 0;
}

int Container_compareInt32(const void * lhs, const void * rhs)
{
    typedef struct { int32_t index; } dummy;

    const dummy *lhd = (const dummy*)lhs;
    const dummy *rhd = (const dummy*)rhs;

    if (lhd->index < rhd->index)
        return -1;
    else if (lhd->index > rhd->index)
        return 1;

    return 0;
}

int Container_compareUint32(const void * lhs, const void * rhs)
{
    typedef struct { uint32_t index; } dummy;

    const dummy *lhd = (const dummy*)lhs;
    const dummy *rhd = (const dummy*)rhs;

    if (lhd->index < rhd->index)
        return -1;
    else if (lhd->index > rhd->index)
        return 1;

    return 0;
}

/*------------------------------------------------------------------
 * Container_simpleFree
 *
 * useful function to pass to CONTAINER_FOR_EACH, when a simple
 * free is needed for every item.
 */
void Container_simpleFree(void *data, void *context)
{
    if (data == NULL)
    return;

    DEBUG_MSGTL(("verbose:container", "Container_simpleFree) called for %p/%p\n", data, context));
    free((void*)data); /* TOOLS_FREE wasted on param */
}
