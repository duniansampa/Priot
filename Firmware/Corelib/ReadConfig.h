#ifndef READCONFIG_H
#define READCONFIG_H

#include "Generals.h"


#define READCONFIG_STRINGMAX 1024

#define NORMAL_CONFIG 0
#define PREMIB_CONFIG 1
#define EITHER_CONFIG 2

/*
 * Defines a set of file types and the parse and free functions
 * which process the syntax following a given token in a given file.
 */
struct ReadConfig_ConfigFiles_s {
    char           *fileHeader;     /* Label for entire file. */
    struct ReadConfig_ConfigLine_s *start;
    struct ReadConfig_ConfigFiles_s *next;
};

struct ReadConfig_ConfigLine_s {
    char           *config_token;   /* Label for each line parser
                                     * in the given file. */
    void            (*parse_line) (const char *, char *);
    void            (*free_func) (void);
    struct ReadConfig_ConfigLine_s *next;
    char            config_time;    /* {NORMAL,PREMIB,EITHER}_CONFIG */
    char           *help;
};

struct ReadConfig_Memory_s {
    char           *line;
    struct ReadConfig_Memory_s *next;
};



void ReadConfig_readConfigs(void);

void ReadConfig_unregisterAllConfigHandlers(void);


void            ReadConfig_readPremibConfigs();

int             ReadConfig_config(char *);     /* parse a simple line: token=values */

void            ReadConfig_remember(char *);    /* process later, during snmp_init() */
void            ReadConfig_processMemories(void);      /* run all memories through parser */
int             ReadConfig_readConfig(const char *, struct ReadConfig_ConfigLine_s *, int);
int             ReadConfig_files(int);

void            ReadConfig_freeConfig(void);


void            ReadConfig_error(const char *, ...)
__attribute__((__format__(__printf__, 1, 2)));
void            ReadConfig_warn(const char *, ...)
__attribute__((__format__(__printf__, 1, 2)));


char *       ReadConfig_skipWhite(char *);

const char * ReadConfig_skipWhiteConst(const char *);

char *       ReadConfig_skipNotWhite(char *);

const char * ReadConfig_skipNotWhiteConst(const char *);

char *       ReadConfig_skipToken(char *);

const char * ReadConfig_skipTokenConst(const char *);

char *       ReadConfig_copyNword(char *, char *, int);

const char * ReadConfig_copyNwordConst(const char *, char *, int);

char *       ReadConfig_copyWord(char *, char *);  /* do not use */

int          ReadConfig_withType(const char *, const char *);

char *       ReadConfig_saveOctetString(char *saveto,
                                              const u_char * str,
                                              size_t len);

char *       ReadConfig_readOctetString(const char *readfrom,
                                              u_char ** str,
                                              size_t * len);

const char * ReadConfig_readOctetStringConst(const char *readfrom,
                                                    u_char ** str,
                                                    size_t * len);

char *       ReadConfig_readObjid(char *readfrom, oid ** objid,
                                       size_t * len);
const char * ReadConfig_readObjidConst(const char *readfrom,
                                             oid ** objid,
                                             size_t * len);

char *       ReadConfig_saveObjid(char *saveto, oid * objid,
                                       size_t len);

char *       ReadConfig_readData(int type, char *readfrom,
                                      void *dataptr, size_t * len);

char *       ReadConfig_readMemory(int type, char *readfrom,
                                        char *dataptr, size_t * len);

char *       ReadConfig_storeData(int type, char *storeto,
                                       void *dataptr, size_t * len);
char *       ReadConfig_storeDataPrefix(char prefix, int type,
                                              char *storeto,
                                              void *dataptr, size_t len);
int  ReadConfig_filesOfType(int when, struct ReadConfig_ConfigFiles_s *ctmp);

void ReadConfig_store(const char *type, const char *line);

void ReadConfig_readAppConfigStore(const char *line);

void ReadConfig_savePersistent(const char *type);

void ReadConfig_cleanPersistent(const char *type);

struct ReadConfig_ConfigLine_s * ReadConfig_getHandlers(const char *type);

/*
 * external memory list handlers
 */
void ReadConfig_rememberInList(char *line, struct ReadConfig_Memory_s **mem);

void ReadConfig_processMemoryList(struct ReadConfig_Memory_s **mem, int, int);

void ReadConfig_rememberFreeList(struct ReadConfig_Memory_s **mem);

void ReadConfig_setConfigurationDirectory(const char *dir);

const char * ReadConfig_getConfigurationDirectory(void);
void         ReadConfig_setPersistentDirectory(const char *dir);
const char * ReadConfig_getPersistentDirectory(void);
void         ReadConfig_setTempFilePattern(const char *pattern);

const char * ReadConfig_getTempFilePattern(void);

void         ReadConfig_handleLongOpt(const char *myoptarg);

void         ReadConfig_configPerror(const char *str);

struct ReadConfig_ConfigLine_s *
ReadConfig_registerConfigHandler(const char *type,
                                 const char *token,
                                 void (*parser) (const char *, char *),
                                 void (*releaser) (void), const char *help );

void
ReadConfig_unregisterConfigHandler(const char *type_param,
                                   const char *token );

struct ReadConfig_ConfigLine_s *
ReadConfig_registerAppConfigHandler(const char *token,
                                    void      (*parser)   (const char *, char *),
                                    void      (*releaser) (void), const char *help );

struct ReadConfig_ConfigLine_s *
ReadConfig_registerPrenetMibHandler(const char *type,
                                    const char *token,
                                    void (*parser) (const char *, char *),
                                    void (*releaser) (void), const char *help);

void ReadConfig_printUsage(const char *lead);

void ReadConfig_unregisterAppConfigHandler(const char *token);

void ReadConfig_configPwarn(const char *str);

#endif // READCONFIG_H
