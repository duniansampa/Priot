/*
 * ipstats data access header
 *
 * $Id$
 */
#ifndef NETSNMP_ACCESS_IPSTATS_H
#define NETSNMP_ACCESS_IPSTATS_H

#include "System/Numerics/Integer64.h"

#define IPSYSTEMSTATSTABLE_HCINRECEIVES       1
#define IPSYSTEMSTATSTABLE_HCINOCTETS         2
#define IPSYSTEMSTATSTABLE_INHDRERRORS        3
#define IPSYSTEMSTATSTABLE_HCINNOROUTES       4 
#define IPSYSTEMSTATSTABLE_INADDRERRORS       5
#define IPSYSTEMSTATSTABLE_INUNKNOWNPROTOS    6
#define IPSYSTEMSTATSTABLE_INTRUNCATEDPKTS    7
#define IPSYSTEMSTATSTABLE_HCINFORWDATAGRAMS  8 
#define IPSYSTEMSTATSTABLE_REASMREQDS         9
#define IPSYSTEMSTATSTABLE_REASMOKS           10
#define IPSYSTEMSTATSTABLE_REASMFAILS         11
#define IPSYSTEMSTATSTABLE_INDISCARDS         12
#define IPSYSTEMSTATSTABLE_HCINDELIVERS       13
#define IPSYSTEMSTATSTABLE_HCOUTREQUESTS      14
#define IPSYSTEMSTATSTABLE_HCOUTNOROUTES      15
#define IPSYSTEMSTATSTABLE_HCOUTFORWDATAGRAMS 16
#define IPSYSTEMSTATSTABLE_HCOUTDISCARDS      17
#define IPSYSTEMSTATSTABLE_HCOUTFRAGREQDS     18
#define IPSYSTEMSTATSTABLE_HCOUTFRAGOKS       19
#define IPSYSTEMSTATSTABLE_HCOUTFRAGFAILS     20
#define IPSYSTEMSTATSTABLE_HCOUTFRAGCREATES   21
#define IPSYSTEMSTATSTABLE_HCOUTTRANSMITS     22
#define IPSYSTEMSTATSTABLE_HCOUTOCTETS        23
#define IPSYSTEMSTATSTABLE_HCINMCASTPKTS      24
#define IPSYSTEMSTATSTABLE_HCINMCASTOCTETS    25
#define IPSYSTEMSTATSTABLE_HCOUTMCASTPKTS     26
#define IPSYSTEMSTATSTABLE_HCOUTMCASTOCTETS   27
#define IPSYSTEMSTATSTABLE_HCINBCASTPKTS      28
#define IPSYSTEMSTATSTABLE_HCOUTBCASTPKTS     29
#define IPSYSTEMSTATSTABLE_DISCONTINUITYTIME  30
#define IPSYSTEMSTATSTABLE_REFRESHRATE        31
    
#define IPSYSTEMSTATSTABLE_LAST IPSYSTEMSTATSTABLE_REFRESHRATE
    
/**---------------------------------------------------------------------*/
/*
 * structure definitions
 */

/*
 * netsnmp_ipstats_entry
 */
typedef struct netsnmp_ipstats_s {

   /* Columns of ipStatsTable. Some of them are HC for computation of the 
    * other columns, when underlying OS does not provide them.
    * Always fill at least 32 bits, the table is periodically polled -> 32 bit
    * overflow shall be detected and 64 bit value should be computed automatically. */
   Integer64             HCInReceives;
   Integer64             HCInOctets;
   u_long          InHdrErrors;
   Integer64             HCInNoRoutes;
   u_long          InAddrErrors;
   u_long          InUnknownProtos;
   u_long          InTruncatedPkts;
   
   /* optional, can be computed from HCInNoRoutes and HCOutForwDatagrams */
   Integer64             HCInForwDatagrams;
   
   u_long          ReasmReqds;
   u_long          ReasmOKs;
   u_long          ReasmFails;
   u_long          InDiscards;
   Integer64             HCInDelivers;
   Integer64             HCOutRequests;
   Integer64             HCOutNoRoutes;
   Integer64             HCOutForwDatagrams;
   Integer64             HCOutDiscards;
   
   /* optional, can be computed from HCOutFragOKs + HCOutFragFails*/
   Integer64             HCOutFragReqds;
   Integer64             HCOutFragOKs;
   Integer64             HCOutFragFails;
   Integer64             HCOutFragCreates;
   
   /* optional, can be computed from 
    * HCOutRequests +HCOutForwDatagrams + HCOutFragCreates
    * - HCOutFragReqds - HCOutNoRoutes  - HCOutDiscards */
   Integer64             HCOutTransmits;
   
   Integer64             HCOutOctets;
   Integer64             HCInMcastPkts;
   Integer64             HCInMcastOctets;
   Integer64             HCOutMcastPkts;
   Integer64             HCOutMcastOctets;
   Integer64             HCInBcastPkts;
   Integer64             HCOutBcastPkts;

   /* Array of available columns.*/
   int             columnAvail[IPSYSTEMSTATSTABLE_LAST+1];
} netsnmp_ipstats;


#endif /* NETSNMP_ACCESS_IPSTATS_H */
