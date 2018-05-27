#include "ContainerBinaryArray.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"
#include "System/Util/Trace.h"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>
#include <string.h>
#include "System/Util/Assert.h"


typedef struct ContainerBinaryArray_Table_s {
    size_t                     maxSize;   /* Size of the current data table */
    size_t                     count;      /* Index of the next free entry */
    int                        dirty;
    void                     **data;       /* The table itself */
} ContainerBinaryArray_Table;

typedef struct ContainerBinaryArray_Iterator_s {
    Container_Iterator base;
    size_t           pos;
} ContainerBinaryArray_Iterator;

static Container_Iterator * _ContainerBinaryArray_baIteratorGet(Container_Container *c);

/**********************************************************************
 *
 *
 *
 */
static void _ContainerBinaryArray_arrayQSort(void **data, int first, int last, Container_FuncCompare *f)
{
    int i, j;
    void *mid, *tmp;

    i = first;
    j = last;
    mid = data[(first+last)/2];

    do {
        while (i < last && (*f)(data[i], mid) < 0)
            ++i;
        while (j > first && (*f)(mid, data[j]) < 0)
            --j;

        if(i < j) {
            tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
            ++i;
            --j;
        }
        else if (i == j) {
            ++i;
            --j;
            break;
        }
    } while(i <= j);

    if (j > first)
        _ContainerBinaryArray_arrayQSort(data, first, j, f);

    if (i < last)
        _ContainerBinaryArray_arrayQSort(data, i, last, f);
}

static int _ContainerBinaryArray_sortArray(Container_Container *c)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table*)c->containerData;
    Assert_assert(t!=NULL);
    Assert_assert(c->compare!=NULL);

    if (c->flags & CONTAINER_KEY_UNSORTED)
        return 0;

    if (t->dirty) {
        /*
         * Sort the table
         */
        if (t->count > 1)
            _ContainerBinaryArray_arrayQSort(t->data, 0, t->count - 1, c->compare);
        t->dirty = 0;

        /*
         * no way to know if it actually changed... just assume so.
         */
        ++c->sync;
    }

    return 1;
}

static int _ContainerBinaryArray_linearSearch(const void *val, Container_Container *c)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table*)c->containerData;
    size_t             pos = 0;

    if (!t->count)
        return -1;

    if (! (c->flags & CONTAINER_KEY_UNSORTED)) {
        Logger_log(LOGGER_PRIORITY_ERR, "linear search on sorted container %s?!?\n", c->containerName);
        return -1;
    }

    for (; pos < t->count; ++pos) {
        if (c->compare(t->data[pos], val) == 0)
            break;
    }

    if (pos >= t->count)
        return -1;

    return pos;
}

static int _ContainerBinaryArray_binarySearch(const void *val, Container_Container *c, int exact)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table * )c->containerData;
    size_t             len = t->count;
    size_t             half;
    size_t             middle = 0;
    size_t             first = 0;
    int                result = 0;

    if (!len)
        return -1;

    if (c->flags & CONTAINER_KEY_UNSORTED) {
        if (!exact) {
            Logger_log(LOGGER_PRIORITY_ERR, "non-exact search on unsorted container %s?!?\n", c->containerName);
            return -1;
        }
        return _ContainerBinaryArray_linearSearch(val, c);
    }

    if (t->dirty)
        _ContainerBinaryArray_sortArray(c);

    while (len > 0) {
        half = len >> 1;
        middle = first;
        middle += half;
        if ((result =
             c->compare(t->data[middle], val)) < 0) {
            first = middle;
            ++first;
            len = len - half - 1;
        } else {
            if(result == 0) {
                first = middle;
                break;
            }
            len = half;
        }
    }

    if (first >= t->count)
        return -1;

    if(first != middle) {
        /* last compare wasn't against first, so get actual result */
        result = c->compare(t->data[first], val);
    }

    if(result == 0) {
        if (!exact) {
            if (++first == t->count)
               first = -1;
        }
    }
    else {
        if(exact)
            first = -1;
    }

    return first;
}

static inline ContainerBinaryArray_Table * _ContainerBinaryArray_initialize(void)
{
    ContainerBinaryArray_Table *t;

    t = MEMORY_MALLOC_TYPEDEF(ContainerBinaryArray_Table);
    if (t == NULL)
        return NULL;

    t->maxSize = 0;
    t->count = 0;
    t->dirty = 0;
    t->data = NULL;

    return t;
}

void ContainerBinaryArray_release(Container_Container *c)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table*)c->containerData;
    MEMORY_FREE(t->data);
    MEMORY_FREE(t);
    MEMORY_FREE(c);
}

int ContainerBinaryArray_optionsSet(Container_Container *c, int set, u_int flags)
{
#define BA_FLAGS (CONTAINER_KEY_ALLOW_DUPLICATES|CONTAINER_KEY_UNSORTED)

    if (set) {
        if ((flags & BA_FLAGS) == flags)
            c->flags = flags;
        else
            flags = (u_int)-1; /* unsupported flag */
    }
    else
        return ((c->flags & flags) == flags);
    return flags;
}

static inline size_t _ContainerBinaryArray_count(Container_Container *c)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table*)c->containerData;
    /*
     * return count
     */
    return t ? t->count : 0;
}

static inline void * _ContainerBinaryArray_get2(Container_Container *c, const void *key, int exact)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table*)c->containerData;
    int             index = 0;

    /*
     * if there is no data, return NULL;
     */
    if (!t->count)
        return NULL;

    /*
     * if the table is dirty, sort it.
     */
    if (t->dirty)
        _ContainerBinaryArray_sortArray(c);

    /*
     * if there is a key, search. Otherwise default is 0;
     */
    if (key) {
        if ((index = _ContainerBinaryArray_binarySearch(key, c, exact)) == -1)
            return NULL;
        if (!exact && c->flags & CONTAINER_KEY_ALLOW_DUPLICATES) {
            int result;

            /*
             * If duplicates are allowed, we have to be extra
             * sure that we didn't just increment to a duplicate,
             * thus causing a getnext loop.
             */
            result = c->compare(t->data[index], key);
            while (result == 0) {
                DEBUG_MSGTL(("container","skipping duplicate key in %s\n", c->containerName));
                if (++index == t->count)
                   return NULL;
                result = c->compare(t->data[index], key);
            }
        }
    }

    return t->data[index];
}

int ContainerBinaryArray_removeAt(Container_Container *c, size_t index, void **save)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table*)c->containerData;

    if (save)
        *save = NULL;

    /*
     * if there is no data, return NULL;
     */
    if (!t->count)
        return 0;

    /*
     * find old data and save it, if ptr provided
     */
    if (save)
        *save = t->data[index];

    /*
     * if entry was last item, just decrement count
     */
    --t->count;
    if (index != t->count) {
        /*
         * otherwise, shift array down
         */
        memmove(&t->data[index], &t->data[index+1],
                sizeof(void*) * (t->count - index));

        ++c->sync;
    }

    return 0;
}

int ContainerBinaryArray_remove(Container_Container *c, const void *key, void **save)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table*)c->containerData;
    int                index = 0;

    if (save)
        *save = NULL;

    /*
     * if there is no data, return NULL;
     */
    if (!t->count)
        return 0;

    /*
     * if the table is dirty, sort it.
     */
    if (t->dirty)
        _ContainerBinaryArray_sortArray(c);

    /*
     * search
     */
    if ((index = _ContainerBinaryArray_binarySearch(key, c, 1)) == -1)
        return -1;

    return ContainerBinaryArray_removeAt(c, (size_t)index, save);
}

static inline void _ContainerBinaryArray_forEach(Container_Container *c,
                                  Container_FuncObjFunc *fe,
                                  void *context, int sort)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table*)c->containerData;
    size_t             i;

    if (sort && t->dirty)
        _ContainerBinaryArray_sortArray(c);

    for (i = 0; i < t->count; ++i)
        (*fe) (t->data[i], context);
}

static inline  void _ContainerBinaryArray_clear(Container_Container *c,
                                Container_FuncObjFunc *fe,
                                void *context)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table*)c->containerData;

    if( NULL != fe ) {
        size_t             i;

        for (i = 0; i < t->count; ++i)
            (*fe) (t->data[i], context);
    }

    t->count = 0;
    t->dirty = 0;
    ++c->sync;
}

static inline int _ContainerBinaryArray_insert(Container_Container *c, const void *entry)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table * ) c->containerData;
    int             was_dirty = 0;
    /*
     * check for duplicates
     */
    if (! (c->flags & CONTAINER_KEY_ALLOW_DUPLICATES)) {
        was_dirty = t->dirty;
        if (NULL != _ContainerBinaryArray_get2(c, entry, 1)) {
            DEBUG_MSGTL(("container","not inserting duplicate key\n"));
            return -1;
        }
    }

    /*
     * check if we need to resize the array
     */
    if (t->maxSize <= t->count) {
        /*
         * Table is full, so extend it to double the size, or use 10 elements
         * if it is empty.
         */
        size_t const new_max = t->maxSize > 0 ? 2 * t->maxSize : 10;
        void ** const new_data =
            (void**) realloc(t->data, new_max * sizeof(void*));

        if (new_data == NULL)
            return -1;

        memset(new_data + t->maxSize, 0x0,
               (new_max - t->maxSize) * sizeof(void*));

        t->data = new_data;
        t->maxSize = new_max;
    }

    /*
     * Insert the new entry into the data array
     */
    t->data[t->count++] = UTILITIES_REMOVE_CONST(void *, entry);
    t->dirty = 1;

    /*
     * if array was dirty before we called get, sync was incremented when
     * get called SortArray. If we didn't call get or the array wasn't dirty,
     * bump sync now.
     */
    if (!was_dirty)
        ++c->sync;

    return 0;
}

/**********************************************************************
 *
 * Special case support for subsets
 *
 */
static int _ContainerBinaryArray_binarySearchForStart(Types_Index *val, Container_Container *c)
{
    ContainerBinaryArray_Table *t = (ContainerBinaryArray_Table*)c->containerData;
    size_t             len = t->count;
    size_t             half;
    size_t             middle;
    size_t             first = 0;
    int                result = 0;

    if (!len)
        return -1;

    if (t->dirty)
        _ContainerBinaryArray_sortArray(c);

    while (len > 0) {
        half = len >> 1;
        middle = first;
        middle += half;
        if ((result = c->nCompare(t->data[middle], val)) < 0) {
            first = middle;
            ++first;
            len = len - half - 1;
        } else
            len = half;
    }

    if ((first >= t->count) ||
        c->nCompare(t->data[first], val) != 0)
        return -1;

    return first;
}

void  ** ContainerBinaryArray_getSubset(Container_Container *c, void *key, int *len)
{
    ContainerBinaryArray_Table *t;
    void          **subset;
    int             start, end;
    size_t          i;

    /*
     * if there is no data, return NULL;
     */
    if (!c || !key)
        return NULL;

    t = (ContainerBinaryArray_Table*)c->containerData;
    Assert_assert(c->nCompare);
    if (!t->count | !c->nCompare)
        return NULL;

    /*
     * if the table is dirty, sort it.
     */
    if (t->dirty)
        _ContainerBinaryArray_sortArray(c);

    /*
     * find matching items
     */
    start = end = _ContainerBinaryArray_binarySearchForStart((Types_Index *)key, c);
    if (start == -1)
        return NULL;

    for (i = start + 1; i < t->count; ++i) {
        if (0 != c->nCompare(t->data[i], key))
            break;
        ++end;
    }

    *len = end - start + 1;
    subset = (void **)malloc((*len) * sizeof(void*));
    if (subset)
        memcpy(subset, &t->data[start], sizeof(void*) * (*len));

    return subset;
}

/**********************************************************************
 *
 * container
 *
 */
static void * _ContainerBinaryArray_baFind(Container_Container *container, const void *data)
{
    return _ContainerBinaryArray_get2(container, data, 1);
}

static void * _ContainerBinaryArray_baFindNext(Container_Container *container, const void *data)
{
    return _ContainerBinaryArray_get2(container, data, 0);
}

static int _ContainerBinaryArray_baInsert(Container_Container *container, const void *data)
{
    return _ContainerBinaryArray_insert(container, data);
}

static int _ContainerBinaryArray_baRemove(Container_Container *container, const void *data)
{
    return ContainerBinaryArray_remove(container,data, NULL);
}

static int _ContainerBinaryArray_baFree(Container_Container *container)
{
    ContainerBinaryArray_release(container);

    return 0;
}

static size_t _ContainerBinaryArray_baSize(Container_Container *container)
{
    return _ContainerBinaryArray_count(container);
}

static void _ContainerBinaryArray_baForEach(Container_Container *container, Container_FuncObjFunc *f,
             void *context)
{
    _ContainerBinaryArray_forEach(container, f, context, 1);
}

static void _ContainerBinaryArray_baClear(Container_Container *container, Container_FuncObjFunc *f,
             void *context)
{
    _ContainerBinaryArray_clear(container, f, context);
}

static Types_VoidArray * _ContainerBinaryArray_baGetSubset(Container_Container *container, void *data)
{
    Types_VoidArray * va;
    void ** rtn;
    int len;

    rtn = ContainerBinaryArray_getSubset(container, data, &len);
    if ((NULL==rtn) || (len <=0))
        return NULL;

    va = MEMORY_MALLOC_TYPEDEF(Types_VoidArray);
    if (NULL==va)
    {
        free (rtn);
        return NULL;
    }

    va->size = len;
    va->array = rtn;

    return va;
}

static int _ContainerBinaryArray_baOptions(Container_Container *c, int set, u_int flags)
{
    return ContainerBinaryArray_optionsSet(c, set, flags);
}

static Container_Container * _ContainerBinaryArray_baDuplicate(Container_Container *c, void *ctx, u_int flags)
{
    Container_Container *dup;
    ContainerBinaryArray_Table *dupt, *t;

    if (flags) {
         Logger_log(LOGGER_PRIORITY_ERR, "binary arry duplicate does not supprt flags yet\n");
        return NULL;
    }

    dup = ContainerBinaryArray_getBinaryArray();
    if (NULL == dup) {
        Logger_log(LOGGER_PRIORITY_ERR," no memory for binary array duplicate\n");
        return NULL;
    }
    /*
     * deal with container stuff
     */
    if (Container_dataDup(dup, c) != 0) {
        ContainerBinaryArray_release(dup);
        return NULL;
    }

    /*
     * deal with data
     */
    dupt = (ContainerBinaryArray_Table*)dup->containerData;
    t = (ContainerBinaryArray_Table*)c->containerData;

    dupt->maxSize = t->maxSize;
    dupt->count = t->count;
    dupt->dirty = t->dirty;

    /*
     * shallow copy
     */
    dupt->data = (void**) malloc(dupt->maxSize * sizeof(void*));
    if (NULL == dupt->data) {
        Logger_log(LOGGER_PRIORITY_ERR, "no memory for binary array duplicate\n");
        ContainerBinaryArray_release(dup);
        return NULL;
    }

    memcpy(dupt->data, t->data, dupt->maxSize * sizeof(void*));

    return dup;
}

Container_Container * ContainerBinaryArray_getBinaryArray(void)
{
    /*
     * allocate memory
     */
    Container_Container *c = MEMORY_MALLOC_TYPEDEF(Container_Container);
    if (NULL==c) {
        Logger_log(LOGGER_PRIORITY_ERR, "couldn't allocate memory\n");
        return NULL;
    }

    c->containerData = _ContainerBinaryArray_initialize();

    /*
     * NOTE: CHANGES HERE MUST BE DUPLICATED IN duplicate AS WELL!!
     */
    Container_init(c, NULL, _ContainerBinaryArray_baFree, _ContainerBinaryArray_baSize, NULL, _ContainerBinaryArray_baInsert, _ContainerBinaryArray_baRemove, _ContainerBinaryArray_baFind);

    c->findNext = _ContainerBinaryArray_baFindNext;
    c->getSubset = _ContainerBinaryArray_baGetSubset;
    c->getIterator = _ContainerBinaryArray_baIteratorGet;
    c->forEach = _ContainerBinaryArray_baForEach;
    c->clear =_ContainerBinaryArray_baClear;
    c->options = _ContainerBinaryArray_baOptions;
    c->duplicate = _ContainerBinaryArray_baDuplicate;

    return c;
}

Factory_Factory * ContainerBinaryArray_getFactory(void)
{
    static Factory_Factory f = { "binaryArray",
                                (Factory_FuncProduce*) ContainerBinaryArray_getBinaryArray };

    return &f;
}

void ContainerBinaryArray_init(void)
{
    Container_register("binaryArray", ContainerBinaryArray_getFactory());
}

/**********************************************************************
 *
 * iterator
 *
 */
static inline ContainerBinaryArray_Table * _ContainerBinaryArray_baIt2cont(ContainerBinaryArray_Iterator *it)
{
    if(NULL == it) {
        Assert_assert(NULL != it);
        return NULL;
    }
    if(NULL == it->base.container) {
        Assert_assert(NULL != it->base.container);
        return NULL;
    }
    if(NULL == it->base.container->containerData) {
        Assert_assert(NULL != it->base.container->containerData);
        return NULL;
    }

    return (ContainerBinaryArray_Table*)(it->base.container->containerData);
}

static inline void * _ContainerBinaryArray_baIteratorPosition(ContainerBinaryArray_Iterator *it, size_t pos)
{
    ContainerBinaryArray_Table *t = _ContainerBinaryArray_baIt2cont(it);
    if (NULL == t)
        return t; /* msg already logged */

    if(it->base.container->sync != it->base.sync) {
        DEBUG_MSGTL(("container:iterator", "out of sync\n"));
        return NULL;
    }

    if(0 == t->count) {
        DEBUG_MSGTL(("container:iterator", "empty\n"));
        return NULL;
    }
    else if(pos >= t->count) {
        DEBUG_MSGTL(("container:iterator", "end of container\n"));
        return NULL;
    }

    return t->data[ pos ];
}

static void * _ContainerBinaryArray_baIteratorCurr(ContainerBinaryArray_Iterator *it)
{
    if(NULL == it) {
        Assert_assert(NULL != it);
        return NULL;
    }

    return _ContainerBinaryArray_baIteratorPosition(it, it->pos);
}

static void * _ContainerBinaryArray_baIteratorFirst(ContainerBinaryArray_Iterator *it)
{
    return _ContainerBinaryArray_baIteratorPosition(it, 0);
}

static void * _ContainerBinaryArray_baIteratorNext(ContainerBinaryArray_Iterator *it)
{
    if(NULL == it) {
        Assert_assert(NULL != it);
        return NULL;
    }

    ++it->pos;

    return _ContainerBinaryArray_baIteratorPosition(it, it->pos);
}

static void * _ContainerBinaryArray_baIteratorLast(ContainerBinaryArray_Iterator *it)
{
    ContainerBinaryArray_Table* t = _ContainerBinaryArray_baIt2cont(it);
    if(NULL == t) {
        Assert_assert(NULL != t);
        return NULL;
    }

    return _ContainerBinaryArray_baIteratorPosition(it, t->count - 1 );
}

static int _ContainerBinaryArray_baIteratorRemove(ContainerBinaryArray_Iterator *it)
{
    ContainerBinaryArray_Table* t = _ContainerBinaryArray_baIt2cont(it);
    if(NULL == t) {
        Assert_assert(NULL != t);
        return -1;
    }

    /*
     * since this iterator was used for the remove, keep it in sync with
     * the container. Also, back up one so that next will be the position
     * that was just removed.
     */
    ++it->base.sync;
    return ContainerBinaryArray_removeAt(it->base.container, it->pos--, NULL);

}

static int _ContainerBinaryArray_baIteratorReset(ContainerBinaryArray_Iterator *it)
{
    ContainerBinaryArray_Table* t = _ContainerBinaryArray_baIt2cont(it);
    if(NULL == t) {
        Assert_assert(NULL != t);
        return -1;
    }

    if (t->dirty)
        _ContainerBinaryArray_sortArray(it->base.container);

    /*
     * save sync count, to make sure container doesn't change while
     * iterator is in use.
     */
    it->base.sync = it->base.container->sync;

    it->pos = 0;

    return 0;
}

static int _ContainerBinaryArray_baIteratorRelease(Container_Iterator *it)
{
    free(it);

    return 0;
}

static Container_Iterator * _ContainerBinaryArray_baIteratorGet(Container_Container *c)
{
    ContainerBinaryArray_Iterator* it;

    if(NULL == c)
        return NULL;

    it = MEMORY_MALLOC_TYPEDEF(ContainerBinaryArray_Iterator);
    if(NULL == it)
        return NULL;

    it->base.container = c;

    it->base.first   = (Container_FuncIteratorRtn*)  _ContainerBinaryArray_baIteratorFirst;
    it->base.next    = (Container_FuncIteratorRtn*)  _ContainerBinaryArray_baIteratorNext;
    it->base.curr    = (Container_FuncIteratorRtn*)  _ContainerBinaryArray_baIteratorCurr;
    it->base.last    = (Container_FuncIteratorRtn*)  _ContainerBinaryArray_baIteratorLast;
    it->base.remove  = (Container_FuncIteratorRc*)   _ContainerBinaryArray_baIteratorRemove;
    it->base.reset   = (Container_FuncIteratorRc*)   _ContainerBinaryArray_baIteratorReset;
    it->base.release = (Container_FuncIteratorRc*)   _ContainerBinaryArray_baIteratorRelease;

    (void) _ContainerBinaryArray_baIteratorReset(it);

    return (Container_Iterator *)it;
}
