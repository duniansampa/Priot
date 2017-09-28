#ifndef BABYSTEPS_H
#define BABYSTEPS_H


#include "AgentHandler.h"

/*
 * Flags for baby step modes
 */
enum BabyStepsMode_e{
    BabyStepsMode_NONE                = 0,
    BabyStepsMode_PRE_REQUEST         = (0x1 <<  1),
    BabyStepsMode_OBJECT_LOOKUP       = (0x1 <<  2),
    BabyStepsMode_CHECK_VALUE         = (0x1 <<  3),
    BabyStepsMode_ROW_CREATE          = (0x1 <<  4),
    BabyStepsMode_UNDO_SETUP          = (0x1 <<  5),
    BabyStepsMode_SET_VALUE           = (0x1 <<  6),
    BabyStepsMode_CHECK_CONSISTENCY   = (0x1 <<  7),
    BabyStepsMode_UNDO_SET            = (0x1 <<  8),
    BabyStepsMode_COMMIT              = (0x1 <<  9),
    BabyStepsMode_UNDO_COMMIT         = (0x1 << 10),
    BabyStepsMode_IRREVERSIBLE_COMMIT = (0x1 << 11),
    BabyStepsMode_UNDO_CLEANUP        = (0x1 << 12),
    BabyStepsMode_POST_REQUEST        = (0x1 << 13),
    BabyStepsMode_ALL                 = (0xffffffff),
    BabyStepsMode_CHECK_OBJECT        = BabyStepsMode_CHECK_VALUE,
    BabyStepsMode_SET_VALUES          = BabyStepsMode_SET_VALUE,
    BabyStepsMode_UNDO_SETS           = BabyStepsMode_UNDO_SET
};

/** @name baby_steps
 *
 * This helper expands the original net-snmp set modes into the newer, finer
 * grained modes.
 *
 *  @{ */

typedef struct BabyStepsModes_s {
   /** Number of handlers whose myvoid pointer points at this object. */
   int         refcnt;
   u_int       registered;
   u_int       completed;
} BabyStepsModes;


/** @name access_multiplexer
 *
 * This helper calls individual access methods based on the mode. All
 * access methods share the same handler, and the same myvoid pointer.
 * If you need individual myvoid pointers, check out the multiplexer
 * handler (though it currently only works for traditional modes).
 *
 *  @{ */

/** @struct netsnmp_mib_handler_access_methods
 *  Defines the access methods to be called by the access_multiplexer helper
 */
typedef struct BabyStepsAccessMethods_s {

   /*
    * baby step modes
    */
   NodeHandlerFT *pre_request;
   NodeHandlerFT *object_lookup;
   NodeHandlerFT *get_values;
   NodeHandlerFT *object_syntax_checks;
   NodeHandlerFT *row_creation;
   NodeHandlerFT *undo_setup;
   NodeHandlerFT *set_values;
   NodeHandlerFT *consistency_checks;
   NodeHandlerFT *commit;
   NodeHandlerFT *undo_sets;
   NodeHandlerFT *undo_cleanup;
   NodeHandlerFT *undo_commit;
   NodeHandlerFT *irreversible_commit;
   NodeHandlerFT *post_request;

   void * my_access_void;

}BabyStepsAccessMethods;


MibHandler*
BabySteps_handlerGet(u_long modes);

MibHandler*
BabySteps_accessMultiplexerGet( BabyStepsAccessMethods* );

int
BabySteps_mode2flag( u_int mode );

/** backwards compatability. don't use in new code */
#define BabySteps_getBabyStepsHandler BabySteps_handlerGet
#define BabySteps_initBabyStepsHelper BabySteps_handlerInit

#endif // BABYSTEPS_H
