#ifndef AGENTXCONFIG_H
#define AGENTXCONFIG_H


void
AgentxConfig_parseMaster( const char *token,
                                char *cptr );

void
AgentxConfig_parseAgentxSocket( const char *token,
                                      char *cptr );

void
AgentxConfig_init( void );


#endif // AGENTXCONFIG_H
