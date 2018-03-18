#include "Factory.h"


/*
 * init factory registry
 */
void Factory_init(void){
}

/*
 * register a factory type
 */
int  Factory_register(Factory_Factory *f){
}

/*
 * get a factory
 */
Factory_Factory* Factory_get(const char* product){
}

/*
 * ask a factory to produce an object
 */
void * Factory_produce(const char* product){
}

/*
 * ask a factory to produce an object in the provided memory
 */
int Factory_produceNoalloc(const char *product, void *memory){
}
