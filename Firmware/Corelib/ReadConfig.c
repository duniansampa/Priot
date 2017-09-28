#include "ReadConfig.h"
#include "DefaultStore.h"
#include "Debug.h"
#include "Tools.h"
#include "Logger.h"
#include "System.h"
#include "Asn01.h"
#include "Mib.h"
#include "Int64.h"
#include "Strlcat.h"
#include "Strlcpy.h"
#include "Priot.h"
#include "Callback.h"
#include "Impl.h"


#include <arpa/inet.h>

/** @defgroup read_config parsing various configuration files at run time
 *  @ingroup library
 *
 * The read_config related functions are a fairly extensible  system  of
 * parsing various configuration files at the run time.
 *
 * The idea is that the calling application is able to register
 * handlers for certain tokens specified in certain types
 * of files.  The read_configs function can then be  called
 * to  look  for all the files that it has registrations for,
 * find the first word on each line, and pass  the  remainder
 * to the appropriately registered handler.
 *
 * For persistent configuration storage you will need to use the
 * read_config_read_data, read_config_store, and read_config_store_data
 * APIs in conjunction with first registering a
 * callback so when the agent shutsdown for whatever reason data is written
 * to your configuration files.  The following explains in more detail the
 * sequence to make this happen.
 *
 * This is the callback registration API, you need to call this API with
 * the appropriate parameters in order to configure persistent storage needs.
 *
 *        int snmp_register_callback(int major, int minor,
 *                                   SNMPCallback *new_callback,
 *                                   void *arg);
 *
 * You will need to set major to CALLBACK_LIBRARY, minor to
 * SNMP_CALLBACK_STORE_DATA. arg is whatever you want.
 *
 * Your callback function's prototype is:
 * int     (SNMPCallback) (int majorID, int minorID, void *serverarg,
 *                        void *clientarg);
 *
 * The majorID, minorID and clientarg are what you passed in the callback
 * registration above.  When the callback is called you have to essentially
 * transfer all your state from memory to disk. You do this by generating
 * configuration lines into a buffer.  The lines are of the form token
 * followed by token parameters.
 *
 * Finally storing is done using read_config_store(type, buffer);
 * type is the application name this can be obtained from:
 *
 * DefaultStore_getString(DSSTORAGE.LIBRARY_ID, NETSNMP_DS_LIB_APPTYPE);
 *
 * Now, reading back the data: This is done by registering a config handler
 * for your token using the ReadConfig_registerConfigHandler function. Your
 * handler will be invoked and you can parse in the data using the
 * read_config_read APIs.
 *
 *  @{
 */

# include <dirent.h>
# define READCONFIG_NAMLEN(dirent) strlen((dirent)->d_name)

static int      readConfig_configErrors;

struct ReadConfig_ConfigFiles_s *readConfig_configFiles = NULL;



static struct ReadConfig_ConfigLine_s *
ReadConfig_internalRegisterConfigHandler(const char *type_param,
                 const char *token,
                 void (*parser) (const char *, char *),
                 void (*releaser) (void), const char *help,
                 int when)
{
    struct ReadConfig_ConfigFiles_s **ctmp = &readConfig_configFiles;
    struct ReadConfig_ConfigLine_s  **ltmp;
    const char           *type = type_param;

    if (type == NULL || *type == '\0') {
        type = DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                    DSLIB_STRING.APPTYPE);
    }

    /*
     * Handle multiple types (recursively)
     */
    if (strchr(type, ':')) {
        struct ReadConfig_ConfigLine_s *ltmp2 = NULL;
        char                buf[READCONFIG_STRINGMAX];
        char               *cptr = buf;

        Strlcpy_strlcpy(buf, type, READCONFIG_STRINGMAX);
        while (cptr) {
            char* c = cptr;
            cptr = strchr(cptr, ':');
            if(cptr) {
                *cptr = '\0';
                ++cptr;
            }
            ltmp2 = ReadConfig_internalRegisterConfigHandler(c, token, parser,
                                                     releaser, help, when);
        }
        return ltmp2;
    }

    /*
     * Find type in current list  -OR-  create a new file type.
     */
    while (*ctmp != NULL && strcmp((*ctmp)->fileHeader, type)) {
        ctmp = &((*ctmp)->next);
    }

    if (*ctmp == NULL) {
        *ctmp = (struct ReadConfig_ConfigFiles_s *)
            calloc(1, sizeof(struct ReadConfig_ConfigFiles_s));
        if (!*ctmp) {
            return NULL;
        }

        (*ctmp)->fileHeader = strdup(type);
        DEBUG_MSGTL(("9:read_config:type", "new type %s\n", type));
    }

    DEBUG_MSGTL(("9:read_config:register_handler", "registering %s %s\n",
                type, token));
    /*
     * Find parser type in current list  -OR-  create a new
     * line parser entry.
     */
    ltmp = &((*ctmp)->start);

    while (*ltmp != NULL && strcmp((*ltmp)->config_token, token)) {
        ltmp = &((*ltmp)->next);
    }

    if (*ltmp == NULL) {
        *ltmp = (struct ReadConfig_ConfigLine_s *)
            calloc(1, sizeof(struct ReadConfig_ConfigLine_s));
        if (!*ltmp) {
            return NULL;
        }

        (*ltmp)->config_time = when;
        (*ltmp)->config_token = strdup(token);
        if (help != NULL)
            (*ltmp)->help = strdup(help);
    }

    /*
     * Add/Replace the parse/free functions for the given line type
     * in the given file type.
     */
    (*ltmp)->parse_line = parser;
    (*ltmp)->free_func = releaser;

    return (*ltmp);

}                               /* end ReadConfig_registerConfigHandler() */

struct ReadConfig_ConfigLine_s *
ReadConfig_registerPrenetMibHandler(const char *type,
                                const char *token,
                                void (*parser) (const char *, char *),
                                void (*releaser) (void), const char *help)
{
    return ReadConfig_internalRegisterConfigHandler(type, token, parser, releaser,
                        help, PREMIB_CONFIG);
}

struct ReadConfig_ConfigLine_s *
ReadConfig_registerAppPrenetMibHandler(const char *token,
                                    void (*parser) (const char *, char *),
                                    void (*releaser) (void),
                                    const char *help)
{
    return (ReadConfig_registerPrenetMibHandler
            (NULL, token, parser, releaser, help));
}

/**
 * register_config_handler registers handlers for certain tokens specified in
 * certain types of files.
 *
 * Allows a module writer use/register multiple configuration files based off
 * of the type parameter.  A module writer may want to set up multiple
 * configuration files to separate out related tasks/variables or just for
 * management of where to put tokens as the module or modules get more complex
 * in regard to handling token registrations.
 *
 * @param type     the configuration file used, e.g., if snmp.conf is the
 *                 file where the token is located use "snmp" here.
 *                 Multiple colon separated tokens might be used.
 *                 If NULL or "" then the configuration file used will be
 *                 \<application\>.conf.
 *
 * @param token    the token being parsed from the file.  Must be non-NULL.
 *
 * @param parser   the handler function pointer that use  the specified
 *                 token and the rest of the line to do whatever is required
 *                 Should be non-NULL in order to make use of this API.
 *
 * @param releaser if non-NULL, the function specified is called when
 *                 unregistering config handler or when configuration
 *                 files are re-read.
 *                 This function should free any resources allocated by
 *                 the token handler function.
 *
 * @param help     if non-NULL, used to display help information on the
 *                 expected arguments after the token.
 *
 * @return Pointer to a new config line entry or NULL on error.
 */
struct ReadConfig_ConfigLine_s *
ReadConfig_registerConfigHandler(const char *type,
            const char *token,
            void (*parser) (const char *, char *),
            void (*releaser) (void), const char *help)
{
    return ReadConfig_internalRegisterConfigHandler(type, token, parser, releaser,
                        help, NORMAL_CONFIG);
}

struct ReadConfig_ConfigLine_s *
ReadConfig_registerConstConfigHandler(const char *type,
                              const char *token,
                              void (*parser) (const char *, const char *),
                              void (*releaser) (void), const char *help)
{
    return ReadConfig_internalRegisterConfigHandler(type, token,
                                            (void(*)(const char *, char *))
                                            parser, releaser,
                        help, NORMAL_CONFIG);
}

struct ReadConfig_ConfigLine_s *
ReadConfig_registerAppConfigHandler(const char *token,
                            void (*parser) (const char *, char *),
                            void (*releaser) (void), const char *help)
{
    return (ReadConfig_registerConfigHandler(NULL, token, parser, releaser, help));
}



/**
 * uregister_config_handler un-registers handlers given a specific type_param
 * and token.
 *
 * @param type_param the configuration file used where the token is located.
 *                   Used to lookup the config file entry
 *
 * @param token      the token that is being unregistered
 *
 * @return void
 */
void
ReadConfig_unregisterConfigHandler(const char *type_param, const char *token)
{
    struct ReadConfig_ConfigFiles_s **ctmp = &readConfig_configFiles;
    struct ReadConfig_ConfigLine_s  **ltmp;
    const char           *type = type_param;

    if (type == NULL || *type == '\0') {
        type = DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                            DSLIB_STRING.APPTYPE);
    }

    /*
     * Handle multiple types (recursively)
     */
    if (strchr(type, ':')) {
        char                buf[READCONFIG_STRINGMAX];
        char               *cptr = buf;

        Strlcpy_strlcpy(buf, type, READCONFIG_STRINGMAX);
        while (cptr) {
            char* c = cptr;
            cptr = strchr(cptr, ':');
            if(cptr) {
                *cptr = '\0';
                ++cptr;
            }
            ReadConfig_unregisterConfigHandler(c, token);
        }
        return;
    }

    /*
     * find type in current list
     */
    while (*ctmp != NULL && strcmp((*ctmp)->fileHeader, type)) {
        ctmp = &((*ctmp)->next);
    }

    if (*ctmp == NULL) {
        /*
         * Not found, return.
         */
        return;
    }

    ltmp = &((*ctmp)->start);
    if (*ltmp == NULL) {
        /*
         * Not found, return.
         */
        return;
    }
    if (strcmp((*ltmp)->config_token, token) == 0) {
        /*
         * found it at the top of the list
         */
        struct ReadConfig_ConfigLine_s *ltmp2 = (*ltmp)->next;
        if ((*ltmp)->free_func)
            (*ltmp)->free_func();
        TOOLS_FREE((*ltmp)->config_token);
        TOOLS_FREE((*ltmp)->help);
        TOOLS_FREE(*ltmp);
        (*ctmp)->start = ltmp2;
        return;
    }
    while ((*ltmp)->next != NULL
           && strcmp((*ltmp)->next->config_token, token)) {
        ltmp = &((*ltmp)->next);
    }
    if ((*ltmp)->next != NULL) {
        struct ReadConfig_ConfigLine_s *ltmp2 = (*ltmp)->next->next;
        if ((*ltmp)->next->free_func)
            (*ltmp)->next->free_func();
        TOOLS_FREE((*ltmp)->next->config_token);
        TOOLS_FREE((*ltmp)->next->help);
        TOOLS_FREE((*ltmp)->next);
        (*ltmp)->next = ltmp2;
    }
}

void ReadConfig_unregisterAppConfigHandler(const char *token)
{
    ReadConfig_unregisterConfigHandler(NULL, token);
}

void ReadConfig_unregisterAllConfigHandlers(void)
{
    struct ReadConfig_ConfigFiles_s *ctmp, *save;
    struct ReadConfig_ConfigLine_s *ltmp;

    /*
     * Keep using config_files until there are no more!
     */
    for (ctmp = readConfig_configFiles; ctmp;) {
        for (ltmp = ctmp->start; ltmp; ltmp = ctmp->start) {
            ReadConfig_unregisterConfigHandler(ctmp->fileHeader,
                                      ltmp->config_token);
        }
        TOOLS_FREE(ctmp->fileHeader);
        save = ctmp->next;
        TOOLS_FREE(ctmp);
        ctmp = save;
        readConfig_configFiles = save;
    }
}

static unsigned int  readConfig_linecount;
static const char   *readConfig_curfilename;

struct ReadConfig_ConfigLine_s *
ReadConfig_getHandlers(const char *type)
{
    struct ReadConfig_ConfigFiles_s *ctmp = readConfig_configFiles;
    for (; ctmp != NULL && strcmp(ctmp->fileHeader, type);
         ctmp = ctmp->next);
    if (ctmp)
        return ctmp->start;
    return NULL;
}

int
ReadConfig_withTypeWhen(const char *filename, const char *type, int when)
{
    struct ReadConfig_ConfigLine_s *ctmp = ReadConfig_getHandlers(type);
    if (ctmp)
        return ReadConfig_readConfig(filename, ctmp, when);
    else
        DEBUG_MSGTL(("read_config",
                    "read_config: I have no registrations for type:%s,file:%s\n",
                    type, filename));
    return ErrorCode_GENERR;     /* No config files read */
}

int
ReadConfig_withType(const char *filename, const char *type)
{
    return ReadConfig_withTypeWhen(filename, type, EITHER_CONFIG);
}


struct ReadConfig_ConfigLine_s *
ReadConfig_findHandler(struct ReadConfig_ConfigLine_s *line_handlers,
                         const char *token)
{
    struct ReadConfig_ConfigLine_s *lptr;

    for (lptr = line_handlers; lptr != NULL; lptr = lptr->next) {
        if (!strcasecmp(token, lptr->config_token)) {
            return lptr;
        }
    }
    return NULL;
}


/*
 * searches a config_line linked list for a match
 */
int
ReadConfig_runConfigHandler(struct ReadConfig_ConfigLine_s *lptr,
    const char *token, char *cptr, int when)
{
    char           *cp;
    lptr = ReadConfig_findHandler(lptr, token);
    if (lptr != NULL) {
        if (when == EITHER_CONFIG || lptr->config_time == when) {
            char tmpbuf[1];
            DEBUG_MSGTL(("read_config:parser",
                        "Found a parser.  Calling it: %s / %s\n", token,
                        cptr));
            /*
             * Make sure cptr is non-null
             */
            if (!cptr) {
                tmpbuf[0] = '\0';
                cptr = tmpbuf;
            }

            /*
             * Stomp on any trailing whitespace
             */
            cp = &(cptr[strlen(cptr)-1]);
            while ((cp > cptr) && isspace((unsigned char)(*cp))) {
                *(cp--) = '\0';
            }
            (*(lptr->parse_line)) (token, cptr);
        }
        else
            DEBUG_MSGTL(("9:read_config:parser",
                        "%s handler not registered for this time\n", token));
    } else if (when != PREMIB_CONFIG &&
           !DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                       DSLIB_BOOLEAN.NO_TOKEN_WARNINGS)) {
    ReadConfig_warn("Unknown token: %s.", token);
        return ErrorCode_GENERR;
    }
    return ErrorCode_SUCCESS;
}

/*
 * takens an arbitrary string and tries to intepret it based on the
 * known configuration handlers for all registered types.  May produce
 * inconsistent results when multiple tokens of the same name are
 * registered under different file types.
 */

/*
 * we allow = delimeters here
 */
#define READCONFIG_CONFIG_DELIMETERS " \t="

int ReadConfig_configWhen(char *line, int when)
{
    char           *cptr, buf[READCONFIG_STRINGMAX];
    struct ReadConfig_ConfigLine_s *lptr = NULL;
    struct ReadConfig_ConfigFiles_s *ctmp = readConfig_configFiles;
    char           *st;

    if (line == NULL) {
        ReadConfig_configPerror("snmp_config() called with a null string.");
        return ErrorCode_GENERR;
    }

    Strlcpy_strlcpy(buf, line, READCONFIG_STRINGMAX);
    cptr = strtok_r(buf, READCONFIG_CONFIG_DELIMETERS, &st);
    if (!cptr) {
        ReadConfig_warn("Wrong format: %s", line);
        return ErrorCode_GENERR;
    }
    if (cptr[0] == '[') {
        if (cptr[strlen(cptr) - 1] != ']') {
        ReadConfig_error("no matching ']' for type %s.", cptr + 1);
            return ErrorCode_GENERR;
        }
        cptr[strlen(cptr) - 1] = '\0';
        lptr = ReadConfig_getHandlers(cptr + 1);
        if (lptr == NULL) {
        ReadConfig_error("No handlers regestered for type %s.",
                 cptr + 1);
            return ErrorCode_GENERR;
        }
        cptr = strtok_r(NULL, READCONFIG_CONFIG_DELIMETERS, &st);
        lptr = ReadConfig_findHandler(lptr, cptr);
    } else {
        /*
         * we have to find a token
         */
        for (; ctmp != NULL && lptr == NULL; ctmp = ctmp->next)
            lptr = ReadConfig_findHandler(ctmp->start, cptr);
    }
    if (lptr == NULL && DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                      DSLIB_BOOLEAN.NO_TOKEN_WARNINGS)) {
    ReadConfig_warn("Unknown token: %s.", cptr);
        return ErrorCode_GENERR;
    }

    /*
     * use the original string instead since strtok_r messed up the original
     */
    line = ReadConfig_skipWhite(line + (cptr - buf) + strlen(cptr) + 1);

    return (ReadConfig_runConfigHandler(lptr, cptr, line, when));
}

int ReadConfig_config(char *line)
{
    int             ret = PRIOT_ERR_NOERROR;
    DEBUG_MSGTL(("snmp_config", "remembering line \"%s\"\n", line));
    ReadConfig_remember(line);      /* always remember it so it's read
                                         * processed after a ReadConfig_freeConfig()
                                         * call */
    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                   DSLIB_BOOLEAN.HAVE_READ_CONFIG)) {
        DEBUG_MSGTL(("snmp_config", "  ... processing it now\n"));
        ret = ReadConfig_configWhen(line, NORMAL_CONFIG);
    }
    return ret;
}

void
ReadConfig_rememberInList(char *line,
                                struct ReadConfig_Memory_s **mem)
{
    if (mem == NULL)
        return;

    while (*mem != NULL)
        mem = &((*mem)->next);

    *mem = TOOLS_MALLOC_STRUCT(ReadConfig_Memory_s);
    if (*mem != NULL) {
        if (line)
            (*mem)->line = strdup(line);
    }
}

void
ReadConfig_rememberFreeList(struct ReadConfig_Memory_s **mem)
{
    struct ReadConfig_Memory_s *tmpmem;
    while (*mem) {
        TOOLS_FREE((*mem)->line);
        tmpmem = (*mem)->next;
        TOOLS_FREE(*mem);
        *mem = tmpmem;
    }
}

void
ReadConfig_processMemoryList(struct ReadConfig_Memory_s **memp,
                                   int when, int clear)
{

    struct ReadConfig_Memory_s *mem;

    if (!memp)
        return;

    mem = *memp;

    while (mem) {
        DEBUG_MSGTL(("read_config:mem", "processing memory: %s\n", mem->line));
        ReadConfig_configWhen(mem->line, when);
        mem = mem->next;
    }

    if (clear)
        ReadConfig_rememberFreeList(memp);
}

/*
 * default storage location implementation
 */
static struct ReadConfig_Memory_s * readConfig_memorylist = NULL;

void
ReadConfig_remember(char *line)
{
    ReadConfig_rememberInList(line, &readConfig_memorylist);
}

void
ReadConfig_processMemories(void)
{
    ReadConfig_processMemoryList(&readConfig_memorylist, EITHER_CONFIG, 1);
}

void
ReadConfig_processMemoriesWhen(int when, int clear)
{
    ReadConfig_processMemoryList(&readConfig_memorylist, when, clear);
}

/*******************************************************************-o-******
 * read_config
 *
 * Parameters:
 *	*filename
 *	*line_handler
 *	 when
 *
 * Read <filename> and process each line in accordance with the list of
 * <line_handler> functions.
 *
 *
 * For each line in <filename>, search the list of <line_handler>'s
 * for an entry that matches the first token on the line.  This comparison is
 * case insensitive.
 *
 * For each match, check that <when> is the designated time for the
 * <line_handler> function to be executed before processing the line.
 *
 * Returns ErrorCode_SUCCESS if the file is processed successfully.
 * Returns ErrorCode_GENERR  if it cannot.
 *    Note that individual config token errors do not trigger ErrorCode_GENERR
 *    It's only if the whole file cannot be processed for some reason.
 */
int
ReadConfig_readConfig(const char *filename,
            struct ReadConfig_ConfigLine_s *line_handler, int when)
{
    static int      depth = 0;
    static int      files = 0;

    const char * const prev_filename = readConfig_curfilename;
    const unsigned int prev_linecount = readConfig_linecount;

    FILE           *ifile;
    char           *line = NULL;  /* current line buffer */
    size_t          linesize = 0; /* allocated size of line */

    /* reset file counter when recursion depth is 0 */
    if (depth == 0)
        files = 0;

    if ((ifile = fopen(filename, "r")) == NULL) {
        if (errno == ENOENT) {
            DEBUG_MSGTL(("read_config", "%s: %s\n", filename,
                        strerror(errno)));
        } else
        if (errno == EACCES) {
            DEBUG_MSGTL(("read_config", "%s: %s\n", filename,
                        strerror(errno)));
        } else
        {
            Logger_logPerror(filename);
        }
        return ErrorCode_GENERR;
    }

#define CONFIG_MAX_FILES 4096
    if (files > CONFIG_MAX_FILES) {
        ReadConfig_error("maximum conf file count (%d) exceeded\n",
                             CONFIG_MAX_FILES);
    fclose(ifile);
        return ErrorCode_GENERR;
    }
#define CONFIG_MAX_RECURSE_DEPTH 16
    if (depth > CONFIG_MAX_RECURSE_DEPTH) {
        ReadConfig_error("nested include depth > %d\n",
                             CONFIG_MAX_RECURSE_DEPTH);
    fclose(ifile);
        return ErrorCode_GENERR;
    }

    readConfig_linecount = 0;
    readConfig_curfilename = filename;

    ++files;
    ++depth;

    DEBUG_MSGTL(("read_config:file", "Reading configuration %s (%d)\n",
                filename, when));

    while (ifile) {
        size_t              linelen = 0; /* strlen of the current line */
        char               *cptr;
        struct ReadConfig_ConfigLine_s *lptr = line_handler;

        for (;;) {
            if (linesize <= linelen + 1) {
                char *tmp = (char *)realloc(line, linesize + 256);
                if (tmp) {
                    line = tmp;
                    linesize += 256;
                } else {
                    ReadConfig_error("Failed to allocate memory\n");
                    free(line);
                    fclose(ifile);
                    return ErrorCode_GENERR;
                }
            }
            if (fgets(line + linelen, linesize - linelen, ifile) == NULL) {
                line[linelen] = '\0';
                fclose (ifile);
                ifile = NULL;
                break;
            }

            linelen += strlen(line + linelen);

            if (line[linelen - 1] == '\n') {
              line[linelen - 1] = '\0';
              break;
            }
        }

        ++readConfig_linecount;
        DEBUG_MSGTL(("9:read_config:line", "%s:%d examining: %s\n",
                    filename, readConfig_linecount, line));
        /*
         * check blank line or # comment
         */
        if ((cptr = ReadConfig_skipWhite(line))) {
            char token[READCONFIG_STRINGMAX];

            cptr = ReadConfig_copyNword(cptr, token, sizeof(token));
            if (token[0] == '[') {
                if (token[strlen(token) - 1] != ']') {
            ReadConfig_error("no matching ']' for type %s.",
                     &token[1]);
                    continue;
                }
                token[strlen(token) - 1] = '\0';
                lptr = ReadConfig_getHandlers(&token[1]);
                if (lptr == NULL) {
            ReadConfig_error("No handlers regestered for type %s.",
                     &token[1]);
                    continue;
                }
                DEBUG_MSGTL(("read_config:context",
                            "Switching to new context: %s%s\n",
                            ((cptr) ? "(this line only) " : ""),
                            &token[1]));
                if (cptr == NULL) {
                    /*
                     * change context permanently
                     */
                    line_handler = lptr;
                    continue;
                } else {
                    /*
                     * the rest of this line only applies.
                     */
                    cptr = ReadConfig_copyNword(cptr, token, sizeof(token));
                }
            } else if ((token[0] == 'i') && (strncasecmp(token,"include", 7 )==0)) {
                if ( strcasecmp( token, "include" )==0) {
                    if (when != PREMIB_CONFIG &&
                    !DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                                DSLIB_BOOLEAN.NO_TOKEN_WARNINGS)) {
                    ReadConfig_warn("Ambiguous token '%s' - use 'includeSearch' (or 'includeFile') instead.", token);
                    }
                    continue;
                } else if ( strcasecmp( token, "includedir" )==0) {
                    DIR *d;
                    struct dirent *entry;
                    char  fname[TOOLS_MAXPATH];
                    int   len;

                    if (cptr == NULL) {
                        if (when != PREMIB_CONFIG)
                    ReadConfig_error("Blank line following %s token.", token);
                        continue;
                    }
                    if ((d=opendir(cptr)) == NULL ) {
                        if (when != PREMIB_CONFIG)
                            ReadConfig_error("Can't open include dir '%s'.", cptr);
                        continue;
                    }
                    while ((entry = readdir( d )) != NULL ) {
                        if ( entry->d_name[0] != '.') {
                            len = READCONFIG_NAMLEN(entry);
                            if ((len > 5) && (strcmp(&(entry->d_name[len-5]),".conf") == 0)) {
                                snprintf(fname, TOOLS_MAXPATH, "%s/%s",
                                         cptr, entry->d_name);
                                (void)ReadConfig_readConfig(fname, line_handler, when);
                            }
                        }
                    }
                    closedir(d);
                    continue;
                } else if ( strcasecmp( token, "includefile" )==0) {
                    char  fname[TOOLS_MAXPATH], *cp;

                    if (cptr == NULL) {
                        if (when != PREMIB_CONFIG)
                    ReadConfig_error("Blank line following %s token.", token);
                        continue;
                    }
                    if ( cptr[0] == '/' ) {
                        Strlcpy_strlcpy(fname, cptr, TOOLS_MAXPATH);
                    } else {
                        Strlcpy_strlcpy(fname, filename, TOOLS_MAXPATH);
                        cp = strrchr(fname, '/');
                        if (!cp)
                            fname[0] = '\0';
                        else
                            *(++cp) = '\0';
                        Strlcat_strlcat(fname, cptr, TOOLS_MAXPATH);
                    }
                    if (ReadConfig_readConfig(fname, line_handler, when) !=
                        ErrorCode_SUCCESS && when != PREMIB_CONFIG)
                        ReadConfig_error("Included file '%s' not found.",
                                             fname);
                    continue;
                } else if ( strcasecmp( token, "includesearch" )==0) {
                    struct ReadConfig_ConfigFiles_s ctmp;
                    int len, ret;

                    if (cptr == NULL) {
                        if (when != PREMIB_CONFIG)
                    ReadConfig_error("Blank line following %s token.", token);
                        continue;
                    }
                    len = strlen(cptr);
                    ctmp.fileHeader = cptr;
                    ctmp.start = line_handler;
                    ctmp.next = NULL;
                    if ((len > 5) && (strcmp(&cptr[len-5],".conf") == 0))
                       cptr[len-5] = 0; /* chop off .conf */
                    ret = ReadConfig_filesOfType(when,&ctmp);
                    if ((len > 5) && (cptr[len-5] == 0))
                       cptr[len-5] = '.'; /* restore .conf */
                    if (( ret != ErrorCode_SUCCESS ) && (when != PREMIB_CONFIG))
                ReadConfig_error("Included config '%s' not found.", cptr);
                    continue;
                } else {
                    lptr = line_handler;
                }
            } else {
                lptr = line_handler;
            }
            if (cptr == NULL) {
        ReadConfig_error("Blank line following %s token.", token);
            } else {
                DEBUG_MSGTL(("read_config:line", "%s:%d examining: %s\n",
                            filename, readConfig_linecount, line));
                ReadConfig_runConfigHandler(lptr, token, cptr, when);
            }
        }
    }
    free(line);
    readConfig_linecount = prev_linecount;
    readConfig_curfilename = prev_filename;
    --depth;
    return ErrorCode_SUCCESS;

}                               /* end read_config() */



void
ReadConfig_freeConfig(void)
{
    struct ReadConfig_ConfigFiles_s *ctmp = readConfig_configFiles;
    struct ReadConfig_ConfigLine_s *ltmp;

    for (; ctmp != NULL; ctmp = ctmp->next)
        for (ltmp = ctmp->start; ltmp != NULL; ltmp = ltmp->next)
            if (ltmp->free_func)
                (*(ltmp->free_func)) ();
}

/*
 * Return ErrorCode_SUCCESS if any config files are processed
 * Return ErrorCode_GENERR if _no_ config files are processed
 *    Whether this is actually an error is left to the application
 */
int
ReadConfig_optional(const char *optional_config, int when)
{
    char *newp, *cp, *st = NULL;
    int              ret = ErrorCode_GENERR;
    char *type = DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                       DSLIB_STRING.APPTYPE);

    if ((NULL == optional_config) || (NULL == type))
        return ret;

    DEBUG_MSGTL(("ReadConfig_optional",
                "reading optional configuration tokens for %s\n", type));

    newp = strdup(optional_config);      /* strtok_r messes it up */
    cp = strtok_r(newp, ",", &st);
    while (cp) {
        struct stat     statbuf;
        if (stat(cp, &statbuf)) {
            DEBUG_MSGTL(("read_config",
                        "Optional File \"%s\" does not exist.\n", cp));
            Logger_logPerror(cp);
        } else {
            DEBUG_MSGTL(("read_config:opt",
                        "Reading optional config file: \"%s\"\n", cp));
            if ( ReadConfig_withTypeWhen(cp, type, when) == ErrorCode_SUCCESS )
                ret = ErrorCode_SUCCESS;
        }
        cp = strtok_r(NULL, ",", &st);
    }
    free(newp);
    return ret;
}

void
ReadConfig_readConfigs(void)
{
    char *optional_config = DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                           DSLIB_STRING.OPTIONALCONFIG);

    Callback_callCallbacks(CALLBACK_LIBRARY,
                        CALLBACK_PRE_READ_CONFIG, NULL);

    DEBUG_MSGTL(("read_config", "reading normal configuration tokens\n"));

    if ((NULL != optional_config) && (*optional_config == '-')) {
        (void)ReadConfig_optional(++optional_config, NORMAL_CONFIG);
        optional_config = NULL; /* clear, so we don't read them twice */
    }

    (void)ReadConfig_files(NORMAL_CONFIG);

    /*
     * do this even when the normal above wasn't done
     */
    if (NULL != optional_config)
        (void)ReadConfig_optional(optional_config, NORMAL_CONFIG);

    ReadConfig_processMemoriesWhen(NORMAL_CONFIG, 1);

    DefaultStore_setBoolean(DSSTORAGE.LIBRARY_ID,
               DSLIB_BOOLEAN.HAVE_READ_CONFIG, 1);

    Callback_callCallbacks(CALLBACK_LIBRARY,
                           CALLBACK_POST_READ_CONFIG, NULL);
}

void
ReadConfig_readPremibConfigs(void)
{
    char *optional_config = DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                           DSLIB_STRING.OPTIONALCONFIG);

    Callback_callCallbacks(CALLBACK_LIBRARY,
                           CALLBACK_PRE_PREMIB_READ_CONFIG, NULL);

    DEBUG_MSGTL(("read_config", "reading premib configuration tokens\n"));

    if ((NULL != optional_config) && (*optional_config == '-')) {
        (void)ReadConfig_optional(++optional_config, PREMIB_CONFIG);
        optional_config = NULL; /* clear, so we don't read them twice */
    }

    (void)ReadConfig_files(PREMIB_CONFIG);

    if (NULL != optional_config)
        (void)ReadConfig_optional(optional_config, PREMIB_CONFIG);

    ReadConfig_processMemoriesWhen(PREMIB_CONFIG, 0);

    DefaultStore_setBoolean(DSSTORAGE.LIBRARY_ID,
               DSLIB_BOOLEAN.HAVE_READ_PREMIB_CONFIG, 1);

    Callback_callCallbacks(CALLBACK_LIBRARY,
                           CALLBACK_POST_PREMIB_READ_CONFIG, NULL);
}

/*******************************************************************-o-******
 * set_configuration_directory
 *
 * Parameters:
 *      char *dir - value of the directory
 * Sets the configuration directory. Multiple directories can be
 * specified, but need to be seperated by 'ENV_SEPARATOR_CHAR'.
 */
void
ReadConfig_setConfigurationDirectory(const char *dir)
{
    DefaultStore_setString(DSSTORAGE.LIBRARY_ID,
              DSLIB_STRING.CONFIGURATION_DIR, dir);
}

/*******************************************************************-o-******
 * get_configuration_directory
 *
 * Parameters: -
 * Retrieve the configuration directory or directories.
 * (For backwards compatibility that is:
 *       SNMPCONFPATH, SNMPSHAREPATH, SNMPLIBPATH, HOME/.snmp
 * First check whether the value is set.
 * If not set give it the default value.
 * Return the value.
 * We always retrieve it new, since we have to do it anyway if it is just set.
 */
const char *
ReadConfig_getConfigurationDirectory(void)
{
    char            defaultPath[IMPL_SPRINT_MAX_LEN];
    char           *homepath;

    if (NULL == DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                      DSLIB_STRING.CONFIGURATION_DIR)) {
        homepath = Tools_getenv("HOME");
        snprintf(defaultPath, sizeof(defaultPath), "%s%c%s%c%s%s%s%s",
                SNMPCONFPATH, ENV_SEPARATOR_CHAR,
                SNMPSHAREPATH, ENV_SEPARATOR_CHAR, SNMPLIBPATH,
                ((homepath == NULL) ? "" : ENV_SEPARATOR),
                ((homepath == NULL) ? "" : homepath),
                ((homepath == NULL) ? "" : "/.snmp"));
        defaultPath[ sizeof(defaultPath)-1 ] = 0;
        ReadConfig_setConfigurationDirectory(defaultPath);
    }
    return (DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                  DSLIB_STRING.CONFIGURATION_DIR));
}

/*******************************************************************-o-******
 * set_persistent_directory
 *
 * Parameters:
 *      char *dir - value of the directory
 * Sets the configuration directory.
 * No multiple directories may be specified.
 * (However, this is not checked)
 */
void
ReadConfig_setPersistentDirectory(const char *dir)
{
    DefaultStore_setString(DSSTORAGE.LIBRARY_ID,
              DSLIB_STRING.PERSISTENT_DIR, dir);
}

/*******************************************************************-o-******
 * ReadConfig_getPersistentDirectory
 *
 * Parameters: -
 * Function will retrieve the persisten directory value.
 * First check whether the value is set.
 * If not set give it the default value.
 * Return the value.
 * We always retrieve it new, since we have to do it anyway if it is just set.
 */
const char *
ReadConfig_getPersistentDirectory(void)
{
    if (NULL == DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                      DSLIB_STRING.PERSISTENT_DIR)) {

        const char *persdir = Tools_getenv("SNMP_PERSISTENT_DIR");
        if (NULL == persdir)
            persdir = PERSISTENT_DIRECTORY;
        ReadConfig_setPersistentDirectory(persdir);
    }
    return (DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                  DSLIB_STRING.PERSISTENT_DIR));
}

/*******************************************************************-o-******
 * set_temp_file_pattern
 *
 * Parameters:
 *      char *pattern - value of the file pattern
 * Sets the temp file pattern.
 * Multiple patterns may not be specified.
 * (However, this is not checked)
 */
void
ReadConfig_setTempFilePattern(const char *pattern)
{
    DefaultStore_setString(DSSTORAGE.LIBRARY_ID,
              DSLIB_STRING.TEMP_FILE_PATTERN, pattern);
}

/*******************************************************************-o-******
 * get_temp_file_pattern
 *
 * Parameters: -
 * Function will retrieve the temp file pattern value.
 * First check whether the value is set.
 * If not set give it the default value.
 * Return the value.
 * We always retrieve it new, since we have to do it anyway if it is just set.
 */
const char     *
ReadConfig_getTempFilePattern(void)
{
    if (NULL == DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                      DSLIB_STRING.TEMP_FILE_PATTERN)) {
        ReadConfig_setTempFilePattern(NETSNMP_TEMP_FILE_PATTERN);
    }
    return (DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                  DSLIB_STRING.TEMP_FILE_PATTERN));
}

/**
 * utility routine for read_config_files
 *
 * Return ErrorCode_SUCCESS if any config files are processed
 * Return ErrorCode_GENERR if _no_ config files are processed
 *    Whether this is actually an error is left to the application
 */
static int
ReadConfig_filesInPath(const char *path, struct ReadConfig_ConfigFiles_s *ctmp,
                          int when, const char *perspath, const char *persfile)
{
    int             done, j;
    char            configfile[300];
    char           *cptr1, *cptr2, *envconfpath;
    struct stat     statbuf;
    int             ret = ErrorCode_GENERR;

    if ((NULL == path) || (NULL == ctmp))
        return ErrorCode_GENERR;

    envconfpath = strdup(path);

    DEBUG_MSGTL(("read_config:path", " config path used for %s:%s (persistent path:%s)\n",
                ctmp->fileHeader, envconfpath, perspath));
    cptr1 = cptr2 = envconfpath;
    done = 0;
    while ((!done) && (*cptr2 != 0)) {
        while (*cptr1 != 0 && *cptr1 != ENV_SEPARATOR_CHAR)
            cptr1++;
        if (*cptr1 == 0)
            done = 1;
        else
            *cptr1 = 0;

        DEBUG_MSGTL(("read_config:dir", " config dir: %s\n", cptr2 ));
        if (stat(cptr2, &statbuf) != 0) {
            /*
             * Directory not there, continue
             */
            DEBUG_MSGTL(("read_config:dir", " Directory not present: %s\n", cptr2 ));
            cptr2 = ++cptr1;
            continue;
        }
        if (!S_ISDIR(statbuf.st_mode)) {
            /*
             * Not a directory, continue
             */
            DEBUG_MSGTL(("read_config:dir", " Not a directory: %s\n", cptr2 ));
            cptr2 = ++cptr1;
            continue;
        }

        /*
         * for proper persistent storage retrieval, we need to read old backup
         * copies of the previous storage files.  If the application in
         * question has died without the proper call to snmp_clean_persistent,
         * then we read all the configuration files we can, starting with
         * the oldest first.
         */
        if (strncmp(cptr2, perspath, strlen(perspath)) == 0 ||
            (persfile != NULL &&
             strncmp(cptr2, persfile, strlen(persfile)) == 0)) {
            DEBUG_MSGTL(("read_config:persist", " persist dir: %s\n", cptr2 ));
            /*
             * limit this to the known storage directory only
             */
            for (j = 0; j <= MAX_PERSISTENT_BACKUPS; j++) {
                snprintf(configfile, sizeof(configfile),
                         "%s/%s.%d.conf", cptr2,
                         ctmp->fileHeader, j);
                configfile[ sizeof(configfile)-1 ] = 0;
                if (stat(configfile, &statbuf) != 0) {
                    /*
                     * file not there, continue
                     */
                    break;
                } else {
                    /*
                     * backup exists, read it
                     */
                    DEBUG_MSGTL(("read_config_files",
                                "old config file found: %s, parsing\n",
                                configfile));
                    if (ReadConfig_readConfig(configfile, ctmp->start, when) == ErrorCode_SUCCESS)
                        ret = ErrorCode_SUCCESS;
                }
            }
        }
        snprintf(configfile, sizeof(configfile),
                 "%s/%s.conf", cptr2, ctmp->fileHeader);
        configfile[ sizeof(configfile)-1 ] = 0;
        if (ReadConfig_readConfig(configfile, ctmp->start, when) == ErrorCode_SUCCESS)
            ret = ErrorCode_SUCCESS;
        snprintf(configfile, sizeof(configfile),
                 "%s/%s.local.conf", cptr2, ctmp->fileHeader);
        configfile[ sizeof(configfile)-1 ] = 0;
        if (ReadConfig_readConfig(configfile, ctmp->start, when) == ErrorCode_SUCCESS)
            ret = ErrorCode_SUCCESS;

        if(done)
            break;

        cptr2 = ++cptr1;
    }
    TOOLS_FREE(envconfpath);
    return ret;
}

/*******************************************************************-o-******
 * read_config_files
 *
 * Parameters:
 *	when	== PREMIB_CONFIG, NORMAL_CONFIG  -or-  EITHER_CONFIG
 *
 *
 * Traverse the list of config file types, performing the following actions
 * for each --
 *
 * First, build a search path for config files.  If the contents of
 * environment variable SNMPCONFPATH are NULL, then use the following
 * path list (where the last entry exists only if HOME is non-null):
 *
 *	SNMPSHAREPATH:SNMPLIBPATH:${HOME}/.snmp
 *
 * Then, In each of these directories, read config files by the name of:
 *
 *	<dir>/<fileHeader>.conf		-AND-
 *	<dir>/<fileHeader>.local.conf
 *
 * where <fileHeader> is taken from the config file type structure.
 *
 *
 * PREMIB_CONFIG causes ReadConfig_freeConfig() to be invoked prior to any other action.
 *
 *
 * EXITs if any 'readConfig_configErrors' are logged while parsing config file lines.
 *
 * Return ErrorCode_SUCCESS if any config files are processed
 * Return ErrorCode_GENERR if _no_ config files are processed
 *    Whether this is actually an error is left to the application
 */
int ReadConfig_filesOfType(int when, struct ReadConfig_ConfigFiles_s *ctmp)
{
    const char     *confpath, *persfile, *envconfpath;
    char           *perspath;
    int             ret = ErrorCode_GENERR;

    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                               DSLIB_BOOLEAN.DONT_PERSIST_STATE)
        || DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                                  DSLIB_BOOLEAN.DONT_READ_CONFIGS)
        || (NULL == ctmp)) return ret;

    /*
     * these shouldn't change
     */
    confpath = ReadConfig_getConfigurationDirectory();
    persfile = Tools_getenv("SNMP_PERSISTENT_FILE");
    envconfpath = Tools_getenv("SNMPCONFPATH");


        /*
         * read the config files. strdup() the result of
         * ReadConfig_getPersistentDirectory() to avoid that parsing the "persistentDir"
         * keyword transforms the perspath pointer into a dangling pointer.
         */
        perspath = strdup(ReadConfig_getPersistentDirectory());
        if (envconfpath == NULL) {
            /*
             * read just the config files (no persistent stuff), since
             * persistent path can change via conf file. Then get the
             * current persistent directory, and read files there.
             */
            if ( ReadConfig_filesInPath(confpath, ctmp, when, perspath,
                                      persfile) == ErrorCode_SUCCESS )
                ret = ErrorCode_SUCCESS;
            free(perspath);
            perspath = strdup(ReadConfig_getPersistentDirectory());
            if ( ReadConfig_filesInPath(perspath, ctmp, when, perspath,
                                      persfile) == ErrorCode_SUCCESS )
                ret = ErrorCode_SUCCESS;
        }
        else {
            /*
             * only read path specified by user
             */
            if ( ReadConfig_filesInPath(envconfpath, ctmp, when, perspath,
                                      persfile) == ErrorCode_SUCCESS )
                ret = ErrorCode_SUCCESS;
        }
        free(perspath);
        return ret;
}

/*
 * Return ErrorCode_SUCCESS if any config files are processed
 * Return ErrorCode_GENERR if _no_ config files are processed
 *    Whether this is actually an error is left to the application
 */
int ReadConfig_files(int when) {

    struct ReadConfig_ConfigFiles_s *ctmp = readConfig_configFiles;
    int                  ret  = ErrorCode_GENERR;

    readConfig_configErrors = 0;

    if (when == PREMIB_CONFIG)
        ReadConfig_freeConfig();

    /*
     * read all config file types
     */
    for (; ctmp != NULL; ctmp = ctmp->next) {
        if ( ReadConfig_filesOfType(when, ctmp) == ErrorCode_SUCCESS )
            ret = ErrorCode_SUCCESS;
    }

    if (readConfig_configErrors) {
        Logger_log(LOGGER_PRIORITY_ERR, "net-snmp: %d error(s) in config file(s)\n",
                 readConfig_configErrors);
    }
    return ret;
}

void ReadConfig_printUsage(const char *lead)
{
    struct ReadConfig_ConfigFiles_s *ctmp = readConfig_configFiles;
    struct ReadConfig_ConfigLine_s *ltmp;

    if (lead == NULL)
        lead = "";

    for (ctmp = readConfig_configFiles; ctmp != NULL; ctmp = ctmp->next) {
        Logger_log(LOGGER_PRIORITY_INFO, "%sIn %s.conf and %s.local.conf:\n", lead,
                 ctmp->fileHeader, ctmp->fileHeader);
        for (ltmp = ctmp->start; ltmp != NULL; ltmp = ltmp->next) {
            DEBUG_IF("read_config_usage") {
                if (ltmp->config_time == PREMIB_CONFIG)
                    DEBUG_MSG(("read_config_usage", "*"));
                else
                    DEBUG_MSG(("read_config_usage", " "));
            }
            if (ltmp->help) {
                Logger_log(LOGGER_PRIORITY_INFO, "%s%s%-24s %s\n", lead, lead,
                         ltmp->config_token, ltmp->help);
            } else {
                DEBUG_IF("read_config_usage") {
                    Logger_log(LOGGER_PRIORITY_INFO, "%s%s%-24s [NO HELP]\n", lead, lead,
                             ltmp->config_token);
                }
            }
        }
    }
}

/**
 * read_config_store intended for use by applications to store permenant
 * configuration information generated by sets or persistent counters.
 * Appends line to a file named either ENV(SNMP_PERSISTENT_FILE) or
 *   "<NETSNMP_PERSISTENT_DIRECTORY>/<type>.conf".
 * Adds a trailing newline to the stored file if necessary.
 *
 * @param type is the application name
 * @param line is the configuration line written to the application name's
 * configuration file
 *
 * @return void
  */
void
ReadConfig_store(const char *type, const char *line)
{
    char            file[512], *filep;
    FILE           *fout;
    mode_t          oldmask;

    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                               DSLIB_BOOLEAN.DONT_PERSIST_STATE)
     || DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                               DSLIB_BOOLEAN.DISABLE_PERSISTENT_LOAD)) return;

    /*
     * store configuration directives in the following order of preference:
     * 1. ENV variable SNMP_PERSISTENT_FILE
     * 2. configured <NETSNMP_PERSISTENT_DIRECTORY>/<type>.conf
     */
    if ((filep = Tools_getenv("SNMP_PERSISTENT_FILE")) == NULL) {
        snprintf(file, sizeof(file),
                 "%s/%s.conf", ReadConfig_getPersistentDirectory(), type);
        file[ sizeof(file)-1 ] = 0;
        filep = file;
    }
    oldmask = umask(PERSISTENT_MASK);

    if (System_mkdirhier(filep, AGENT_DIRECTORY_MODE, 1)) {
        Logger_log(LOGGER_PRIORITY_ERR,
                 "Failed to create the persistent directory for %s\n",
                 file);
    }
    if ((fout = fopen(filep, "a")) != NULL) {
        fprintf(fout, "%s", line);
        if (line[strlen(line)] != '\n')
            fprintf(fout, "\n");
        DEBUG_MSGTL(("read_config:store", "storing: %s\n", line));
        fclose(fout);
    } else {
        Logger_log(LOGGER_PRIORITY_ERR, "read_config_store open failure on %s\n", filep);
    }
    umask(oldmask);

}                               /* end read_config_store() */

void
ReadConfig_readAppConfigStore(const char *line)
{
    ReadConfig_store(DefaultStore_getString(DSSTORAGE.LIBRARY_ID,
                        DSLIB_STRING.APPTYPE), line);
}




/*******************************************************************-o-******
 * snmp_save_persistent
 *
 * Parameters:
 *	*type
 *
 *
 * Save the file "<NETSNMP_PERSISTENT_DIRECTORY>/<type>.conf" into a backup copy
 * called "<NETSNMP_PERSISTENT_DIRECTORY>/<type>.%d.conf", which %d is an
 * incrementing number on each call, but less than NETSNMP_MAX_PERSISTENT_BACKUPS.
 *
 * Should be called just before all persistent information is supposed to be
 * written to move aside the existing persistent cache.
 * snmp_clean_persistent should then be called afterward all data has been
 * saved to remove these backup files.
 *
 * Note: on an rename error, the files are removed rather than saved.
 *
 */
void
ReadConfig_savePersistent(const char *type)
{
    char            file[512], fileold[IMPL_SPRINT_MAX_LEN];
    struct stat     statbuf;
    int             j;

    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                               DSLIB_BOOLEAN.DONT_PERSIST_STATE)
     || DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                               DSLIB_BOOLEAN.DISABLE_PERSISTENT_SAVE)) return;

    DEBUG_MSGTL(("snmp_save_persistent", "saving %s files...\n", type));
    snprintf(file, sizeof(file),
             "%s/%s.conf", ReadConfig_getPersistentDirectory(), type);
    file[ sizeof(file)-1 ] = 0;
    if (stat(file, &statbuf) == 0) {
        for (j = 0; j <= MAX_PERSISTENT_BACKUPS; j++) {
            snprintf(fileold, sizeof(fileold),
                     "%s/%s.%d.conf", ReadConfig_getPersistentDirectory(), type, j);
            fileold[ sizeof(fileold)-1 ] = 0;
            if (stat(fileold, &statbuf) != 0) {
                DEBUG_MSGTL(("snmp_save_persistent",
                            " saving old config file: %s -> %s.\n", file,
                            fileold));
                if (rename(file, fileold)) {
                    Logger_log(LOGGER_PRIORITY_ERR, "Cannot rename %s to %s\n", file, fileold);
                     /* moving it failed, try nuking it, as leaving
                      * it around is very bad. */
                    if (unlink(file) == -1)
                        Logger_log(LOGGER_PRIORITY_ERR, "Cannot unlink %s\n", file);
                }
                break;
            }
        }
    }
    /*
     * save a warning header to the top of the new file
     */
    snprintf(fileold, sizeof(fileold),
            "%s%s# Please save normal configuration tokens for %s in SNMPCONFPATH/%s.conf.\n# Only \"createUser\" tokens should be placed here by %s administrators.\n%s",
            "#\n# net-snmp (or ucd-snmp) persistent data file.\n#\n############################################################################\n# STOP STOP STOP STOP STOP STOP STOP STOP STOP \n",
            "#\n#          **** DO NOT EDIT THIS FILE ****\n#\n# STOP STOP STOP STOP STOP STOP STOP STOP STOP \n############################################################################\n#\n# DO NOT STORE CONFIGURATION ENTRIES HERE.\n",
            type, type, type,
        "# (Did I mention: do not edit this file?)\n#\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    fileold[ sizeof(fileold)-1 ] = 0;
    ReadConfig_store(type, fileold);
}


/*******************************************************************-o-******
 * snmp_clean_persistent
 *
 * Parameters:
 *	*type
 *
 *
 * Unlink all backup files called "<NETSNMP_PERSISTENT_DIRECTORY>/<type>.%d.conf".
 *
 * Should be called just after we successfull dumped the last of the
 * persistent data, to remove the backup copies of previous storage dumps.
 *
 * XXX  Worth overwriting with random bytes first?  This would
 *	ensure that the data is destroyed, even a buffer containing the
 *	data persists in memory or swap.  Only important if secrets
 *	will be stored here.
 */
void
ReadConfig_cleanPersistent(const char *type)
{
    char            file[512];
    struct stat     statbuf;
    int             j;

    if (DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                               DSLIB_BOOLEAN.DONT_PERSIST_STATE)
     || DefaultStore_getBoolean(DSSTORAGE.LIBRARY_ID,
                               DSLIB_BOOLEAN.DISABLE_PERSISTENT_SAVE)) return;

    DEBUG_MSGTL(("snmp_clean_persistent", "cleaning %s files...\n", type));
    snprintf(file, sizeof(file),
             "%s/%s.conf", ReadConfig_getPersistentDirectory(), type);
    file[ sizeof(file)-1 ] = 0;
    if (stat(file, &statbuf) == 0) {
        for (j = 0; j <= MAX_PERSISTENT_BACKUPS; j++) {
            snprintf(file, sizeof(file),
                     "%s/%s.%d.conf", ReadConfig_getPersistentDirectory(), type, j);
            file[ sizeof(file)-1 ] = 0;
            if (stat(file, &statbuf) == 0) {
                DEBUG_MSGTL(("snmp_clean_persistent",
                            " removing old config file: %s\n", file));
                if (unlink(file) == -1)
                    Logger_log(LOGGER_PRIORITY_ERR, "Cannot unlink %s\n", file);
            }
        }
    }
}




/*
 * ReadConfig_configPerror: prints a warning string associated with a file and
 * line number of a .conf file and increments the error count.
 */
static void ReadConfig_configVlog(int level, const char *levelmsg, const char *str, va_list args)
{
    char tmpbuf[256];
    char* buf = tmpbuf;
    int len = snprintf(tmpbuf, sizeof(tmpbuf), "%s: line %d: %s: %s\n",
               readConfig_curfilename, readConfig_linecount, levelmsg, str);
    if (len >= (int)sizeof(tmpbuf)) {
    buf = (char*)malloc(len + 1);
    sprintf(buf, "%s: line %d: %s: %s\n",
        readConfig_curfilename, readConfig_linecount, levelmsg, str);
    }
    Logger_vlog(level, buf, args);
    if (buf != tmpbuf)
    free(buf);
}

void
ReadConfig_error(const char *str, ...)
{
    va_list args;
    va_start(args, str);
    ReadConfig_configVlog(LOGGER_PRIORITY_ERR, "Error", str, args);
    va_end(args);
    readConfig_configErrors++;
}

void
ReadConfig_warn(const char *str, ...)
{
    va_list args;
    va_start(args, str);
    ReadConfig_configVlog(LOGGER_PRIORITY_WARNING, "Warning", str, args);
    va_end(args);
}

void
ReadConfig_configPerror(const char *str)
{
    ReadConfig_error("%s", str);
}

void
ReadConfig_configPwarn(const char *str)
{
    ReadConfig_warn("%s", str);
}

/*
 * skip all white spaces and return 1 if found something either end of
 * line or a comment character
 */
char   *
ReadConfig_skipWhite(char *ptr)
{
    return TOOLS_REMOVE_CONST(char *, ReadConfig_skipWhiteConst(ptr));
}

const char  *
ReadConfig_skipWhiteConst(const char *ptr)
{
    if (ptr == NULL)
        return (NULL);
    while (*ptr != 0 && isspace((unsigned char)*ptr))
        ptr++;
    if (*ptr == 0 || *ptr == '#')
        return (NULL);
    return (ptr);
}

char *
ReadConfig_skipNotWhite(char *ptr)
{
    return TOOLS_REMOVE_CONST(char *, ReadConfig_skipNotWhiteConst(ptr));
}

const char *
ReadConfig_skipNotWhiteConst(const char *ptr)
{
    if (ptr == NULL)
        return (NULL);
    while (*ptr != 0 && !isspace((unsigned char)*ptr))
        ptr++;
    if (*ptr == 0 || *ptr == '#')
        return (NULL);
    return (ptr);
}

char  *
ReadConfig_skipToken(char *ptr)
{
    return TOOLS_REMOVE_CONST(char *, ReadConfig_skipTokenConst(ptr));
}

const char     *
ReadConfig_skipTokenConst(const char *ptr)
{
    ptr = ReadConfig_skipWhiteConst(ptr);
    ptr = ReadConfig_skipNotWhiteConst(ptr);
    ptr = ReadConfig_skipWhiteConst(ptr);
    return (ptr);
}

/*
 * copy_word
 * copies the next 'token' from 'from' into 'to', maximum len-1 characters.
 * currently a token is anything seperate by white space
 * or within quotes (double or single) (i.e. "the red rose"
 * is one token, \"the red rose\" is three tokens)
 * a '\' character will allow a quote character to be treated
 * as a regular character
 * It returns a pointer to first non-white space after the end of the token
 * being copied or to 0 if we reach the end.
 * Note: Partially copied words (greater than len) still returns a !NULL ptr
 * Note: partially copied words are, however, null terminated.
 */

char *
ReadConfig_copyNword(char *from, char *to, int len)
{
    return TOOLS_REMOVE_CONST(char *, ReadConfig_copyNwordConst(from, to, len));
}

const char *
ReadConfig_copyNwordConst(const char *from, char *to, int len)
{
    char            quote;
    if (!from || !to)
        return NULL;
    if ((*from == '\"') || (*from == '\'')) {
        quote = *(from++);
        while ((*from != quote) && (*from != 0)) {
            if ((*from == '\\') && (*(from + 1) != 0)) {
                if (len > 0) {  /* don't copy beyond len bytes */
                    *to++ = *(from + 1);
                    if (--len == 0)
                        *(to - 1) = '\0';       /* null protect the last spot */
                }
                from = from + 2;
            } else {
                if (len > 0) {  /* don't copy beyond len bytes */
                    *to++ = *from++;
                    if (--len == 0)
                        *(to - 1) = '\0';       /* null protect the last spot */
                } else
                    from++;
            }
        }
        if (*from == 0) {
            DEBUG_MSGTL(("read_config_copy_word",
                        "no end quote found in config string\n"));
        } else
            from++;
    } else {
        while (*from != 0 && !isspace((unsigned char)(*from))) {
            if ((*from == '\\') && (*(from + 1) != 0)) {
                if (len > 0) {  /* don't copy beyond len bytes */
                    *to++ = *(from + 1);
                    if (--len == 0)
                        *(to - 1) = '\0';       /* null protect the last spot */
                }
                from = from + 2;
            } else {
                if (len > 0) {  /* don't copy beyond len bytes */
                    *to++ = *from++;
                    if (--len == 0)
                        *(to - 1) = '\0';       /* null protect the last spot */
                } else
                    from++;
            }
        }
    }
    if (len > 0)
        *to = 0;
    from = ReadConfig_skipWhiteConst(from);
    return (from);
}                               /* ReadConfig_copyNword */

/*
 * copy_word
 * copies the next 'token' from 'from' into 'to'.
 * currently a token is anything seperate by white space
 * or within quotes (double or single) (i.e. "the red rose"
 * is one token, \"the red rose\" is three tokens)
 * a '\' character will allow a quote character to be treated
 * as a regular character
 * It returns a pointer to first non-white space after the end of the token
 * being copied or to 0 if we reach the end.
 */

static int      readConfig_haveWarned = 0;
char           *
ReadConfig_copyWord(char *from, char *to)
{
    if (!readConfig_haveWarned) {
        Logger_log(LOGGER_PRIORITY_INFO,
                 "copy_word() called.  Use ReadConfig_copyNword() instead.\n");
        readConfig_haveWarned = 1;
    }
    return ReadConfig_copyNword(from, to, IMPL_SPRINT_MAX_LEN);
}                               /* copy_word */

/**
 * Stores an quoted version of the first len bytes from str into saveto.
 *
 * If all octets in str are in the set [[:alnum:] ] then the quotation
 * is to enclose the string in quotation marks ("str") otherwise the
 * quotation is to prepend the string 0x and then add the hex representation
 * of all characters from str (0x737472)
 *
 * @param[in] saveto pointer to output stream, is assumed to be big enough.
 * @param[in] str pointer of the data that is to be stored.
 * @param[in] len length of the data that is to be stored.
 * @return A pointer to saveto after str is added to it.
 */
char *
ReadConfig_saveOctetString(char *saveto, const u_char * str, size_t len)
{
    size_t          i;
    const u_char   *cp;

    /*
     * is everything easily printable
     */
    for (i = 0, cp = str; i < len && cp &&
         (isalpha(*cp) || isdigit(*cp) || *cp == ' '); cp++, i++);

    if (len != 0 && i == len) {
        *saveto++ = '"';
        memcpy(saveto, str, len);
        saveto += len;
        *saveto++ = '"';
        *saveto = '\0';
    } else {
        if (str != NULL) {
            sprintf(saveto, "0x");
            saveto += 2;
            for (i = 0; i < len; i++) {
                sprintf(saveto, "%02x", str[i]);
                saveto = saveto + 2;
            }
        } else {
            sprintf(saveto, "\"\"");
            saveto += 2;
        }
    }
    return saveto;
}

/**
 * Reads an octet string that was saved by the
 * read_config_save_octet_string() function.
 *
 * @param[in]     readfrom Pointer to the input data to be parsed.
 * @param[in,out] str      Pointer to the output buffer pointer. The data
 *   written to the output buffer will be '\0'-terminated. If *str == NULL,
 *   an output buffer will be allocated that is one byte larger than the
 *   data stored.
 * @param[in,out] len      If str != NULL, *len is the size of the buffer *str
 *   points at. If str == NULL, the value passed via *len is ignored.
 *   Before this function returns the number of bytes read will be stored
 *   in *len. If a buffer overflow occurs, *len will be set to 0.
 *
 * @return A pointer to the next character in the input to be parsed if
 *   parsing succeeded; NULL when the end of the input string has been reached
 *   or if an error occurred.
 */
char *
ReadConfig_readOctetString(const char *readfrom, u_char ** str,
                              size_t * len)
{
    return TOOLS_REMOVE_CONST(char *,
               ReadConfig_readOctetStringConst(readfrom, str, len));
}

const char *
ReadConfig_readOctetStringConst(const char *readfrom, u_char ** str,
                                    size_t * len)
{
    u_char         *cptr;
    const char     *cptr1;
    u_int           tmp;
    size_t          i, ilen;

    if (readfrom == NULL || str == NULL || len == NULL)
        return NULL;

    if (strncasecmp(readfrom, "0x", 2) == 0) {
        /*
         * A hex string submitted. How long?
         */
        readfrom += 2;
        cptr1 = ReadConfig_skipNotWhiteConst(readfrom);
        if (cptr1)
            ilen = (cptr1 - readfrom);
        else
            ilen = strlen(readfrom);

        if (ilen % 2) {
            Logger_log(LOGGER_PRIORITY_WARNING,"invalid hex string: wrong length\n");
            DEBUG_MSGTL(("read_config_read_octet_string",
                        "invalid hex string: wrong length"));
            return NULL;
        }
        ilen = ilen / 2;

        /*
         * malloc data space if needed (+1 for good measure)
         */
        if (*str == NULL) {
            *str = (u_char *) malloc(ilen + 1);
            if (!*str)
                return NULL;
        } else {
            /*
             * require caller to have +1, and bail if not enough space.
             */
            if (ilen >= *len) {
                Logger_log(LOGGER_PRIORITY_WARNING,"buffer too small to read octet string (%lu < %lu)\n",
                         (unsigned long)*len, (unsigned long)ilen);
                DEBUG_MSGTL(("read_config_read_octet_string",
                            "buffer too small (%lu < %lu)", (unsigned long)*len, (unsigned long)ilen));
                *len = 0;
                cptr1 = ReadConfig_skipNotWhiteConst(readfrom);
                return ReadConfig_skipWhiteConst(cptr1);
            }
        }

        /*
         * copy validated data
         */
        cptr = *str;
        for (i = 0; i < ilen; i++) {
            if (1 == sscanf(readfrom, "%2x", &tmp))
                *cptr++ = (u_char) tmp;
            else {
                /*
                 * we may lose memory, but don't know caller's buffer XX free(cptr);
                 */
                return (NULL);
            }
            readfrom += 2;
        }
        /*
         * Terminate the output buffer.
         */
        *cptr++ = '\0';
        *len = ilen;
        readfrom = ReadConfig_skipWhiteConst(readfrom);
    } else {
        /*
         * Normal string
         */

        /*
         * malloc string space if needed (including NULL terminator)
         */
        if (*str == NULL) {
            char            buf[TOOLS_MAXBUF];
            readfrom = ReadConfig_copyNwordConst(readfrom, buf, sizeof(buf));

            *len = strlen(buf);
            *str = (u_char *) malloc(*len + 1);
            if (*str == NULL)
                return NULL;
            memcpy(*str, buf, *len + 1);
        } else {
            readfrom = ReadConfig_copyNwordConst(readfrom, (char *) *str, *len);
            if (*len)
                *len = strlen((char *) *str);
        }
    }

    return readfrom;
}

/*
 * read_config_save_objid(): saves an objid as a numerical string
 */
char * ReadConfig_saveObjid(char *saveto, oid * objid, size_t len)
{
    int             i;

    if (len == 0) {
        strcat(saveto, "NULL");
        saveto += strlen(saveto);
        return saveto;
    }

    /*
     * in case len=0, this makes it easier to read it back in
     */
    for (i = 0; i < (int) len; i++) {
        sprintf(saveto, ".%" "l" "d", objid[i]);
        saveto += strlen(saveto);
    }
    return saveto;
}

/*
 * read_config_read_objid(): reads an objid from a format saved by the above
 */
char * ReadConfig_readObjid(char *readfrom, oid ** objid, size_t * len)
{
    return TOOLS_REMOVE_CONST(char *,
             ReadConfig_readObjidConst(readfrom, objid, len));
}

const char  *
ReadConfig_readObjidConst(const char *readfrom, oid ** objid, size_t * len)
{

    if (objid == NULL || readfrom == NULL || len == NULL)
        return NULL;

    if (*objid == NULL) {
        *len = 0;
        if ((*objid = (oid *) malloc(TYPES_MAX_OID_LEN * sizeof(oid))) == NULL)
            return NULL;
        *len = TYPES_MAX_OID_LEN;
    }

    if (strncmp(readfrom, "NULL", 4) == 0) {
        /*
         * null length oid
         */
        *len = 0;
    } else {
        /*
         * qualify the string for read_objid
         */
        char            buf[IMPL_SPRINT_MAX_LEN];
        ReadConfig_copyNwordConst(readfrom, buf, sizeof(buf));

        if (!Mib_readObjid(buf, *objid, len)) {
            DEBUG_MSGTL(("read_config_read_objid", "Invalid OID"));
            *len = 0;
            return NULL;
        }
    }

    readfrom = ReadConfig_skipTokenConst(readfrom);
    return readfrom;
}

/**
 * read_config_read_data reads data of a given type from a token(s) on a
 * configuration line.  The supported types are:
 *
 *    - ASN_INTEGER
 *    - ASN_TIMETICKS
 *    - ASN_UNSIGNED
 *    - ASN_OCTET_STR
 *    - ASN_BIT_STR
 *    - ASN_OBJECT_ID
 *
 * @param type the asn data type to be read in.
 *
 * @param readfrom the configuration line data to be read.
 *
 * @param dataptr an allocated pointer expected to match the type being read
 *        (int *, u_int *, char **, oid **)
 *
 * @param len is the length of an asn oid or octet/bit string, not required
 *            for the asn integer, unsigned integer, and timeticks types
 *
 * @return the next token in the configuration line.  NULL if none left or
 * if an unknown type.
 *
 */
char  *
ReadConfig_readData(int type, char *readfrom, void *dataptr,
                      size_t * len)
{
    int            *intp;
    char          **charpp;
    oid           **oidpp;
    unsigned int   *uintp;

    if (dataptr && readfrom)
        switch (type) {
        case ASN01_INTEGER:
            intp = (int *) dataptr;
            *intp = atoi(readfrom);
            readfrom = ReadConfig_skipToken(readfrom);
            return readfrom;

        case ASN01_TIMETICKS:
        case ASN01_UNSIGNED:
            uintp = (unsigned int *) dataptr;
            *uintp = strtoul(readfrom, NULL, 0);
            readfrom = ReadConfig_skipToken(readfrom);
            return readfrom;

        case ASN01_IPADDRESS:
            intp = (int *) dataptr;
            *intp = inet_addr(readfrom);
            if ((*intp == -1) &&
                (strncmp(readfrom, "255.255.255.255", 15) != 0))
                return NULL;
            readfrom = ReadConfig_skipToken(readfrom);
            return readfrom;

        case ASN01_OCTET_STR:
        case ASN01_BIT_STR:
            charpp = (char **) dataptr;
            return ReadConfig_readOctetString(readfrom,
                                                 (u_char **) charpp, len);

        case ASN01_OBJECT_ID:
            oidpp = (oid **) dataptr;
            return ReadConfig_readObjid(readfrom, oidpp, len);

        default:
            DEBUG_MSGTL(("read_config_read_data", "Fail: Unknown type: %d",
                        type));
            return NULL;
        }
    return NULL;
}

/*
 * read_config_read_memory():
 *
 * similar to read_config_read_data, but expects a generic memory
 * pointer rather than a specific type of pointer.  Len is expected to
 * be the amount of available memory.
 */
char *
ReadConfig_readMemory(int type, char *readfrom,
                        char *dataptr, size_t * len)
{
    int            *intp;
    unsigned int   *uintp;
    char            buf[IMPL_SPRINT_MAX_LEN];

    if (!dataptr || !readfrom)
        return NULL;

    switch (type) {
    case ASN01_INTEGER:
        if (*len < sizeof(int))
            return NULL;
        intp = (int *) dataptr;
        readfrom = ReadConfig_copyNword(readfrom, buf, sizeof(buf));
        *intp = atoi(buf);
        *len = sizeof(int);
        return readfrom;

    case ASN01_COUNTER:
    case ASN01_TIMETICKS:
    case ASN01_UNSIGNED:
        if (*len < sizeof(unsigned int))
            return NULL;
        uintp = (unsigned int *) dataptr;
        readfrom = ReadConfig_copyNword(readfrom, buf, sizeof(buf));
        *uintp = strtoul(buf, NULL, 0);
        *len = sizeof(unsigned int);
        return readfrom;

    case ASN01_IPADDRESS:
        if (*len < sizeof(int))
            return NULL;
        intp = (int *) dataptr;
        readfrom = ReadConfig_copyNword(readfrom, buf, sizeof(buf));
        *intp = inet_addr(buf);
        if ((*intp == -1) && (strcmp(buf, "255.255.255.255") != 0))
            return NULL;
        *len = sizeof(int);
        return readfrom;

    case ASN01_OCTET_STR:
    case ASN01_BIT_STR:
    case ASN01_PRIV_IMPLIED_OCTET_STR:
        return ReadConfig_readOctetString(readfrom,
                                             (u_char **) & dataptr, len);

    case ASN01_PRIV_IMPLIED_OBJECT_ID:
    case ASN01_OBJECT_ID:
        readfrom =
            ReadConfig_readObjid(readfrom, (oid **) & dataptr, len);
        *len *= sizeof(oid);
        return readfrom;

    case ASN01_COUNTER64:
        if (*len < sizeof(Int64_U64))
            return NULL;
        *len = sizeof(Int64_U64);
        Int64_read64((Int64_U64 *) dataptr, readfrom);
        readfrom = ReadConfig_skipToken(readfrom);
        return readfrom;
    }

    DEBUG_MSGTL(("read_config_read_memory", "Fail: Unknown type: %d", type));
    return NULL;
}

/**
 * read_config_store_data stores data of a given type to a configuration line
 * into the storeto buffer.
 * Calls read_config_store_data_prefix with the prefix parameter set to a char
 * space.  The supported types are:
 *
 *    - ASN01_INTEGER
 *    - ASN01_TIMETICKS
 *    - ASN01_UNSIGNED
 *    - ASN01_OCTET_STR
 *    - ASN01_BIT_STR
 *    - ASN01_OBJECT_ID
 *
 * @param type    the asn data type to be stored
 *
 * @param storeto a pre-allocated char buffer which will contain the data
 *                to be stored
 *
 * @param dataptr contains the value to be stored, the supported pointers:
 *                (int *, u_int *, char **, oid **)
 *
 * @param len     is the length of the value to be stored
 *                (not required for the asn integer, unsigned integer,
 *                 and timeticks types)
 *
 * @return character pointer to the end of the line. NULL if an unknown type.
 */
char *
ReadConfig_storeData(int type, char *storeto, void *dataptr, size_t * len)
{
    return ReadConfig_storeDataPrefix(' ', type, storeto, dataptr,
                                                         (len ? *len : 0));
}

char *
ReadConfig_storeDataPrefix(char prefix, int type, char *storeto,
                              void *dataptr, size_t len)
{
    int            *intp;
    u_char        **charpp;
    unsigned int   *uintp;
    struct in_addr  in;
    oid           **oidpp;

    if (dataptr && storeto)
        switch (type) {
        case ASN01_INTEGER:
            intp = (int *) dataptr;
            sprintf(storeto, "%c%d", prefix, *intp);
            return (storeto + strlen(storeto));

        case ASN01_TIMETICKS:
        case ASN01_UNSIGNED:
            uintp = (unsigned int *) dataptr;
            sprintf(storeto, "%c%u", prefix, *uintp);
            return (storeto + strlen(storeto));

        case ASN01_IPADDRESS:
            in.s_addr = *(unsigned int *) dataptr;
            sprintf(storeto, "%c%s", prefix, inet_ntoa(in));
            return (storeto + strlen(storeto));

        case ASN01_OCTET_STR:
        case ASN01_BIT_STR:
            *storeto++ = prefix;
            charpp = (u_char **) dataptr;
            return ReadConfig_saveOctetString(storeto, *charpp, len);

        case ASN01_OBJECT_ID:
            *storeto++ = prefix;
            oidpp = (oid **) dataptr;
            return ReadConfig_saveObjid(storeto, *oidpp, len);

        default:
            DEBUG_MSGTL(("read_config_store_data_prefix",
                        "Fail: Unknown type: %d", type));
            return NULL;
        }
    return NULL;
}
