/*
 * SNMPv3 View-based Access Control Model
 */

#ifndef _MIBGROUP_VACM_H
#define _MIBGROUP_VACM_H

#include "Vars.h"
#include "Vacm.h"

     void            init_vacm_vars(void);

     extern FindVarMethodFT var_vacm_sec2group;
     extern FindVarMethodFT var_vacm_access;
     extern FindVarMethodFT var_vacm_view;

     WriteMethodFT     write_vacmGroupName;
     WriteMethodFT     write_vacmSecurityToGroupStatus;
     WriteMethodFT     write_vacmSecurityToGroupStorageType;

     WriteMethodFT     write_vacmAccessContextMatch;
     WriteMethodFT     write_vacmAccessNotifyViewName;
     WriteMethodFT     write_vacmAccessReadViewName;
     WriteMethodFT     write_vacmAccessWriteViewName;
     WriteMethodFT     write_vacmAccessStatus;
     WriteMethodFT     write_vacmAccessStorageType;

     WriteMethodFT     write_vacmViewSpinLock;
     WriteMethodFT     write_vacmViewMask;
     WriteMethodFT     write_vacmViewStatus;
     WriteMethodFT     write_vacmViewStorageType;
     WriteMethodFT     write_vacmViewType;


     struct Vacm_AccessEntry_s *access_parse_accessEntry(oid * name,
                                                       size_t name_len);
     int             access_parse_oid(oid * oidIndex, size_t oidLen,
                                      unsigned char **groupName,
                                      size_t * groupNameLen,
                                      unsigned char **contextPrefix,
                                      size_t * contextPrefixLen,
                                      int *model, int *level);


     int             sec2group_parse_oid(oid * oidIndex, size_t oidLen,
                                         int *model, unsigned char **name,
                                         size_t * nameLen);
     struct Vacm_GroupEntry_s *sec2group_parse_groupEntry(oid * name,
                                                        size_t name_len);

     int             view_parse_oid(oid * oidIndex, size_t oidLen,
                                    unsigned char **viewName,
                                    size_t * viewNameLen, oid ** subtree,
                                    size_t * subtreeLen);

     struct Vacm_ViewEntry_s *view_parse_viewEntry(oid * name,
                                                 size_t name_len);


#define OID_SNMPVACMMIB		PRIOT_OID_SNMPMODULES, 16
#define OID_VACMMIBOBJECTS	OID_SNMPVACMMIB, 1

#define OID_VACMCONTEXTTABLE	OID_VACMMIBOBJECTS, 1
#define OID_VACMCONTEXTENTRY	OID_VACMCONTEXTTABLE, 1

#define OID_VACMGROUPTABLE	OID_VACMMIBOBJECTS, 2
#define OID_VACMGROUPENTRY	OID_VACMGROUPTABLE, 1

#define OID_VACMACCESSTABLE	OID_VACMMIBOBJECTS, 4
#define OID_VACMACCESSENTRY	OID_VACMACCESSTABLE, 1

#define OID_VACMMIBVIEWS	OID_VACMMIBOBJECTS, 5
#define OID_VACMVIEWTABLE	OID_VACMMIBVIEWS, 2
#define OID_VACMVIEWENTRY	OID_VACMVIEWTABLE, 1
#define SEC2GROUP_MIB_LENGTH 11
#define ACCESS_MIB_LENGTH 11
#define VIEW_MIB_LENGTH 12
#define CM_EXACT 1
#define CM_PREFIX 2

#endif                          /* _MIBGROUP_VACM_H */
