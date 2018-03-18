#include "AgentCallbacks.h"


const struct PriotdCallback_s PRIOTDCALLBACK = {

    .ACM_CHECK              = 0,
    .REGISTER_OID           = 1,
    .UNREGISTER_OID         = 2,
    .REG_SYSOR              = 3,
    .UNREG_SYSOR            = 4,
    .ACM_CHECK_INITIAL      = 5,
    .SEND_TRAP1             = 6,
    .SEND_TRAP2             = 7,
    .REGISTER_NOTIFICATIONS = 8,
    .PRE_UPDATE_CONFIG      = 9,
    .INDEX_START            = 10,
    .INDEX_STOP             = 11,
    .ACM_CHECK_SUBTREE      = 12,
    .REQ_REG_SYSOR          = 13,
    .REQ_UNREG_SYSOR        = 14,
    .REQ_UNREG_SYSOR_SESS   = 15

};
