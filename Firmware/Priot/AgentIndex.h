#ifndef AGENTINDEX_H
#define AGENTINDEX_H

#include "Generals.h"
#include "Types.h"

#define ALLOCATE_THIS_INDEX		0x0
#define ALLOCATE_ANY_INDEX		0x1
#define ALLOCATE_NEW_INDEX		0x3
        /*
         * N.B: it's deliberate that NEW_INDEX & ANY_INDEX == ANY_INDEX
         */

#define ANY_INTEGER_INDEX		 -1
#define ANY_STRING_INDEX		NULL
#define ANY_OID_INDEX			NULL

#define	INDEX_ERR_GENERR		    -1
#define	INDEX_ERR_WRONG_TYPE		-2
#define	INDEX_ERR_NOT_ALLOCATED		-3
#define	INDEX_ERR_WRONG_SESSION		-4

char*
AgentIndex_registerStringIndex( oid*  name,
                                size_t name_len,
                                char*  cp );

int
AgentIndex_registerIntIndex( oid*  name,
                             size_t name_len,
                             int    val );

VariableList*
AgentIndex_registerOidIndex( oid*  name,
                             size_t name_len,
                             oid*  value,
                             size_t value_len );

VariableList*
AgentIndex_registerIndex( VariableList* varbind,
                          int                 flags,
                          Types_Session*      ss );

int
AgentIndex_unregisterStringIndex( oid*  name,
                                  size_t name_len,
                                  char*  cp );

int
AgentIndex_unregisterIntIndex( oid*  name,
                               size_t name_len,
                               int    val );

int
AgentIndex_unregisterOidIndex( oid*  name,
                               size_t name_len,
                               oid*  value,
                               size_t value_len );

int
AgentIndex_releaseIndex( VariableList* varbind );

int
AgentIndex_removeIndex( VariableList * varbind,
                        Types_Session * ss );

void
AgentIndex_unregisterIndexBySession( Types_Session* ss );

int
AgentIndex_unregisterIndex( VariableList* varbind,
                            int                 remember,
                            Types_Session*      ss );

unsigned long
AgentIndex_countIndexes( oid*  name,
                         size_t namelen,
                         int    include_unallocated );

#endif // AGENTINDEX_H
