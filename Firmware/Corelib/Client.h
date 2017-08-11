#ifndef CLIENT_H
#define CLIENT_H

#include "Generals.h"
#include "Types.h"

struct Client_SynchState_s {
    int             waiting;
    int             status;
    /*
     * status codes
     */
#define CLIENT_STAT_SUCCESS	   0
#define CLIENT_STAT_ERROR	   1
#define CLIENT_STAT_TIMEOUT    2
    int         reqid;
    Types_Pdu * pdu;
};


void            Client_replaceVarTypes(Types_VariableList * vbl,
                                       u_char old_type,
                                       u_char new_type);

void            Client_resetVarBuffers(Types_VariableList * var);
void            Client_resetVarTypes(Types_VariableList * vbl,
                                     u_char new_type);

int             Client_countVarbinds(Types_VariableList * var_ptr);

int             Client_countVarbindsOfType(Types_VariableList * var_ptr,
                                       u_char type);
Types_VariableList * Client_findVarbindOfType(Types_VariableList *
                                            var_ptr, u_char type);

Types_VariableList * Client_findVarbindInList(Types_VariableList *vblist,
                                            const _oid *name, size_t len);

Types_Pdu    * Client_splitPdu(Types_Pdu *, int skipCount,
                               int copyCount);

unsigned long   Client_varbindLen(Types_Pdu *pdu);

int             Client_cloneVar(Types_VariableList *,
                               Types_VariableList *);

int             Client_synchResponseCb(Types_Session *,
                                       Types_Pdu *, Types_Pdu **,
                                       Types_CallbackFT);
int Client_synchResponse(Types_Session * ss,
                    Types_Pdu *pdu, Types_Pdu **response);

int             Client_cloneMem(void **, const void *, unsigned);



void Client_querySetDefaultSession(Types_Session *);

Types_Session * Client_queryGetDefaultSessionUnchecked( void );

Types_Session * Client_queryGetDefaultSession( void );

int Client_queryGet(     Types_VariableList *, Types_Session *);

int Client_queryGetnext( Types_VariableList *, Types_Session *);

int Client_queryWalk(    Types_VariableList *, Types_Session *);

int Client_querySet(     Types_VariableList *, Types_Session *);

/** **************************************************************************
*
* state machine
*
*/
/** forward declare */
struct Client_StateMachineStep_s;
struct Client_stateMachineInput_s;

/** state machine process */
typedef int (Client_StateMachineFT)(struct Client_StateMachineInput_s *input,
                                         struct Client_StateMachineStep_s *step);

typedef struct Client_StateMachineStep_s {

    const char   *name; /* primarily for logging/debugging */
    u_int         sm_flags;

    Client_StateMachineFT *run;
    int                         result; /* return code for this step */


    struct Client_StateMachineStep_s *on_success;
    struct Client_StateMachineStep_s *on_error;

    /*
     * user fields (not touched by state machine functions)
     */
    u_int         flags;
    void         *step_context;

} Client_StateMachineStep;

typedef struct Client_StateMachineInput_s {
    const char                  *name;
    int                          steps_so_far;
    Client_StateMachineStep  *steps;
    Client_StateMachineStep  *cleanup;
    Client_StateMachineStep  *last_run;

    /*
     * user fields (not touched by state machine functions)
     */
    void         *input_context;

} Client_StateMachineInput;


int Client_stateMachineRun( Client_StateMachineInput *input );

int Client_rowCreate(Types_Session *sess, Types_VariableList *vars,
                   int row_status_index);

Types_Pdu * Client_pduCreate(int command);

Types_VariableList *
Client_addNullVar(Types_Pdu *pdu, const _oid * name, size_t name_length);

int Client_sessSynchResponse(void *sessp, Types_Pdu *pdu, Types_Pdu **response);

Types_Pdu * Client_clonePdu(Types_Pdu *pdu);

int Client_setVarObjid(Types_VariableList * vp, const _oid * objid, size_t name_length);

int Client_setVarValue(Types_VariableList * vars, const void * value, size_t len);

#endif // CLIENT_H
