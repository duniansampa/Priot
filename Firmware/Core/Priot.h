#ifndef PRIOT_H
#define PRIOT_H

/*
 * Definitions for the Simple Network Management Protocol (RFC 1067).
 *
 *
 */

#include "Asn01.h"

#define PRIOT_PORT	        161 /* standard UDP port for PRIOT agents
                             * to receive requests messages */

#define PRIOT_TRAP_PORT	    162 /* standard UDP port for PRIOT
                             * managers to receive notificaion
                             * (trap and inform) messages */

#define PRIOT_MAX_LEN	    1500        /* typical maximum message size */
#define PRIOT_MIN_MAX_LEN    484 /* minimum maximum message size */

/*
 * versions based on version field
 */
#define PRIOT_VERSION_3     3

/*
 * versions not based on a version field
 */
//#define PRIOT_VERSION_sec   128  /* not (will never be) supported by this code */
//#define PRIOT_VERSION_2p	129  /* no longer supported by this code (> 4.0) */
//#define PRIOT_VERSION_2star 130  /* not (will never be) supported by this code */


/*
 * versions based on version field
 */

//#define PRIOT_VERSION_1	    0
//#define PRIOT_VERSION_2c    1
//#define PRIOT_VERSION_2u    2    /* not (will never be) supported by this code */
#define PRIOT_VERSION_3     3


/*
 * PDU types in SNMPv1, SNMPsec, SNMPv2p, SNMPv2c, SNMPv2u, SNMPv2*, and SNMPv3
 */
#define PRIOT_MSG_GET        (ASN01_CONTEXT | ASN01_CONSTRUCTOR | 0x0) /* a0=160 */
#define PRIOT_MSG_GETNEXT    (ASN01_CONTEXT | ASN01_CONSTRUCTOR | 0x1) /* a1=161 */
#define PRIOT_MSG_RESPONSE   (ASN01_CONTEXT | ASN01_CONSTRUCTOR | 0x2) /* a2=162 */
#define PRIOT_MSG_SET        (ASN01_CONTEXT | ASN01_CONSTRUCTOR | 0x3) /* a3=163 */


/*
 * PDU types in SNMPv1 and SNMPsec
 */
#define PRIOT_MSG_TRAP       (ASN01_CONTEXT | ASN01_CONSTRUCTOR | 0x4) /* a4=164 */

/*
 * PDU types in SNMPv3
 */
#define PRIOT_MSG_GETBULK    (ASN01_CONTEXT | ASN01_CONSTRUCTOR | 0x5) /* a5=165 */
#define PRIOT_MSG_INFORM     (ASN01_CONTEXT | ASN01_CONSTRUCTOR | 0x6) /* a6=166 */
#define PRIOT_MSG_TRAP2      (ASN01_CONTEXT | ASN01_CONSTRUCTOR | 0x7) /* a7=167 */

/*
 * PDU types in SNMPv3
 */
#define PRIOT_MSG_REPORT     (ASN01_CONTEXT | ASN01_CONSTRUCTOR | 0x8) /* a8=168 */


/*
 * internal modes that should never be used by the protocol for the
 * pdu type.
 *
 * All modes < 128 are reserved for SET requests.
 */
#define PRIOT_MSG_INTERNAL_SET_BEGIN        -1
#define PRIOT_MSG_INTERNAL_SET_RESERVE1     0    /* these should match snmp.h */
#define PRIOT_MSG_INTERNAL_SET_RESERVE2     1
#define PRIOT_MSG_INTERNAL_SET_ACTION       2
#define PRIOT_MSG_INTERNAL_SET_COMMIT       3
#define PRIOT_MSG_INTERNAL_SET_FREE         4
#define PRIOT_MSG_INTERNAL_SET_UNDO         5
#define PRIOT_MSG_INTERNAL_SET_MAX          6

#define PRIOT_MSG_INTERNAL_CHECK_VALUE           17
#define PRIOT_MSG_INTERNAL_ROW_CREATE            18
#define PRIOT_MSG_INTERNAL_UNDO_SETUP            19
#define PRIOT_MSG_INTERNAL_SET_VALUE             20
#define PRIOT_MSG_INTERNAL_CHECK_CONSISTENCY     21
#define PRIOT_MSG_INTERNAL_UNDO_SET              22
#define PRIOT_MSG_INTERNAL_COMMIT                23
#define PRIOT_MSG_INTERNAL_UNDO_COMMIT           24
#define PRIOT_MSG_INTERNAL_IRREVERSIBLE_COMMIT   25
#define PRIOT_MSG_INTERNAL_UNDO_CLEANUP          26



/*
 * modes > 128 for non sets.
 * Note that 160-168 overlap with PRIOT ASN1 pdu types
 */
#define PRIOT_MSG_INTERNAL_PRE_REQUEST           128
#define PRIOT_MSG_INTERNAL_OBJECT_LOOKUP         129
#define PRIOT_MSG_INTERNAL_POST_REQUEST          130
#define PRIOT_MSG_INTERNAL_GET_STASH             131



#define PRIOT_CMD_CONFIRMED(c) (c == PRIOT_MSG_INFORM || c == PRIOT_MSG_GETBULK ||\
                               c == PRIOT_MSG_GETNEXT || c == PRIOT_MSG_GET || \
                               c == PRIOT_MSG_SET )


/*
 * Exception values for SNMPv3
 */
#define PRIOT_NOSUCHOBJECT    (ASN01_CONTEXT | ASN01_PRIMITIVE | 0x0) /* 80=128 */
#define PRIOT_NOSUCHINSTANCE  (ASN01_CONTEXT | ASN01_PRIMITIVE | 0x1) /* 81=129 */
#define PRIOT_ENDOFMIBVIEW    (ASN01_CONTEXT | ASN01_PRIMITIVE | 0x2) /* 82=130 */

/*
 * Error codes (the value of the field error-status in PDUs)
 */

/*
 * in SNMPv3 PDUs
 */
#define PRIOT_ERR_NOERROR                (0)     /* XXX  Used only for PDUs? */
#define PRIOT_ERR_TOOBIG	             (1)
#define PRIOT_ERR_NOSUCHNAME             (2)
#define PRIOT_ERR_BADVALUE               (3)
#define PRIOT_ERR_READONLY               (4)
#define PRIOT_ERR_GENERR	             (5)

/*
 * in  SNMPv3 PDUs
 */
#define PRIOT_ERR_NOACCESS		    (6)
#define PRIOT_ERR_WRONGTYPE		    (7)
#define PRIOT_ERR_WRONGLENGTH		(8)
#define PRIOT_ERR_WRONGENCODING		(9)
#define PRIOT_ERR_WRONGVALUE		(10)
#define PRIOT_ERR_NOCREATION		(11)
#define PRIOT_ERR_INCONSISTENTVALUE	(12)
#define PRIOT_ERR_RESOURCEUNAVAILABLE	(13)
#define PRIOT_ERR_COMMITFAILED		    (14)
#define PRIOT_ERR_UNDOFAILED		    (15)
#define PRIOT_ERR_AUTHORIZATIONERROR	(16)
#define PRIOT_ERR_NOTWRITABLE		    (17)

/*
 * in SNMPv3 PDUs
 */
#define PRIOT_ERR_INCONSISTENTNAME	(18)

#define PRIOT_MAX_PRIOT_ERR	18

#define PRIOT_VALIDATE_ERR(x)  ( (x > PRIOT_MAX_PRIOT_ERR) ? \
                                 PRIOT_ERR_GENERR : \
                                 (x < PRIOT_ERR_NOERROR) ? \
                                 PRIOT_ERR_GENERR : \
                                 x )

/*
 * values of the generic-trap field in trap PDUs
 */
#define PRIOT_TRAP_COLDSTART            (0)
#define PRIOT_TRAP_WARMSTART            (1)
#define PRIOT_TRAP_LINKDOWN             (2)
#define PRIOT_TRAP_LINKUP               (3)
#define PRIOT_TRAP_AUTHFAIL             (4)
#define PRIOT_TRAP_EGPNEIGHBORLOSS      (5)
#define PRIOT_TRAP_ENTERPRISESPECIFIC	(6)

/*
 * row status values
 */
#define PRIOT_ROW_NONEXISTENT		0
#define PRIOT_ROW_ACTIVE			1
#define PRIOT_ROW_NOTINSERVICE		2
#define PRIOT_ROW_NOTREADY          3
#define PRIOT_ROW_CREATEANDGO		4
#define PRIOT_ROW_CREATEANDWAIT		5
#define PRIOT_ROW_DESTROY           6

/*
 * row storage values
 */
#define PRIOT_STORAGE_NONE          0
#define PRIOT_STORAGE_OTHER         1
#define PRIOT_STORAGE_VOLATILE		2
#define PRIOT_STORAGE_NONVOLATILE	3
#define PRIOT_STORAGE_PERMANENT		4
#define PRIOT_STORAGE_READONLY		5

/*
 * message processing models
 */
//#define PRIOT_MP_MODEL_SNMPv1		0
//#define PRIOT_MP_MODEL_SNMPv2c		1
//#define PRIOT_MP_MODEL_SNMPv2u		2
#define PRIOT_MP_MODEL_SNMPv3		3
//#define PRIOT_MP_MODEL_SNMPv2p		256

/*
 * security values
 */
#define PRIOT_SEC_MODEL_ANY         0
#define PRIOT_SEC_MODEL_SNMPv1		1
#define PRIOT_SEC_MODEL_SNMPv2c		2
#define PRIOT_SEC_MODEL_USM         3
#define PRIOT_SEC_MODEL_TSM         4
#define PRIOT_SEC_MODEL_SNMPv2p		256

#define PRIOT_SEC_LEVEL_NOAUTH		1
#define PRIOT_SEC_LEVEL_AUTHNOPRIV	2
#define PRIOT_SEC_LEVEL_AUTHPRIV	3

#define PRIOT_MSG_FLAG_AUTH_BIT          0x01
#define PRIOT_MSG_FLAG_PRIV_BIT          0x02
#define PRIOT_MSG_FLAG_RPRT_BIT          0x04

/*
 * control PDU handling characteristics
 */
#define PRIOT_UCD_MSG_FLAG_RESPONSE_PDU            0x100
#define PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE         0x200
#define PRIOT_UCD_MSG_FLAG_FORCE_PDU_COPY          0x400
#define PRIOT_UCD_MSG_FLAG_ALWAYS_IN_VIEW          0x800
#define PRIOT_UCD_MSG_FLAG_PDU_TIMEOUT            0x1000
#define PRIOT_UCD_MSG_FLAG_ONE_PASS_ONLY          0x2000
#define PRIOT_UCD_MSG_FLAG_TUNNELED               0x4000

/*
 * view status
 */
#define PRIOT_VIEW_INCLUDED		1
#define PRIOT_VIEW_EXCLUDED		2

/*
 * basic oid values
 */
#define PRIOT_OID_INTERNET		    1, 3, 6, 1
#define PRIOT_OID_ENTERPRISES		PRIOT_OID_INTERNET, 4, 1
#define PRIOT_OID_MIB2			    PRIOT_OID_INTERNET, 2, 1
#define PRIOT_OID_SNMPV2			PRIOT_OID_INTERNET, 6
#define PRIOT_OID_SNMPMODULES		PRIOT_OID_SNMPV2, 3

/*
 * lengths as defined by TCs
 */
#define PRIOT_ADMINLENGTH 255

void    Priot_xdump(const void *, size_t, const char *);

u_char * Priot_parseVarOp(u_char *, oid *, size_t *, u_char *,
                                  size_t *, u_char **, size_t *);

u_char * Priot_buildVarOp(u_char *, oid *, size_t *, u_char,
                                  size_t, u_char *, size_t *);


int Priot_reallocRbuildVarOp(u_char ** pkt,
                               size_t * pkt_len,
                               size_t * offset,
                               int allow_realloc,
                               const oid * name,
                               size_t * name_len,
                               u_char value_type,
                               u_char * value,
                               size_t value_length);

#endif // PRIOT_H
