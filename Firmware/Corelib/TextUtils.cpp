#include "TextUtils.h"
#include "Logger.h"
#include "ReadConfig.h"
#include "Logger.h"
#include "Tools.h"
#include "Assert.h"
#include "Debug.h"
/*------------------------------------------------------------------
 *
 * Prototypes
 *
 */
/*
 * parse methods
 */
void TextUtils_pmSaveIndexStringString(FILE *f, Container_Container *cin,
                             int flags);

void TextUtils_pmSaveEverything(FILE *f, Container_Container *cin, int flags);

void TextUtils_pmUserFunction(FILE *f, Container_Container *cin,
                  TextUtils_LineProcessInfo *lpi, int flags);


/*
 * line processors
 */
int TextUtils_processLineTvi(TextUtils_LineInfo *line_info, void *mem,
                      struct TextUtils_LineProcessInfo_s* lpi);



/*------------------------------------------------------------------
 *
 * Text file processing functions
 *
 */

/**
 * process text file, reading into extras
 */
Container_Container *
TextUtils_fileTextParse(FileUtils_File *f, Container_Container *cin,
                        int parse_mode, u_int flags, void *context)
{
    Container_Container *c = cin;
    FILE              *fin;
    int                rc;

    if (NULL == f)
        return NULL;

    if ((NULL == c) && (!(flags & TEXTUTILS_PM_FLAG_NO_CONTAINER))) {
        c = Container_find("text_parse:binary_array");
        if (NULL == c)
            return NULL;
    }

    rc = FileUtils_open(f);
    if (rc < 0) { /** error already logged */
        if ((NULL !=c) && (c != cin))
            CONTAINER_FREE(c);
        return NULL;
    }

    /*
     * get a stream from the file descriptor. This DOES NOT rewind the
     * file (if fd was previously opened).
     */
    fin = fdopen(f->fd, "r");
    if (NULL == fin) {
        if (FILEUTILS_NS_FI_AUTOCLOSE(f->nsFlags))
            close(f->fd);
        if ((NULL !=c) && (c != cin))
            CONTAINER_FREE(c);
        return NULL;
    }

    switch (parse_mode) {

        case TEXTUTILS_PM_SAVE_EVERYTHING:
            TextUtils_pmSaveEverything(fin, c, flags);
            break;

        case TEXTUTILS_PM_INDEX_STRING_STRING:
            TextUtils_pmSaveIndexStringString(fin, c, flags);
            break;

        case TEXTUTILS_PM_USER_FUNCTION:
            if (NULL != context)
                TextUtils_pmUserFunction(fin, c, (TextUtils_LineProcessInfo*)context,
                                  flags);
            break;

        default:
            Logger_log(LOGGER_PRIORITY_ERR, "unknown parse mode %d\n", parse_mode);
            break;
    }


    /*
     * close the stream, which will have the side effect of also closing
     * the file descriptor, so we need to reset it.
     */
    fclose(fin);
    f->fd = -1;

    return c;
}

Container_Container *
TextUtils_textTokenContainerFromFile(const char *file, u_int flags,
                                       Container_Container *cin, void *context)
{
    TextUtils_LineProcessInfo  lpi;
    Container_Container         *c = cin, *c_rc;
    FileUtils_File              *fp;

    if (NULL == file)
        return NULL;

    /*
     * allocate file resources
     */
    fp = FileUtils_fill(NULL, file, O_RDONLY, 0, 0);
    if (NULL == fp) /** msg already logged */
        return NULL;

    memset(&lpi, 0x0, sizeof(lpi));
    lpi.mem_size = sizeof(TextUtils_TokenValueIndex);
    lpi.process = TextUtils_processLineTvi;
    lpi.user_context = context;

    if (NULL == c) {
        c = Container_find("string:binary_array");
        if (NULL == c) {
            Logger_log(LOGGER_PRIORITY_ERR,"malloc failed\n");
            FileUtils_release(fp);
            return NULL;
        }
    }

    c_rc = TextUtils_fileTextParse(fp, c, TEXTUTILS_PM_USER_FUNCTION, 0, &lpi);

    /*
     * if we got a bad return and the user didn't pass us a container,
     * we need to release the container we allocated.
     */
    if ((NULL == c_rc) && (NULL == cin)) {
        CONTAINER_FREE(c);
        c = NULL;
    }
    else
        c = c_rc;

    /*
     * release file resources
     */
    FileUtils_release(fp);

    return c;
}


/*------------------------------------------------------------------
 *
 * Text file process modes helper functions
 *
 */

/**
 * @internal
 * parse mode: save everything
 */
void
TextUtils_pmSaveEverything(FILE *f, Container_Container *cin, int flags)
{
    char               line[READCONFIG_STRINGMAX], *ptr;
    size_t             len;

    Assert_assert(NULL != f);
    Assert_assert(NULL != cin);

    while (fgets(line, sizeof(line), f) != NULL) {

        ptr = line;
        len = strlen(line) - 1;
        if (line[len] == '\n')
            line[len] = 0;

        /*
         * save blank line or comment?
         */
        if (flags & TEXTUTILS_PM_FLAG_SKIP_WHITESPACE) {
            if (NULL == (ptr = ReadConfig_skipWhite(ptr)))
                continue;
        }

        ptr = strdup(line);
        if (NULL == ptr) {
            Logger_log(LOGGER_PRIORITY_ERR,"malloc failed\n");
            break;
        }

        CONTAINER_INSERT(cin,ptr);
    }
}

/**
 * @internal
 * parse mode:
 */
void
TextUtils_pmSaveIndexStringString(FILE *f, Container_Container *cin,
                             int flags)
{
    char                        line[READCONFIG_STRINGMAX], *ptr;
    TextUtils_TokenValueIndex  *tvi;
    size_t                      count = 0, len;

    Assert_assert(NULL != f);
    Assert_assert(NULL != cin);

    while (fgets(line, sizeof(line), f) != NULL) {

        ++count;
        ptr = line;
        len = strlen(line) - 1;
        if (line[len] == '\n')
            line[len] = 0;

        /*
         * save blank line or comment?
         */
        if (flags & TEXTUTILS_PM_FLAG_SKIP_WHITESPACE) {
            if (NULL == (ptr = ReadConfig_skipWhite(ptr)))
                continue;
        }

        tvi = TOOLS_MALLOC_TYPEDEF(TextUtils_TokenValueIndex);
        if (NULL == tvi) {
            Logger_log(LOGGER_PRIORITY_ERR,"malloc failed\n");
            break;
        }

        /*
         * copy whole line, then set second pointer to
         * after token. One malloc, 2 strings!
         */
        tvi->index = count;
        tvi->token = strdup(line);
        if (NULL == tvi->token) {
            Logger_log(LOGGER_PRIORITY_ERR,"malloc failed\n");
            free(tvi);
            break;
        }
        tvi->value.cp = ReadConfig_skipNotWhite(tvi->token);
        if (NULL != tvi->value.cp) {
            *(tvi->value.cp) = 0;
            ++(tvi->value.cp);
        }
        CONTAINER_INSERT(cin, tvi);
    }
}

/**
 * @internal
 * parse mode:
 */
void
TextUtils_pmUserFunction(FILE *f, Container_Container *cin,
                  TextUtils_LineProcessInfo *lpi, int flags)
{
    char                        buf[READCONFIG_STRINGMAX];
    TextUtils_LineInfo           li;
    void                       *mem = NULL;
    int                         rc;

    Assert_assert(NULL != f);
    Assert_assert(NULL != cin);

    /*
     * static buf, or does the user want the memory?
     */
    if (flags & TEXTUTILS_PMLP_FLAG_ALLOC_LINE) {
        if (0 != lpi->line_max)
            li.line_max =  lpi->line_max;
        else
            li.line_max = READCONFIG_STRINGMAX;
        li.line = (char *)calloc(li.line_max, 1);
        if (NULL == li.line) {
            Logger_log(LOGGER_PRIORITY_ERR,"malloc failed\n");
            return;
        }
    }
    else {
        li.line = buf;
        li.line_max = sizeof(buf);
    }

    li.index = 0;
    while (fgets(li.line, li.line_max, f) != NULL) {

        ++li.index;
        li.start = li.line;
        li.line_len = strlen(li.line) - 1;
        if ((!(lpi->flags & TEXTUTILS_PMLP_FLAG_LEAVE_NEWLINE)) &&
            (li.line[li.line_len] == '\n'))
            li.line[li.line_len] = 0;

        /*
         * save blank line or comment?
         */
        if (!(lpi->flags & TEXTUTILS_PMLP_FLAG_PROCESS_WHITESPACE)) {
            if (NULL == (li.start = ReadConfig_skipWhite(li.start)))
                continue;
        }

        /*
         *  do we need to allocate memory for the use?
         * if the last call didn't use the memory we allocated,
         * re-use it. Otherwise, allocate new chunk.
         */
        if ((0 != lpi->mem_size) && (NULL == mem)) {
            mem = calloc(lpi->mem_size, 1);
            if (NULL == mem) {
                Logger_log(LOGGER_PRIORITY_ERR,"malloc failed\n");
                break;
            }
        }

        /*
         * do they want a copy ot the line?
         */
        if (lpi->flags & TEXTUTILS_PMLP_FLAG_STRDUP_LINE) {
            li.start = strdup(li.start);
            if (NULL == li.start) {
                Logger_log(LOGGER_PRIORITY_ERR,"malloc failed\n");
                break;
            }
        }
        else if (lpi->flags & TEXTUTILS_PMLP_FLAG_ALLOC_LINE) {
            li.start = li.line;
        }

        /*
         * call the user function. If the function wants to save
         * the memory chunk, insert it in the container, the clear
         * pointer so we reallocate next time.
         */
        li.start_len = strlen(li.start);
        rc = (*lpi->process)(&li, mem, lpi);
        if (TEXTUTILS_PMLP_RC_MEMORY_USED == rc) {

            if (!(lpi->flags & TEXTUTILS_PMLP_FLAG_NO_CONTAINER))
                CONTAINER_INSERT(cin, mem);

            mem = NULL;

            if (lpi->flags & TEXTUTILS_PMLP_FLAG_ALLOC_LINE) {
            li.line = (char *)calloc(li.line_max, 1);
                if (NULL == li.line) {
                    Logger_log(LOGGER_PRIORITY_ERR,"malloc failed\n");
                    break;
                }
            }
        }
        else if (TEXTUTILS_PMLP_RC_MEMORY_UNUSED == rc ) {
            /*
             * they didn't use the memory. if li.start was a strdup, we have
             * to release it. leave mem, we can re-use it (its a fixed size).
             */
            if (lpi->flags & TEXTUTILS_PMLP_FLAG_STRDUP_LINE)
                free(li.start); /* no point in SNMP_FREE */
        }
        else {
            if (TEXTUTILS_PMLP_RC_STOP_PROCESSING != rc )
                Logger_log(LOGGER_PRIORITY_ERR, "unknown rc %d from text processor\n", rc);
            break;
        }
    }
    TOOLS_FREE(mem);
}

/*------------------------------------------------------------------
 *
 * Test line process helper functions
 *
 */
/**
 * @internal
 * process token value index line
 */
int TextUtils_processLineTvi(TextUtils_LineInfo *line_info, void *mem,
                  struct TextUtils_LineProcessInfo_s* lpi)
{
    TextUtils_TokenValueIndex *tvi = (TextUtils_TokenValueIndex *)mem;
    char                      *ptr;

    /*
     * get token
     */
    ptr = ReadConfig_skipNotWhite(line_info->start);
    if (NULL == ptr) {
        DEBUG_MSGTL(("text:util:tvi", "no value after token '%s'\n",
                    line_info->start));
        return TEXTUTILS_PMLP_RC_MEMORY_UNUSED;
    }

    /*
     * null terminate, search for value;
     */
    *(ptr++) = 0;
    ptr = ReadConfig_skipWhite(ptr);
    if (NULL == ptr) {
        DEBUG_MSGTL(("text:util:tvi", "no value after token '%s'\n",
                    line_info->start));
        return TEXTUTILS_PMLP_RC_MEMORY_UNUSED;
    }

    /*
     * get value
     */
    switch((int)(intptr_t)lpi->user_context) {

        case TEXTUTILS_PMLP_TYPE_UNSIGNED:
            tvi->value.ul = strtoul(ptr, NULL, 0);
            if ((errno == ERANGE) && (ULONG_MAX == tvi->value.ul))
                Logger_log(LOGGER_PRIORITY_WARNING,"value overflow\n");
            break;


        case TEXTUTILS_PMLP_TYPE_INTEGER:
            tvi->value.sl = strtol(ptr, NULL, 0);
            if ((errno == ERANGE) &&
                ((LONG_MAX == tvi->value.sl) ||
                 (LONG_MIN == tvi->value.sl)))
                Logger_log(LOGGER_PRIORITY_WARNING,"value over/under-flow\n");
            break;

        case TEXTUTILS_PMLP_TYPE_STRING:
            tvi->value.cp = strdup(ptr);
            break;

        case TEXTUTILS_PMLP_TYPE_BOOLEAN:
            if (isdigit((unsigned char)(*ptr)))
                tvi->value.ul = strtoul(ptr, NULL, 0);
            else if (strcasecmp(ptr,"true") == 0)
                tvi->value.ul = 1;
            else if (strcasecmp(ptr,"false") == 0)
                tvi->value.ul = 0;
            else {
                Logger_log(LOGGER_PRIORITY_WARNING,"bad value for boolean\n");
                return TEXTUTILS_PMLP_RC_MEMORY_UNUSED;
            }
            break;

        default:
            Logger_log(LOGGER_PRIORITY_ERR,"unsupported value type %d\n",
                     (int)(intptr_t)lpi->user_context);
            break;
    }

    /*
     * save token and value
     */
    tvi->token = strdup(line_info->start);
    tvi->index = line_info->index;

    return TEXTUTILS_PMLP_RC_MEMORY_USED;
}
