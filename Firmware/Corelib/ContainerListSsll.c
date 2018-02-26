#include "ContainerListSsll.h"
#include "Tools.h"
#include "Logger.h"
#include "Debug.h"
#include "Assert.h"

typedef struct ContainerListSsll_SlNode_s {
   void           *data;
   struct ContainerListSsll_SlNode_s *next;
} ContainerListSsll_SlNode;

typedef struct ContainerListSsll_SlContainer_s {
   Container_Container          c;

   size_t                     count;      /* Index of the next free entry */
   ContainerListSsll_SlNode  *head;       /* head of list */

   int                        unsorted;   /* unsorted list? */
   int                        fifo;       /* lifo or fifo? */

} ContainerListSsll_SlContainer;

typedef struct ContainerListSsll_SsllIterator_s {
    Container_Iterator base;

    ContainerListSsll_SlNode *pos;
    ContainerListSsll_SlNode *last;
} ContainerListSsll_SsllIterator;

static Container_Iterator * _ContainerListSsll_iteratorGet(Container_Container *c);


static void * _ContainerListSsll_get(Container_Container *c, const void *key, int exact)
{
    ContainerListSsll_SlContainer *sl = (ContainerListSsll_SlContainer*)c;
    ContainerListSsll_SlNode  *curr = sl->head;
    int rc = 0;

    /*
     * note: get-next on unsorted list is meaningless. we
     * don't try to search whole array, looking for the next highest.
     */
    if( (NULL != curr) && (NULL != key)) {
        while (curr) {
            rc = sl->c.compare(curr->data, key);
            if (rc == 0)
                break;
            else if (rc > 0) {
                if (0 == sl->unsorted) {
                    /*
                     * if sorted, we can stop.
                     */
                    break;
                }
            }
            curr = curr->next;
        }

        if((curr) && (!exact) && (rc == 0)) {
            curr = curr->next;
        }
    }

    return curr ? curr->data : NULL;
}

/**********************************************************************
 *
 *
 *
 **********************************************************************/
static int _ContainerListSsll_free(Container_Container *c)
{
    if(c) {
        free(c);
    }
    return 0;
}

static void * _ContainerListSsll_find(Container_Container *c, const void *data)
{
    if((NULL == c) || (NULL == data))
        return NULL;

    return _ContainerListSsll_get(c, data, 1);
}

static void * _ContainerListSsll_findNext(Container_Container *c, const void *data)
{
    if(NULL == c)
        return NULL;

    return _ContainerListSsll_get(c, data, 0);
}

static int _ContainerListSsll_insert(Container_Container *c, const void *data)
{
    ContainerListSsll_SlContainer *sl = (ContainerListSsll_SlContainer*)c;
    ContainerListSsll_SlNode  *newNode, *curr = sl->head;

    if(NULL == c)
        return -1;

    newNode = TOOLS_MALLOC_TYPEDEF(ContainerListSsll_SlNode);
    if(NULL == newNode)
        return -1;
    newNode->data = TOOLS_REMOVE_CONST(void *, data);
    ++sl->count;
    ++c->sync;

    /*
     * first node?
     */
    if(NULL == sl->head) {
        sl->head = newNode;
        return 0;
    }

    /*
     * sorted or unsorted insert?
     */
    if (1 == sl->unsorted) {
        /*
         * unsorted: fifo, or lifo?
         */
        if (1 == sl->fifo) {
            /*
             * fifo: insert at tail
             */
            while(NULL != curr->next)
                curr = curr->next;
            curr->next = newNode;
        }
        else {
            /*
             * lifo: insert at head
             */
            newNode->next = sl->head;
            sl->head = newNode;
        }
    }
    else {
        /*
         * sorted
         */
        ContainerListSsll_SlNode *last = NULL;
        for( ; curr; last = curr, curr = curr->next) {
            if(sl->c.compare(curr->data, data) > 0)
                break;
        }
        if(NULL == last) {
            newNode->next = sl->head;
            sl->head = newNode;
        }
        else {
            newNode->next = last->next;
            last->next = newNode;
        }
    }

    return 0;
}

static int _ContainerListSsll_remove(Container_Container *c, const void *data)
{
    ContainerListSsll_SlContainer *sl = (ContainerListSsll_SlContainer*)c;
    ContainerListSsll_SlNode  *curr = sl->head;

    if((NULL == c) || (NULL == curr))
        return -1;

    /*
     * special case for NULL data, act like stack
     */
    if ((NULL == data) ||
        (sl->c.compare(sl->head->data, data) == 0)) {
        curr = sl->head;
        sl->head = sl->head->next;
    }
    else {
        ContainerListSsll_SlNode *last = sl->head;
        int rc;
        for(curr = sl->head->next ; curr; last = curr, curr = curr->next) {
            rc = sl->c.compare(curr->data, data);
            if (rc == 0) {
                last->next = curr->next;
                break;
            }
            else if ((rc > 0) && (0 == sl->unsorted)) {
                /*
                 * if sorted and rc > 0, didn't find entry
                 */
                curr = NULL;
                break;
            }
        }
    }

    if(NULL == curr)
        return -2;

    /*
     * free our node structure, but not the data
     */
    free(curr);
    --sl->count;
    ++c->sync;

    return 0;
}

static size_t _ContainerListSsll_size(Container_Container *c)
{
    ContainerListSsll_SlContainer *sl = (ContainerListSsll_SlContainer*)c;

    if(NULL == c)
        return 0;

    return sl->count;
}

static void _ContainerListSsll_forEach(Container_Container *c, Container_FuncObjFunc *f, void *context)
{
    ContainerListSsll_SlContainer *sl = (ContainerListSsll_SlContainer*)c;
    ContainerListSsll_SlNode  *curr;

    if(NULL == c)
        return;

    for(curr = sl->head; curr; curr = curr->next)
        (*f) ((void *)curr->data, context);
}

static void _ContainerListSsll_clear(Container_Container *c, Container_FuncObjFunc *f, void *context)
{
    ContainerListSsll_SlContainer *sl = (ContainerListSsll_SlContainer*)c;
    ContainerListSsll_SlNode  *curr, *next;

    if(NULL == c)
        return;

    for(curr = sl->head; curr; curr = next) {

        next = curr->next;

        if( NULL != f ) {
            curr->next = NULL;
            (*f) ((void *)curr->data, context);
        }

        /*
         * free our node structure, but not the data
         */
        free(curr);
    }
    sl->head = NULL;
    sl->count = 0;
    ++c->sync;
}

/**********************************************************************
 *
 *
 *
 **********************************************************************/
Container_Container * ContainerListSsll_getSsll(void)
{
    /*
     * allocate memory
     */
    ContainerListSsll_SlContainer *sl = TOOLS_MALLOC_TYPEDEF(ContainerListSsll_SlContainer);
    if (NULL==sl) {
        Logger_log(LOGGER_PRIORITY_ERR, "couldn't allocate memory\n");
        return NULL;
    }

    Container_init((Container_Container *)sl, NULL, _ContainerListSsll_free,
                    _ContainerListSsll_size, NULL, _ContainerListSsll_insert,
                    _ContainerListSsll_remove, _ContainerListSsll_find);

    sl->c.findNext = _ContainerListSsll_findNext;
    sl->c.getSubset = NULL;
    sl->c.getIterator = _ContainerListSsll_iteratorGet;
    sl->c.forEach = _ContainerListSsll_forEach;
    sl->c.clear = _ContainerListSsll_clear;


    return (Container_Container*)sl;
}

Factory_Factory * ContainerListSsll_getSsllFactory(void)
{
    static Factory_Factory f = {"sortedSinglyLinkedList",
                                (Factory_FuncProduce*)
                                ContainerListSsll_getSsll };

    return &f;
}


Container_Container * ContainerListSsll_getUsll(void)
{
    /*
     * allocate memory
     */
    ContainerListSsll_SlContainer *sl = (ContainerListSsll_SlContainer *) ContainerListSsll_getSsll();
    if (NULL==sl)
        return NULL; /* msg already logged */

    sl->unsorted = 1;

    return (Container_Container*)sl;
}

Container_Container * ContainerListSsll_getSinglyLinkedList(int fifo)
{
    ContainerListSsll_SlContainer *sl = (ContainerListSsll_SlContainer *)ContainerListSsll_getUsll();
    if (NULL == sl)
        return NULL; /* error already logged */

    sl->fifo = fifo;

    return (Container_Container *)sl;
}

Container_Container * ContainerListSsll_getFifo(void)
{
    return ContainerListSsll_getSinglyLinkedList(1);
}

Factory_Factory * ContainerListSsll_getUsllFactory(void)
{
    static Factory_Factory f = {"unsortedSinglyLinkedList-Lifo",
                                (Factory_FuncProduce*)
                                ContainerListSsll_getUsll };

    return &f;
}

Factory_Factory * ContainerListSsll_getFifoFactory(void)
{
    static Factory_Factory f = {"unsortedSinglyLinkedList-Fifo",
                                (Factory_FuncProduce*)
                                ContainerListSsll_getFifo };

    return &f;
}

void ContainerListSsll_init(void)
{
    Container_register("sortedSinglyLinkedList",
                        ContainerListSsll_getSsllFactory());
    Container_register("unsortedSinglyLinkedList",
                        ContainerListSsll_getUsllFactory());
    Container_register("lifo",
                        ContainerListSsll_getUsllFactory());
    Container_register("fifo",
                        ContainerListSsll_getFifoFactory());
}


/**********************************************************************
 *
 * iterator
 *
 */
static inline ContainerListSsll_SlContainer * _ContainerListSsll_it2cont(ContainerListSsll_SsllIterator *it)
{
    if(NULL == it) {
        Assert_assert(NULL != it);
        return NULL;
    }

    if(NULL == it->base.container) {
        Assert_assert(NULL != it->base.container);
        return NULL;
    }

    if(it->base.container->sync != it->base.sync) {
        DEBUG_MSGTL(("container:iterator", "out of sync\n"));
        return NULL;
    }

    return (ContainerListSsll_SlContainer *)it->base.container;
}

static void * _ContainerListSsll_iteratorCurr(ContainerListSsll_SsllIterator *it)
{
    ContainerListSsll_SlContainer *t = _ContainerListSsll_it2cont(it);
    if ((NULL == t) || (NULL == it->pos))
        return NULL;

    return it->pos->data;
}

static void * _ContainerListSsll_iteratorFirst(ContainerListSsll_SsllIterator *it)
{
    ContainerListSsll_SlContainer *t = _ContainerListSsll_it2cont(it);
    if ((NULL == t) || (NULL == t->head))
        return NULL;

    return t->head->data;
}

static void * _ContainerListSsll_iteratorNext(ContainerListSsll_SsllIterator *it)
{
    ContainerListSsll_SlContainer *t = _ContainerListSsll_it2cont(it);
    if ((NULL == t) || (NULL == it->pos))
        return NULL;

    it->pos = it->pos->next;
    if (NULL == it->pos)
        return NULL;

    return it->pos->data;
}

static void * _ContainerListSsll_iteratorLast(ContainerListSsll_SsllIterator *it)
{
    ContainerListSsll_SlNode      *n;
    ContainerListSsll_SlContainer *t = _ContainerListSsll_it2cont(it);
    if(NULL == t)
        return NULL;

    if (it->last)
        return it->last;

    n = it->pos ? it->pos : t->head;
    if (NULL == n)
        return NULL;

    while(n->next)
        n = n->next;

    if (NULL == n)
        return NULL;

    it->last = n;

    return it->last->data;
}

static int _ContainerListSsll_iteratorReset(ContainerListSsll_SsllIterator *it)
{
    ContainerListSsll_SlContainer *t;

    /** can't use it2conf cuz we might be out of sync */
    if(NULL == it) {
        Assert_assert(NULL != it);
        return 0;
    }

    if(NULL == it->base.container) {
        Assert_assert(NULL != it->base.container);
        return 0;
    }
    t = (ContainerListSsll_SlContainer *)it->base.container;
    if(NULL == t)
        return -1;

    it->last = NULL;
    it->pos = t->head;

    /*
     * save sync count, to make sure container doesn't change while
     * iterator is in use.
     */
    it->base.sync = it->base.container->sync;

    return 0;
}

static int _ContainerListSsll_iteratorRelease(Container_Iterator *it)
{
    free(it);

    return 0;
}

static Container_Iterator * _ContainerListSsll_iteratorGet(Container_Container *c)
{
    ContainerListSsll_SsllIterator* it;

    if(NULL == c)
        return NULL;

    it = TOOLS_MALLOC_TYPEDEF(ContainerListSsll_SsllIterator);
    if(NULL == it)
        return NULL;

    it->base.container = c;

    it->base.first = (Container_FuncIteratorRtn*) _ContainerListSsll_iteratorFirst;
    it->base.next = (Container_FuncIteratorRtn*) _ContainerListSsll_iteratorNext;
    it->base.curr = (Container_FuncIteratorRtn*) _ContainerListSsll_iteratorCurr;
    it->base.last = (Container_FuncIteratorRtn*) _ContainerListSsll_iteratorLast;
    it->base.reset = (Container_FuncIteratorRc*) _ContainerListSsll_iteratorReset;
    it->base.release = (Container_FuncIteratorRc*) _ContainerListSsll_iteratorRelease;

    (void) _ContainerListSsll_iteratorReset(it);

    return (Container_Iterator *)it;
}
