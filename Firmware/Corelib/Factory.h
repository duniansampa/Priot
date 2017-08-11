#ifndef FACTORY_H
#define FACTORY_H



typedef void * (Factory_FuncProduce)(void);
typedef int (Factory_FuncProduceNoalloc)(void *);

typedef struct Factory_Factory_s {
    /*
     * a string describing the product the factory creates
     */
    const char  *product;

    /*
     * a function to create an object in newly allcoated memory
     */
    Factory_FuncProduce *produce;

    /*
     * a function to create an object in previously allcoated memory
     */
    Factory_FuncProduceNoalloc    *produceNoalloc;

} Factory_Factory;

/*
 * init factory registry
 */
void Factory_init(void);

/*
 * register a factory type
 */
int  Factory_register(Factory_Factory *f);

/*
 * get a factory
 */
Factory_Factory* Factory_get(const char* product);

/*
 * ask a factory to produce an object
 */
void * Factory_produce(const char* product);

/*
 * ask a factory to produce an object in the provided memory
 */
int Factory_produceNoalloc(const char *product, void *memory);

/*
 * factory return codes
 */
enum {
    FACTORY_NOERROR = 0,
    FACTORY_EXISTS,
    FACTORY_NOTFOUND,
    FACTORY_NOMEMORY,
    FACTORY_GENERR,
    FACTORY_MAXIMUM_ERROR
};

#endif // FACTORY_H
