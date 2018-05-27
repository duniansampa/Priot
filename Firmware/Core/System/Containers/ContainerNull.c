#include "ContainerNull.h"

#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "System/Util/Utilities.h"


/** @defgroup null_container null_container
 *  Helps you implement specialized containers.
 *  @ingroup container
 *
 *  This is a simple container that doesn't actually contain anything.
 *  All the methods simply log a debug message and return.
 *
 *  The original intent for this container is as a wrapper for a specialized
 *  container. Implement the functions you need, create a null_container,
 *  and override the default functions with your specialized versions.
 *
 *  You can use the 'container:null' debug token to see what functions are
 *  being called, to help determine if you need to implement them.
 *
 *  @{
 */

/**********************************************************************
 *
 * container
 *
 */
static void * _ContainerNull_find(Container_Container *container, const void *data)
{
    DEBUG_MSGTL(("container:null:find","in\n"));
    return NULL;
}

static void * _ContainerNull_findNext(Container_Container *container, const void *data)
{
    DEBUG_MSGTL(("container:null:findNext","in\n"));
    return NULL;
}

static int _ContainerNull_insert(Container_Container *container, const void *data)
{
    DEBUG_MSGTL(("container:null:insert","in\n"));
    return 0;
}

static int _ContainerNull_remove(Container_Container *container, const void *data)
{
    DEBUG_MSGTL(("container:null:remove","in\n"));
    return 0;
}

static int _ContainerNull_free(Container_Container *container)
{
    DEBUG_MSGTL(("container:null:free","in\n"));
    free(container);
    return 0;
}

static int _ContainerNull_init(Container_Container *container)
{
    DEBUG_MSGTL(("container:null:","in\n"));
    return 0;
}

static size_t _ContainerNull_size(Container_Container *container)
{
    DEBUG_MSGTL(("container:null:size","in\n"));
    return 0;
}

static void _ContainerNull_forEach(Container_Container *container, Container_FuncObjFunc *f,
             void *context)
{
    DEBUG_MSGTL(("container:null:forEach","in\n"));
}

static Types_VoidArray * _ContainerNull_getSubset(Container_Container *container, void *data)
{
    DEBUG_MSGTL(("container:null:getSubset","in\n"));
    return NULL;
}

static void _ContainerNull_clear(Container_Container *container, Container_FuncObjFunc *f,
                 void *context)
{
    DEBUG_MSGTL(("container:null:clear","in\n"));
}

/**********************************************************************
 *
 * factory
 *
 */

Container_Container * ContainerNull_get(void)
{
    /*
     * allocate memory
     */
    Container_Container *c;
    DEBUG_MSGTL(("container:null:getNull","in\n"));
    c = MEMORY_MALLOC_TYPEDEF(Container_Container);
    if (NULL==c) {
        Logger_log(LOGGER_PRIORITY_ERR, "couldn't allocate memory\n");
        return NULL;
    }

    c->containerData = NULL;

    c->getSize = _ContainerNull_size;
    c->init = _ContainerNull_init;
    c->cfree = _ContainerNull_free;
    c->insert = _ContainerNull_insert;
    c->remove = _ContainerNull_remove;
    c->find = _ContainerNull_find;
    c->findNext = _ContainerNull_findNext;
    c->getSubset = _ContainerNull_getSubset;
    c->getIterator = NULL; /* _null_iterator; */
    c->forEach =  _ContainerNull_forEach;
    c->clear = _ContainerNull_clear;

    return c;
}

Factory_Factory * ContainerNull_getFactory(void)
{
    static Factory_Factory f = { "null",
                                 (Factory_FuncProduce*)
                                 ContainerNull_get};

    DEBUG_MSGTL(("container:null:getNullFactory","in\n"));
    return &f;
}

void ContainerNull_init(void)
{
    Container_register("null", ContainerNull_getFactory());
}
