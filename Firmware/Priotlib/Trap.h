#ifndef TRAP_H
#define TRAP_H

#include "Types.h"

struct AgentAddTrapArgs_s {
    Types_Session *ss;
    int             confirm;
};

void
Trap_initTraps(void);

void
Trap_sendEasyTrap(int, int);

void
Trap_sendTrapPdu(Types_Pdu *);

void
Trap_sendV2trap(Types_VariableList *);

void
Trap_sendV3trap(Types_VariableList *vars, const char *context);

void
Trap_sendTrapVars(int, int, Types_VariableList *);

void
Trap_sendTrapVarsWithContext(int trap, int specific,
                                            Types_VariableList *vars,
                                            const char *context);
void
Trap_sendEnterpriseTrapVars(int trap, int specific,
                          const oid * enterprise,
                          int enterprise_length,
                          Types_VariableList * vars);
int
Trap_sendTraps( int                 trap,
                int                 specific,
                const oid*         enterprise,
                int                 enterprise_length,
                Types_VariableList* vars,
                /* flags are currently unused */
                const char*         context,
                int                 flags );

void
Trap_priotdParseConfigAuthtrap(const char *, char *);

void
Trap_priotdParseConfigTrapsess(const char *, char *);

void
Trap_priotdFreeTrapsinks(void);

void
Trap_sendTrapToSess(Types_Session * sess, Types_Pdu *template_pdu);

int
Trap_addTrapSession(Types_Session *, int, int, int);

int
Trap_removeTrapSession(Types_Session *);

Types_Pdu*
Trap_convertV2pduToV1(Types_Pdu *);

Types_Pdu*
Trap_convertV1pduToV2(Types_Pdu *);

#endif // TRAP_H
