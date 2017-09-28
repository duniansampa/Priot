#ifndef MIBMODULES_H
#define MIBMODULES_H

#define DO_INITIALIZE   1
#define DONT_INITIALIZE 0

typedef struct ModuleInitList_s {
    char           *module_name;
    struct ModuleInitList_s *next;
} ModuleInitList;

void  MibModules_addToInitList(char *module_list);
int   MibModules_shouldInit(const char *module_name);
void  MibModules_initMibModules(void);

#endif // MIBMODULES_H
