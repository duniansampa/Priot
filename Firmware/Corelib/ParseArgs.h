#ifndef PARSEARGS_H
#define PARSEARGS_H

#include "Generals.h"
#include "Types.h"


/**
 * @file ParseArgs.h
 *
 * Support for initializing variables of type netsnmp_session from command
 * line arguments
 */

/** Don't enable any logging even if there is no -L argument */
#define PARSEARGS_NOLOGGING    0x0001
/** Don't zero out sensitive arguments as they are not on the command line
 *  anyway, typically used when the function is called from an internal
 *  config-line handler
 */
#define PARSEARGS_NOZERO       0x0002

/**
 * Parsing of command line arguments succeeded and application is expected
 * to continue with normal operation.
 */
#define PARSEARGS_SUCCESS       0
/**
  * Parsing of command line arguments succeeded, but the application is expected
  * to exit with zero exit code. For example, '-V' parameter has been found.
  */
#define PARSEARGS_SUCCESS_EXIT  -2
/**
 * Parsing of command line arguments failed and application is expected to show
 * usage (i.e. list of parameters) and exit with nozero exit code.
 */
#define PARSEARGS_ERROR_USAGE   -1
/**
 * Parsing of command line arguments failed and application is expected to exit
 * with nozero exit code.  ParseArgs_parseArgs() has already printed what went
 * wrong.
 */
#define PARSEARGS_ERROR         -3

/**
 *  Parse an argument list and initialize \link netsnmp_session
 *  session\endlink
 *  from it.
 *  @param argc Number of elements in argv
 *  @param argv string array of at least argc elements
 *  @param session
 *  @param localOpts Additional option characters to accept
 *  @param proc function pointer used to process any unhandled arguments
 *  @param flags flags directing how to handle the string
 *
 *  @retval 0 (= #NETSNMP_PARSE_ARGS_SUCCESS) on success
 *  @retval #NETSNMP_PARSE_ARGS_SUCCESS_EXIT when the application is expected
 *  to exit with zero exit code (e.g. '-V' option was found)
 *  @retval #NETSNMP_PARSE_ARGS_ERROR_USAGE when the function failed to parse
 *  the command line and the application is expected to show it's usage
 *  @retval #NETSNMP_PARSE_ARGS_ERROR when the function failed to parse
 *  the command line and it has already printed enough information for the user
 *  and no other output is needed
 *
 *  The proc function is called with argc, argv and the currently processed
 *  option as arguments
 */
 int ParseArgs_parseArgs(int argc, char **argv, Types_Session *session,
                   const char *localOpts, void (*proc)(int, char *const *, int),
                   int flags);

/**
 *  Calls \link netsnmp_parse_args()
 *  netsnmp_parse_args(argc, argv, session, localOpts, proc, 0)\endlink
 */

int ParseArgs_parseArgs2(int argc, char **argv, Types_Session *session,
        const char *localOpts, void (*proc)(int, char *const *, int));


void ParseArgs_descriptions(FILE *);


void ParseArgs_usage(FILE *);

#endif // PARSEARGS_H
