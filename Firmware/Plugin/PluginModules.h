#ifndef PLUGINMODULES_H
#define PLUGINMODULES_H


#define DO_INITIALIZE   1
#define DONT_INITIALIZE 0

typedef struct ModuleInitList_s {
    char           *module_name;
    struct ModuleInitList_s *next;
} ModuleInitList;

void            PluginModules_initModules(void);


#endif // PLUGINMODULES_H
