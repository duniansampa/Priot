#ifndef EXECUTECMD_H
#define EXECUTECMD_H


int
ExecuteCmd_runShellCommand( char * command,
                         char * input,
                         char * output,
                         int  * out_len );

int
ExecuteCmd_runExecCommand( char * command,
                        char * input,
                        char * output,
                        int  * out_len );

#endif // EXECUTECMD_H
