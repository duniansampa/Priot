#include "BabySteps.h"
#include "System/Util/Utilities.h"
#include "System/Util/Debug.h"
#include "System/Util/Logger.h"
#include "AgentHandler.h"
#include "System/Containers/MapList.h"
#include "System/Util/Assert.h"

#include "Priot.h"

#define BABY_STEPS_PER_MODE_MAX     4
#define BSTEP_USE_ORIGINAL          0xffff

static u_short _babySteps_getModeMap[BABY_STEPS_PER_MODE_MAX] = {
                MODE_BSTEP_PRE_REQUEST, MODE_BSTEP_OBJECT_LOOKUP,
                BSTEP_USE_ORIGINAL, MODE_BSTEP_POST_REQUEST };

static u_short _babySteps_setModeMap[PRIOT_MSG_INTERNAL_SET_MAX][BABY_STEPS_PER_MODE_MAX] = {
    /*R1*/
    { MODE_BSTEP_PRE_REQUEST, MODE_BSTEP_OBJECT_LOOKUP, MODE_BSTEP_ROW_CREATE, MODE_BSTEP_CHECK_VALUE },
    /*R2*/
    { MODE_BSTEP_UNDO_SETUP, BabyStepsMode_NONE, BabyStepsMode_NONE, BabyStepsMode_NONE },
    /*A */
    { MODE_BSTEP_SET_VALUE,MODE_BSTEP_CHECK_CONSISTENCY, MODE_BSTEP_COMMIT, BabyStepsMode_NONE },
    /*C */
    { MODE_BSTEP_IRREVERSIBLE_COMMIT, MODE_BSTEP_UNDO_CLEANUP, MODE_BSTEP_POST_REQUEST, BabyStepsMode_NONE},
    /*F */
    { MODE_BSTEP_UNDO_CLEANUP, MODE_BSTEP_POST_REQUEST, BabyStepsMode_NONE, BabyStepsMode_NONE },
    /*U */
    { MODE_BSTEP_UNDO_COMMIT, MODE_BSTEP_UNDO_SET, MODE_BSTEP_UNDO_CLEANUP, MODE_BSTEP_POST_REQUEST}
};

static int
_BabySteps_helper(  MibHandler *handler,
                   HandlerRegistration *reginfo,
                   AgentRequestInfo *reqinfo,
                   RequestInfo *requests);
static int
_BabySteps_accessMultiplexer(   MibHandler *handler,
                               HandlerRegistration *reginfo,
                               AgentRequestInfo *reqinfo,
                               RequestInfo *requests);

/** @defgroup baby_steps baby_steps
 *  Calls your handler in baby_steps for set processing.
 *  @ingroup handler
 *  @{
 */

static BabyStepsModes *
_BabySteps_modesRef(BabyStepsModes *md)
{
    md->refcnt++;
    return md;
}

static void
_BabySteps_modesDeref(BabyStepsModes *md)
{
    if (--md->refcnt == 0)
    free(md);
}

/** returns a baby_steps handler that can be injected into a given
 *  handler chain.
 */
MibHandler *
BabySteps_handlerGet(u_long modes)
{
    MibHandler *mh;
    BabyStepsModes *md;

    mh = AgentHandler_createHandler("babySteps", _BabySteps_helper);
    if(!mh)
        return NULL;

    md = MEMORY_MALLOC_TYPEDEF(BabyStepsModes);
    if (NULL == md) {
        Logger_log(LOGGER_PRIORITY_ERR,"malloc failed in BabySteps_handlerGet\n");
        AgentHandler_handlerFree(mh);
        mh = NULL;
    }
    else {
    md->refcnt = 1;
        mh->myvoid = md;
    mh->data_clone = (void *(*)(void *))_BabySteps_modesRef;
    mh->data_free = (void (*)(void *))_BabySteps_modesDeref;
        if (0 == modes)
            modes = BabyStepsMode_ALL;
        md->registered = modes;
    }

    /*
     * don't set MIB_HANDLER_AUTO_NEXT, since we need to call lower
     * handlers with a munged mode.
     */

    return mh;
}

/** @internal Implements the baby_steps handler */
static int
_BabySteps_helper(MibHandler *handler,
                         HandlerRegistration *reginfo,
                         AgentRequestInfo *reqinfo,
                         RequestInfo *requests)
{
    BabyStepsModes *bs_modes;
    int save_mode, i, rc = PRIOT_ERR_NOERROR;
    u_short *mode_map_ptr;

    DEBUG_MSGTL(("babySteps", "Got request, mode %s\n",
                MapList_findLabel("agentMode",reqinfo->mode)));

    bs_modes = (BabyStepsModes*)handler->myvoid;
    Assert_assert(NULL != bs_modes);

    switch (reqinfo->mode) {

    case MODE_SET_RESERVE1:
        /*
         * clear completed modes
         * xxx-rks: this will break for pdus with set requests to different
         * rows in the same table when the handler is set up to use the row
         * merge helper as well (or if requests are serialized).
         */
        bs_modes->completed = 0;
        /** fall through */

    case MODE_SET_RESERVE2:
    case MODE_SET_ACTION:
    case MODE_SET_COMMIT:
    case MODE_SET_FREE:
    case MODE_SET_UNDO:
        mode_map_ptr = _babySteps_setModeMap[reqinfo->mode];
        break;

    default:
        /*
         * clear completed modes
         */
        bs_modes->completed = 0;

        mode_map_ptr = _babySteps_getModeMap;
    }

    /*
     * NOTE: if you update this chart, please update the versions in
     *       local/mib2c-conf.d/parent-set.m2i
     *       agent/mibgroup/helpers/baby_steps.c
     * while you're at it.
     */
    /*
     ***********************************************************************
     * Baby Steps Flow Chart (2004.06.05)                                  *
     *                                                                     *
     * +--------------+    +================+    U = unconditional path    *
     * |optional state|    ||required state||    S = path for success      *
     * +--------------+    +================+    E = path for error        *
     ***********************************************************************
     *
     *                        +--------------+
     *                        |     pre      |
     *                        |   request    |
     *                        +--------------+
     *                               | U
     * +-------------+        +==============+
     * |    row    |f|<-------||  object    ||
     * |  create   |1|      E ||  lookup    ||
     * +-------------+        +==============+
     *     E |   | S                 | S
     *       |   +------------------>|
     *       |                +==============+
     *       |              E ||   check    ||
     *       |<---------------||   values   ||
     *       |                +==============+
     *       |                       | S
     *       |                +==============+
     *       |       +<-------||   undo     ||
     *       |       |      E ||   setup    ||
     *       |       |        +==============+
     *       |       |               | S
     *       |       |        +==============+
     *       |       |        ||    set     ||-------------------------->+
     *       |       |        ||   value    || E                         |
     *       |       |        +==============+                           |
     *       |       |               | S                                 |
     *       |       |        +--------------+                           |
     *       |       |        |    check     |-------------------------->|
     *       |       |        |  consistency | E                         |
     *       |       |        +--------------+                           |
     *       |       |               | S                                 |
     *       |       |        +==============+         +==============+  |
     *       |       |        ||   commit   ||-------->||     undo   ||  |
     *       |       |        ||            || E       ||    commit  ||  |
     *       |       |        +==============+         +==============+  |
     *       |       |               | S                     U |<--------+
     *       |       |        +--------------+         +==============+
     *       |       |        | irreversible |         ||    undo    ||
     *       |       |        |    commit    |         ||     set    ||
     *       |       |        +--------------+         +==============+
     *       |       |               | U                     U |
     *       |       +-------------->|<------------------------+
     *       |                +==============+
     *       |                ||   undo     ||
     *       |                ||  cleanup   ||
     *       |                +==============+
     *       +---------------------->| U
     *                               |
     *                          (err && f1)------------------->+
     *                               |                         |
     *                        +--------------+         +--------------+
     *                        |    post      |<--------|      row     |
     *                        |   request    |       U |    release   |
     *                        +--------------+         +--------------+
     *
     */
    /*
     * save original mode
     */
    save_mode = reqinfo->mode;
    for(i = 0; i < BABY_STEPS_PER_MODE_MAX; ++i ) {
        /*
         * break if we run out of baby steps for this mode
         */
        if(mode_map_ptr[i] == BabyStepsMode_NONE)
            break;

        DEBUG_MSGTL(("babySteps", " baby step mode %s\n",
                    MapList_findLabel("babystepMode",mode_map_ptr[i])));

        /*
         * skip modes the handler didn't register for
         */
        if (BSTEP_USE_ORIGINAL != mode_map_ptr[i]) {
            u_int    mode_flag;

            /*
             * skip undo commit if commit wasn't hit, and
             * undo_cleanup if undo_setup wasn't hit.
             */
            if((MODE_SET_UNDO == save_mode) &&
               (MODE_BSTEP_UNDO_COMMIT == mode_map_ptr[i]) &&
               !(BabyStepsMode_COMMIT & bs_modes->completed)) {
                DEBUG_MSGTL(("babySteps",
                            "   skipping commit undo (no commit)\n"));
                continue;
            }
            else if((MODE_SET_FREE == save_mode) &&
               (MODE_BSTEP_UNDO_CLEANUP == mode_map_ptr[i]) &&
               !(BabyStepsMode_UNDO_SETUP & bs_modes->completed)) {
                DEBUG_MSGTL(("babySteps",
                            "   skipping undo cleanup (no undo setup)\n"));
                continue;
            }

            reqinfo->mode = mode_map_ptr[i];
            mode_flag = BabySteps_mode2flag( mode_map_ptr[i] );
            if((mode_flag & bs_modes->registered))
                bs_modes->completed |= mode_flag;
            else {
                DEBUG_MSGTL(("babySteps",
                            "   skipping mode (not registered)\n"));
                continue;
            }


        }
        else {
            reqinfo->mode = save_mode;
        }

        /*
         * call handlers for baby step
         */
        rc = AgentHandler_callNextHandler(handler, reginfo, reqinfo,
                                       requests);

        /*
         * check for error calling handler (unlikely, but...)
         */
        if(rc) {
            DEBUG_MSGTL(("babySteps", "   ERROR:handler error\n"));
            break;
        }

        /*
         * check for errors in any of the requests for GET-like, reserve1,
         * reserve2 and action. (there is no recovery from errors
         * in commit, free or undo.)
         */
        if (MODE_IS_GET(save_mode)
            || (save_mode < PRIOT_MSG_INTERNAL_SET_COMMIT)
            ) {
            rc = Agent_checkRequestsError(requests);
            if(rc) {
                DEBUG_MSGTL(("babySteps", "   ERROR:request error\n"));
                break;
            }
        }
    }

    /*
     * restore original mode
     */
    reqinfo->mode = save_mode;


    return rc;
}

/** initializes the baby_steps helper which then registers a baby_steps
 *  handler as a run-time injectable handler for configuration file
 *  use.
 */
void
BabySteps_handlerInit(void)
{
    AgentHandler_registerHandlerByName("babySteps",
                                     BabySteps_handlerGet(BabyStepsMode_ALL));
}


/** @defgroup access_multiplexer baby_steps_access_multiplexer: calls individual access methods based on baby_step mode.
 *  @ingroup baby_steps
 *  @{
 */

/** returns a baby_steps handler that can be injected into a given
 *  handler chain.
 */
MibHandler *
BabySteps_accessMultiplexerGet(BabyStepsAccessMethods *am)
{
    MibHandler *mh;

    mh = AgentHandler_createHandler("babyStepsMux",
                                _BabySteps_accessMultiplexer);
    if(!mh)
        return NULL;

    mh->myvoid = am;
    mh->flags |= MIB_HANDLER_AUTO_NEXT;

    return mh;
}

/** @internal Implements the baby_steps handler */
static int
_BabySteps_accessMultiplexer(MibHandler *handler,
                               HandlerRegistration *reginfo,
                               AgentRequestInfo *reqinfo,
                               RequestInfo *requests)
{
    void *temp_void;
    NodeHandlerFT *method = NULL;
    BabyStepsAccessMethods *access_methods;
    int rc = PRIOT_ERR_NOERROR;

    /** call handlers should enforce these */
    Assert_assert((handler!=NULL) && (reginfo!=NULL) && (reqinfo!=NULL) &&
                   (requests!=NULL));

    DEBUG_MSGT(("babyStepsMux", "mode %s\n",
               MapList_findLabel("babystepMode",reqinfo->mode)));

    access_methods = (BabyStepsAccessMethods *)handler->myvoid;
    if(!access_methods) {
        Logger_log(LOGGER_PRIORITY_ERR,"baby_steps_access_multiplexer has no methods\n");
        return ErrorCode_GENERR;
    }

    switch(reqinfo->mode) {

    case MODE_BSTEP_PRE_REQUEST:
        if( access_methods->pre_request )
            method = access_methods->pre_request;
        break;

    case MODE_BSTEP_OBJECT_LOOKUP:
        if( access_methods->object_lookup )
            method = access_methods->object_lookup;
        break;

    case PRIOT_MSG_GET:
    case PRIOT_MSG_GETNEXT:
        if( access_methods->get_values )
            method = access_methods->get_values;
        break;

    case MODE_BSTEP_CHECK_VALUE:
        if( access_methods->object_syntax_checks )
            method = access_methods->object_syntax_checks;
        break;

    case MODE_BSTEP_ROW_CREATE:
        if( access_methods->row_creation )
            method = access_methods->row_creation;
        break;

    case MODE_BSTEP_UNDO_SETUP:
        if( access_methods->undo_setup )
            method = access_methods->undo_setup;
        break;

    case MODE_BSTEP_SET_VALUE:
        if( access_methods->set_values )
            method = access_methods->set_values;
        break;

    case MODE_BSTEP_CHECK_CONSISTENCY:
        if( access_methods->consistency_checks )
            method = access_methods->consistency_checks;
        break;

    case MODE_BSTEP_UNDO_SET:
        if( access_methods->undo_sets )
            method = access_methods->undo_sets;
        break;

    case MODE_BSTEP_COMMIT:
        if( access_methods->commit )
            method = access_methods->commit;
        break;

    case MODE_BSTEP_UNDO_COMMIT:
        if( access_methods->undo_commit )
            method = access_methods->undo_commit;
        break;

    case MODE_BSTEP_IRREVERSIBLE_COMMIT:
        if( access_methods->irreversible_commit )
            method = access_methods->irreversible_commit;
        break;

    case MODE_BSTEP_UNDO_CLEANUP:
        if( access_methods->undo_cleanup )
            method = access_methods->undo_cleanup;
        break;

    case MODE_BSTEP_POST_REQUEST:
        if( access_methods->post_request )
            method = access_methods->post_request;
        break;

    default:
        Logger_log(LOGGER_PRIORITY_ERR,"unknown mode %d\n", reqinfo->mode);
        return PRIOT_ERR_GENERR;
    }

    /*
     * if method exists, set up handler void and call method.
     */
    if(NULL != method) {
        temp_void = handler->myvoid;
        handler->myvoid = access_methods->my_access_void;
        rc = (*method)(handler, reginfo, reqinfo, requests);
        handler->myvoid = temp_void;
    }
    else {
        rc = PRIOT_ERR_GENERR;
        Logger_log(LOGGER_PRIORITY_ERR,"baby steps multiplexer handler called for a mode "
                 "with no handler\n");
        Assert_assert(NULL != method);
    }

    /*
     * don't call any lower handlers, it will be done for us
     * since we set MIB_HANDLER_AUTO_NEXT
     */

    return rc;
}

/*
 * give a baby step mode, return the flag for that mode
 */
int
BabySteps_mode2flag( u_int mode )
{
    switch( mode ) {
        case MODE_BSTEP_OBJECT_LOOKUP:
            return BabyStepsMode_OBJECT_LOOKUP;
        case MODE_BSTEP_SET_VALUE:
            return BabyStepsMode_SET_VALUE;
        case MODE_BSTEP_IRREVERSIBLE_COMMIT:
            return BabyStepsMode_IRREVERSIBLE_COMMIT;
        case MODE_BSTEP_CHECK_VALUE:
            return BabyStepsMode_CHECK_VALUE;
        case MODE_BSTEP_PRE_REQUEST:
            return BabyStepsMode_PRE_REQUEST;
        case MODE_BSTEP_POST_REQUEST:
            return BabyStepsMode_POST_REQUEST;
        case MODE_BSTEP_UNDO_SETUP:
            return BabyStepsMode_UNDO_SETUP;
        case MODE_BSTEP_UNDO_CLEANUP:
            return BabyStepsMode_UNDO_CLEANUP;
        case MODE_BSTEP_UNDO_SET:
            return BabyStepsMode_UNDO_SET;
        case MODE_BSTEP_ROW_CREATE:
            return BabyStepsMode_ROW_CREATE;
        case MODE_BSTEP_CHECK_CONSISTENCY:
            return BabyStepsMode_CHECK_CONSISTENCY;
        case MODE_BSTEP_COMMIT:
            return BabyStepsMode_COMMIT;
        case MODE_BSTEP_UNDO_COMMIT:
            return BabyStepsMode_UNDO_COMMIT;
        default:
            Assert_assert("unknown flag");
            break;
    }
    return 0;
}

