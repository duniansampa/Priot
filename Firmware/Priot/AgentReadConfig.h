#ifndef AGENTREADCONFIG_H
#define AGENTREADCONFIG_H

void
AgentReadConfig_initAgentReadConfig(const char *);

void
AgentReadConfig_updateConfig(void);

void
AgentReadConfig_priotdRegisterConfigHandler( const char*   token,
                                             void        (*parser)   ( const char*, char* ),
                                             void        (*releaser) ( void ),
                                             const char*   help );
void
AgentReadConfig_priotdRegisterConstConfigHandler( const char*,
                                                  void        (*parser) (const char*, const char*),
                                                  void        (*releaser) (void),
                                                  const char* );
void
AgentReadConfig_priotdUnregisterConfigHandler( const char* );

void
AgentReadConfig_priotdStoreConfig( const char* );

#endif // AGENTREADCONFIG_H
