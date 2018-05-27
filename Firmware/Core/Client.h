#ifndef CLIENT_H
#define CLIENT_H

#include "Generals.h"
#include "Types.h"

struct Client_SynchState_s {
    int waiting;
    int status;
/*
 * status codes
 */
#define CLIENT_STAT_SUCCESS 0
#define CLIENT_STAT_ERROR 1
#define CLIENT_STAT_TIMEOUT 2
    int reqid;
    Types_Pdu* pdu;
};

void Client_replaceVarTypes( VariableList* vbl, u_char old_type,
    u_char new_type );

void Client_resetVarBuffers( VariableList* var );
void Client_resetVarTypes( VariableList* vbl, u_char new_type );

int Client_countVarbinds( VariableList* var_ptr );

int Client_countVarbindsOfType( VariableList* var_ptr, u_char type );
VariableList* Client_findVarbindOfType( VariableList* var_ptr,
    u_char type );

VariableList* Client_findVarbindInList( VariableList* vblist,
    const oid* name, size_t len );

Types_Pdu* Client_splitPdu( Types_Pdu*, int skipCount, int copyCount );

unsigned long Client_varbindLen( Types_Pdu* pdu );

int Client_cloneVar( VariableList*, VariableList* );

int Client_synchResponseCb( Types_Session*, Types_Pdu*, Types_Pdu**,
    Types_CallbackFT );
int Client_synchResponse( Types_Session* ss, Types_Pdu* pdu,
    Types_Pdu** response );

int Client_cloneMem( void**, const void*, unsigned );

void Client_querySetDefaultSession( Types_Session* );

Types_Session* Client_queryGetDefaultSessionUnchecked( void );

Types_Session* Client_queryGetDefaultSession( void );

int Client_queryGet( VariableList*, Types_Session* );

int Client_queryGetnext( VariableList*, Types_Session* );

int Client_queryWalk( VariableList*, Types_Session* );

int Client_querySet( VariableList*, Types_Session* );

/** **************************************************************************
*
* state machine
*
*/
/** forward declare */
struct Client_StateMachineStep_s;
struct Client_StateMachineInput_s;

/** state machine process */
typedef int( Client_StateMachineFT )( struct Client_StateMachineInput_s* input,
    struct Client_StateMachineStep_s* step );

typedef struct Client_StateMachineStep_s {

    const char* name; /* primarily for logging/debugging */
    u_int sm_flags;

    Client_StateMachineFT* run;
    int result; /* return code for this step */

    struct Client_StateMachineStep_s* on_success;
    struct Client_StateMachineStep_s* on_error;

    /*
   * user fields (not touched by state machine functions)
   */
    u_int flags;
    void* step_context;

} Client_StateMachineStep;

typedef struct Client_StateMachineInput_s {
    const char* name;
    int steps_so_far;
    Client_StateMachineStep* steps;
    Client_StateMachineStep* cleanup;
    Client_StateMachineStep* last_run;

    /*
   * user fields (not touched by state machine functions)
   */
    void* input_context;

} Client_StateMachineInput;

int Client_stateMachineRun( Client_StateMachineInput* input );

int Client_rowCreate( Types_Session* sess, VariableList* vars,
    int row_status_index );

Types_Pdu* Client_pduCreate( int command );

VariableList* Client_addNullVar( Types_Pdu* pdu, const oid* name,
    size_t name_length );

int Client_sessSynchResponse( void* sessp, Types_Pdu* pdu,
    Types_Pdu** response );

Types_Pdu* Client_clonePdu( Types_Pdu* pdu );

int Client_setVarObjid( VariableList* vp, const oid* objid,
    size_t name_length );

int Client_setVarValue( VariableList* vars, const void* value,
    size_t len );

VariableList* Client_cloneVarbind( VariableList* varlist );

int Client_setVarTypedValue( VariableList* newvar, u_char type,
    const void* val_str, size_t valLen );

int Client_setVarTypedInteger( VariableList* newvar, u_char type,
    long val );

const char* Client_errstring( int errstat );

#endif // CLIENT_H
