#ifndef IOT_FILEPARSER_H
#define IOT_FILEPARSER_H

#include "Generals.h"
#include "System/Containers/Container.h"
#include "System/Util/File.h"
#include "Types.h"

/** ============================[ Macros ]============================ */

#define fpPARSE_MODE_SAVE_EVERYTHING 1
#define fpPARSE_MODE_INDEX_STRING_STRING 2
#define fpPARSE_MODE_USER_FUNCTION 3

#define fpFLAG_NO_CONTAINER 0x00000001
#define fpFLAG_SKIP_WHITESPACE 0x00000002

/** ALLOC_LINE: wasteful, but fast */
#define fpFLAG_ALLOC_LINE 0x00000001
/** STRDUP_LINE: slower if you don't keep memory in most cases */
#define fpFLAG_STRDUP_LINE 0x00000002
/** don't strip trailing newlines */
#define fpFLAG_LEAVE_NEWLINE 0x00000004
/** don't skip blank or comment lines */
#define fpFLAG_PROCESS_WHITESPACE 0x00000008
/** just process line, don't save it */
#define fpFLAG_LINE_PROCESS_NO_CONTAINER 0x00000010

/*
* user function return codes
*/
#define fpRETURN_CODE_STOP_PROCESSING -1
#define fpRETURN_CODE_MEMORY_USED 0
#define fpRETURN_CODE_MEMORY_UNUSED 1

#define fpTYPE_UNSIGNED 1
#define fpTYPE_INTEGER 2
#define fpTYPE_STRING 3
#define fpTYPE_BOOLEAN 4

/** ============================[ Types ]================== */

struct TextLineProcessInfo_s;

typedef struct TextLineInfo_s {

    size_t index;

    char* line;
    size_t lineLength;
    size_t lineMax;

    char* start;
    size_t startLength;

} TextLineInfo_t;

typedef int( ProcessTextLine_f )( TextLineInfo_t* lineInfo, void* mem, struct TextLineProcessInfo_s* textLineProcessInfo );

typedef struct TextLineProcessInfo_s {

    size_t lineMax; /* defaults to STRINGMAX if 0 */
    size_t memSize;

    u_int flags;

    ProcessTextLine_f* processFunc;

    void* userContext;

} TextLineProcessInfo_t;

/*
 * a few useful pre-defined helpers
 */
typedef struct TextTokenValueIndex_s {

    char* token;
    Types_CValue value;
    size_t index;

} TextTokenValueIndex_t;

/** =============================[ Functions Prototypes ]================== */

/**
 * @brief FileParser_parse
 *        process text file.
 *
 * @param file - the file to be parsed
 * @param container - the container to be used
 * @param parseMode - the TextParseMode option
 * @param flags - the flag: TextParseModeFlag_* or TextLineProcessFlag_*
 * @param textLineProcessInfo - Contains information to be used to process the text line.
 * @return
 */
Container_Container* FileParser_parse( File* file, Container_Container* container,
    int parseMode, u_int flags, void* textLineProcessInfo );

/**
 * @brief FileParser_parseTokenContainerFromFile
 * @param file
 * @param flags
 * @param container
 * @param context
 * @return
 */
Container_Container* FileParser_parseTokenContainerFromFile( const char* file, u_int flags, Container_Container* container, void* context );

#endif // IOT_FILEPARSER_H
