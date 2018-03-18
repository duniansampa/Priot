#ifndef CONTAINERBINARYARRAY_H
#define CONTAINERBINARYARRAY_H

#include "Container.h"

/*
 * initialize binary array container. call at startup.
 */
void ContainerBinaryArray_init(void);

/*
 * get an container which uses an binary_array for storage
 */
Container_Container * ContainerBinaryArray_getBinaryArray(void);

/*
 * get a factory for producing binary_array objects
 */
Factory_Factory *   ContainerBinaryArray_getFactory(void);


int ContainerBinaryArray_remove(Container_Container *c, const void *key,
                                void **save);

void ContainerBinaryArray_release(Container_Container *c);

int ContainerBinaryArray_optionsSet(Container_Container *c, int set, u_int flags);


int ContainerBinaryArray_removeAt(Container_Container *c, size_t index, void **save);

void  ** ContainerBinaryArray_getSubset(Container_Container *c, void *key, int *len);



#endif // CONTAINERBINARYARRAY_H
