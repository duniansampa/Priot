#ifndef TEXTUTILS_H
#define TEXTUTILS_H

#include "Generals.h"
#include "Types.h"
#include "Container.h"
#include "FileUtils.h"

/*------------------------------------------------------------------
 *
 * text file processing
 *
 */

#define TEXTUTILS_PM_SAVE_EVERYTHING                                             1
#define TEXTUTILS_PM_INDEX_STRING_STRING                                         2
#define TEXTUTILS_PM_USER_FUNCTION                                               3

#define TEXTUTILS_PM_FLAG_NO_CONTAINER                                  0x00000001
#define TEXTUTILS_PM_FLAG_SKIP_WHITESPACE                               0x00000002


/*
* user function return codes
*/
#define TEXTUTILS_PMLP_RC_STOP_PROCESSING                           -1
#define TEXTUTILS_PMLP_RC_MEMORY_USED                                0
#define TEXTUTILS_PMLP_RC_MEMORY_UNUSED                              1


/** ALLOC_LINE: wasteful, but fast */
#define TEXTUTILS_PMLP_FLAG_ALLOC_LINE                               0x00000001
/** STRDUP_LINE: slower if you don't keep memory in most cases */
#define TEXTUTILS_PMLP_FLAG_STRDUP_LINE                              0x00000002
/** don't strip trailing newlines */
#define TEXTUTILS_PMLP_FLAG_LEAVE_NEWLINE                            0x00000004
/** don't skip blank or comment lines */
#define TEXTUTILS_PMLP_FLAG_PROCESS_WHITESPACE                       0x00000008
/** just process line, don't save it */
#define TEXTUTILS_PMLP_FLAG_NO_CONTAINER                             0x00000010


/*
* flags
*/
#define TEXTUTILS_NSTTC_FLAG_TYPE_CONTEXT_DIRECT                      0x00000001


#define TEXTUTILS_PMLP_TYPE_UNSIGNED                                  1
#define TEXTUTILS_PMLP_TYPE_INTEGER                                   2
#define TEXTUTILS_PMLP_TYPE_STRING                                    3
#define TEXTUTILS_PMLP_TYPE_BOOLEAN                                   4


/*
 * line processing user function
 */

struct TextUtils_LineProcessInfo_s;

typedef struct TextUtils_LineInfo_s {

    size_t                     index;

    char                      *line;
    size_t                     line_len;
    size_t                     line_max;

    char                      *start;
    size_t                     start_len;

} TextUtils_LineInfo;

typedef int (TextUtils_ProcessTextLineFT)
    (TextUtils_LineInfo *line_info, void *mem,
     struct TextUtils_LineProcessInfo_s* lpi);

typedef struct TextUtils_LineProcessInfo_s {

    size_t  line_max; /* defaults to STRINGMAX if 0 */
    size_t  mem_size;

    u_int   flags;

    TextUtils_ProcessTextLineFT * process;

    void  * user_context;

} TextUtils_LineProcessInfo;


/*
 * a few useful pre-defined helpers
 */

typedef struct TextUtils_TokenValueIndex_s {

    char               *token;
    Types_CValue      value;
    size_t              index;

} TextUtils_TokenValueIndex;

Container_Container * TextUtils_fileTextParse(FileUtils_File      *f,
                                              Container_Container *cin,
                                              int                  parse_mode,
                                              u_int                flags,
                                              void                 *context );

Container_Container * TextUtils_textTokenContainerFromFile(const char          *file,
                                                           u_int                flags,
                                                           Container_Container *c,
                                                           void                *context );


#endif // TEXTUTILS_H
