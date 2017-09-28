#ifndef CONTAINER_H
#define CONTAINER_H

#include <sys/types.h>
#include "Types.h"
#include "Factory.h"

/*************************************************************************
 *
 * function pointer definitions
 *
 *************************************************************************/
struct Container_Iterator_s; /** forward declare */
struct Container_Container_s; /** forward declare */

/*
 * function for performing an operation on a container which
 * returns (maybe the same) container.
 */
typedef struct Container_Container_s* (Container_FuncModOp) (struct Container_Container_s *, void *context, u_int flags);

/*
 * function for setting an option on a container
 */
typedef int (Container_FuncOption)(struct Container_Container_s *, int set, u_int flags);

/*
 * function returning an int for an operation on a container
 */
typedef int (Container_FuncRc)(struct Container_Container_s *);

/*
 * function returning an iterator for a container
 */
typedef struct Container_Iterator_s * (Container_FuncIt) (struct Container_Container_s *);

/*
 * function returning a size_t for an operation on a container
 */
typedef size_t (Container_FuncSize)(struct Container_Container_s *);

/*
 * function returning an int for an operation on an object and
 * a container
 */
typedef int (Container_FuncOp)(struct Container_Container_s *, const void *data);

/*
 * function returning an oject for an operation on an object and a
 * container
 */
typedef void * (Container_FuncRtn)(struct Container_Container_s *, const void *data);

/*
 * function with no return which acts on an object
 */
typedef void (Container_FuncObjFunc)(void *data, void *context);

/*
 * function with no return which calls a function on an object
 */
typedef void (Container_FuncFunc)(struct Container_Container_s *, Container_FuncObjFunc *, void *context);

/*
 * function returning an array of objects for an operation on an
 * ojbect and a container
 */
typedef Types_VoidArray * (Container_FuncSet) (struct Container_Container_s *, void *data);

/*
 * function returning an int for a comparison between two objects
 */
typedef int (Container_FuncCompare)(const void *lhs, const void *rhs);

///*************************************************************************
// *
// * Basic container
// *
// *************************************************************************/
typedef struct Container_Container_s {

   /*
    * pointer for container implementation
    */
   void *         containerData;

   /*
    * returns the number of items in a container
    */
   Container_FuncSize  *getSize;

   /*
    * initialize a container
    */
   Container_FuncRc    *init;

   /*
    * release memory used by a container.
    *
    * Note: if your data structures contained allocated
    * memory, you are responsible for releasing that
    * memory before calling this function!
    */
   Container_FuncRc    *cfree;

   /*
    * add an entry to the container
    */
   Container_FuncOp    *insert;

   /*
    * remove an entry from the container
    */
   Container_FuncOp    *remove;

   /*
    * release memory for an entry from the container
    */
   Container_FuncOp    *release; /* NOTE: deprecated. Use free_item */

   Container_FuncObjFunc *freeItem;

   /*
    * find the entry in the container with the same key
    *
    * Note: do not change the key!  If you need to
    * change a key, remove the entry, change the key,
    * and the re-add the entry.
    */
   Container_FuncRtn   *find;

   /*
    * find the entry in the container with the next highest key
    *
    * If the key is NULL, return the first item in the container.
    */
   Container_FuncRtn   *findNext;

   /*
    * find all entries in the container which match the partial key
    * returns allocated memory (Types_VoidArray). User is responsible
    * for releasing this memory (free(array->array), free(array)).
    * DO NOT FREE ELEMENTS OF THE ARRAY, because they are the same pointers
    * stored in the container.
    */
   Container_FuncSet *getSubset;

   /*
    * function to return an iterator for the container
    */
   Container_FuncIt *getIterator;

   /*
    * function to call another function for each object in the container
    */
   Container_FuncFunc   *forEach;

   /*
    * specialized version of for_each used to optimize cleanup.
    * clear the container, optionally calling a function for each item.
    */
   Container_FuncFunc   *clear;

   /*
    * OPTIONAL function to filter inserts to the container
    *  (intended for a secondary container, which only wants
    *   a sub-set of the objects in the primary/parent container)
    * Returns:
    *   1 : filter matched (don't insert)
    *   0 : no match (insert)
    */
   Container_FuncOp    *insertFilter;

    /*
     * OPTIONAL function to duplicate a container. Defaults to a shallow
     * copy. Only the specified container is copied (i.e. sub-containers
     * not included).
     */
    Container_FuncModOp *duplicate;


   /*
    * function to compare two object stored in the container.
    *
    * Returns:
    *
    *   -1  LHS < RHS
    *    0  LHS = RHS
    *    1  LHS > RHS
    */
   Container_FuncCompare    *compare;

   /*
    * same as compare, but RHS will be a partial key
    */
   Container_FuncCompare    *nCompare;

   /*
    * function to set container options
    */
   Container_FuncOption *options;

   /*
    * unique name for finding a particular container in a list
    */
   char *containerName;

   /*
    * sort count, for iterators to track (insert/delete
    * bumps counter, invalidates iterator)
    */
   u_long   sync;

   /*
    * flags
    */
   u_int   flags;

   /*
    * containers can contain other containers (additional indexes)
    */
   struct Container_Container_s *next, *prev;

} Container_Container;

/*
 * initialize/free a container of container factories. used by
 * Container_find* functions.
 */
void Container_initList(void);

void Container_freeList(void);

/*
 * register a new container factory
 */
int Container_registerWithCompare(const char* name, Factory_Factory *f, Container_FuncCompare *c);

int Container_register(const char* name, Factory_Factory *f);

/*
 * search for and create a container from a list of types or a
 * specific type.
 */
Container_Container * Container_find(const char *typeList);

Container_Container * Container_get(const char *type);

/*
 * utility routines
 */
void Container_addIndex(Container_Container *primary, Container_Container *newIndex);


Factory_Factory * Container_getFactory(const char *type);

/*
 * common comparison routines
 */
/** first data element is a 'Types_Index' */

int Container_compareIndex(const void *lhs, const void *rhs);

int Container_nCompareIndex(const void *lhs, const void *rhs);

/** first data element is a 'char *' */
int Container_compareCstring(const void * lhs, const void * rhs);

int Container_nCompareCstring(const void * lhs, const void * rhs);

/** useful for octet strings */
int Container_compareMem(const char * lhs, size_t lhs_len,
                        const char * rhs, size_t rhs_len);

/** no structure, just 'char *' pointers */
int Container_compareDirectCstring(const void * lhs, const void * rhs);

int Container_compareLong(const void * lhs, const void * rhs);
int Container_compareUlong(const void * lhs, const void * rhs);
int Container_compareInt32(const void * lhs, const void * rhs);
int Container_compareUint32(const void * lhs, const void * rhs);

/** for_each callback to call free on data item */

void  Container_simpleFree(void *data, void *context);

/*
* container optionflags
*/
#define CONTAINER_KEY_ALLOW_DUPLICATES             0x00000001
#define CONTAINER_KEY_UNSORTED                     0x00000002

#define CONTAINER_SET_OPTIONS(x,o,rc)  do {                         \
    if (NULL==(x)->options)                                         \
        rc = -1;                                                    \
    else {                                                          \
        rc = (x)->options(x, 1, o);                                 \
        if (rc != -1 )                                              \
            (x)->flags |= o;                                        \
    }                                                               \
} while(0)

#define CONTAINER_CHECK_OPTION(x,o,rc)    do {                      \
    rc = x->flags & 0;                                              \
} while(0)


/*
 * useful macros (x = container; k = key; c = user context)
 */
#define CONTAINER_FIRST(x)          (x)->findNext(x,NULL)
#define CONTAINER_FIND(x,k)         (x)->find(x,k)
#define CONTAINER_NEXT(x,k)         (x)->findNext(x,k)
/*
* GET_SUBSET returns allocated memory (netsnmp_void_array). User is responsible
* for releasing this memory (free(array->array), free(array)).
* DO NOT FREE ELEMENTS OF THE ARRAY, because they are the same pointers
* stored in the container.
*/
#define CONTAINER_GET_SUBSET(x,k)   (x)->getSubset(x,k)
#define CONTAINER_SIZE(x)           (x)->getSize(x)
#define CONTAINER_ITERATOR(x)       (x)->getIterator(x)
#define CONTAINER_COMPARE(x,l,r)    (x)->compare(l,r)
#define CONTAINER_FOR_EACH(x,f,c)   (x)->forEach(x,f,c)

/*
 * if you are getting multiple definitions of these three
 * inline functions, you most likely have optimizations turned off.
 * Either turn them back on, or define PRIOT_NO_INLINE
 */
/*
 * insert k into all containers
 */

int CONTAINER_INSERT(Container_Container *x, const void *k);

/*
 * remove k from all containers
 */

int CONTAINER_REMOVE(Container_Container *x, const void *k);

/*
 * duplicate container
 */

Container_Container * CONTAINER_DUP(Container_Container *x, void *ctx, u_int flags);

/*
 * clear all containers. When clearing the *first* container, and
 * *only* the first container, call the function f for each item.
 * After calling this function, all containers should be empty.
 */
void CONTAINER_CLEAR(Container_Container *x, Container_FuncObjFunc *f, void *c);

/*
 * clear all containers. When clearing the *first* container, and
 * *only* the first container, call the free_item function for each item.
 * After calling this function, all containers should be empty.
 */

void CONTAINER_FREE_ALL(Container_Container *x, void *c);

/*
 * free all containers
 */

int CONTAINER_FREE(Container_Container *x);

Container_Container *SUBCONTAINER_FIND(Container_Container *x, const char* name);

/*
 * INTERNAL utility routines for container implementations
 */
void Container_init( Container_Container   *c,
                     Container_FuncRc      *init,
                     Container_FuncRc      *cfree,
                     Container_FuncSize    *size,
                     Container_FuncCompare *cmp,
                     Container_FuncOp      *ins,
                     Container_FuncOp      *rem,
                     Container_FuncRtn     *fnd);

/** Duplicate container meta-data. */
int Container_dataDup(Container_Container *dup, Container_Container *c);


/*************************************************************************
 *
 * container iterator
 *
 *************************************************************************/
/*
 * function returning an int for an operation on an iterator
 */
typedef int (Container_FuncIteratorRc)(struct Container_Iterator_s *);

/*
 * function returning an oject for an operation on an iterator
 */
typedef void * (Container_FuncIteratorRtn)(struct Container_Iterator_s *);


/*
 * iterator structure
 */
typedef struct Container_Iterator_s {

    Container_Container *container;

    /*
     * sync from container when iterator created. used to invalidate
     * the iterator when the container changes.
     */
    u_long  sync;

    /*
     * reset iterator position to beginning of container.
     */
    Container_FuncIteratorRc    *reset;

    /*
     * release iterator and memory it uses
     */
    Container_FuncIteratorRc    *release;

    /*
     * first, last and current DO NOT advance the iterator
     */
    Container_FuncIteratorRtn   *first;
    Container_FuncIteratorRtn   *curr;
    Container_FuncIteratorRtn   *last;

    Container_FuncIteratorRtn   *next;

    //    /*
    //     * remove will remove the item at the current position, then back up
    //     * the iterator to the previous item. That way next will move to the
    //     * item (the one that replaced the removed item.
    //     */
    Container_FuncIteratorRc    *remove;

} Container_Iterator;


#define CONTAINER_ITERATOR_FIRST(x)  x->first(x)
#define CONTAINER_ITERATOR_NEXT(x)   x->next(x)
#define CONTAINER_ITERATOR_LAST(x)   x->last(x)
#define CONTAINER_ITERATOR_REMOVE(x) x->remove(x)
#define CONTAINER_ITERATOR_RELEASE(x) do { x->release(x); x = NULL; } while(0)


#endif // CONTAINER_H
