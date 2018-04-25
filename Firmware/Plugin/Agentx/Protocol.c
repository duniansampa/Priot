#include "Protocol.h"
#include "Debug.h"
#include "Tools.h"
#include "Priot.h"
#include "Mib.h"
#include "Client.h"

/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright � 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */


const char  *
Protocol_cmd(u_char code)
{
    switch (code) {
    case AGENTX_MSG_OPEN:
        return "Open";
    case AGENTX_MSG_CLOSE:
        return "Close";
    case AGENTX_MSG_REGISTER:
        return "Register";
    case AGENTX_MSG_UNREGISTER:
        return "Unregister";
    case AGENTX_MSG_GET:
        return "Get";
    case AGENTX_MSG_GETNEXT:
        return "Get Next";
    case AGENTX_MSG_GETBULK:
        return "Get Bulk";
    case AGENTX_MSG_TESTSET:
        return "Test Set";
    case AGENTX_MSG_COMMITSET:
        return "Commit Set";
    case AGENTX_MSG_UNDOSET:
        return "Undo Set";
    case AGENTX_MSG_CLEANUPSET:
        return "Cleanup Set";
    case AGENTX_MSG_NOTIFY:
        return "Notify";
    case AGENTX_MSG_PING:
        return "Ping";
    case AGENTX_MSG_INDEX_ALLOCATE:
        return "Index Allocate";
    case AGENTX_MSG_INDEX_DEALLOCATE:
        return "Index Deallocate";
    case AGENTX_MSG_ADD_AGENT_CAPS:
        return "Add Agent Caps";
    case AGENTX_MSG_REMOVE_AGENT_CAPS:
        return "Remove Agent Caps";
    case AGENTX_MSG_RESPONSE:
        return "Response";
    default:
        return "Unknown";
    }
}

int
Protocol_reallocBuildInt(u_char ** buf, size_t * buf_len, size_t * out_len,
                         int allow_realloc,
                         unsigned int value, int network_order)
{
    unsigned int    ivalue = value;
    size_t          ilen = *out_len;

    while ((*out_len + 4) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
    }

    if (network_order) {
        value = ntohl(value);
        memmove((*buf + *out_len), &value, 4);
        *out_len += 4;
    } else {
        memmove((*buf + *out_len), &value, 4);
        *out_len += 4;
    }
    DEBUG_DUMPSETUP("send", (*buf + ilen), 4);
    DEBUG_MSG(("dumpv_send", "  Integer:\t%u (0x%.2X)\n", ivalue,
              ivalue));
    return 1;
}

void
Protocol_buildInt(u_char * bufp, u_int value, int network_byte_order)
{
    u_char         *orig_bufp = bufp;
    u_int           orig_val = value;

    if (network_byte_order) {
        value = ntohl(value);
        memmove(bufp, &value, 4);
    } else {
        memmove(bufp, &value, 4);

    }
    DEBUG_DUMPSETUP("send", orig_bufp, 4);
    DEBUG_MSG(("dumpv_send", "  Integer:\t%u (0x%.2X)\n", orig_val,
              orig_val));
}

int
Protocol_reallocBuildShort(u_char ** buf, size_t * buf_len,
                           size_t * out_len, int allow_realloc,
                           unsigned short value, int network_order)
{
    unsigned short  ivalue = value;
    size_t          ilen = *out_len;


    while ((*out_len + 2) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
    }

    if (network_order) {
        value = ntohs(value);
        memmove((*buf + *out_len), &value, 2);
        *out_len += 2;
    } else {
        memmove((*buf + *out_len), &value, 2);
        *out_len += 2;

    }
    DEBUG_DUMPSETUP("send", (*buf + ilen), 2);
    DEBUG_MSG(("dumpv_send", "  Short:\t%hu (0x%.2hX)\n", ivalue, ivalue));
    return 1;
}

int
Protocol_reallocBuildOid(u_char ** buf, size_t * buf_len, size_t * out_len,
                         int allow_realloc,
                         int inclusive, oid * name, size_t name_len,
                         int network_order)
{
    size_t          ilen = *out_len, i = 0;
    int             prefix = 0;

    DEBUG_PRINTINDENT("dumpv_send");
    DEBUG_MSG(("dumpv_send", "OID: "));
    DEBUG_MSGOID(("dumpv_send", name, name_len));
    DEBUG_MSG(("dumpv_send", "\n"));

    /*
     * Updated clarification from the AgentX mailing list.
     * The "null Object Identifier" mentioned in RFC 2471,
     * section 5.1 is a special placeholder value, and
     * should only be used when explicitly mentioned in
     * this RFC.  In particular, it does *not* mean {0, 0}
     */
    if (name_len == 0)
        inclusive = 0;

    /*
     * 'Compact' internet OIDs
     */
    if (name_len >= 5 && (name[0] == 1 && name[1] == 3 &&
                          name[2] == 6 && name[3] == 1 &&
                          name[4] > 0 && name[4] < 256)) {
        prefix = name[4];
        name += 5;
        name_len -= 5;
    }

    while ((*out_len + 4 + (4 * name_len)) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
    }

    *(*buf + *out_len) = (u_char) name_len;
    (*out_len)++;
    *(*buf + *out_len) = (u_char) prefix;
    (*out_len)++;
    *(*buf + *out_len) = (u_char) inclusive;
    (*out_len)++;
    *(*buf + *out_len) = (u_char) 0x00;
    (*out_len)++;

    DEBUG_DUMPHEADER("send", "OID Header");
    DEBUG_DUMPSETUP("send", (*buf + ilen), 4);
    DEBUG_MSG(("dumpv_send", "  # subids:\t%d (0x%.2X)\n", (int)name_len,
              (unsigned int)name_len));
    DEBUG_PRINTINDENT("dumpv_send");
    DEBUG_MSG(("dumpv_send", "  prefix:\t%d (0x%.2X)\n", prefix, prefix));
    DEBUG_PRINTINDENT("dumpv_send");
    DEBUG_MSG(("dumpv_send", "  inclusive:\t%d (0x%.2X)\n", inclusive,
              inclusive));
    DEBUG_INDENTLESS();
    DEBUG_DUMPHEADER("send", "OID Segments");

    for (i = 0; i < name_len; i++) {
        if (!Protocol_reallocBuildInt(buf, buf_len, out_len, allow_realloc,
                                      name[i], network_order)) {
            DEBUG_INDENTLESS();
            return 0;
        }
    }
    DEBUG_INDENTLESS();

    return 1;
}

int
Protocol_reallocBuildString(u_char ** buf, size_t * buf_len,
                            size_t * out_len, int allow_realloc,
                            u_char * string, size_t string_len,
                            int network_order)
{
    size_t          ilen = *out_len, i = 0;

    while ((*out_len + 4 + (4 * ((string_len + 3) / 4))) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
    }

    DEBUG_DUMPHEADER("send", "Build String");
    DEBUG_DUMPHEADER("send", "length");
    if (!Protocol_reallocBuildInt(buf, buf_len, out_len, allow_realloc,
                                  string_len, network_order)) {
        DEBUG_INDENTLESS();
        DEBUG_INDENTLESS();
        return 0;
    }

    if (string_len == 0) {
        DEBUG_MSG(("dumpv_send", "  String: <empty>\n"));
        DEBUG_INDENTLESS();
        DEBUG_INDENTLESS();
        return 1;
    }

    memmove((*buf + *out_len), string, string_len);
    *out_len += string_len;

    /*
     * Pad to a multiple of 4 bytes if necessary (per RFC 2741).
     */

    if (string_len % 4 != 0) {
        for (i = 0; i < 4 - (string_len % 4); i++) {
            *(*buf + *out_len) = 0;
            (*out_len)++;
        }
    }

    DEBUG_DUMPSETUP("send", (*buf + ilen + 4), ((string_len + 3) / 4) * 4);
    DEBUG_MSG(("dumpv_send", "  String:\t%s\n", string));
    DEBUG_INDENTLESS();
    DEBUG_INDENTLESS();
    return 1;
}

int
Protocol_reallocBuildDouble(u_char ** buf, size_t * buf_len,
                            size_t * out_len, int allow_realloc,
                            double double_val, int network_order)
{
    union {
        double          doubleVal;
        int             intVal[2];
        char            c[sizeof(double)];
    } du;
    int             tmp;
    u_char          opaque_buffer[3 + sizeof(double)];

    opaque_buffer[0] = ASN01_OPAQUE_TAG1;
    opaque_buffer[1] = ASN01_OPAQUE_DOUBLE;
    opaque_buffer[2] = sizeof(double);

    du.doubleVal = double_val;
    tmp = htonl(du.intVal[0]);
    du.intVal[0] = htonl(du.intVal[1]);
    du.intVal[1] = tmp;
    memcpy(&opaque_buffer[3], &du.c[0], sizeof(double));

    return Protocol_reallocBuildString(buf, buf_len, out_len,
                                       allow_realloc, opaque_buffer,
                                       3 + sizeof(double), network_order);
}

int
Protocol_reallocBuildFloat(u_char ** buf, size_t * buf_len,
                           size_t * out_len, int allow_realloc,
                           float float_val, int network_order)
{
    union {
        float           floatVal;
        int             intVal;
        char            c[sizeof(float)];
    } fu;
    u_char          opaque_buffer[3 + sizeof(float)];

    opaque_buffer[0] = ASN01_OPAQUE_TAG1;
    opaque_buffer[1] = ASN01_OPAQUE_FLOAT;
    opaque_buffer[2] = sizeof(float);

    fu.floatVal = float_val;
    fu.intVal = htonl(fu.intVal);
    memcpy(&opaque_buffer[3], &fu.c[0], sizeof(float));

    return Protocol_reallocBuildString(buf, buf_len, out_len,
                                       allow_realloc, opaque_buffer,
                                       3 + sizeof(float), network_order);
}

int
Protocol_reallocBuildVarbind(u_char ** buf, size_t * buf_len,
                             size_t * out_len, int allow_realloc,
                             Types_VariableList * vp, int network_order)
{
    DEBUG_DUMPHEADER("send", "VarBind");
    DEBUG_DUMPHEADER("send", "type");
    if ((vp->type == ASN01_OPAQUE_FLOAT) || (vp->type == ASN01_OPAQUE_DOUBLE)
        || (vp->type == ASN01_OPAQUE_I64) || (vp->type == ASN01_OPAQUE_U64)
        || (vp->type == ASN01_OPAQUE_COUNTER64)) {
        if (!Protocol_reallocBuildShort
            (buf, buf_len, out_len, allow_realloc,
             (unsigned short) ASN01_OPAQUE, network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
    } else
    if (vp->type == ASN01_PRIV_INCL_RANGE || vp->type == ASN01_PRIV_EXCL_RANGE) {
        if (!Protocol_reallocBuildShort
            (buf, buf_len, out_len, allow_realloc,
             (unsigned short) ASN01_OBJECT_ID, network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
    } else {
        if (!Protocol_reallocBuildShort
            (buf, buf_len, out_len, allow_realloc,
             (unsigned short) vp->type, network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
    }

    while ((*out_len + 2) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
    }

    *(*buf + *out_len) = 0;
    (*out_len)++;
    *(*buf + *out_len) = 0;
    (*out_len)++;
    DEBUG_INDENTLESS();

    DEBUG_DUMPHEADER("send", "name");
    if (!Protocol_reallocBuildOid(buf, buf_len, out_len, allow_realloc, 0,
                                  vp->name, vp->nameLength,
                                  network_order)) {
        DEBUG_INDENTLESS();
        DEBUG_INDENTLESS();
        return 0;
    }
    DEBUG_INDENTLESS();

    DEBUG_DUMPHEADER("send", "value");
    switch (vp->type) {

    case ASN01_INTEGER:
    case ASN01_COUNTER:
    case ASN01_GAUGE:
    case ASN01_TIMETICKS:
    case ASN01_UINTEGER:
        if (!Protocol_reallocBuildInt(buf, buf_len, out_len, allow_realloc,
                                      *(vp->val.integer), network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        break;

    case ASN01_OPAQUE_FLOAT:
        DEBUG_DUMPHEADER("send", "Build Opaque Float");
        DEBUG_PRINTINDENT("dumpv_send");
        DEBUG_MSG(("dumpv_send", "  Float:\t%f\n", *(vp->val.floatVal)));
        if (!Protocol_reallocBuildFloat
            (buf, buf_len, out_len, allow_realloc, *(vp->val.floatVal),
             network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_INDENTLESS();
        break;

    case ASN01_OPAQUE_DOUBLE:
        DEBUG_DUMPHEADER("send", "Build Opaque Double");
        DEBUG_PRINTINDENT("dumpv_send");
        DEBUG_MSG(("dumpv_send", "  Double:\t%f\n", *(vp->val.doubleVal)));
        if (!Protocol_reallocBuildDouble
            (buf, buf_len, out_len, allow_realloc, *(vp->val.doubleVal),
             network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_INDENTLESS();
        break;

    case ASN01_OPAQUE_I64:
    case ASN01_OPAQUE_U64:
    case ASN01_OPAQUE_COUNTER64:
        /*
         * XXX - TODO - encode as raw OPAQUE for now (so fall through
         * here).
         */

    case ASN01_OCTET_STR:
    case ASN01_IPADDRESS:
    case ASN01_OPAQUE:
        if (!Protocol_reallocBuildString
            (buf, buf_len, out_len, allow_realloc, vp->val.string,
             vp->valLen, network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        break;

    case ASN01_OBJECT_ID:
    case ASN01_PRIV_EXCL_RANGE:
    case ASN01_PRIV_INCL_RANGE:
        if (!Protocol_reallocBuildOid
            (buf, buf_len, out_len, allow_realloc, 1, vp->val.objid,
             vp->valLen / sizeof(oid), network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        break;

    case ASN01_COUNTER64:
        if (network_order) {
            DEBUG_DUMPHEADER("send", "Build Counter64 (high, low)");
            if (!Protocol_reallocBuildInt
                (buf, buf_len, out_len, allow_realloc,
                 vp->val.counter64->high, network_order)
                || !Protocol_reallocBuildInt(buf, buf_len, out_len,
                                             allow_realloc,
                                             vp->val.counter64->low,
                                             network_order)) {
                DEBUG_INDENTLESS();
                DEBUG_INDENTLESS();
                DEBUG_INDENTLESS();
                return 0;
            }
            DEBUG_INDENTLESS();
        } else {
            DEBUG_DUMPHEADER("send", "Build Counter64 (low, high)");
            if (!Protocol_reallocBuildInt
                (buf, buf_len, out_len, allow_realloc,
                 vp->val.counter64->low, network_order)
                || !Protocol_reallocBuildInt(buf, buf_len, out_len,
                                             allow_realloc,
                                             vp->val.counter64->high,
                                             network_order)) {
                DEBUG_INDENTLESS();
                DEBUG_INDENTLESS();
                DEBUG_INDENTLESS();
                return 0;
            }
            DEBUG_INDENTLESS();
        }
        break;

    case ASN01_NULL:
    case PRIOT_NOSUCHOBJECT:
    case PRIOT_NOSUCHINSTANCE:
    case PRIOT_ENDOFMIBVIEW:
        break;

    default:
        DEBUG_MSGTL(("agentx_build_varbind", "unknown type %d (0x%02x)\n",
                    vp->type, vp->type));
        DEBUG_INDENTLESS();
        DEBUG_INDENTLESS();
        return 0;
    }
    DEBUG_INDENTLESS();
    DEBUG_INDENTLESS();
    return 1;
}

int
Protocol_reallocBuildHeader(u_char ** buf, size_t * buf_len,
                            size_t * out_len, int allow_realloc,
                            Types_Pdu *pdu)
{
    size_t          ilen = *out_len;
    const int       network_order =
        pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER;

    while ((*out_len + 4) >= *buf_len) {
        if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
            return 0;
        }
    }

    /*
     * First 4 bytes are version, pdu type, flags, and a 0 reserved byte.
     */

    *(*buf + *out_len) = 1;
    (*out_len)++;
    *(*buf + *out_len) = pdu->command;
    (*out_len)++;
    *(*buf + *out_len) = (u_char) (pdu->flags & AGENTX_MSG_FLAGS_MASK);
    (*out_len)++;
    *(*buf + *out_len) = 0;
    (*out_len)++;

    DEBUG_DUMPHEADER("send", "AgentX Header");
    DEBUG_DUMPSETUP("send", (*buf + ilen), 4);
    DEBUG_MSG(("dumpv_send", "  Version:\t%d\n", (int) *(*buf + ilen)));
    DEBUG_PRINTINDENT("dumpv_send");
    DEBUG_MSG(("dumpv_send", "  Command:\t%d (%s)\n", pdu->command,
              Protocol_cmd((u_char)pdu->command)));
    DEBUG_PRINTINDENT("dumpv_send");
    DEBUG_MSG(("dumpv_send", "  Flags:\t%02x\n", (int) *(*buf + ilen + 2)));

    DEBUG_DUMPHEADER("send", "Session ID");
    if (!Protocol_reallocBuildInt(buf, buf_len, out_len, allow_realloc,
                                  pdu->sessid, network_order)) {
        DEBUG_INDENTLESS();
        DEBUG_INDENTLESS();
        return 0;
    }
    DEBUG_INDENTLESS();

    DEBUG_DUMPHEADER("send", "Transaction ID");
    if (!Protocol_reallocBuildInt(buf, buf_len, out_len, allow_realloc,
                                  pdu->transid, network_order)) {
        DEBUG_INDENTLESS();
        DEBUG_INDENTLESS();
        return 0;
    }
    DEBUG_INDENTLESS();

    DEBUG_DUMPHEADER("send", "Request ID");
    if (!Protocol_reallocBuildInt(buf, buf_len, out_len, allow_realloc,
                                  pdu->reqid, network_order)) {
        DEBUG_INDENTLESS();
        DEBUG_INDENTLESS();
        return 0;
    }
    DEBUG_INDENTLESS();

    DEBUG_DUMPHEADER("send", "Dummy Length :-(");
    if (!Protocol_reallocBuildInt(buf, buf_len, out_len, allow_realloc,
                                  0, network_order)) {
        DEBUG_INDENTLESS();
        DEBUG_INDENTLESS();
        return 0;
    }
    DEBUG_INDENTLESS();

    if (pdu->flags & AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT) {
        DEBUG_DUMPHEADER("send", "Community");
        if (!Protocol_reallocBuildString
            (buf, buf_len, out_len, allow_realloc, pdu->community,
             pdu->communityLen, network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_INDENTLESS();
    }

    DEBUG_INDENTLESS();
    return 1;
}

static int
_Protocol_reallocBuild(u_char ** buf, size_t * buf_len, size_t * out_len,
                      int allow_realloc,
                      Types_Session * session, Types_Pdu *pdu)
{
    size_t          ilen = *out_len;
    Types_VariableList *vp;
    int             inc, i = 0;
    const int       network_order =
        pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER;

    session->s_snmp_errno = 0;
    session->s_errno = 0;

    /*
     * Various PDU types don't include context information (RFC 2741, p. 20).
     */
    switch (pdu->command) {
    case AGENTX_MSG_OPEN:
    case AGENTX_MSG_CLOSE:
    case AGENTX_MSG_RESPONSE:
    case AGENTX_MSG_COMMITSET:
    case AGENTX_MSG_UNDOSET:
    case AGENTX_MSG_CLEANUPSET:
        pdu->flags &= ~(AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT);
    }

    /* We've received a PDU that has specified a context.  NetSNMP however, uses
     * the pdu->community field to specify context when using the AgentX
     * protocol.  Therefore we need to copy the context name and length into the
     * pdu->community and pdu->community_len fields, respectively. */
    if (pdu->contextName != NULL && pdu->community == NULL)
    {
        pdu->community     = (u_char *) strdup(pdu->contextName);
        pdu->communityLen = pdu->contextNameLen;
        pdu->flags |= AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT;
    }

    /*
     * Build the header (and context if appropriate).
     */
    if (!Protocol_reallocBuildHeader
        (buf, buf_len, out_len, allow_realloc, pdu)) {
        return 0;
    }

    /*
     * Everything causes a response, except for agentx-Response-PDU and
     * agentx-CleanupSet-PDU.
     */

    pdu->flags |= PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE;

    DEBUG_DUMPHEADER("send", "AgentX Payload");
    switch (pdu->command) {

    case AGENTX_MSG_OPEN:
        /*
         * Timeout
         */
        while ((*out_len + 4) >= *buf_len) {
            if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
                DEBUG_INDENTLESS();
                return 0;
            }
        }
        *(*buf + *out_len) = (u_char) pdu->time;
        (*out_len)++;
        for (i = 0; i < 3; i++) {
            *(*buf + *out_len) = 0;
            (*out_len)++;
        }
        DEBUG_DUMPHEADER("send", "Open Timeout");
        DEBUG_DUMPSETUP("send", (*buf + *out_len - 4), 4);
        DEBUG_MSG(("dumpv_send", "  Timeout:\t%d\n",
                  (int) *(*buf + *out_len - 4)));
        DEBUG_INDENTLESS();

        DEBUG_DUMPHEADER("send", "Open ID");
        if (!Protocol_reallocBuildOid
            (buf, buf_len, out_len, allow_realloc, 0, pdu->variables->name,
             pdu->variables->nameLength, network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_INDENTLESS();
        DEBUG_DUMPHEADER("send", "Open Description");
        if (!Protocol_reallocBuildString
            (buf, buf_len, out_len, allow_realloc,
             pdu->variables->val.string, pdu->variables->valLen,
             network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_INDENTLESS();
        break;

    case AGENTX_MSG_CLOSE:
        /*
         * Reason
         */
        while ((*out_len + 4) >= *buf_len) {
            if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
                DEBUG_INDENTLESS();
                return 0;
            }
        }
        *(*buf + *out_len) = (u_char) pdu->errstat;
        (*out_len)++;
        for (i = 0; i < 3; i++) {
            *(*buf + *out_len) = 0;
            (*out_len)++;
        }
        DEBUG_DUMPHEADER("send", "Close Reason");
        DEBUG_DUMPSETUP("send", (*buf + *out_len - 4), 4);
        DEBUG_MSG(("dumpv_send", "  Reason:\t%d\n",
                  (int) *(*buf + *out_len - 4)));
        DEBUG_INDENTLESS();
        break;

    case AGENTX_MSG_REGISTER:
    case AGENTX_MSG_UNREGISTER:
        while ((*out_len + 4) >= *buf_len) {
            if (!(allow_realloc && Tools_realloc2(buf, buf_len))) {
                DEBUG_INDENTLESS();
                return 0;
            }
        }
        if (pdu->command == AGENTX_MSG_REGISTER) {
            *(*buf + *out_len) = (u_char) pdu->time;
        } else {
            *(*buf + *out_len) = 0;
        }
        (*out_len)++;
        *(*buf + *out_len) = (u_char) pdu->priority;
        (*out_len)++;
        *(*buf + *out_len) = (u_char) pdu->range_subid;
        (*out_len)++;
        *(*buf + *out_len) = (u_char) 0;
        (*out_len)++;

        DEBUG_DUMPHEADER("send", "(Un)Register Header");
        DEBUG_DUMPSETUP("send", (*buf + *out_len - 4), 4);
        if (pdu->command == AGENTX_MSG_REGISTER) {
            DEBUG_MSG(("dumpv_send", "  Timeout:\t%d\n",
                      (int) *(*buf + *out_len - 4)));
            DEBUG_PRINTINDENT("dumpv_send");
        }
        DEBUG_MSG(("dumpv_send", "  Priority:\t%d\n",
                  (int) *(*buf + *out_len - 3)));
        DEBUG_PRINTINDENT("dumpv_send");
        DEBUG_MSG(("dumpv_send", "  Range SubID:\t%d\n",
                  (int) *(*buf + *out_len - 2)));
        DEBUG_INDENTLESS();

        vp = pdu->variables;
        DEBUG_DUMPHEADER("send", "(Un)Register Prefix");
        if (!Protocol_reallocBuildOid
            (buf, buf_len, out_len, allow_realloc, 0, vp->name,
             vp->nameLength, network_order)) {

            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_INDENTLESS();

        if (pdu->range_subid) {
            DEBUG_DUMPHEADER("send", "(Un)Register Range");
            if (!Protocol_reallocBuildInt
                (buf, buf_len, out_len, allow_realloc,
                 vp->val.objid[pdu->range_subid - 1], network_order)) {
                DEBUG_INDENTLESS();
                DEBUG_INDENTLESS();
                return 0;
            }
            DEBUG_INDENTLESS();
        }
        break;

    case AGENTX_MSG_GETBULK:
        DEBUG_DUMPHEADER("send", "GetBulk Non-Repeaters");
        if (!Protocol_reallocBuildShort
            (buf, buf_len, out_len, allow_realloc,
            (u_short)pdu->non_repeaters,
             network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_INDENTLESS();

        DEBUG_DUMPHEADER("send", "GetBulk Max-Repetitions");
        if (!Protocol_reallocBuildShort
            (buf, buf_len, out_len, allow_realloc,
            (u_short)pdu->max_repetitions,
             network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_INDENTLESS();

        /*
         * Fallthrough
         */

    case AGENTX_MSG_GET:
    case AGENTX_MSG_GETNEXT:
        DEBUG_DUMPHEADER("send", "Get* Variable List");
        for (vp = pdu->variables; vp != NULL; vp = vp->nextVariable) {
            inc = (vp->type == ASN01_PRIV_INCL_RANGE);
            if (!Protocol_reallocBuildOid
                (buf, buf_len, out_len, allow_realloc, inc, vp->name,
                 vp->nameLength, network_order)) {
                DEBUG_INDENTLESS();
                DEBUG_INDENTLESS();
                return 0;
            }
            if (!Protocol_reallocBuildOid
                (buf, buf_len, out_len, allow_realloc, 0, vp->val.objid,
                 vp->valLen / sizeof(oid), network_order)) {
                DEBUG_INDENTLESS();
                DEBUG_INDENTLESS();
                return 0;
            }
        }
        DEBUG_INDENTLESS();
        break;

    case AGENTX_MSG_RESPONSE:
        pdu->flags &= ~(PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE);
        if (!Protocol_reallocBuildInt(buf, buf_len, out_len, allow_realloc,
                                      pdu->time, network_order)) {
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_DUMPHEADER("send", "Response");
        DEBUG_DUMPSETUP("send", (*buf + *out_len - 4), 4);
        DEBUG_MSG(("dumpv_send", "  sysUpTime:\t%lu\n", pdu->time));
        DEBUG_INDENTLESS();

        if (!Protocol_reallocBuildShort
            (buf, buf_len, out_len, allow_realloc,
            (u_short)pdu->errstat,
             network_order)
            || !Protocol_reallocBuildShort(buf, buf_len, out_len,
                                           allow_realloc,
                                           (u_short)pdu->errindex,
                                           network_order)) {
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_DUMPHEADER("send", "Response errors");
        DEBUG_DUMPSETUP("send", (*buf + *out_len - 4), 4);
        DEBUG_MSG(("dumpv_send", "  errstat:\t%ld\n", pdu->errstat));
        DEBUG_PRINTINDENT("dumpv_send");
        DEBUG_MSG(("dumpv_send", "  errindex:\t%ld\n", pdu->errindex));
        DEBUG_INDENTLESS();

        /*
         * Fallthrough
         */

    case AGENTX_MSG_INDEX_ALLOCATE:
    case AGENTX_MSG_INDEX_DEALLOCATE:
    case AGENTX_MSG_NOTIFY:
    case AGENTX_MSG_TESTSET:
        DEBUG_DUMPHEADER("send", "Get* Variable List");
        for (vp = pdu->variables; vp != NULL; vp = vp->nextVariable) {
            if (!Protocol_reallocBuildVarbind
                (buf, buf_len, out_len, allow_realloc, vp,
                 network_order)) {
                DEBUG_INDENTLESS();
                DEBUG_INDENTLESS();
                return 0;
            }
        }
        DEBUG_INDENTLESS();
        break;

    case AGENTX_MSG_COMMITSET:
    case AGENTX_MSG_UNDOSET:
    case AGENTX_MSG_PING:
        /*
         * "Empty" packet.
         */
        break;

    case AGENTX_MSG_CLEANUPSET:
        pdu->flags &= ~(PRIOT_UCD_MSG_FLAG_EXPECT_RESPONSE);
        break;

    case AGENTX_MSG_ADD_AGENT_CAPS:
        DEBUG_DUMPHEADER("send", "AgentCaps OID");
        if (!Protocol_reallocBuildOid
            (buf, buf_len, out_len, allow_realloc, 0, pdu->variables->name,
             pdu->variables->nameLength, network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_INDENTLESS();

        DEBUG_DUMPHEADER("send", "AgentCaps Description");
        if (!Protocol_reallocBuildString
            (buf, buf_len, out_len, allow_realloc,
             pdu->variables->val.string, pdu->variables->valLen,
             network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_INDENTLESS();
        break;

    case AGENTX_MSG_REMOVE_AGENT_CAPS:
        DEBUG_DUMPHEADER("send", "AgentCaps OID");
        if (!Protocol_reallocBuildOid
            (buf, buf_len, out_len, allow_realloc, 0, pdu->variables->name,
             pdu->variables->nameLength, network_order)) {
            DEBUG_INDENTLESS();
            DEBUG_INDENTLESS();
            return 0;
        }
        DEBUG_INDENTLESS();
        break;

    default:
        session->s_snmp_errno = ErrorCode_UNKNOWN_PDU;
        return 0;
    }
    DEBUG_INDENTLESS();

    /*
     * Fix the payload length (ignoring the 20-byte header).
     */

    Protocol_buildInt((*buf + 16), (*out_len - ilen) - 20, network_order);

    DEBUG_MSGTL(("agentx_build", "packet built okay\n"));
    return 1;
}

int
Protocol_reallocBuild(Types_Session * session, Types_Pdu *pdu,
                     u_char ** buf, size_t * buf_len, size_t * out_len)
{
    if (session == NULL || buf_len == NULL ||
        out_len == NULL || pdu == NULL || buf == NULL) {
        return -1;
    }
    if (!_Protocol_reallocBuild(buf, buf_len, out_len, 1, session, pdu)) {
        if (session->s_snmp_errno == 0) {
            session->s_snmp_errno = ErrorCode_BAD_ASN1_BUILD;
        }
        return -1;
    }

    return 0;
}

        /***********************
    *
    *  Utility functions for parsing an AgentX packet
    *
    ***********************/

int
Protocol_parseInt(u_char * data, u_int network_byte_order)
{
    u_int           value = 0;

    /*
     *  Note - this doesn't handle 'PDP_ENDIAN' systems
     *      If anyone needs this added, contact the coders list
     */
    DEBUG_DUMPSETUP("recv", data, 4);
    if (network_byte_order) {
        memmove(&value, data, 4);
        value = ntohl(value);
    } else {
        memmove(&value, data, 4);

    }
    DEBUG_MSG(("dumpv_recv", "  Integer:\t%u (0x%.2X)\n", value, value));

    return value;
}


int
Protocol_parseShort(u_char * data, u_int network_byte_order)
{
    u_short         value = 0;

    if (network_byte_order) {
        memmove(&value, data, 2);
        value = ntohs(value);
    } else {
        memmove(&value, data, 2);

    }

    DEBUG_DUMPSETUP("recv", data, 2);
    DEBUG_MSG(("dumpv_recv", "  Short:\t%hu (0x%.2X)\n", value, value));
    return value;
}


u_char         *
Protocol_parseOid(u_char * data, size_t * length, int *inc,
                 oid * oid_buf, size_t * oid_len, u_int network_byte_order)
{
    u_int           n_subid;
    u_int           prefix;
    u_int           tmp_oid_len;
    int             i;
    int             int_offset;
    u_int          *int_ptr = (u_int *)oid_buf;
    u_char         *buf_ptr = data;

    if (*length < 4) {
        DEBUG_MSGTL(("agentx", "Incomplete Object ID\n"));
        return NULL;
    }

    DEBUG_DUMPHEADER("recv", "OID Header");
    DEBUG_DUMPSETUP("recv", data, 4);
    DEBUG_MSG(("dumpv_recv", "  # subids:\t%d (0x%.2X)\n", data[0],
              data[0]));
    DEBUG_PRINTINDENT("dumpv_recv");
    DEBUG_MSG(("dumpv_recv", "  prefix: \t%d (0x%.2X)\n", data[1],
              data[1]));
    DEBUG_PRINTINDENT("dumpv_recv");
    DEBUG_MSG(("dumpv_recv", "  inclusive:\t%d (0x%.2X)\n", data[2],
              data[2]));

    DEBUG_INDENTLESS();
    DEBUG_DUMPHEADER("recv", "OID Segments");

    n_subid = data[0];
    prefix = data[1];
    if (inc)
        *inc = data[2];
    int_offset = sizeof(oid)/4;

    buf_ptr += 4;
    *length -= 4;

    DEBUG_MSG(("djp", "  parse_oid\n"));
    DEBUG_MSG(("djp", "  sizeof(oid) = %d\n", (int)sizeof(oid)));
    if (n_subid == 0 && prefix == 0) {
        /*
         * Null OID
         */
        memset(int_ptr, 0, 2 * sizeof(oid));
        *oid_len = 2;
        DEBUG_PRINTINDENT("dumpv_recv");
        DEBUG_MSG(("dumpv_recv", "OID: NULL (0.0)\n"));
        DEBUG_INDENTLESS();
        return buf_ptr;
    }

    /*
     * Check that the expanded OID will fit in the buffer provided
     */
    tmp_oid_len = (prefix ? n_subid + 5 : n_subid);
    if (*oid_len < tmp_oid_len) {
        DEBUG_MSGTL(("agentx", "Oversized Object ID (buf=%" NETSNMP_PRIz "d"
            " pdu=%d)\n", *oid_len, tmp_oid_len));
        DEBUG_INDENTLESS();
        return NULL;
    }


# define endianoff 0
    if (*length < 4 * n_subid) {
        DEBUG_MSGTL(("agentx", "Incomplete Object ID\n"));
        DEBUG_INDENTLESS();
        return NULL;
    }

    if (prefix) {
        if (int_offset == 2) {  	/* align OID values in 64 bit agent */
        memset(int_ptr, 0, 10*sizeof(int_ptr[0]));
        int_ptr[0+endianoff] = 1;
        int_ptr[2+endianoff] = 3;
        int_ptr[4+endianoff] = 6;
        int_ptr[6+endianoff] = 1;
        int_ptr[8+endianoff] = prefix;
        } else { /* assume int_offset == 1 */
        int_ptr[0] = 1;
        int_ptr[1] = 3;
        int_ptr[2] = 6;
        int_ptr[3] = 1;
        int_ptr[4] = prefix;
        }
        int_ptr = int_ptr + (int_offset * 5);
    }

    for (i = 0; i < (int) (int_offset * n_subid); i = i + int_offset) {
    int x;

    x = Protocol_parseInt(buf_ptr, network_byte_order);
    if (int_offset == 2) {
            int_ptr[i+0] = 0;
        int_ptr[i+1] = 0;
        int_ptr[i+endianoff]=x;
        } else {
        int_ptr[i] = x;
        }
        buf_ptr += 4;
        *length -= 4;
    }

    *oid_len = tmp_oid_len;

    DEBUG_INDENTLESS();
    DEBUG_PRINTINDENT("dumpv_recv");
    DEBUG_MSG(("dumpv_recv", "OID: "));
    DEBUG_MSGOID(("dumpv_recv", oid_buf, *oid_len));
    DEBUG_MSG(("dumpv_recv", "\n"));

    return buf_ptr;
}



u_char         *
Protocol_parseString(u_char * data, size_t * length,
                    u_char * string, size_t * str_len,
                    u_int network_byte_order)
{
    u_int           len;

    if (*length < 4) {
        DEBUG_MSGTL(("agentx", "Incomplete string (too short: %d)\n",
                    (int)*length));
        return NULL;
    }

    len = Protocol_parseInt(data, network_byte_order);
    if (*length < len + 4) {
        DEBUG_MSGTL(("agentx", "Incomplete string (still too short: %d)\n",
                    (int)*length));
        return NULL;
    }
    if (len > *str_len) {
        DEBUG_MSGTL(("agentx", "String too long (too long)\n"));
        return NULL;
    }
    memmove(string, data + 4, len);
    string[len] = '\0';
    *str_len = len;

    len += 3;                   /* Extend the string length to include the padding */
    len >>= 2;
    len <<= 2;

    *length -= (len + 4);
    DEBUG_DUMPSETUP("recv", data, (len + 4));
    DEBUG_IF("dumpv_recv") {
        u_char         *buf = NULL;
        size_t          buf_len = 0, out_len = 0;

        if (Mib_sprintReallocAsciiString(&buf, &buf_len, &out_len, 1,
                                       string, len)) {
            DEBUG_MSG(("dumpv_recv", "String: %s\n", buf));
        } else {
            DEBUG_MSG(("dumpv_recv", "String: %s [TRUNCATED]\n", buf));
        }
        if (buf != NULL) {
            free(buf);
        }
    }
    return data + (len + 4);
}

u_char         *
Protocol_parseOpaque(u_char * data, size_t * length, int *type,
                    u_char * opaque_buf, size_t * opaque_len,
                    u_int network_byte_order)
{
    union {
        float           floatVal;
        double          doubleVal;
        int             intVal[2];
        char            c[sizeof(double)];
    } fu;
    int             tmp;
    u_char         *buf;
    u_char         *const cp =
        Protocol_parseString(data, length,
                            opaque_buf, opaque_len, network_byte_order);

    if (cp == NULL)
        return NULL;

    buf = opaque_buf;

    if ((*opaque_len <= 3) || (buf[0] != ASN01_OPAQUE_TAG1))
        return cp;              /* Unrecognised opaque type */

    switch (buf[1]) {
    case ASN01_OPAQUE_FLOAT:
        if ((*opaque_len != (3 + sizeof(float))) ||
            (buf[2] != sizeof(float)))
            return cp;          /* Encoding isn't right for FLOAT */

        memcpy(&fu.c[0], &buf[3], sizeof(float));
        fu.intVal[0] = ntohl(fu.intVal[0]);
        *opaque_len = sizeof(float);
        memcpy(opaque_buf, &fu.c[0], sizeof(float));
        *type = ASN01_OPAQUE_FLOAT;
        DEBUG_MSG(("dumpv_recv", "Float: %f\n", fu.floatVal));
        return cp;

    case ASN01_OPAQUE_DOUBLE:
        if ((*opaque_len != (3 + sizeof(double))) ||
            (buf[2] != sizeof(double)))
            return cp;          /* Encoding isn't right for DOUBLE */

        memcpy(&fu.c[0], &buf[3], sizeof(double));
        tmp = ntohl(fu.intVal[1]);
        fu.intVal[1] = ntohl(fu.intVal[0]);
        fu.intVal[0] = tmp;
        *opaque_len = sizeof(double);
        memcpy(opaque_buf, &fu.c[0], sizeof(double));
        *type = ASN01_OPAQUE_DOUBLE;
        DEBUG_MSG(("dumpv_recv", "Double: %f\n", fu.doubleVal));
        return cp;

    case ASN01_OPAQUE_I64:
    case ASN01_OPAQUE_U64:
    case ASN01_OPAQUE_COUNTER64:
    default:
        return cp;              /* Unrecognised opaque sub-type */
    }

}


u_char         *
Protocol_parseVarbind(u_char * data, size_t * length, int *type,
                     oid * oid_buf, size_t * oid_len,
                     u_char * data_buf, size_t * data_len,
                     u_int network_byte_order)
{
    u_char         *bufp = data;
    u_int           int_val;
    struct Asn01_Counter64_s tmp64;

    DEBUG_DUMPHEADER("recv", "VarBind:");
    DEBUG_DUMPHEADER("recv", "Type");
    *type = Protocol_parseShort(bufp, network_byte_order);
    DEBUG_INDENTLESS();
    bufp += 4;
    *length -= 4;

    bufp = Protocol_parseOid(bufp, length, NULL, oid_buf, oid_len,
                            network_byte_order);
    if (bufp == NULL) {
        DEBUG_INDENTLESS();
        return NULL;
    }

    switch (*type) {
    case ASN01_INTEGER:
    case ASN01_COUNTER:
    case ASN01_GAUGE:
    case ASN01_TIMETICKS:
    case ASN01_UINTEGER:
        int_val = Protocol_parseInt(bufp, network_byte_order);
        memmove(data_buf, &int_val, 4);
        *data_len = 4;
        bufp += 4;
        *length -= 4;
        break;

    case ASN01_OCTET_STR:
    case ASN01_IPADDRESS:
        bufp = Protocol_parseString(bufp, length, data_buf, data_len,
                                   network_byte_order);
        break;

    case ASN01_OPAQUE:
        bufp = Protocol_parseOpaque(bufp, length, type, data_buf, data_len,
                                   network_byte_order);
        break;

    case ASN01_PRIV_INCL_RANGE:
    case ASN01_PRIV_EXCL_RANGE:
    case ASN01_OBJECT_ID:
        bufp =
            Protocol_parseOid(bufp, length, NULL, (oid *) data_buf,
                             data_len, network_byte_order);
        *data_len *= sizeof(oid);
        /*
         * 'Protocol_parseOid()' returns the number of sub_ids
         */
        break;

    case ASN01_COUNTER64:
        memset(&tmp64, 0, sizeof(tmp64));
    if (network_byte_order) {
        tmp64.high = Protocol_parseInt(bufp,   network_byte_order);
        tmp64.low  = Protocol_parseInt(bufp+4, network_byte_order);
    } else {
        tmp64.high = Protocol_parseInt(bufp+4, network_byte_order);
        tmp64.low  = Protocol_parseInt(bufp,   network_byte_order);
    }

        memcpy(data_buf, &tmp64, sizeof(tmp64));
    *data_len = sizeof(tmp64);
    bufp    += 8;
    *length -= 8;
        break;

    case ASN01_NULL:
    case PRIOT_NOSUCHOBJECT:
    case PRIOT_NOSUCHINSTANCE:
    case PRIOT_ENDOFMIBVIEW:
        /*
         * No data associated with these types.
         */
        *data_len = 0;
        break;

    default:
        DEBUG_MSG(("recv", "Can not parse type %x", *type));
        DEBUG_INDENTLESS();
        return NULL;
    }
    DEBUG_INDENTLESS();
    return bufp;
}

/*
 *  AgentX header:
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |    h.version  |   h.type      |   h.flags     |  <reserved>   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       h.sessionID                             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     h.transactionID                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       h.packetID                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     h.payload_length                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *    Total length = 20 bytes
 *
 *  If we don't seem to have the full packet, return NULL
 *    and let the driving code go back for the rest.
 *  Don't report this as an error, as it's quite "normal"
 *    with a connection-oriented service.
 *
 *  Note that once the header has been successfully processed
 *    (and hence we should have the full packet), any subsequent
 *    "running out of room" is indeed an error.
 */
u_char         *
Protocol_parseHeader(Types_Pdu *pdu, u_char * data, size_t * length)
{
    register u_char *bufp = data;
    size_t          payload;

    if (*length < 20) {         /* Incomplete header */
        return NULL;
    }

    DEBUG_DUMPHEADER("recv", "AgentX Header");
    DEBUG_DUMPHEADER("recv", "Version");
    DEBUG_DUMPSETUP("recv", bufp, 1);
    pdu->version = AGENTX_VERSION_BASE | *bufp;
    DEBUG_MSG(("dumpv_recv", "  Version:\t%d\n", *bufp));
    DEBUG_INDENTLESS();
    bufp++;

    DEBUG_DUMPHEADER("recv", "Command");
    DEBUG_DUMPSETUP("recv", bufp, 1);
    pdu->command = *bufp;
    DEBUG_MSG(("dumpv_recv", "  Command:\t%d (%s)\n", *bufp,
              Protocol_cmd(*bufp)));
    DEBUG_INDENTLESS();
    bufp++;

    DEBUG_DUMPHEADER("recv", "Flags");
    DEBUG_DUMPSETUP("recv", bufp, 1);
    pdu->flags |= *bufp;
    DEBUG_MSG(("dumpv_recv", "  Flags:\t0x%x\n", *bufp));
    DEBUG_INDENTLESS();
    bufp++;

    DEBUG_DUMPHEADER("recv", "Reserved Byte");
    DEBUG_DUMPSETUP("recv", bufp, 1);
    DEBUG_MSG(("dumpv_recv", "  Reserved:\t0x%x\n", *bufp));
    DEBUG_INDENTLESS();
    bufp++;

    DEBUG_DUMPHEADER("recv", "Session ID");
    pdu->sessid = Protocol_parseInt(bufp,
                                   pdu->
                                   flags &
                                   AGENTX_FLAGS_NETWORK_BYTE_ORDER);
    DEBUG_INDENTLESS();
    bufp += 4;

    DEBUG_DUMPHEADER("recv", "Transaction ID");
    pdu->transid = Protocol_parseInt(bufp,
                                    pdu->
                                    flags &
                                    AGENTX_FLAGS_NETWORK_BYTE_ORDER);
    DEBUG_INDENTLESS();
    bufp += 4;

    DEBUG_DUMPHEADER("recv", "Packet ID");
    pdu->reqid = Protocol_parseInt(bufp,
                                  pdu->
                                  flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
    DEBUG_INDENTLESS();
    bufp += 4;

    DEBUG_DUMPHEADER("recv", "Payload Length");
    payload = Protocol_parseInt(bufp,
                               pdu->
                               flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
    DEBUG_INDENTLESS();
    bufp += 4;

    DEBUG_INDENTLESS();
    *length -= 20;
    if (*length != payload) {   /* Short payload */
        return NULL;
    }
    return bufp;
}


int
Protocol_parse(Types_Session * session, Types_Pdu *pdu, u_char * data,
             size_t len)
{
    register u_char *bufp = data;
    u_char          buffer[API_MAX_MSG_SIZE];
    oid             oid_buffer[ASN01_MAX_OID_LEN], end_oid_buf[ASN01_MAX_OID_LEN];
    size_t          buf_len = sizeof(buffer);
    size_t          oid_buf_len = ASN01_MAX_OID_LEN;
    size_t          end_oid_buf_len = ASN01_MAX_OID_LEN;

    int             range_bound;        /* OID-range upper bound */
    int             inc;        /* Inclusive SearchRange flag */
    int             type;       /* VarBind data type */
    size_t         *length = &len;

    if (pdu == NULL)
        return (0);

    if (!IS_AGENTX_VERSION(session->version))
        return ErrorCode_BAD_VERSION;

    /*
     *  Ideally, "short" packets on stream connections should
     *    be handled specially, and the driving code set up to
     *    keep reading until the full packet is received.
     *
     *  For now, lets assume that all packets are read in one go.
     *    I've probably inflicted enough damage on the UCD library
     *    for one week!
     *
     *  I'll come back to this once Wes is speaking to me again.
     */
#define ErrorCode_INCOMPLETE_PACKET ErrorCode_ASN_PARSE_ERR


    /*
     *  Handle (common) header ....
     */
    bufp = Protocol_parseHeader(pdu, bufp, length);
    if (bufp == NULL)
        return ErrorCode_INCOMPLETE_PACKET;       /* i.e. wait for the rest */

    /*
     * Control PDU handling
     */
    pdu->flags |= PRIOT_UCD_MSG_FLAG_ALWAYS_IN_VIEW;
    pdu->flags |= PRIOT_UCD_MSG_FLAG_FORCE_PDU_COPY;
    pdu->flags &= (~PRIOT_UCD_MSG_FLAG_RESPONSE_PDU);

    /*
     *  ... and (not-un-common) context
     */
    if (pdu->flags & AGENTX_MSG_FLAG_NON_DEFAULT_CONTEXT) {
        DEBUG_DUMPHEADER("recv", "Context");
        bufp = Protocol_parseString(bufp, length, buffer, &buf_len,
                                   pdu->flags &
                                   AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUG_INDENTLESS();
        if (bufp == NULL)
            return ErrorCode_ASN_PARSE_ERR;

        pdu->communityLen = buf_len;
        Client_cloneMem((void **) &pdu->community,
                       (void *) buffer, (unsigned) buf_len);

        /* The NetSNMP API stuffs the context into the PDU's community string
         * field, when using the AgentX Protocol.  The rest of the code however,
         * expects to find the context in the PDU's context field.  Therefore we
         * need to copy the context into the PDU's context fields.  */
        if (pdu->communityLen > 0 && pdu->contextName == NULL)
        {
            pdu->contextName    = strdup((char *) pdu->community);
            pdu->contextNameLen = pdu->communityLen;
        }

        buf_len = sizeof(buffer);
    }

    DEBUG_DUMPHEADER("recv", "PDU");
    switch (pdu->command) {
    case AGENTX_MSG_OPEN:
        pdu->time = *bufp;      /* Timeout */
        bufp += 4;
        *length -= 4;

        /*
         * Store subagent OID & description in a VarBind
         */
        DEBUG_DUMPHEADER("recv", "Subagent OID");
        bufp = Protocol_parseOid(bufp, length, NULL,
                                oid_buffer, &oid_buf_len,
                                pdu->
                                flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUG_INDENTLESS();
        if (bufp == NULL) {
            DEBUG_INDENTLESS();
            return ErrorCode_ASN_PARSE_ERR;
        }
        DEBUG_DUMPHEADER("recv", "Subagent Description");
        bufp = Protocol_parseString(bufp, length, buffer, &buf_len,
                                   pdu->
                                   flags &
                                   AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUG_INDENTLESS();
        if (bufp == NULL) {
            DEBUG_INDENTLESS();
            return ErrorCode_ASN_PARSE_ERR;
        }
        Api_pduAddVariable(pdu, oid_buffer, oid_buf_len,
                              ASN01_OCTET_STR, buffer, buf_len);

        oid_buf_len = ASN01_MAX_OID_LEN;
        buf_len = sizeof(buffer);
        break;

    case AGENTX_MSG_CLOSE:
        pdu->errstat = *bufp;   /* Reason */
        bufp += 4;
        *length -= 4;

        break;

    case AGENTX_MSG_UNREGISTER:
    case AGENTX_MSG_REGISTER:
        DEBUG_DUMPHEADER("recv", "Registration Header");
        if (pdu->command == AGENTX_MSG_REGISTER) {
            pdu->time = *bufp;  /* Timeout (Register only) */
            DEBUG_DUMPSETUP("recv", bufp, 1);
            DEBUG_MSG(("dumpv_recv", "  Timeout:     \t%d\n", *bufp));
        }
        bufp++;
        pdu->priority = *bufp;
        DEBUG_DUMPSETUP("recv", bufp, 1);
        DEBUG_MSG(("dumpv_recv", "  Priority:    \t%d\n", *bufp));
        bufp++;
        pdu->range_subid = *bufp;
        DEBUG_DUMPSETUP("recv", bufp, 1);
        DEBUG_MSG(("dumpv_recv", "  Range Sub-Id:\t%d\n", *bufp));
        bufp++;
        bufp++;
        *length -= 4;
        DEBUG_INDENTLESS();

        DEBUG_DUMPHEADER("recv", "Registration OID");
        bufp = Protocol_parseOid(bufp, length, NULL,
                                oid_buffer, &oid_buf_len,
                                pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUG_INDENTLESS();
        if (bufp == NULL) {
            DEBUG_INDENTLESS();
            return ErrorCode_ASN_PARSE_ERR;
        }

        if (pdu->range_subid) {
            range_bound = Protocol_parseInt(bufp, pdu->flags &
                                           AGENTX_FLAGS_NETWORK_BYTE_ORDER);
            bufp += 4;
            *length -= 4;

            /*
             * Construct the end-OID.
             */
            end_oid_buf_len = oid_buf_len * sizeof(oid);
            memcpy(end_oid_buf, oid_buffer, end_oid_buf_len);
            end_oid_buf[pdu->range_subid - 1] = range_bound;

            Api_pduAddVariable(pdu, oid_buffer, oid_buf_len,
                                  ASN01_PRIV_INCL_RANGE,
                                  (u_char *) end_oid_buf, end_oid_buf_len);
        } else {
            Client_addNullVar(pdu, oid_buffer, oid_buf_len);
        }

        oid_buf_len = ASN01_MAX_OID_LEN;
        break;

    case AGENTX_MSG_GETBULK:
        DEBUG_DUMPHEADER("recv", "Non-repeaters");
        pdu->non_repeaters = Protocol_parseShort(bufp, pdu->flags &
                                                AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUG_INDENTLESS();
        DEBUG_DUMPHEADER("recv", "Max-repeaters");
        pdu->max_repetitions = Protocol_parseShort(bufp + 2, pdu->flags &
                                                  AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        DEBUG_INDENTLESS();
        bufp += 4;
        *length -= 4;
        /*
         * Fallthrough - SearchRange handling is the same
         */

    case AGENTX_MSG_GETNEXT:
    case AGENTX_MSG_GET:

        /*
         * *  SearchRange List
         * *  Keep going while we have data left
         */
        DEBUG_DUMPHEADER("recv", "Search Range");
        while (*length > 0) {
            bufp = Protocol_parseOid(bufp, length, &inc,
                                    oid_buffer, &oid_buf_len,
                                    pdu->flags &
                                    AGENTX_FLAGS_NETWORK_BYTE_ORDER);
            if (bufp == NULL) {
                DEBUG_INDENTLESS();
                DEBUG_INDENTLESS();
                return ErrorCode_ASN_PARSE_ERR;
            }
            bufp = Protocol_parseOid(bufp, length, NULL,
                                    end_oid_buf, &end_oid_buf_len,
                                    pdu->flags &
                                    AGENTX_FLAGS_NETWORK_BYTE_ORDER);
            if (bufp == NULL) {
                DEBUG_INDENTLESS();
                DEBUG_INDENTLESS();
                return ErrorCode_ASN_PARSE_ERR;
            }
            end_oid_buf_len *= sizeof(oid);
            /*
             * 'Protocol_parseOid()' returns the number of sub_ids
             */

            if (inc) {
                Api_pduAddVariable(pdu, oid_buffer, oid_buf_len,
                                      ASN01_PRIV_INCL_RANGE,
                                      (u_char *) end_oid_buf,
                                      end_oid_buf_len);
            } else {
                Api_pduAddVariable(pdu, oid_buffer, oid_buf_len,
                                      ASN01_PRIV_EXCL_RANGE,
                                      (u_char *) end_oid_buf,
                                      end_oid_buf_len);
            }
            oid_buf_len = ASN01_MAX_OID_LEN;
            end_oid_buf_len = ASN01_MAX_OID_LEN;
        }

        DEBUG_INDENTLESS();
        break;


    case AGENTX_MSG_RESPONSE:

        pdu->flags |= PRIOT_UCD_MSG_FLAG_RESPONSE_PDU;

        /*
         * sysUpTime
         */
        pdu->time = Protocol_parseInt(bufp, pdu->flags &
                                     AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        bufp += 4;
        *length -= 4;

        pdu->errstat = Protocol_parseShort(bufp, pdu->flags &
                                          AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        pdu->errindex =
            Protocol_parseShort(bufp + 2,
                               pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        bufp += 4;
        *length -= 4;
        /*
         * Fallthrough - VarBind handling is the same
         */

    case AGENTX_MSG_INDEX_ALLOCATE:
    case AGENTX_MSG_INDEX_DEALLOCATE:
    case AGENTX_MSG_NOTIFY:
    case AGENTX_MSG_TESTSET:

        /*
         * *  VarBind List
         * *  Keep going while we have data left
         */

        DEBUG_DUMPHEADER("recv", "VarBindList");
        while (*length > 0) {
            bufp = Protocol_parseVarbind(bufp, length, &type,
                                        oid_buffer, &oid_buf_len,
                                        buffer, &buf_len,
                                        pdu->flags &
                                        AGENTX_FLAGS_NETWORK_BYTE_ORDER);
            if (bufp == NULL) {
                DEBUG_INDENTLESS();
                DEBUG_INDENTLESS();
                return ErrorCode_ASN_PARSE_ERR;
            }
            Api_pduAddVariable(pdu, oid_buffer, oid_buf_len,
                                  (u_char) type, buffer, buf_len);

            oid_buf_len = ASN01_MAX_OID_LEN;
            buf_len = sizeof(buffer);
        }
        DEBUG_INDENTLESS();
        break;

    case AGENTX_MSG_COMMITSET:
    case AGENTX_MSG_UNDOSET:
    case AGENTX_MSG_CLEANUPSET:
    case AGENTX_MSG_PING:

        /*
         * "Empty" packet
         */
        break;


    case AGENTX_MSG_ADD_AGENT_CAPS:
        /*
         * Store AgentCap OID & description in a VarBind
         */
        bufp = Protocol_parseOid(bufp, length, NULL,
                                oid_buffer, &oid_buf_len,
                                pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        if (bufp == NULL)
            return ErrorCode_ASN_PARSE_ERR;
        bufp = Protocol_parseString(bufp, length, buffer, &buf_len,
                                   pdu->flags &
                                   AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        if (bufp == NULL)
            return ErrorCode_ASN_PARSE_ERR;
        Api_pduAddVariable(pdu, oid_buffer, oid_buf_len,
                              ASN01_OCTET_STR, buffer, buf_len);

        oid_buf_len = ASN01_MAX_OID_LEN;
        buf_len = sizeof(buffer);
        break;

    case AGENTX_MSG_REMOVE_AGENT_CAPS:
        /*
         * Store AgentCap OID & description in a VarBind
         */
        bufp = Protocol_parseOid(bufp, length, NULL,
                                oid_buffer, &oid_buf_len,
                                pdu->flags & AGENTX_FLAGS_NETWORK_BYTE_ORDER);
        if (bufp == NULL)
            return ErrorCode_ASN_PARSE_ERR;
        Client_addNullVar(pdu, oid_buffer, oid_buf_len);

        oid_buf_len = ASN01_MAX_OID_LEN;
        break;

    default:
        DEBUG_INDENTLESS();
        DEBUG_MSGTL(("agentx", "Unrecognised PDU type: %d\n",
                    pdu->command));
        return ErrorCode_UNKNOWN_PDU;
    }
    DEBUG_INDENTLESS();
    return PRIOT_ERR_NOERROR;
}






/*
 * returns the proper length of an incoming agentx packet.
 */
/*
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |   h.version   |    h.type     |    h.flags    |  <reserved>   |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                          h.sessionID                          |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                        h.transactionID                        |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                          h.packetID                           |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    |                        h.payload_length                       |
 *    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *    20 bytes in header
 */

int
Protocol_checkPacket(u_char * packet, size_t packet_len)
{

    if (packet_len < 20)
        return 0;               /* minimum header length == 20 */

    return Protocol_parseInt(packet + 16,
                            *(packet +
                              2) & AGENTX_FLAGS_NETWORK_BYTE_ORDER) + 20;
}
