#ifndef _MIBGROUP_USMUSER_H
#define _MIBGROUP_USMUSER_H

#include "Vars.h"
#include "System/Security/Usm.h"

/*
 * <...prefix>.<engineID_length>.<engineID>.<user_name_length>.<user_name>
 * = 1 + 32 + 1 + 32 
 */
#define USM_LENGTH_OID_MAX	66

/*
 * we use header_generic from the util_funcs module
 */

    /*
     * Magic number definitions: 
     */
#define   USMUSERSPINLOCK       1
#define   USMUSERSECURITYNAME   2
#define   USMUSERCLONEFROM      3
#define   USMUSERAUTHPROTOCOL   4
#define   USMUSERAUTHKEYCHANGE  5
#define   USMUSEROWNAUTHKEYCHANGE  6
#define   USMUSERPRIVPROTOCOL   7
#define   USMUSERPRIVKEYCHANGE  8
#define   USMUSEROWNPRIVKEYCHANGE  9
#define   USMUSERPUBLIC         10
#define   USMUSERSTORAGETYPE    11
#define   USMUSERSTATUS         12
    /*
     * function definitions 
     */
     extern void     init_usmUser(void);
     extern FindVarMethodFT var_usmUser;
     void init_register_usmUser_context(const char *contextName);

     oid            *usm_generate_OID(oid * prefix, size_t prefixLen,
                                      struct Usm_User_s *uptr,
                                      size_t * length);
     int             usm_parse_oid(oid * oidIndex, size_t oidLen,
                                   unsigned char **engineID,
                                   size_t * engineIDLen,
                                   unsigned char **name, size_t * nameLen);

     WriteMethodFT     write_usmUserSpinLock;
     WriteMethodFT     write_usmUserCloneFrom;
     WriteMethodFT     write_usmUserAuthProtocol;
     WriteMethodFT     write_usmUserAuthKeyChange;
     WriteMethodFT     write_usmUserPrivProtocol;
     WriteMethodFT     write_usmUserPrivKeyChange;
     WriteMethodFT     write_usmUserPublic;
     WriteMethodFT     write_usmUserStorageType;
     WriteMethodFT     write_usmUserStatus;

#endif                          /* _MIBGROUP_USMUSER_H */
