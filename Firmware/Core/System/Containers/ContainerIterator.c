#include "ContainerIterator.h"

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/types.h>
#include <string.h>

#include "System/Util/Logger.h"
#include "System/Util/Debug.h"
#include "Priot.h"
#include "System/Util/Utilities.h"

/**
 *  Holds iterator information containing functions which should be called
 *  by the iterator_handler to loop over your data set and sort it in a
 *  PRIOT specific manner.
 *
 *  The ContainerIterator_IteratorInfo_s typedef can be used instead of directly calling this
 *  struct if you would prefer.
 */
typedef struct ContainerIterator_IteratorInfo_s {
   /*
    * Container_Container  must be first
    */
   Container_Container c;

   /*
    * iterator data
    */
   ContainerIterator_FuncIteratorLoopKey *getFirst;
   ContainerIterator_FuncIteratorLoopKey *getNext;

   ContainerIterator_FuncIteratorLoopData *getData;

   ContainerIterator_FuncIteratorData *freeUserCtx;

   ContainerIterator_FuncIteratorCtx *initLoopCtx;
   ContainerIterator_FuncIteratorCtx *cleanupLoopCtx;
   ContainerIterator_FuncIteratorCtxDup *savePos;

   ContainerIterator_FuncIteratorData * releaseData;
   ContainerIterator_FuncIteratorData * insertData;
   ContainerIterator_FuncIteratorData * removeData;

   ContainerIterator_FuncIteratorOp * getSize;

   int sorted;

   /** This can be used by client handlers to store any
       information they need */
   void *userCtx;

} ContainerIterator_IteratorInfo;


/**********************************************************************
 *
 * iterator
 *
 **********************************************************************/
static void * _ContainerIterator_iteratorGet(ContainerIterator_IteratorInfo *ii, const void *key)
{
    int cmp, rc = PRIOT_ERR_NOERROR;
    Types_RefVoid best = { NULL };
    Types_RefVoid tmp = { NULL };
    Types_RefVoid loopCtx = { NULL };

    DEBUG_MSGT(("containerIterator",">%s\n", "ContainerIterator_iteratorGet"));

    if(ii->initLoopCtx)
        ii->initLoopCtx(ii->userCtx, &loopCtx);

    rc = ii->getFirst(ii->userCtx, &loopCtx, &tmp);
    if(PRIOT_ERR_NOERROR != rc) {
        if(PRIOT_ENDOFMIBVIEW != rc)
            Logger_log(LOGGER_PRIORITY_ERR, "bad rc %d from get_next\n", rc);
    }
    else {
        for( ;
             (NULL != tmp.val) && (PRIOT_ERR_NOERROR == rc);
             rc = ii->getNext(ii->userCtx, &loopCtx, &tmp) ) {

            /*
             * if keys are equal, we are done.
             */
            cmp = ii->c.compare(tmp.val, key);
            if(0 == cmp) {
                best.val = tmp.val;
                if(ii->getData)
                    ii->getData(ii->userCtx, &loopCtx, &best);
            }

            /*
             * if data is sorted and if key is greater,
             * we are done (not found)
             */
            if((cmp > 0) && ii->sorted)
                break;
        } /* end for */
    }

    if(ii->cleanupLoopCtx)
        ii->cleanupLoopCtx(ii->userCtx,&loopCtx);

    return best.val;
}

/**
 *
 * NOTE: the returned data context can be reused, to save from
 *   having to allocate memory repeatedly. However, in this case,
 *   the get_data and get_pos functions must be implemented to
 *   return unique memory that will be saved for later comparisons.
 */
static void * _ContainerIterator_iteratorGetNext(ContainerIterator_IteratorInfo *ii, const void *key)
{
    int cmp, rc = PRIOT_ERR_NOERROR;
    Types_RefVoid bestVal = { NULL };
    Types_RefVoid bestCtx = { NULL };
    Types_RefVoid tmp = { NULL };
    Types_RefVoid loopCtx = { NULL };

    DEBUG_MSGT(("containerIterator",">%s\n", "_ContainerIterator_iteratorGetNext"));

    /*
     * initialize loop context
     */
    if(ii->initLoopCtx)
        ii->initLoopCtx(ii->userCtx, &loopCtx);

    /*
     * get first item
     */
    rc = ii->getFirst(ii->userCtx, &loopCtx, &tmp);
    if(PRIOT_ERR_NOERROR == rc) {
        /*
         * special case: if key is null, find the first item.
         * this is each if the container is sorted, since we're
         * already done!  Otherwise, get the next item for the
         * first comparison in the loop below.
         */
        if (NULL == key) {
            if(ii->getData)
                ii->savePos(ii->userCtx, &loopCtx, &bestCtx, 1);
            bestVal.val = tmp.val;
            if(ii->sorted)
                tmp.val = NULL; /* so we skip for loop */
            else
                rc = ii->getNext(ii->userCtx, &loopCtx, &tmp);
        }
        /*
         * loop over remaining items
         */
        for( ;
             (NULL != tmp.val) && (rc == PRIOT_ERR_NOERROR);
             rc = ii->getNext(ii->userCtx, &loopCtx, &tmp) ) {

            /*
             * if we have a key, this is a get-next, and we need to compare
             * the key to the tmp value to see if the tmp value is greater
             * than the key, but less than any previous match.
             *
             * if there is no key, this is a get-first, and we need to
             * compare the best value agains the tmp value to see if the
             * tmp value is lesser than the best match.
             */
            if(key) /* get next */
                cmp = ii->c.compare(tmp.val, key);
            else { /* get first */
                /*
                 * best value and tmp value should never be equal,
                 * otherwise we'd be comparing a pointer to itself.
                 * (see note on context reuse in comments above function.
                 */
                if(bestVal.val == tmp.val) {
                    Logger_log(LOGGER_PRIORITY_ERR,"illegal reuse of data context in "
                             "containerIterator\n");
                    rc = PRIOT_ERR_GENERR;
                    break;
                }
                cmp = ii->c.compare(bestVal.val, tmp.val);
            }
            if(cmp > 0) {
                /*
                 * if we don't have a key (get-first) or a current best match,
                 * then the comparison above is all we need to know that
                 * tmp is the best match. otherwise, compare against the
                 * current best match.
                 */
                if((NULL == key) || (NULL == bestVal.val) ||
                   ((cmp=ii->c.compare(tmp.val, bestVal.val)) < 0) ) {
                    DEBUG_MSGT(("containerIterator:results"," best match\n"));
                    bestVal.val = tmp.val;
                    if(ii->getData)
                        ii->savePos(ii->userCtx, &loopCtx, &bestCtx, 1);
                }
            }
            else if((cmp == 0) && ii->sorted && key) {
                /*
                 * if keys are equal and container is sorted, then we know
                 * the next key will be the one we want.
                 * NOTE: if no vars, treat as generr, since we
                 *    went past the end of the container when we know
                 *    the next item is the one we want. (IGN-A)
                 */
                rc = ii->getNext(ii->userCtx, &loopCtx, &tmp);
                if(PRIOT_ERR_NOERROR == rc) {
                    bestVal.val = tmp.val;
                    if(ii->getData)
                        ii->savePos(ii->userCtx, &loopCtx, &bestCtx, 1);
                }
                else if(PRIOT_ENDOFMIBVIEW == rc)
                    rc = ErrorCode_GENERR; /* not found */
                break;
            }

        } /* end for */
    }

    /*
     * no vars is ok, except as noted above (IGN-A)
     */
    if(PRIOT_ENDOFMIBVIEW == rc)
        rc = PRIOT_ERR_NOERROR;

    /*
     * get data, iff necessary
     * clear return value iff errors
     */
    if(PRIOT_ERR_NOERROR == rc) {
        if(ii->getData && bestVal.val) {
            rc = ii->getData(ii->userCtx, &bestCtx, &bestVal);
            if(PRIOT_ERR_NOERROR != rc) {
                Logger_log(LOGGER_PRIORITY_ERR, "bad rc %d from get_data\n", rc);
                bestVal.val = NULL;
            }
        }
    }
    else if(PRIOT_ENDOFMIBVIEW != rc) {
        Logger_log(LOGGER_PRIORITY_ERR, "bad rc %d from get_next\n", rc);
        bestVal.val = NULL;
    }

    /*
     * if we have a saved loop ctx, clean it up
     */
    if((bestCtx.val != NULL) && (bestCtx.val != loopCtx.val) &&
       (ii->cleanupLoopCtx))
        ii->cleanupLoopCtx(ii->userCtx,&bestCtx);

    /*
     * clean up loop ctx
     */
    if(ii->cleanupLoopCtx)
        ii->cleanupLoopCtx(ii->userCtx,&loopCtx);

    DEBUG_MSGT(("containerIterator:results"," returning %p\n", bestVal.val));
    return bestVal.val;
}

/**********************************************************************
 *
 * container
 *
 **********************************************************************/
static void _ContainerIterator_iteratorFree(ContainerIterator_IteratorInfo *ii)
{
    DEBUG_MSGT(("containerIterator",">%s\n", "ContainerIterator_iteratorFree"));

    if(NULL == ii)
        return;

    if(ii->userCtx)
        ii->freeUserCtx(ii->userCtx,ii->userCtx);

    free(ii);
}

static void * _ContainerIterator_iteratorFind(ContainerIterator_IteratorInfo *ii, const void *data)
{
    DEBUG_MSGT(("containerIterator",">%s\n", "_ContainerIterator_iteratorFind"));

    if((NULL == ii) || (NULL == data))
        return NULL;

    return _ContainerIterator_iteratorGet(ii, data);
}

static void * _ContainerIterator_iteratorFindNext(ContainerIterator_IteratorInfo *ii, const void *data)
{
    DEBUG_MSGT(("containerIterator",">%s\n", "_ContainerIterator_iteratorFindNext"));

    if(NULL == ii)
        return NULL;

    return _ContainerIterator_iteratorGetNext(ii, data);
}

static int _ContainerIterator_iteratorInsert(ContainerIterator_IteratorInfo *ii, const void *data)
{
    DEBUG_MSGT(("containerIterator",">%s\n", "ContainerIterator_iteratorInsert"));

    if(NULL == ii)
        return -1;

    if(NULL == ii->insertData)
        return -1;

    return ii->insertData(ii->userCtx, data);
}

static int _ContainerIterator_iteratorRemove(ContainerIterator_IteratorInfo *ii, const void *data)
{
    DEBUG_MSGT(("containerIterator",">%s\n", "ContainerIterator_iteratorRemove"));

    if(NULL == ii)
        return -1;

    if(NULL == ii->removeData)
        return -1;

    return ii->removeData(ii->userCtx, data);
}

static int _ContainerIterator_iteratorRelease(ContainerIterator_IteratorInfo *ii, const void *data)
{
    DEBUG_MSGT(("containerIterator",">%s\n", "ContainerIterator_iteratorRelease"));

    if(NULL == ii)
        return -1;

    if(NULL == ii->releaseData)
        return -1;

    return ii->releaseData(ii->userCtx, data);
}

static size_t _ContainerIterator_iteratorSize(ContainerIterator_IteratorInfo *ii)
{
    size_t count = 0;
    Types_RefVoid loopCtx = { NULL };
    Types_RefVoid tmp = { NULL };

    DEBUG_MSGT(("containerIterator",">%s\n", "_iterator_size"));

    if(NULL == ii)
        return -1;

    if(NULL != ii->getSize)
        return ii->getSize(ii->userCtx);

    /*
     * no get_size. loop and count ourselves
     */
    if(ii->initLoopCtx)
        ii->initLoopCtx(ii->userCtx, &loopCtx);

    for( ii->getFirst(ii->userCtx, &loopCtx, &tmp);
         NULL != tmp.val;
         ii->getNext(ii->userCtx, &loopCtx, &tmp) )
        ++count;

    if(ii->cleanupLoopCtx)
        ii->cleanupLoopCtx(ii->userCtx,&loopCtx);

    return count;
}

static void _ContainerIterator_iteratorForEach(ContainerIterator_IteratorInfo *ii, Container_FuncObjFunc *f, void *ctx)
{
    Types_RefVoid loopCtx = { NULL };
    Types_RefVoid tmp = { NULL };

    DEBUG_MSGT(("containerIterator",">%s\n", "ContainerIterator_iteratorForEach"));

    if(NULL == ii)
        return;

    if(ii->initLoopCtx)
        ii->initLoopCtx(ii->userCtx, &loopCtx);

    for( ii->getFirst(ii->userCtx, &loopCtx, &tmp);
         NULL != tmp.val;
         ii->getNext(ii->userCtx, &loopCtx, &tmp) )
        (*f) (tmp.val, ctx);

    if(ii->cleanupLoopCtx)
        ii->cleanupLoopCtx(ii->userCtx, &loopCtx);
}

static void _ContainerIterator_iteratorClear(Container_Container *container, Container_FuncObjFunc *f,
                 void *context)
{
    Logger_log(LOGGER_PRIORITY_WARNING,"clear is meaningless for iterator container.\n");
}

/**********************************************************************
 *
 */
Container_Container * ContainerIterator_get(void *iteratorUserCtx,
                                            Container_FuncCompare * compare,
                                            ContainerIterator_FuncIteratorLoopKey * getFirst,
                                            ContainerIterator_FuncIteratorLoopKey * getNext,
                                            ContainerIterator_FuncIteratorLoopData * getData,
                                            ContainerIterator_FuncIteratorCtxDup * savePos, /* iff returning static data */
                                            ContainerIterator_FuncIteratorCtx * initLoopCtx,
                                            ContainerIterator_FuncIteratorCtx * cleanupLoopCtx,
                                            ContainerIterator_FuncIteratorData * freeUserCtx,
                                            int sorted)
{
    ContainerIterator_IteratorInfo *ii;

    /*
     * sanity checks
     */
    if(getData && ! savePos) {
        Logger_log(LOGGER_PRIORITY_ERR, "savePos required with getData\n");
        return NULL;
    }

    /*
     * allocate memory
     */
    ii = MEMORY_MALLOC_TYPEDEF(ContainerIterator_IteratorInfo);
    if (NULL==ii) {
        Logger_log(LOGGER_PRIORITY_ERR, "couldn't allocate memory\n");
        return NULL;
    }

    /*
     * init container structure with iterator functions
     */
    ii->c.cfree    = (Container_FuncRc*)_ContainerIterator_iteratorFree;
    ii->c.compare  = compare;
    ii->c.getSize  = (Container_FuncSize*) _ContainerIterator_iteratorSize;
    ii->c.init     = NULL;
    ii->c.insert   = (Container_FuncOp*) _ContainerIterator_iteratorInsert;
    ii->c.remove   = (Container_FuncOp*) _ContainerIterator_iteratorRemove;
    ii->c.release  = (Container_FuncOp*) _ContainerIterator_iteratorRelease;
    ii->c.find     = (Container_FuncRtn*) _ContainerIterator_iteratorFind;
    ii->c.findNext = (Container_FuncRtn*) _ContainerIterator_iteratorFindNext;
    ii->c.getSubset   = NULL;
    ii->c.getIterator = NULL;
    ii->c.forEach = (Container_FuncFunc*) _ContainerIterator_iteratorForEach;
    ii->c.clear   = _ContainerIterator_iteratorClear;

    /*
     * init iterator structure with user functions
     */
    ii->getFirst = getFirst;
    ii->getNext = getNext;
    ii->getData = getData;
    ii->savePos = savePos;
    ii->initLoopCtx = initLoopCtx;
    ii->cleanupLoopCtx = cleanupLoopCtx;
    ii->freeUserCtx = freeUserCtx;
    ii->sorted = sorted;

    ii->userCtx = iteratorUserCtx;

    return (Container_Container*)ii;
}

void ContainerIterator_iteratorSetDataCb(Container_Container *c,
                                       ContainerIterator_FuncIteratorData * insertData,
                                       ContainerIterator_FuncIteratorData * removeData,
                                       ContainerIterator_FuncIteratorOp * getSize)
{
    ContainerIterator_IteratorInfo *ii = (ContainerIterator_IteratorInfo *)c;
    if(NULL == ii)
        return;

    ii->insertData = insertData;
    ii->removeData = removeData;
    ii->getSize = getSize;
}
