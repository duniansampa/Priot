#ifndef AGENTCALLBACKS_H
#define AGENTCALLBACKS_H


enum PriotdCallback_s {

   PriotdCallback_ACM_CHECK              ,
   PriotdCallback_REGISTER_OID           ,
   PriotdCallback_UNREGISTER_OID         ,
   PriotdCallback_REG_SYSOR              ,
   PriotdCallback_UNREG_SYSOR            ,
   PriotdCallback_ACM_CHECK_INITIAL      ,
   PriotdCallback_SEND_TRAP1             ,
   PriotdCallback_SEND_TRAP2             ,
   PriotdCallback_REGISTER_NOTIFICATIONS ,
   PriotdCallback_PRE_UPDATE_CONFIG      ,
   PriotdCallback_INDEX_START            ,
   PriotdCallback_INDEX_STOP             ,
   PriotdCallback_ACM_CHECK_SUBTREE      ,
   PriotdCallback_REQ_REG_SYSOR          ,
   PriotdCallback_REQ_UNREG_SYSOR        ,
   PriotdCallback_REQ_UNREG_SYSOR_SESS
};

#endif // AGENTCALLBACKS_H
