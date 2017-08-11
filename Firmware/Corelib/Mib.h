#ifndef MIB_H
#define MIB_H

/*
 * mib.h - Definitions for the variables as defined in the MIB
 */

#include "Generals.h"

#include "Types.h"

#define NETSNMP_MIB2_OID 1, 3, 6, 1, 2, 1

#ifndef NETSNMP_NO_LEGACY_DEFINITIONS
#define MIB NETSNMP_MIB2_OID
#endif

#define MIB_IFTYPE_OTHER		             1
#define MIB_IFTYPE_REGULAR1822		         2
#define MIB_IFTYPE_HDH1822		             3
#define MIB_IFTYPE_DDNX25		             4
#define MIB_IFTYPE_RFC877X25		         5
#define MIB_IFTYPE_ETHERNETCSMACD	         6
#define MIB_IFTYPE_ISO88023CSMACD	         7
#define MIB_IFTYPE_ISO88024TOKENBUS	         8
#define MIB_IFTYPE_ISO88025TOKENRING	     9
#define MIB_IFTYPE_ISO88026MAN		        10
#define MIB_IFTYPE_STARLAN		            11
#define MIB_IFTYPE_PROTEON10MBIT	        12
#define MIB_IFTYPE_PROTEON80MBIT	        13
#define MIB_IFTYPE_HYPERCHANNEL		        14
#define MIB_IFTYPE_FDDI			            15
#define MIB_IFTYPE_LAPB			            16
#define MIB_IFTYPE_SDLC			            17
#define MIB_IFTYPE_T1CARRIER		        18
#define MIB_IFTYPE_CEPT			            19
#define MIB_IFTYPE_BASICISDN		        20
#define MIB_IFTYPE_PRIMARYISDN		        21
#define MIB_IFTYPE_PROPPOINTTOPOINTSERIAL   22

#define MIB_IFSTATUS_UP		    1
#define MIB_IFSTATUS_DOWN	    2
#define MIB_IFSTATUS_TESTING	3

#define MIB_FORWARD_GATEWAY	    1
#define MIB_FORWARD_HOST	    2

#define MIB_IPROUTETYPE_OTHER	1
#define MIB_IPROUTETYPE_INVALID	2
#define MIB_IPROUTETYPE_DIRECT	3
#define MIB_IPROUTETYPE_REMOTE	4

#define MIB_IPROUTEPROTO_OTHER	    1
#define MIB_IPROUTEPROTO_LOCAL	    2
#define MIB_IPROUTEPROTO_NETMGMT    3
#define MIB_IPROUTEPROTO_ICMP	    4
#define MIB_IPROUTEPROTO_EGP	    5
#define MIB_IPROUTEPROTO_GGP	    6
#define MIB_IPROUTEPROTO_HELLO	    7
#define MIB_IPROUTEPROTO_RIP	    8
#define MIB_IPROUTEPROTO_ISIS	    9
#define MIB_IPROUTEPROTO_ESIS	    10
#define MIB_IPROUTEPROTO_CISCOIGRP  11
#define MIB_IPROUTEPROTO_BBNSPFIGP  12
#define MIB_IPROUTEPROTO_OIGP	    13

#define MIB_TCPRTOALG_OTHER	    1
#define MIB_TCPRTOALG_CONSTANT	2
#define MIB_TCPRTOALG_RSRE	    3
#define MIB_TCPRTOALG_VANJ	    4

#define MIB_TCPCONNSTATE_CLOSED		    1
#define MIB_TCPCONNSTATE_LISTEN		    2
#define MIB_TCPCONNSTATE_SYNSENT	    3
#define MIB_TCPCONNSTATE_SYNRECEIVED	4
#define MIB_TCPCONNSTATE_ESTABLISHED	5
#define MIB_TCPCONNSTATE_FINWAIT1	6
#define MIB_TCPCONNSTATE_FINWAIT2	7
#define MIB_TCPCONNSTATE_CLOSEWAIT	8
#define MIB_TCPCONNSTATE_LASTACK	9
#define MIB_TCPCONNSTATE_CLOSING	10
#define MIB_TCPCONNSTATE_TIMEWAIT	11

#define MIB_EGPNEIGHSTATE_IDLE		    1
#define MIB_EGPNEIGHSTATE_AQUISITION	2
#define MIB_EGPNEIGHSTATE_DOWN		    3
#define MIB_EGPNEIGHSTATE_UP		    4
#define MIB_EGPNEIGHSTATE_CEASE		    5

#define MIB_STRING_OUTPUT_GUESS  1
#define MIB_STRING_OUTPUT_ASCII  2
#define MIB_STRING_OUTPUT_HEX    3

#define MIB_OID_OUTPUT_SUFFIX  1
#define MIB_OID_OUTPUT_MODULE  2
#define MIB_OID_OUTPUT_FULL    3
#define MIB_OID_OUTPUT_NUMERIC 4
#define MIB_OID_OUTPUT_UCD     5
#define MIB_OID_OUTPUT_NONE    6

struct variable_list;
struct Parse_EnumList_s;


void            Mib_shutdownMib(void);

void            Mib_initMib();

void            Mib_printAsciiDump(FILE *);
void            Mib_registerMibHandlers(void);
void            Mib_setMibDirectory(const char *dir);
char *          Mib_getMibDirectory(void);
void            Mib_fixupMibDirectory(void);
void            Mib_mibindexLoad( void );
char *          Mib_mibindexLookup( const char * );
FILE *          Mib_mibindexNew( const char * );
int             Mib_sprintReallocDescription(u_char ** buf, size_t * buf_len, size_t * out_len, int allow_realloc,
                                             _oid * objid, size_t objidlen, int width);

int             Mib_getWildNode(const char *, _oid *, size_t *);

int             Mib_getNode(const char *, _oid *, size_t *);

struct Parse_Tree_s *   Mib_getTree(const _oid *, size_t, struct Parse_Tree_s *);

struct Parse_Tree_s *   Mib_getTreeHead(void);

void            Mib_setFunction(struct Parse_Tree_s *);


int             Mib_parseOneOidIndex(_oid ** oidStart, size_t * oidLen,
                                    Types_VariableList * data,
                                    int complete);

int             Mib_parseOidIndexes(_oid * oidIndex, size_t oidLen,
                                  Types_VariableList * data);

int             Mib_buildOidNoalloc(_oid * in, size_t in_len,
                                  size_t * out_len, _oid * prefix,
                                  size_t prefix_len,
                                  Types_VariableList * indexes);

int             Mib_buildOid(_oid ** out, size_t * out_len, _oid * prefix,
                             size_t prefix_len,
                             Types_VariableList * indexes);

int             Mib_buildOidSegment(Types_VariableList * var);


int             Mib_sprintReallocVariable(u_char ** buf, size_t * buf_len,
                               size_t * out_len, int allow_realloc,
                               const _oid * objid, size_t objidlen,
                               const Types_VariableList * variable);


struct Parse_Tree_s *  Mib_sprintReallocObjidTree(u_char ** buf,
                                                  size_t * buf_len,
                                                  size_t * out_len,
                                                  int allow_realloc,
                                                  int *buf_overflow,
                                                  const _oid * objid,
                                                  size_t objidlen);

void            Mib_sprintReallocObjid(u_char ** buf,
                                             size_t * buf_len,
                                             size_t * out_len,
                                             int allow_realloc,
                                             int *buf_overflow,
                                             const _oid * objid,
                                             size_t objidlen);


int             Mib_sprintReallocValue(u_char ** buf, size_t * buf_len,
                             size_t * out_len, int allow_realloc,
                             const _oid * objid, size_t objidlen,
                             const Types_VariableList * variable);

int             Mib_sprintReallocObjid(u_char ** buf, size_t * buf_len,
                             size_t * out_len, int allow_realloc,
                             const _oid * objid, size_t objidlen);


int             Mib_sprintReallocByType(u_char ** buf, size_t * buf_len,
                                       size_t * out_len,
                                       int allow_realloc,
                                       const Types_VariableList * var,
                                       const struct Parse_EnumList_s *enums,
                                       const char *hint,
                                       const char *units);


int             Mib_sprintReallocHexString(u_char ** buf,
                                         size_t * buf_len,
                                         size_t * out_len,
                                         int allow_realloc,
                                         const u_char *, size_t);


int             Mib_sprintReallocAsciiString(u_char ** buf,
                                           size_t * buf_len,
                                           size_t * out_len,
                                           int allow_realloc,
                                           const u_char * cp,
                                           size_t len);

int             Mib_sprintReallocOctetString(u_char ** buf,
                                            size_t * buf_len,
                                            size_t * out_len,
                                            int allow_realloc,
                                            const Types_VariableList *,
                                            const struct Parse_EnumList_s *,
                                            const char *,
                                            const char *);

int             Mib_sprintReallocOpaque(u_char ** buf, size_t * buf_len,
                                      size_t * out_len,
                                      int allow_realloc,
                                      const Types_VariableList *,
                                      const struct Parse_EnumList_s *, const char *,
                                      const char *);

int             Mib_sprintReallocObjectIdentifier(u_char ** buf,
                                                 size_t * buf_len,
                                                 size_t * out_len,
                                                 int allow_realloc,
                                                 const Types_VariableList
                                                 *, const struct Parse_EnumList_s *,
                                                 const char *,
                                                 const char *);

int             Mib_sprintReallocTimeticks(u_char ** buf,
                                         size_t * buf_len,
                                         size_t * out_len,
                                         int allow_realloc,
                                         const Types_VariableList *,
                                         const struct Parse_EnumList_s *,
                                         const char *, const char *);

int             Mib_sprintReallocHintedInteger(u_char ** buf,
                                              size_t * buf_len,
                                              size_t * out_len,
                                              int allow_realloc, long,
                                              const char, const char *,
                                              const char *);

int             Mib_sprintReallocInteger(u_char ** buf, size_t * buf_len,
                                       size_t * out_len,
                                       int allow_realloc,
                                       const Types_VariableList *,
                                       const struct Parse_EnumList_s *,
                                       const char *, const char *);

int             Mib_sprintReallocUinteger(u_char ** buf,
                                        size_t * buf_len,
                                        size_t * out_len,
                                        int allow_realloc,
                                        const Types_VariableList *,
                                        const struct Parse_EnumList_s *,
                                        const char *, const char *);

int             Mib_sprintReallocQauge(u_char ** buf, size_t * buf_len,
                                     size_t * out_len,
                                     int allow_realloc,
                                     const Types_VariableList *,
                                     const struct Parse_EnumList_s *, const char *,
                                     const char *);

int             Mib_sprintReallocCounter(u_char ** buf, size_t * buf_len,
                                       size_t * out_len,
                                       int allow_realloc,
                                       const Types_VariableList *,
                                       const struct Parse_EnumList_s *,
                                       const char *, const char *);

int             Mib_sprintReallocNetworkAddress(u_char ** buf,
                                              size_t * buf_len,
                                              size_t * out_len,
                                              int allow_realloc,
                                              const Types_VariableList *,
                                              const struct Parse_EnumList_s *,
                                              const char *,
                                              const char *);

int             Mib_sprintReallocIpAddress(u_char ** buf,
                                         size_t * buf_len,
                                         size_t * out_len,
                                         int allow_realloc,
                                         const Types_VariableList *,
                                         const struct Parse_EnumList_s *,
                                         const char *, const char *);

int             Mib_sprintReallocNull(u_char ** buf, size_t * buf_len,
                                    size_t * out_len,
                                    int allow_realloc,
                                    const Types_VariableList *,
                                    const struct Parse_EnumList_s *, const char *,
                                    const char *);

int             Mib_sprintReallocBitString(u_char ** buf,
                                         size_t * buf_len,
                                         size_t * out_len,
                                         int allow_realloc,
                                         const Types_VariableList *,
                                         const struct Parse_EnumList_s *,
                                         const char *, const char *);

int             Mib_sprintReallocNsapAddress(u_char ** buf,
                                           size_t * buf_len,
                                           size_t * out_len,
                                           int allow_realloc,
                                           const Types_VariableList *,
                                           const struct Parse_EnumList_s *,
                                           const char *, const char *);

int             Mib_sprintReallocCounter64(u_char ** buf,
                                         size_t * buf_len,
                                         size_t * out_len,
                                         int allow_realloc,
                                         const Types_VariableList *,
                                         const struct Parse_EnumList_s *,
                                         const char *, const char *);

int             Mib_sprintReallocBadType(u_char ** buf, size_t * buf_len,
                                       size_t * out_len,
                                       int allow_realloc,
                                       const Types_VariableList *,
                                       const struct Parse_EnumList_s *,
                                       const char *, const char *);


int             Mib_snprintByType(char *buf, size_t buf_len,
                                Types_VariableList * var,
                                const struct Parse_EnumList_s *enums,
                                const char *hint, const char *units);

int             Mib_snprintHexString(char *buf, size_t buf_len,
                                  const u_char *, size_t);

int             Mib_snprintAsciiString(char *buf, size_t buf_len,
                                    const u_char * cp, size_t len);

int             Mib_snprintOctetString(char *buf, size_t buf_len,
                                     const Types_VariableList *,
                                     const struct Parse_EnumList_s *, const char *,
                                     const char *);

int             Mib_snprintOpaque(char *buf, size_t buf_len,
                               const Types_VariableList *,
                               const struct Parse_EnumList_s *, const char *,
                               const char *);

int             Mib_snprintObjectIdentifier(char *buf, size_t buf_len,
                                          const Types_VariableList *,
                                          const struct Parse_EnumList_s *,
                                          const char *, const char *);

int             Mib_snprintTimeticks(char *buf, size_t buf_len,
                                  const Types_VariableList *,
                                  const struct Parse_EnumList_s *, const char *,
                                  const char *);

int             Mib_snprintHintedInteger(char *buf, size_t buf_len,
                                       long, const char *,
                                       const char *);

int             Mib_snprintInteger(char *buf, size_t buf_len,
                                const Types_VariableList *,
                                const struct Parse_EnumList_s *, const char *,
                                const char *);

int             Mib_snprintUinteger(char *buf, size_t buf_len,
                                 const Types_VariableList *,
                                 const struct Parse_EnumList_s *, const char *,
                                 const char *);

int             Mib_snprintGauge(char *buf, size_t buf_len,
                              const Types_VariableList *,
                              const struct Parse_EnumList_s *, const char *,
                              const char *);

int             Mib_snprintCounter(char *buf, size_t buf_len,
                                const Types_VariableList *,
                                const struct Parse_EnumList_s *, const char *,
                                const char *);

int             Mib_snprintNetworkaddress(char *buf, size_t buf_len,
                                       const Types_VariableList *,
                                       const struct Parse_EnumList_s *,
                                       const char *, const char *);

int             Mib_snprintIpAddress(char *buf, size_t buf_len,
                                  const Types_VariableList *,
                                  const struct Parse_EnumList_s *, const char *,
                                  const char *);

int             Mib_snprintNull(char *buf, size_t buf_len,
                             const Types_VariableList *,
                             const struct Parse_EnumList_s *, const char *,
                             const char *);

int             Mib_snprintBitString(char *buf, size_t buf_len,
                                  const Types_VariableList *,
                                  const struct Parse_EnumList_s *, const char *,
                                  const char *);

int             Mib_snprintNsapAddress(char *buf, size_t buf_len,
                                    const Types_VariableList *,
                                    const struct Parse_EnumList_s *, const char *,
                                    const char *);

int             Mib_snprintCounter64(char *buf, size_t buf_len,
                                  const Types_VariableList *,
                                  const struct Parse_EnumList_s *, const char *,
                                  const char *);

int             Mib_snprintBadType(char *buf, size_t buf_len,
                                const Types_VariableList *,
                                const struct Parse_EnumList_s *, const char *,
                                const char *);

void            Mib_printOidReport(FILE *);

void            Mib_printOidReportEnableLabeledOid(void);

void            Mib_printOidReportEnableOid(void);

void            Mib_printOidReportEnableSuffix(void);

void            Mib_printOidReportEnableSymbolic(void);

void            Mib_printOidReportEnableMibChildOid(void);

const char *    Mib_parseOctetHint(const char *hint, const char *value,
                                     unsigned char **new_val, int *new_val_len);


void            Mib_clearTreeFlags(register struct Parse_Tree_s *tp);


char *          Mib_outToggleOptions(char *);

void            Mib_outToggleOptionsUsage(const char *, FILE *);

char *          Mib_inToggleOptions(char *);
char *          Mib_inOptions(char *, int, char * const *);

void            Mib_inToggleOptionsUsage(const char *, FILE *);

u_char          Mib_toAsnType(int mib_type);


int             Mib_str2oid(const char *S, _oid * O, int L);

void            Mib_mibIndexLoad( void );

void            Mib_fprintObjid(FILE * f, const _oid * objid, size_t objidlen);

void            Mib_fprintVariable(FILE * f, const _oid * objid,
                                   size_t objidlen, const Types_VariableList * variable);

void            Mib_fprintValue(FILE * f, const _oid * objid,
                     size_t objidlen, const Types_VariableList * variable);

void            Mib_fprintDescription(FILE * f, _oid * objid, size_t objidlen, int width);

char *          Mib_uptimeStringN(u_long timeticks, char *buf, size_t buflen);

char *          Mib_mibIndexLookup( const char *dirname );

FILE *          Mib_mibIndexNew( const char *dirname );

int             Mib_readObjid(const char *input, _oid * output, size_t * out_len);

_oid *          Mib_parseOid(const char *argv, _oid * root, size_t * rootlen);

#endif // MIB_H
