#ifndef VARS_H
#define VARS_H


#include "Types.h"
#include "Agent.h"
/*
 * fail overloads non-negative integer value. it must be -1 !
 */
#define MATCH_FAILED	(-1)
#define MATCH_SUCCEEDED	0


struct Variable_s;

/**
 * Duplicates a variable.
 *
 * @return Pointer to the duplicate variable upon success; NULL upon
 *   failure.
 *
 * @see struct Variable_s
 * @see struct Variable1_s
 * @see struct Variable2_s
 * @see struct Variable3_s
 * @see struct Variable4_s
 * @see struct Variable7_s
 * @see struct Variable8_s
 * @see struct Variable13_s
 */

/*
 * Function pointer called by the master agent for writes.
 */
typedef int
(WriteMethodFT) ( int     action,
                  u_char* var_val,
                  u_char  var_val_type,
                  size_t  var_val_len,
                  u_char* statP,
                  oid*    name,
                  size_t  length );

/*
 * Function pointer called by the master agent for mib information retrieval
 */
typedef u_char*
(FindVarMethodFT) ( struct Variable_s* vp,
                    oid*              name,
                    size_t*            length,
                    int                exact,
                    size_t*            var_len,
                    WriteMethodFT **   write_method );

/*
 * Function pointer called by the master agent for setting up subagent requests
 */
typedef int
(AddVarMethod) ( AgentSession*        asp,
                 Types_VariableList*  vbp );


extern long     vars_longReturn;

extern u_char   vars_returnBuf[];

extern oid      vars_nullOid[];
extern int      vars_nullOidLen;

#define INST	0xFFFFFFFF      /* used to fill out the instance field of the variables table */

struct Variable_s {
    u_char           magic;  /* passed to function as a hint */
    char             type;   /* type of variable */
    /*
     * See important comment in snmp_vars.c relating to acl
     */
    u_short          acl;    /* access control list for variable */
    FindVarMethodFT* findVar;        /* function that finds variable */
    u_char           namelen;        /* length of above */
    oid             name[TYPES_MAX_OID_LEN];      /* object identifier of variable */
};

int   Vars_initAgent( const char * );
void  Vars_shutdownAgent( void );

int   Vars_shouldInit( const char *module_name );
void  Vars_addToInitList( char *module_list );

#endif // VARS_H
