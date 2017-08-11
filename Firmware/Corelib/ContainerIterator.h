#ifndef CONTAINERITERATOR_H
#define CONTAINERITERATOR_H

#include "Container.h"

typedef int (ContainerIterator_FuncIteratorLoopKey) (void *iteratorCtx, Types_RefVoid* loopCtx, Types_RefVoid* key);

typedef int (ContainerIterator_FuncIteratorLoopData)(void *iteratorCtx, Types_RefVoid* loopCtx, Types_RefVoid* data);

typedef int (ContainerIterator_FuncIteratorCtx) (void *iteratorCtx, Types_RefVoid* loopCtx);

typedef int (ContainerIterator_FuncIteratorCtxDup) (void *iteratorCtx, Types_RefVoid* loopCtx, Types_RefVoid* dupCtx, int reuse);

typedef int (ContainerIterator_FuncIteratorOp) (void *iteratorCtx);

typedef int (ContainerIterator_FuncIteratorData) (void *iteratorCtx, const void *data);

void ContainerIratoter_init(void);

Container_Container* ContainerIterator_get(
                            void *iteratorUserCtx,
                            Container_FuncCompare * compare,
                            ContainerIterator_FuncIteratorLoopKey * getFirst,
                            ContainerIterator_FuncIteratorLoopKey * getNext,
                            ContainerIterator_FuncIteratorLoopData * getData,
                            ContainerIterator_FuncIteratorCtxDup * savePos, /* iff returning static data */
                            ContainerIterator_FuncIteratorCtx * initLoopCtx,
                            ContainerIterator_FuncIteratorCtx * cleanupLoopCtx,
                            ContainerIterator_FuncIteratorData * freeUserCtx,
                            int sorted);

/*
 * set up optional callbacks/
 * NOTE: even though the first parameter is a generic Container_Container,
 *       this function should only be called for a container created
 *       by ContainerIterator_get.
 */
void ContainerIratoter_iteratorSetDataCb(Container_Container *c,
                                       ContainerIterator_FuncIteratorData * insertData,
                                       ContainerIterator_FuncIteratorData * removeData,
                                       ContainerIterator_FuncIteratorOp * getSize);

#endif // CONTAINERITERATOR_H
