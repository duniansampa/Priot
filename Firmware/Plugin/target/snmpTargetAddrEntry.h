
/*
 * This file was generated by mib2c and is intended for use as a mib module
 * for the ucd-snmp snmpd agent.  Edited by Michael Baer
 * 
 * last changed 2/2/99.
 */

#ifndef _MIBGROUP_SNMPTARGETADDRENTRY_H
#define _MIBGROUP_SNMPTARGETADDRENTRY_H

#include "Vars.h"

/*
 * we use header_generic from the util_funcs module
 */



    /*
     * add the SNMPv2-TM mib into the default list of mibs to load, since
     * it contains the Domain definitions (EG, netsnmpUDPDomain) 
     */

    /*
     * Magic number definitions: 
     */
#define   SNMPTARGETADDRTDOMAIN      1
#define   SNMPTARGETADDRTADDRESS     2
#define   SNMPTARGETADDRTIMEOUT      3
#define   SNMPTARGETADDRRETRYCOUNT   4
#define   SNMPTARGETADDRTAGLIST      5
#define   SNMPTARGETADDRPARAMS       6
#define   SNMPTARGETADDRSTORAGETYPE  7
#define   SNMPTARGETADDRROWSTATUS    8
#define	  SNMPTARGETSPINLOCK	     99
#define   SNMPTARGETADDRTDOMAINCOLUMN      2
#define   SNMPTARGETADDRTADDRESSCOLUMN     3
#define   SNMPTARGETADDRTIMEOUTCOLUMN      4
#define   SNMPTARGETADDRRETRYCOUNTCOLUMN   5
#define   SNMPTARGETADDRTAGLISTCOLUMN      6
#define   SNMPTARGETADDRPARAMSCOLUMN       7
#define   SNMPTARGETADDRSTORAGETYPECOLUMN  8
#define   SNMPTARGETADDRROWSTATUSCOLUMN    9
    /*
     * structure definitions 
     */
     struct targetAddrTable_struct {
         char           *nameData;
         unsigned char   nameLen;
         oid             tDomain[asnMAX_OID_LEN];
         int             tDomainLen;
         unsigned char  *tAddress;
         size_t          tAddressLen;
         int             timeout;	/* Timeout in centiseconds */
         int             retryCount;
         char           *tagList;
         char           *params;
         int             storageType;
         int             rowStatus;
         struct targetAddrTable_struct *next;
         Types_Session *sess; /* a snmp session to the target host */
         time_t          sessionCreationTime;
     };

/*
 * function definitions 
 */

     void            init_snmpTargetAddrEntry(void);
     void            shutdown_snmpTargetAddrEntry(void);
     FindVarMethodFT   var_snmpTargetAddrEntry;

     struct targetAddrTable_struct *get_addrTable(void);
     struct targetAddrTable_struct *get_addrForName2(const char *name,
                                                     unsigned char nameLen);
     struct targetAddrTable_struct *snmpTargetAddrTable_create(void);
     void            snmpTargetAddrTable_add(struct targetAddrTable_struct
                                             *newEntry);

     void            snmpd_parse_config_targetAddr(const char *, char *);

     WriteMethodFT     write_snmpTargetAddrTDomain;
     WriteMethodFT     write_snmpTargetAddrTAddress;
     WriteMethodFT     write_snmpTargetAddrTimeout;
     WriteMethodFT     write_snmpTargetAddrRetryCount;
     WriteMethodFT     write_snmpTargetAddrTagList;
     WriteMethodFT     write_snmpTargetAddrParams;
     WriteMethodFT     write_snmpTargetAddrStorageType;
     WriteMethodFT     write_snmpTargetAddrRowStatus;

     WriteMethodFT     write_targetSpinLock;
     FindVarMethodFT   var_targetSpinLock;

#endif                          /* _MIBGROUP_SNMPTARGETADDRENTRY_H */
