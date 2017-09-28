#include "Auth.h"
#include "Asn01.h"
#include "Debug.h"
#include "Tools.h"
#include "Impl.h"

/*
 * Globals.
 */

/*******************************************************************-o-******
 * snmp_comstr_parse
 *
 * Parameters:
 *	*data		(I)   Message.
 *	*length		(I/O) Bytes left in message.
 *	*psid		(O)   Community string.
 *	*slen		(O)   Length of community string.
 *	*version	(O)   Message version.
 *
 * Returns:
 *	Pointer to the remainder of data.
 *
 *
 * Parse the header of a community string-based message such as that found
 * in SNMPv1 and SNMPv2c.
 */
u_char * Auth_comstrParse(u_char * data,
                  size_t * length,
                  u_char * psid, size_t * slen, long *version)
{
    u_char          type;
    long            ver;
    size_t          origlen = *slen;

    /*
     * Message is an ASN.1 SEQUENCE.
     */
    data = Asn01_parseSequence(data, length, &type,
                              (ASN01_SEQUENCE | ASN01_CONSTRUCTOR),
                              "auth message");
    if (data == NULL) {
        return NULL;
    }

    /*
     * First field is the version.
     */
    DEBUG_DUMPHEADER("recv", "SNMP version");
    data = Asn01_parseInt(data, length, &type, &ver, sizeof(ver));
    DEBUG_INDENTLESS();
    *version = ver;
    if (data == NULL) {
        IMPL_ERROR_MSG("bad parse of version");
        return NULL;
    }

    /*
     * second field is the community string for SNMPv1 & SNMPv2c
     */
    DEBUG_DUMPHEADER("recv", "community string");
    data = Asn01_parseString(data, length, &type, psid, slen);
    DEBUG_INDENTLESS();
    if (data == NULL) {
        IMPL_ERROR_MSG("bad parse of community");
        return NULL;
    }
    psid[TOOLS_MIN(*slen, origlen - 1)] = '\0';
    return (u_char *) data;

}                               /* end snmp_comstr_parse() */




/*******************************************************************-o-******
 * snmp_comstr_build
 *
 * Parameters:
 *	*data
 *	*length
 *	*psid
 *	*slen
 *	*version
 *	 messagelen
 *
 * Returns:
 *	Pointer into 'data' after built section.
 *
 *
 * Build the header of a community string-based message such as that found
 * in SNMPv1 and SNMPv2c.
 *
 * NOTE:	The length of the message will have to be inserted later,
 *		if not known.
 *
 * NOTE:	Version is an 'int'.  (CMU had it as a long, but was passing
 *		in a *int.  Grrr.)  Assign version to verfix and pass in
 *		that to asn_build_int instead which expects a long.  -- WH
 */
u_char * Auth_comstrBuild(u_char * data,
                  size_t * length,
                  u_char * psid,
                  size_t * slen, long *version, size_t messagelen)
{
    long            verfix = *version;
    u_char         *h1 = data;
    u_char         *h1e;
    size_t          hlength = *length;


    /*
     * Build the the message wrapper (note length will be inserted later).
     */
    data =
        Asn01_buildSequence(data, length,
                           (u_char) (ASN01_SEQUENCE | ASN01_CONSTRUCTOR), 0);
    if (data == NULL) {
        return NULL;
    }
    h1e = data;


    /*
     * Store the version field.
     */
    data = Asn01_buildInt(data, length,
                         (u_char) (ASN01_UNIVERSAL | ASN01_PRIMITIVE |
                                   ASN01_INTEGER), &verfix, sizeof(verfix));
    if (data == NULL) {
        return NULL;
    }


    /*
     * Store the community string.
     */
    data = Asn01_buildString(data, length,
                            (u_char) (ASN01_UNIVERSAL | ASN01_PRIMITIVE |
                                      ASN01_OCTET_STR), psid,
                            *(u_char *) slen);
    if (data == NULL) {
        return NULL;
    }


    /*
     * Insert length.
     */
    Asn01_buildSequence(h1, &hlength,
                       (u_char) (ASN01_SEQUENCE | ASN01_CONSTRUCTOR),
                       data - h1e + messagelen);


    return data;

}                               /* end snmp_comstr_build() */
