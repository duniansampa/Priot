#include "Client.h"
#include "Priot.h"
#include "Api.h"
#include "DefaultStore.h"
#include "Logger.h"
#include "printf.h"
#include "Assert.h"
#include "Usm.h"
#include "Session.h"
#include "Tools.h"
#include "Debug.h"
#include "Tc.h"

#include <sys/time.h>

/** @defgroup snmp_client various PDU processing routines
 *  @ingroup library
 */

/*
 * Prototype definitions
 */
static int      Client_synchInput(int op, Types_Session * session,
                                 int reqid, Types_Pdu *pdu, void *magic);

Types_Pdu    *
Client_pduCreate(int command)
{
    Types_Pdu    *pdu;

    pdu = (Types_Pdu *) calloc(1, sizeof(Types_Pdu));
    if (pdu) {
        pdu->version = API_DEFAULT_VERSION;
        pdu->command = command;
        pdu->errstat = API_DEFAULT_ERRSTAT;
        pdu->errindex = API_DEFAULT_ERRINDEX;
        pdu->securityModel = API_DEFAULT_SECMODEL;
        pdu->transportData = NULL;
        pdu->transportDataLength = 0;
        pdu->securityNameLen = 0;
        pdu->contextNameLen = 0;
        pdu->time = 0;
        pdu->reqid = Api_getNextReqid();
        pdu->msgid = Api_getNextMsgid();
    }
    return pdu;

}


/*
 * Add a null variable with the requested name to the end of the list of
 * variables for this pdu.
 */
Types_VariableList *
Client_addNullVar(Types_Pdu *pdu, const oid * name, size_t name_length)
{
    return Api_pduAddVariable(pdu, name, name_length, ASN01_NULL, NULL, 0);
}


static int
Client_synchInput(int op,
                 Types_Session * session,
                 int reqid, Types_Pdu *pdu, void *magic)
{
    struct Client_SynchState_s *state = (struct Client_SynchState_s *) magic;
    int             rpt_type;

    if (reqid != state->reqid && pdu && pdu->command != PRIOT_MSG_REPORT) {
        DEBUG_MSGTL(("snmp_synch", "Unexpected response (ReqID: %d,%d - Cmd %d)\n",
                                   reqid, state->reqid, pdu->command ));
        return 0;
    }

    state->waiting = 0;
    DEBUG_MSGTL(("snmp_synch", "Response (ReqID: %d - Cmd %d)\n",
                               reqid, (pdu ? pdu->command : -1)));

    if (op == API_CALLBACK_OP_RECEIVED_MESSAGE && pdu) {
        if (pdu->command == PRIOT_MSG_REPORT) {
            rpt_type = Api_v3GetReportType(pdu);
            if (API_V3_IGNORE_UNAUTH_REPORTS ||
                rpt_type == ErrorCode_NOT_IN_TIME_WINDOW) {
                state->waiting = 1;
            }
            state->pdu = NULL;
            state->status = CLIENT_STAT_ERROR;
            session->s_snmp_errno = rpt_type;
            API_SET_PRIOT_ERROR(rpt_type);
        } else if (pdu->command == PRIOT_MSG_RESPONSE) {
            /*
             * clone the pdu to return to Client_synchResponse
             */
            state->pdu = Client_clonePdu(pdu);
            state->status = CLIENT_STAT_SUCCESS;
            session->s_snmp_errno = ErrorCode_SUCCESS;
        }
        else {
            char msg_buf[50];
            state->status = CLIENT_STAT_ERROR;
            session->s_snmp_errno = ErrorCode_PROTOCOL;
            API_SET_PRIOT_ERROR(ErrorCode_PROTOCOL);
            snprintf(msg_buf, sizeof(msg_buf), "Expected RESPONSE-PDU but got %s-PDU",
                     Api_pduType(pdu->command));
            Api_setDetail(msg_buf);
            return 0;
        }
    } else if (op == API_CALLBACK_OP_TIMED_OUT) {
        state->pdu = NULL;
        state->status = CLIENT_STAT_TIMEOUT;
        session->s_snmp_errno = ErrorCode_TIMEOUT;
        API_SET_PRIOT_ERROR(ErrorCode_TIMEOUT);
    } else if (op == API_CALLBACK_OP_DISCONNECT) {
        state->pdu = NULL;
        state->status = CLIENT_STAT_ERROR;
        session->s_snmp_errno = ErrorCode_ABORT;
        API_SET_PRIOT_ERROR(ErrorCode_ABORT);
    }

    return 1;
}


/*
 * Clone an SNMP variable data structure.
 * Sets pointers to structure private storage, or
 * allocates larger object identifiers and values as needed.
 *
 * Caller must make list association for cloned variable.
 *
 * Returns 0 if successful.
 */
int
Client_cloneVar(Types_VariableList * var, Types_VariableList * newvar)
{
    if (!newvar || !var)
        return 1;

    memmove(newvar, var, sizeof(Types_VariableList));
    newvar->nextVariable = NULL;
    newvar->name = NULL;
    newvar->val.string = NULL;
    newvar->data = NULL;
    newvar->dataFreeHook = NULL;
    newvar->index = 0;

    /*
     * Clone the object identifier and the value.
     * Allocate memory iff original will not fit into local storage.
     */
    if (Client_setVarObjid(newvar, var->name, var->nameLength))
        return 1;

    /*
     * need a pointer to copy a string value.
     */
    if (var->val.string) {
        if (var->val.string != &var->buf[0]) {
            if (var->valLen <= sizeof(var->buf))
                newvar->val.string = newvar->buf;
            else {
                newvar->val.string = (u_char *) malloc(var->valLen);
                if (!newvar->val.string)
                    return 1;
            }
            memmove(newvar->val.string, var->val.string, var->valLen);
        } else {                /* fix the pointer to new local store */
            newvar->val.string = newvar->buf;
            /*
             * no need for a memmove, since we copied the whole var
             * struct (and thus var->buf) at the beginning of this function.
             */
        }
    } else {
        newvar->val.string = NULL;
        newvar->valLen = 0;
    }

    return 0;
}


/*
 * Possibly make a copy of source memory buffer.
 * Will reset destination pointer if source pointer is NULL.
 * Returns 0 if successful, 1 if memory allocation fails.
 */
int
Client_cloneMem(void **dstPtr, const void *srcPtr, unsigned len)
{
    *dstPtr = NULL;
    if (srcPtr) {
        *dstPtr = malloc(len + 1);
        if (!*dstPtr) {
            return 1;
        }
        memmove(*dstPtr, srcPtr, len);
        /*
         * this is for those routines that expect 0-terminated strings!!!
         * someone should rather have called strdup
         */
        ((char *) *dstPtr)[len] = 0;
    }
    return 0;
}


/*
 * Walks through a list of varbinds and frees and allocated memory,
 * restoring pointers to local buffers
 */
void
Client_resetVarBuffers(Types_VariableList * var)
{
    while (var) {
        if (var->name != var->nameLoc) {
            if(NULL != var->name)
                free(var->name);
            var->name = var->nameLoc;
            var->nameLength = 0;
        }
        if (var->val.string != var->buf) {
            if (NULL != var->val.string)
                free(var->val.string);
            var->val.string = var->buf;
            var->valLen = 0;
        }
        var = var->nextVariable;
    }
}

/*
 * Creates and allocates a clone of the input PDU,
 * but does NOT copy the variables.
 * This function should be used with another function,
 * such as Client_copyPduVars.
 *
 * Returns a pointer to the cloned PDU if successful.
 * Returns 0 if failure.
 */
static
Types_Pdu    *
Client_clonePduHeader(Types_Pdu *pdu)
{
    Types_Pdu    *newpdu;
    struct Secmod_Def_s *sptr;
    int ret;

    newpdu = (Types_Pdu *) malloc(sizeof(Types_Pdu));
    if (!newpdu)
        return NULL;
    memmove(newpdu, pdu, sizeof(Types_Pdu));

    /*
     * reset copied pointers if copy fails
     */
    newpdu->variables = NULL;
    newpdu->enterprise = NULL;
    newpdu->community = NULL;
    newpdu->securityEngineID = NULL;
    newpdu->securityName = NULL;
    newpdu->contextEngineID = NULL;
    newpdu->contextName = NULL;
    newpdu->transportData = NULL;

    /*
     * copy buffers individually. If any copy fails, all are freed.
     */
    if (Client_cloneMem((void **) &newpdu->enterprise, pdu->enterprise,
                       sizeof(oid) * pdu->enterpriseLength) ||
        Client_cloneMem((void **) &newpdu->community, pdu->community,
                       pdu->communityLen) ||
        Client_cloneMem((void **) &newpdu->contextEngineID,
                       pdu->contextEngineID, pdu->contextEngineIDLen)
        || Client_cloneMem((void **) &newpdu->securityEngineID,
                          pdu->securityEngineID, pdu->securityEngineIDLen)
        || Client_cloneMem((void **) &newpdu->contextName, pdu->contextName,
                          pdu->contextNameLen)
        || Client_cloneMem((void **) &newpdu->securityName,
                          pdu->securityName, pdu->securityNameLen)
        || Client_cloneMem((void **) &newpdu->transportData,
                          pdu->transportData,
                          pdu->transportDataLength)) {
        Api_freePdu(newpdu);
        return NULL;
    }

    if (pdu != NULL && pdu->securityStateRef &&
        pdu->command == PRIOT_MSG_TRAP2) {

        ret = Usm_cloneUsmStateReference((struct Usm_StateReference_s *) pdu->securityStateRef,
                (struct Usm_StateReference_s **) &newpdu->securityStateRef );

        if (ret)
        {
            Api_freePdu(newpdu);
            return 0;
        }
    }

    if ((sptr = Secmod_find(newpdu->securityModel)) != NULL &&
        sptr->pdu_clone != NULL) {
        /*
         * call security model if it needs to know about this
         */
        (*sptr->pdu_clone) (pdu, newpdu);
    }

    return newpdu;
}

static
Types_VariableList *
Client_copyVarlist(Types_VariableList * var,      /* source varList */
              int errindex,     /* index of variable to drop (if any) */
              int copy_count)
{                               /* !=0 number variables to copy */
    Types_VariableList *newhead, *newvar, *oldvar;
    int             ii = 0;

    newhead = NULL;
    oldvar = NULL;

    while (var && (copy_count-- > 0)) {
        /*
         * Drop the specified variable (if applicable)
         */
        if (++ii == errindex) {
            var = var->nextVariable;
            continue;
        }

        /*
         * clone the next variable. Cleanup if alloc fails
         */
        newvar = (Types_VariableList *)
            malloc(sizeof(Types_VariableList));
        if (Client_cloneVar(var, newvar)) {
            if (newvar)
                free((char *) newvar);
            Api_freeVarbind(newhead);
            return NULL;
        }

        /*
         * add cloned variable to new list
         */
        if (NULL == newhead)
            newhead = newvar;
        if (oldvar)
            oldvar->nextVariable = newvar;
        oldvar = newvar;

        var = var->nextVariable;
    }
    return newhead;
}


/*
 * Copy some or all variables from source PDU to target PDU.
 * This function consolidates many of the needs of PDU variables:
 * Clone PDU : copy all the variables.
 * Split PDU : skip over some variables to copy other variables.
 * Fix PDU   : remove variable associated with error index.
 *
 * Designed to work with Client_clonePduHeader.
 *
 * If drop_err is set, drop any variable associated with errindex.
 * If skip_count is set, skip the number of variable in pdu's list.
 * While copy_count is greater than zero, copy pdu variables to newpdu.
 *
 * If an error occurs, newpdu is freed and pointer is set to 0.
 *
 * Returns a pointer to the cloned PDU if successful.
 * Returns 0 if failure.
 */
static
Types_Pdu    *
Client_copyPduVars(Types_Pdu *pdu,        /* source PDU */
               Types_Pdu *newpdu,     /* target PDU */
               int drop_err,    /* !=0 drop errored variable */
               int skip_count,  /* !=0 number of variables to skip */
               int copy_count)
{                               /* !=0 number of variables to copy */
    Types_VariableList *var;

    int             drop_idx;

    if (!newpdu)
        return NULL;            /* where is PDU to copy to ? */

    if (drop_err)
        drop_idx = pdu->errindex - skip_count;
    else
        drop_idx = 0;

    var = pdu->variables;
    while (var && (skip_count-- > 0))   /* skip over pdu variables */
        var = var->nextVariable;

    newpdu->variables = Client_copyVarlist(var, drop_idx, copy_count);

    return newpdu;
}


/*
 * Creates (allocates and copies) a clone of the input PDU.
 * If drop_err is set, don't copy any variable associated with errindex.
 * This function is called by Client_clonePdu and Client_fixPdu.
 *
 * Returns a pointer to the cloned PDU if successful.
 * Returns 0 if failure.
 */
static
Types_Pdu    *
Client_clonePdu2(Types_Pdu *pdu, int drop_err)
{
    Types_Pdu    *newpdu;
    newpdu = Client_clonePduHeader(pdu);
    newpdu = Client_copyPduVars(pdu, newpdu, drop_err, 0, 10000);   /* skip none, copy all */

    return newpdu;
}


/*
 * This function will clone a full varbind list
 *
 * Returns a pointer to the cloned varbind list if successful.
 * Returns 0 if failure
 */
Types_VariableList *
Client_cloneVarbind(Types_VariableList * varlist)
{
    return Client_copyVarlist(varlist, 0, 10000);    /* skip none, copy all */
}

/*
 * This function will clone a PDU including all of its variables.
 *
 * Returns a pointer to the cloned PDU if successful.
 * Returns 0 if failure
 */
Types_Pdu    *
Client_clonePdu(Types_Pdu *pdu)
{
    return Client_clonePdu2(pdu, 0);  /* copies all variables */
}


/*
 * This function will clone a PDU including some of its variables.
 *
 * If skip_count is not zero, it defines the number of variables to skip.
 * If copy_count is not zero, it defines the number of variables to copy.
 *
 * Returns a pointer to the cloned PDU if successful.
 * Returns 0 if failure.
 */
Types_Pdu    *
Client_splitPdu(Types_Pdu *pdu, int skip_count, int copy_count)
{
    Types_Pdu    *newpdu;
    newpdu = Client_clonePduHeader(pdu);
    newpdu = Client_copyPduVars(pdu, newpdu, 0,     /* don't drop any variables */
                            skip_count, copy_count);

    return newpdu;
}


/*
 * If there was an error in the input pdu, creates a clone of the pdu
 * that includes all the variables except the one marked by the errindex.
 * The command is set to the input command and the reqid, errstat, and
 * errindex are set to default values.
 * If the error status didn't indicate an error, the error index didn't
 * indicate a variable, the pdu wasn't a get response message, the
 * marked variable was not present in the initial request, or there
 * would be no remaining variables, this function will return 0.
 * If everything was successful, a pointer to the fixed cloned pdu will
 * be returned.
 */
Types_Pdu    *
Client_fixPdu(Types_Pdu *pdu, int command)
{
    Types_Pdu    *newpdu;

    if ((pdu->command != PRIOT_MSG_RESPONSE)
        || (pdu->errstat == PRIOT_ERR_NOERROR)
        || (NULL == pdu->variables)
        || (pdu->errindex > (int)Client_varbindLen(pdu))
        || (pdu->errindex <= 0)) {
        return NULL;            /* pre-condition tests fail */
    }

    newpdu = Client_clonePdu2(pdu, 1);        /* copies all except errored variable */
    if (!newpdu)
        return NULL;
    if (!newpdu->variables) {
        Api_freePdu(newpdu);
        return NULL;            /* no variables. "should not happen" */
    }
    newpdu->command = command;
    newpdu->reqid = Api_getNextReqid();
    newpdu->msgid = Api_getNextMsgid();
    newpdu->errstat = API_DEFAULT_ERRSTAT;
    newpdu->errindex = API_DEFAULT_ERRINDEX;

    return newpdu;
}


/*
 * Returns the number of variables bound to a PDU structure
 */
unsigned long
Client_varbindLen(Types_Pdu *pdu)
{
    register Types_VariableList *vars;
    unsigned long   retVal = 0;
    if (pdu)
        for (vars = pdu->variables; vars; vars = vars->nextVariable) {
            retVal++;
        }

    return retVal;
}

/*
 * Add object identifier name to SNMP variable.
 * If the name is large, additional memory is allocated.
 * Returns 0 if successful.
 */

int
Client_setVarObjid(Types_VariableList * vp,
                   const oid * objid, size_t name_length)
{
    size_t          len = sizeof(oid) * name_length;

    if (vp->name != vp->nameLoc && vp->name != NULL) {
        /*
         * Probably previously-allocated "big storage".  Better free it
         * else memory leaks possible.
         */
        free(vp->name);
    }

    /*
     * use built-in storage for smaller values
     */
    if (len <= sizeof(vp->nameLoc)) {
        vp->name = vp->nameLoc;
    } else {
        vp->name = (oid *) malloc(len);
        if (!vp->name)
            return 1;
    }
    if (objid)
        memmove(vp->name, objid, len);
    vp->nameLength = name_length;
    return 0;
}

/**
 * Client_setVarTypedValue is used to set data into the Types_VariableList
 * structure.  Used to return data to the snmp request via the
 * netsnmp_request_info structure's requestvb pointer.
 *
 * @param newvar   the structure gets populated with the given data, type,
 *                 val_str, and valLen.
 * @param type     is the asn data type to be copied
 * @param val_str  is a buffer containing the value to be copied into the
 *                 newvar structure.
 * @param valLen  the length of val_str
 *
 * @return returns 0 on success and 1 on a malloc error
 */

int
Client_setVarTypedValue(Types_VariableList * newvar, u_char type,
                         const void * val_str, size_t valLen)
{
    newvar->type = type;
    return Client_setVarValue(newvar, val_str, valLen);
}

int
Client_setVarTypedInteger(Types_VariableList * newvar,
                           u_char type, long val)
{
    newvar->type = type;
    return Client_setVarValue(newvar, &val, sizeof(long));
}

int
Client_countVarbinds(Types_VariableList * var_ptr)
{
    int             count = 0;

    for (; var_ptr != NULL; var_ptr = var_ptr->nextVariable)
        count++;

    return count;
}


int
Client_countVarbindsOfType(Types_VariableList * var_ptr, u_char type)
{
    int             count = 0;

    for (; var_ptr != NULL; var_ptr = var_ptr->nextVariable)
        if (var_ptr->type == type)
            count++;

    return count;
}

Types_VariableList *
Client_findVarbindOfType(Types_VariableList * var_ptr, u_char type)
{
    for (; var_ptr != NULL && var_ptr->type != type;
         var_ptr = var_ptr->nextVariable);

    return var_ptr;
}

Types_VariableList*
Client_findVarbindInList( Types_VariableList *vblist,
                      const oid *name, size_t len)
{
    for (; vblist != NULL; vblist = vblist->nextVariable)
        if (!Api_oidCompare(vblist->name, vblist->nameLength, name, len))
            return vblist;

    return NULL;
}

/*
 * Add some value to SNMP variable.
 * If the value is large, additional memory is allocated.
 * Returns 0 if successful.
 */

int
Client_setVarValue(Types_VariableList * vars,
                   const void * value, size_t len)
{
    int             largeval = 1;

    /*
     * xxx-rks: why the unconditional free? why not use existing
     * memory, if len < vars->valLen ?
     */
    if (vars->val.string && vars->val.string != vars->buf) {
        free(vars->val.string);
    }
    vars->val.string = NULL;
    vars->valLen = 0;

    if (value == NULL && len > 0) {
        Logger_log(LOGGER_PRIORITY_ERR, "bad size for NULL value\n");
        return 1;
    }

    /*
     * use built-in storage for smaller values
     */
    if (len <= sizeof(vars->buf)) {
        vars->val.string = (u_char *) vars->buf;
        largeval = 0;
    }

    if ((0 == len) || (NULL == value)) {
        vars->val.string[0] = 0;
        return 0;
    }

    vars->valLen = len;
    switch (vars->type) {
    case ASN01_INTEGER:
    case ASN01_UNSIGNED:
    case ASN01_TIMETICKS:
    case ASN01_COUNTER:
    case ASN01_UINTEGER:
        if (vars->valLen == sizeof(int)) {
            if (ASN01_INTEGER == vars->type) {
                const int      *val_int
                    = (const int *) value;
                *(vars->val.integer) = (long) *val_int;
            } else {
                const u_int    *val_uint
                    = (const u_int *) value;
                *(vars->val.integer) = (unsigned long) *val_uint;
            }
        }
        else if (vars->valLen == sizeof(long)){
            const u_long   *val_ulong
                = (const u_long *) value;
            *(vars->val.integer) = *val_ulong;
            if (*(vars->val.integer) > 0xffffffff) {
                Logger_log(LOGGER_PRIORITY_ERR,"truncating integer value > 32 bits\n");
                *(vars->val.integer) &= 0xffffffff;
            }
        }
        else if (vars->valLen == sizeof(short)) {
            if (ASN01_INTEGER == vars->type) {
                const short      *val_short
                    = (const short *) value;
                *(vars->val.integer) = (long) *val_short;
            } else {
                const u_short    *val_ushort
                    = (const u_short *) value;
                *(vars->val.integer) = (unsigned long) *val_ushort;
            }
        }
        else if (vars->valLen == sizeof(char)) {
            if (ASN01_INTEGER == vars->type) {
                const char      *val_char
                    = (const char *) value;
                *(vars->val.integer) = (long) *val_char;
            } else {
                    const u_char    *val_uchar
                    = (const u_char *) value;
                *(vars->val.integer) = (unsigned long) *val_uchar;
            }
        }
        else {
            Logger_log(LOGGER_PRIORITY_ERR,"bad size for integer-like type (%d)\n",
                     (int)vars->valLen);
            return (1);
        }
        vars->valLen = sizeof(long);
        break;

    case ASN01_OBJECT_ID:
    case ASN01_PRIV_IMPLIED_OBJECT_ID:
    case ASN01_PRIV_INCL_RANGE:
    case ASN01_PRIV_EXCL_RANGE:
        if (largeval) {
            vars->val.objid = (oid *) malloc(vars->valLen);
        }
        if (vars->val.objid == NULL) {
            Logger_log(LOGGER_PRIORITY_ERR,"no storage for OID\n");
            return 1;
        }
        memmove(vars->val.objid, value, vars->valLen);
        break;

    case ASN01_IPADDRESS: /* snmp_build_var_op treats IPADDR like a string */
        if (4 != vars->valLen) {
            Assert_assert("ipaddress length == 4");
        }
        /** FALL THROUGH */
    case ASN01_PRIV_IMPLIED_OCTET_STR:
    case ASN01_OCTET_STR:
    case ASN01_BIT_STR:
    case ASN01_OPAQUE:
    case ASN01_NSAP:
        if (vars->valLen >= sizeof(vars->buf)) {
            vars->val.string = (u_char *) malloc(vars->valLen + 1);
        }
        if (vars->val.string == NULL) {
            Logger_log(LOGGER_PRIORITY_ERR,"no storage for string\n");
            return 1;
        }
        memmove(vars->val.string, value, vars->valLen);
        /*
         * Make sure the string is zero-terminated; some bits of code make
         * this assumption.  Easier to do this here than fix all these wrong
         * assumptions.
         */
        vars->val.string[vars->valLen] = '\0';
        break;

    case PRIOT_NOSUCHOBJECT:
    case PRIOT_NOSUCHINSTANCE:
    case PRIOT_ENDOFMIBVIEW:
    case ASN01_NULL:
        vars->valLen = 0;
        vars->val.string = NULL;
        break;

    case ASN01_OPAQUE_U64:
    case ASN01_OPAQUE_I64:
    case ASN01_COUNTER64:
        if (largeval) {
            Logger_log(LOGGER_PRIORITY_ERR,"bad size for counter 64 (%d)\n",
                     (int)vars->valLen);
            return (1);
        }
        vars->valLen = sizeof(struct Asn01_Counter64_s);
        memmove(vars->val.counter64, value, vars->valLen);
        break;

    case ASN01_OPAQUE_FLOAT:
        if (largeval) {
            Logger_log(LOGGER_PRIORITY_ERR,"bad size for opaque float (%d)\n",
                     (int)vars->valLen);
            return (1);
        }
        vars->valLen = sizeof(float);
        memmove(vars->val.floatVal, value, vars->valLen);
        break;

    case ASN01_OPAQUE_DOUBLE:
        if (largeval) {
            Logger_log(LOGGER_PRIORITY_ERR,"bad size for opaque double (%d)\n",
                     (int)vars->valLen);
            return (1);
        }
        vars->valLen = sizeof(double);
        memmove(vars->val.doubleVal, value, vars->valLen);
        break;

    default:
        Logger_log(LOGGER_PRIORITY_ERR,"Internal error in type switching\n");
        Api_setDetail("Internal error in type switching\n");
        return (1);
    }

    return 0;
}

void
Client_replaceVarTypes(Types_VariableList * vbl, u_char old_type,
                       u_char new_type)
{
    while (vbl) {
        if (vbl->type == old_type) {
            Client_setVarTypedValue(vbl, new_type, NULL, 0);
        }
        vbl = vbl->nextVariable;
    }
}

void
Client_resetVarTypes(Types_VariableList * vbl, u_char new_type)
{
    while (vbl) {
        Client_setVarTypedValue(vbl, new_type, NULL, 0);
        vbl = vbl->nextVariable;
    }
}

int
Client_synchResponseCb(Types_Session * ss,
                       Types_Pdu *pdu,
                       Types_Pdu **response, Types_CallbackFT pcb)
{
    struct Client_SynchState_s lstate, *state;
    Types_CallbackFT   cbsav;
    void           *cbmagsav;
    int             numfds, count;
    fd_set          fdset;
    struct timeval  timeout, *tvp;
    int             block;

    memset((void *) &lstate, 0, sizeof(lstate));
    state = &lstate;
    cbsav = ss->callback;
    cbmagsav = ss->callback_magic;
    ss->callback = pcb;
    ss->callback_magic = (void *) state;

    if ((state->reqid = Api_send(ss, pdu)) == 0) {
        Api_freePdu(pdu);
        state->status = CLIENT_STAT_ERROR;
    } else
        state->waiting = 1;

    while (state->waiting) {
        numfds = 0;
        FD_ZERO(&fdset);
        block = PRIOT_SNMPBLOCK;
        tvp = &timeout;
        timerclear(tvp);
        Api_sessSelectInfoFlags(0, &numfds, &fdset, tvp, &block,
                                    SESSION_SELECT_NOALARMS);
        if (block == 1)
            tvp = NULL;         /* block without timeout */
        count = select(numfds, &fdset, NULL, NULL, tvp);
        if (count > 0) {
            Api_read(&fdset);
        } else {
            switch (count) {
            case 0:
                Api_timeout();
                break;
            case -1:
                if (errno == EINTR) {
                    continue;
                } else {
                    api_priotErrno = ErrorCode_GENERR;    /*MTCRITICAL_RESOURCE */
                    /*
                     * CAUTION! if another thread closed the socket(s)
                     * waited on here, the session structure was freed.
                     * It would be nice, but we can't rely on the pointer.
                     * ss->s_snmp_errno = ErrorCode_GENERR;
                     * ss->s_errno = errno;
                     */
                    Api_setDetail(strerror(errno));
                }
                /*
                 * FALLTHRU
                 */
            default:
                state->status = CLIENT_STAT_ERROR;
                state->waiting = 0;
            }
        }

        if ( ss->flags & API_FLAGS_RESP_CALLBACK ) {
            void (*cb)(void);
            cb = (void (*)(void))(ss->myvoid);
            cb();        /* Used to invoke 'netsnmp_check_outstanding_agent_requests();'
                            on internal AgentX queries.  */
        }
    }
    *response = state->pdu;
    ss->callback = cbsav;
    ss->callback_magic = cbmagsav;
    return state->status;
}

int
Client_synchResponse(Types_Session * ss,
                    Types_Pdu *pdu, Types_Pdu **response)
{
    return Client_synchResponseCb(ss, pdu, response, Client_synchInput);
}

int
Client_sessSynchResponse(void *sessp,
                         Types_Pdu *pdu, Types_Pdu **response)
{
    Types_Session *ss;
    struct Client_SynchState_s lstate, *state;
    Types_CallbackFT   cbsav;
    void           *cbmagsav;
    int             numfds, count;
    fd_set          fdset;
    struct timeval  timeout, *tvp;
    int             block;

    ss = Api_sessSession(sessp);
    if (ss == NULL) {
        return CLIENT_STAT_ERROR;
    }

    memset((void *) &lstate, 0, sizeof(lstate));
    state = &lstate;
    cbsav = ss->callback;
    cbmagsav = ss->callback_magic;
    ss->callback = Client_synchInput;
    ss->callback_magic = (void *) state;

    if ((state->reqid = Api_sessSend(sessp, pdu)) == 0) {
        Api_freePdu(pdu);
        state->status = CLIENT_STAT_ERROR;
    } else
        state->waiting = 1;

    while (state->waiting) {
        numfds = 0;
        FD_ZERO(&fdset);
        block = PRIOT_SNMPBLOCK;
        tvp = &timeout;
        timerclear(tvp);
        Api_sessSelectInfoFlags(sessp, &numfds, &fdset, tvp, &block,
                                    SESSION_SELECT_NOALARMS);
        if (block == 1)
            tvp = NULL;         /* block without timeout */
        count = select(numfds, &fdset, NULL, NULL, tvp);
        if (count > 0) {
            Api_sessRead(sessp, &fdset);
        } else
            switch (count) {
            case 0:
                Api_sessTimeout(sessp);
                break;
            case -1:
                if (errno == EINTR) {
                    continue;
                } else {
                    api_priotErrno = ErrorCode_GENERR;    /*MTCRITICAL_RESOURCE */
                    /*
                     * CAUTION! if another thread closed the socket(s)
                     * waited on here, the session structure was freed.
                     * It would be nice, but we can't rely on the pointer.
                     * ss->s_snmp_errno = ErrorCode_GENERR;
                     * ss->s_errno = errno;
                     */
                    Api_setDetail(strerror(errno));
                }
                /*
                 * FALLTHRU
                 */
            default:
                state->status = CLIENT_STAT_ERROR;
                state->waiting = 0;
            }
    }
    *response = state->pdu;
    ss->callback = cbsav;
    ss->callback_magic = cbmagsav;
    return state->status;
}


const char     *
Client_errstring(int errstat)
{
    const char * const error_string[19] = {
        "(noError) No Error",
        "(tooBig) Response message would have been too large.",
        "(noSuchName) There is no such variable name in this MIB.",
        "(badValue) The value given has the wrong type or length.",
        "(readOnly) The two parties used do not have access to use the specified SNMP PDU.",
        "(genError) A general failure occured",
        "noAccess",
        "wrongType (The set datatype does not match the data type the agent expects)",
        "wrongLength (The set value has an illegal length from what the agent expects)",
        "wrongEncoding",
        "wrongValue (The set value is illegal or unsupported in some way)",
        "noCreation (That table does not support row creation or that object can not ever be created)",
        "inconsistentValue (The set value is illegal or unsupported in some way)",
        "resourceUnavailable (This is likely a out-of-memory failure within the agent)",
        "commitFailed",
        "undoFailed",
        "authorizationError (access denied to that object)",
        "notWritable (That object does not support modification)",
        "inconsistentName (That object can not currently be created)"
    };

    if (errstat <= PRIOT_MAX_PRIOT_ERR && errstat >= PRIOT_ERR_NOERROR) {
        return error_string[errstat];
    } else {
        return "Unknown Error";
    }
}



/*
 *
 *  Convenience routines to make various requests
 *  over the specified SNMP session.
 *
 */

static Types_Session *client_defQuerySession = NULL;

void
Client_querySetDefaultSession( Types_Session *sess) {
    DEBUG_MSGTL(("iquery", "set default session %p\n", sess));
    client_defQuerySession = sess;
}

/**
 * Return a pointer to the default internal query session.
 */
Types_Session *
Client_queryGetDefaultSessionUnchecked( void ) {
    DEBUG_MSGTL(("iquery", "get default session %p\n", client_defQuerySession));
    return client_defQuerySession;
}

/**
 * Return a pointer to the default internal query session and log a
 * warning message once if this session does not exist.
 */
Types_Session *
Client_queryGetDefaultSession( void ) {
    static int warning_logged = 0;

    if (! client_defQuerySession && ! warning_logged) {
        if (! DefaultStore_getString(DSSTORAGE.APPLICATION_ID,
                                    DSAGENT_INTERNAL_SECNAME)) {
            Logger_log(LOGGER_PRIORITY_WARNING,
                     "iquerySecName has not been configured - internal queries will fail\n");
        } else {
            Logger_log(LOGGER_PRIORITY_WARNING,
                     "default session is not available - internal queries will fail\n");
        }
        warning_logged = 1;
    }

    return Client_queryGetDefaultSessionUnchecked();
}


/*
 * Internal utility routine to actually send the query
 */
static int Client_query(Types_VariableList *list,
                  int                    request,
                  Types_Session       *session) {

    Types_Pdu *pdu      = Client_pduCreate( request );
    Types_Pdu *response = NULL;
    Types_VariableList *vb1, *vb2, *vtmp;
    int ret, count;

    DEBUG_MSGTL(("iquery", "query on session %p\n", session));
    /*
     * Clone the varbind list into the request PDU...
     */
    pdu->variables = Client_cloneVarbind( list );
retry:
    if ( session )
        ret = Client_synchResponse(            session, pdu, &response );
    else if (client_defQuerySession)
        ret = Client_synchResponse( client_defQuerySession, pdu, &response );
    else {
        /* No session specified */
        Api_freePdu(pdu);
        return PRIOT_ERR_GENERR;
    }
    DEBUG_MSGTL(("iquery", "query returned %d\n", ret));

    /*
     * ....then copy the results back into the
     * list (assuming the request succeeded!).
     * This avoids having to worry about how this
     * list was originally allocated.
     */
    if ( ret == PRIOT_ERR_NOERROR ) {
        if ( response->errstat != PRIOT_ERR_NOERROR ) {
            DEBUG_MSGT(("iquery", "Error in packet: %s\n",
                       Client_errstring(response->errstat)));
            /*
             * If the request failed, then remove the
             *  offending varbind and try again.
             *  (all except SET requests)
             *
             * XXX - implement a library version of
             *       NETSNMP_DS_APP_DONT_FIX_PDUS ??
             */
            ret = response->errstat;
            if (response->errindex != 0) {
                DEBUG_MSGT(("iquery:result", "Failed object:\n"));
                for (count = 1, vtmp = response->variables;
                     vtmp && count != response->errindex;
                     vtmp = vtmp->nextVariable, count++)
                    /*EMPTY*/;
                if (vtmp)
                    DEBUG_MSGVAR(("iquery:result", vtmp));
                DEBUG_MSG(("iquery:result", "\n"));
            }
            if (request != PRIOT_MSG_SET &&
                response->errindex != 0) {
                DEBUG_MSGTL(("iquery", "retrying query (%d, %ld)\n", ret, response->errindex));
                pdu = Client_fixPdu( response, request );
                Api_freePdu( response );
                response = NULL;
                if ( pdu != NULL )
                    goto retry;
            }
        } else {
            for (vb1 = response->variables, vb2 = list;
                 vb1;
                 vb1 = vb1->nextVariable,  vb2 = vb2->nextVariable) {
                DEBUG_MSGVAR(("iquery:result", vb1));
                DEBUG_MSG(("iquery:results", "\n"));
                if ( !vb2 ) {
                    ret = PRIOT_ERR_GENERR;
                    break;
                }
                vtmp = vb2->nextVariable;
                Api_freeVarInternals( vb2 );
                Client_cloneVar( vb1, vb2 );
                vb2->nextVariable = vtmp;
            }
        }
    } else {
        /* Distinguish snmp_send errors from SNMP errStat errors */
        ret = -ret;
    }
    Api_freePdu( response );
    return ret;
}

/*
 * These are simple wrappers round the internal utility routine
 */
int Client_queryGet(Types_VariableList *list,
                      Types_Session       *session){
    return Client_query( list, PRIOT_MSG_GET, session );
}


int Client_queryGetnext(Types_VariableList *list,
                          Types_Session       *session){
    return Client_query( list, PRIOT_MSG_GETNEXT, session );
}


int Client_querySet(Types_VariableList *list,
                      Types_Session       *session){
    return Client_query( list, PRIOT_MSG_SET, session );
}

/*
 * A walk needs a bit more work.
 */
int Client_queryWalk(Types_VariableList *list,
                       Types_Session       *session) {
    /*
     * Create a working copy of the original (single)
     * varbind, so we can use this varbind parameter
     * to check when we've finished walking this subtree.
     */
    Types_VariableList *vb = Client_cloneVarbind( list );
    Types_VariableList *res_list = NULL;
    Types_VariableList *res_last = NULL;
    int ret;

    /*
     * Now walk the tree as usual
     */
    ret = Client_query( vb, PRIOT_MSG_GETNEXT, session );
    while ( ret == PRIOT_ERR_NOERROR &&
        Api_oidtreeCompare( list->name, list->nameLength,
                                vb->name,   vb->nameLength ) == 0) {

    if (vb->type == PRIOT_ENDOFMIBVIEW ||
        vb->type == PRIOT_NOSUCHOBJECT ||
        vb->type == PRIOT_NOSUCHINSTANCE)
        break;

        /*
         * Copy each response varbind to the end of the result list
         * and then re-use this to ask for the next entry.
         */
        if ( res_last ) {
            res_last->nextVariable = Client_cloneVarbind( vb );
            res_last = res_last->nextVariable;
        } else {
            res_list = Client_cloneVarbind( vb );
            res_last = res_list;
        }
        ret = Client_query( vb, PRIOT_MSG_GETNEXT, session );
    }
    /*
     * Copy the first result back into the original varbind parameter,
     * add the rest of the results (if any), and clean up.
     */
    if ( res_list ) {
        Client_cloneVar( res_list, list );
        list->nextVariable = res_list->nextVariable;
        res_list->nextVariable = NULL;
        Api_freeVarbind( res_list );
    }
    Api_freeVarbind( vb );
    return ret;
}

/** **************************************************************************
 *
 * state machine
 *
 */
int
Client_stateMachineRun( Client_StateMachineInput *input)
{
    Client_StateMachineStep *current, *last;

    Assert_requirePtrLRV( input, ErrorCode_GENERR );
    Assert_requirePtrLRV( input->steps, ErrorCode_GENERR );
    last = current = input->steps;

    DEBUG_MSGT(("state_machine:run", "starting step: %s\n", current->name));

    while (current) {

        /*
         * log step and check for required data
         */
        DEBUG_MSGT(("state_machine:run", "at step: %s\n", current->name));
        if (NULL == current->run) {
            DEBUG_MSGT(("state_machine:run", "no run step\n"));
            current->result = last->result;
            break;
        }

        /*
         * run step
         */
        DEBUG_MSGT(("state_machine:run", "running step: %s\n", current->name));
        current->result = (*current->run)( input, current );
        ++input->steps_so_far;

        /*
         * log result and move to next step
         */
        input->last_run = current;
        DEBUG_MSGT(("state_machine:run:result", "step %s returned %d\n",
                   current->name, current->result));
        if (ErrorCode_SUCCESS == current->result)
            current = current->on_success;
        else if (ErrorCode_ABORT == current->result) {
            DEBUG_MSGT(("state_machine:run:result", "ABORT from %s\n",
                       current->name));
            break;
        }
        else
            current = current->on_error;
    }

    /*
     * run cleanup
     */
    if ((input->cleanup) && (input->cleanup->run))
        (*input->cleanup->run)( input, input->last_run );

    return input->last_run->result;
}

/** **************************************************************************
 *
 * row create state machine steps
 *
 */
typedef struct Client_RowcreateState_s {

    Types_Session        *session;
    Types_VariableList  *vars;
    int                     row_status_index;
} Client_RowcreateState;

static Types_VariableList *
Client_getVbNum2(Types_VariableList *vars, int index)
{
    for (; vars && index > 0; --index)
        vars = vars->nextVariable;

    if (!vars || index > 0)
        return NULL;

    return vars;
}


/*
 * cleanup
 */
static int
Client_rowStatusStateCleanup(Client_StateMachineInput *input,
                 Client_StateMachineStep *step)
{
    Client_RowcreateState       *ctx;

    Assert_requirePtrLRV( input, ErrorCode_ABORT );
    Assert_requirePtrLRV( step, ErrorCode_ABORT );

    DEBUG_MSGT(("row_create:called", "Client_rowStatusStateCleanup, last run step was %s rc %d\n",
               step->name, step->result));

    ctx = (Client_RowcreateState *)input->input_context;
    if (ctx && ctx->vars)
        Api_freeVarbind( ctx->vars );

    return ErrorCode_SUCCESS;
}

/*
 * send a request to activate the row
 */
static int
Client_rowStatusStateActivate(Client_StateMachineInput *input,
                  Client_StateMachineStep *step)
{
    Client_RowcreateState       *ctx;
    Types_VariableList *rs_var, *var = NULL;
    int32_t                rc, val = TC_RS_ACTIVE;

    Assert_requirePtrLRV( input, ErrorCode_GENERR );
    Assert_requirePtrLRV( step, ErrorCode_GENERR );
    Assert_requirePtrLRV( input->input_context, ErrorCode_GENERR );

    ctx = (Client_RowcreateState *)input->input_context;

    DEBUG_MSGT(("row_create:called", "called %s\n", step->name));

    /*
     * just send the rowstatus varbind
     */
    rs_var = Client_getVbNum2(ctx->vars, ctx->row_status_index);
    Assert_requirePtrLRV(rs_var, ErrorCode_GENERR);

    var = Api_varlistAddVariable(&var, rs_var->name, rs_var->nameLength,
                                    rs_var->type, &val, sizeof(val));
    Assert_requirePtrLRV( var, ErrorCode_GENERR );

    /*
     * send set
     */
    rc = Client_querySet( var, ctx->session );
    if (-2 == rc)
        rc = ErrorCode_ABORT;

    Api_freeVarbind(var);

    return rc;
}

/*
 * send each non-row status column, one at a time
 */
static int
Client_rowStatusStateSingleValueCols(Client_StateMachineInput *input,
                                    Client_StateMachineStep *step)
{
    Client_RowcreateState       *ctx;
    Types_VariableList *var, *tmp_next, *row_status;
    int                    rc = ErrorCode_GENERR;

    Assert_requirePtrLRV( input, ErrorCode_GENERR );
    Assert_requirePtrLRV( step, ErrorCode_GENERR );
    Assert_requirePtrLRV( input->input_context, ErrorCode_GENERR );

    ctx = (Client_RowcreateState *)input->input_context;

    DEBUG_MSGT(("row_create:called", "called %s\n", step->name));

    row_status = Client_getVbNum2(ctx->vars, ctx->row_status_index);
    Assert_requirePtrLRV(row_status, ErrorCode_GENERR);

    /*
     * try one varbind at a time
     */
    for (var = ctx->vars; var; var = var->nextVariable) {
        if (var == row_status)
            continue;

        tmp_next = var->nextVariable;
        var->nextVariable = NULL;

        /*
         * send set
         */
        rc = Client_querySet( var, ctx->session );
        var->nextVariable = tmp_next;
        if (-2 == rc)
            rc = ErrorCode_ABORT;
        if (rc != ErrorCode_SUCCESS)
            break;
    }

    return rc;
}

/*
 * send all values except row status
 */
static int
Client_rowStatusStateMultipleValuesCols(Client_StateMachineInput *input,
                                       Client_StateMachineStep *step)
{
    Client_RowcreateState       *ctx;
    Types_VariableList *vars, *var, *last, *row_status;
    int                    rc;

    Assert_requirePtrLRV( input, ErrorCode_GENERR );
    Assert_requirePtrLRV( step, ErrorCode_GENERR );
    Assert_requirePtrLRV( input->input_context, ErrorCode_GENERR );

    ctx = (Client_RowcreateState *)input->input_context;

    DEBUG_MSGT(("row_create:called", "called %s\n", step->name));

    vars = Client_cloneVarbind(ctx->vars);
    Assert_requirePtrLRV(vars, ErrorCode_GENERR);

    row_status = Client_getVbNum2(vars, ctx->row_status_index);
    if (NULL == row_status) {
        Api_freeVarbind(vars);
        return ErrorCode_GENERR;
    }

    /*
     * remove row status varbind
     */
    if (row_status == vars) {
        vars = row_status->nextVariable;
        row_status->nextVariable = NULL;
    }
    else {
        for (last=vars, var=last->nextVariable;
             var;
             last=var, var = var->nextVariable) {
            if (var == row_status) {
                last->nextVariable = var->nextVariable;
                break;
            }
        }
    }
    Api_freeVar(row_status);

    /*
     * send set
     */
    rc = Client_querySet( vars, ctx->session );
    if (-2 == rc)
        rc = ErrorCode_ABORT;

    Api_freeVarbind(vars);

    return rc;
}

/*
 * send a createAndWait request with no other values
 */
static int
Client_rowStatusStateSingleValueCreateAndWait(Client_StateMachineInput *input,
                                             Client_StateMachineStep *step)
{
    Client_RowcreateState       *ctx;
    Types_VariableList *var = NULL, *rs_var;
    int32_t                rc, val = TC_RS_CREATEANDWAIT;

    Assert_requirePtrLRV( input, ErrorCode_GENERR );
    Assert_requirePtrLRV( step, ErrorCode_GENERR );
    Assert_requirePtrLRV( input->input_context, ErrorCode_GENERR );

    ctx = (Client_RowcreateState *)input->input_context;

    DEBUG_MSGT(("row_create:called", "called %s\n", step->name));

    rs_var = Client_getVbNum2(ctx->vars, ctx->row_status_index);
    Assert_requirePtrLRV(rs_var, ErrorCode_GENERR);

    var = Api_varlistAddVariable(&var, rs_var->name, rs_var->nameLength,
                                    rs_var->type, &val, sizeof(val));
    Assert_requirePtrLRV(var, ErrorCode_GENERR);

    /*
     * send set
     */
    rc = Client_querySet( var, ctx->session );
    if (-2 == rc)
        rc = ErrorCode_ABORT;

    Api_freeVarbind(var);

    return rc;
}

/*
 * send a creatAndWait request with all values
 */
static int
Client_rowStatusStateAllValuesCreateAndWait(Client_StateMachineInput *input,
                                           Client_StateMachineStep *step)
{
    Client_RowcreateState       *ctx;
    Types_VariableList *vars, *rs_var;
    int                    rc;

    Assert_requirePtrLRV( input, ErrorCode_GENERR );
    Assert_requirePtrLRV( step, ErrorCode_GENERR );
    Assert_requirePtrLRV( input->input_context, ErrorCode_GENERR );

    ctx = (Client_RowcreateState *)input->input_context;

    DEBUG_MSGT(("row_create:called", "called %s\n", step->name));

    vars = Client_cloneVarbind(ctx->vars);
    Assert_requirePtrLRV(vars, ErrorCode_GENERR);

    /*
     * make sure row stats is createAndWait
     */
    rs_var = Client_getVbNum2(vars, ctx->row_status_index);
    if (NULL == rs_var) {
        Api_freeVarbind(vars);
        return ErrorCode_GENERR;
    }

    if (*rs_var->val.integer != TC_RS_CREATEANDWAIT)
        *rs_var->val.integer = TC_RS_CREATEANDWAIT;

    /*
     * send set
     */
    rc = Client_querySet( vars, ctx->session );
    if (-2 == rc)
        rc = ErrorCode_ABORT;

    Api_freeVarbind(vars);

    return rc;
}


/**
 * send createAndGo request with all values
 */
static int
Client_rowStatusStateAllValuesCreateAndGo(Client_StateMachineInput *input,
                                         Client_StateMachineStep *step)
{
    Client_RowcreateState       *ctx;
    Types_VariableList *vars, *rs_var;
    int                    rc;

    Assert_requirePtrLRV( input, ErrorCode_GENERR );
    Assert_requirePtrLRV( step, ErrorCode_GENERR );
    Assert_requirePtrLRV( input->input_context, ErrorCode_GENERR );

    ctx = (Client_RowcreateState *)input->input_context;

    DEBUG_MSGT(("row_create:called", "called %s\n", step->name));

    vars = Client_cloneVarbind(ctx->vars);
    Assert_requirePtrLRV(vars, ErrorCode_GENERR);

    /*
     * make sure row stats is createAndGo
     */
    rs_var = Client_getVbNum2(vars, ctx->row_status_index + 1);
    if (NULL == rs_var) {
        Api_freeVarbind(vars);
        return ErrorCode_GENERR;
    }

    if (*rs_var->val.integer != TC_RS_CREATEANDGO)
        *rs_var->val.integer = TC_RS_CREATEANDGO;

    /*
     * send set
     */
    rc = Client_querySet( vars, ctx->session );
    if (-2 == rc)
        rc = ErrorCode_ABORT;

    Api_freeVarbind(vars);

    return rc;
}

/** **************************************************************************
 *
 * row api
 *
 */
int
Client_rowCreate(Types_Session *sess, Types_VariableList *vars,
                   int row_status_index)
{
    Client_StateMachineStep rc_cleanup =
        { "row_create_cleanup", 0, Client_rowStatusStateCleanup,
          0, NULL, NULL, 0, NULL };
    Client_StateMachineStep rc_activate =
        { "row_create_activate", 0, Client_rowStatusStateActivate,
          0, NULL, NULL, 0, NULL };
    Client_StateMachineStep rc_sv_cols =
        { "row_create_single_value_cols", 0,
          Client_rowStatusStateSingleValueCols, 0, &rc_activate,NULL, 0, NULL };
    Client_StateMachineStep rc_mv_cols =
        { "row_create_multiple_values_cols", 0,
          Client_rowStatusStateMultipleValuesCols, 0, &rc_activate, &rc_sv_cols,
          0, NULL };
    Client_StateMachineStep rc_sv_caw =
        { "row_create_single_value_createAndWait", 0,
          Client_rowStatusStateSingleValueCreateAndWait, 0, &rc_mv_cols, NULL,
          0, NULL };
    Client_StateMachineStep rc_av_caw =
        { "row_create_all_values_createAndWait", 0,
          Client_rowStatusStateAllValuesCreateAndWait, 0, &rc_activate,
          &rc_sv_caw, 0, NULL };
    Client_StateMachineStep rc_av_cag =
        { "row_create_all_values_createAndGo", 0,
          Client_rowStatusStateAllValuesCreateAndGo, 0, NULL, &rc_av_caw, 0,
          NULL };

    Client_StateMachineInput sm_input = { "row_create_machine", 0,
                                             &rc_av_cag, &rc_cleanup };
    Client_RowcreateState state;

    Assert_requirePtrLRV( sess, ErrorCode_GENERR);
    Assert_requirePtrLRV( vars, ErrorCode_GENERR);

    state.session = sess;
    state.vars = vars;

    state.row_status_index = row_status_index;
    sm_input.input_context = &state;

    Client_stateMachineRun( &sm_input);

    return ErrorCode_SUCCESS;
}
