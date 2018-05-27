#include "Priot.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"
#include "Impl.h"

/** @mainpage Net-SNMP Coding Documentation
 * @section Introduction

   This is the Net-SNMP coding and API reference documentation.  It is
   incomplete, but when combined with the manual page set and
   tutorials forms a pretty comprehensive starting point.

   @section Starting out

   The best places to start learning are the @e Net-SNMP @e tutorial
   (http://www.Net-SNMP.org/tutorial-5/) and the @e Modules and @e
   Examples sections of this document.

*/

void
Priot_xdump(const void * data, size_t length, const char *prefix)
{
    const u_char * const cp = (const u_char*)data;
    int                  col, count;
    char                *buffer;

    buffer = (char *) malloc(strlen(prefix) + 80);
    if (!buffer) {
        Logger_log(LOGGER_PRIORITY_NOTICE,
                 "xdump: malloc failed. packet-dump skipped\n");
        return;
    }

    count = 0;
    while (count < (int) length) {
        strcpy(buffer, prefix);
        sprintf(buffer + strlen(buffer), "%.4d: ", count);

        for (col = 0; ((count + col) < (int) length) && col < 16; col++) {
            sprintf(buffer + strlen(buffer), "%02X ", cp[count + col]);
            if (col % 4 == 3)
                strcat(buffer, " ");
        }
        for (; col < 16; col++) {       /* pad end of buffer with zeros */
            strcat(buffer, "   ");
            if (col % 4 == 3)
                strcat(buffer, " ");
        }
        strcat(buffer, "  ");
        for (col = 0; ((count + col) < (int) length) && col < 16; col++) {
            buffer[col + 60] =
                isprint(cp[count + col]) ? cp[count + col] : '.';
        }
        buffer[col + 60] = '\n';
        buffer[col + 60 + 1] = 0;
        Logger_log(LOGGER_PRIORITY_DEBUG, "%s", buffer);
        count += col;
    }
    Logger_log(LOGGER_PRIORITY_DEBUG, "\n");
    free(buffer);

}                               /* end xdump() */

/*
 * u_char * snmp_parse_var_op(
 * u_char *data              IN - pointer to the start of object
 * oid *var_name             OUT - object id of variable
 * int *var_name_len         IN/OUT - length of variable name
 * u_char *var_val_type      OUT - type of variable (int or octet string) (one byte)
 * int *var_val_len          OUT - length of variable
 * u_char **var_val          OUT - pointer to ASN1 encoded value of variable
 * int *listlength          IN/OUT - number of valid bytes left in var_op_list
 */

u_char         *
Priot_parseVarOp(u_char * data,
                  oid * var_name,
                  size_t * var_name_len,
                  u_char * var_val_type,
                  size_t * var_val_len,
                  u_char ** var_val, size_t * listlength)
{
    u_char          var_op_type;
    size_t          var_op_len = *listlength;
    u_char         *var_op_start = data;

    data = Asn01_parseSequence(data, &var_op_len, &var_op_type,
                              (ASN01_SEQUENCE | ASN01_CONSTRUCTOR), "var_op");
    if (data == NULL) {
        /*
         * msg detail is set
         */
        return NULL;
    }
    DEBUG_DUMPHEADER("recv", "Name");
    data =
        Asn01_parseObjid(data, &var_op_len, &var_op_type, var_name,
                        var_name_len);
    DEBUG_INDENTLESS();
    if (data == NULL) {
        IMPL_ERROR_MSG("No OID for variable");
        return NULL;
    }
    if (var_op_type !=
        (u_char) (ASN01_UNIVERSAL | ASN01_PRIMITIVE | ASN01_OBJECT_ID))
        return NULL;
    *var_val = data;            /* save pointer to this object */
    /*
     * find out what type of object this is
     */
    data = Asn01_parseHeader(data, &var_op_len, var_val_type);
    if (data == NULL) {
        IMPL_ERROR_MSG("No header for value");
        return NULL;
    }
    /*
     * XXX no check for type!
     */
    *var_val_len = var_op_len;
    data += var_op_len;
    *listlength -= (int) (data - var_op_start);
    return data;
}

/*
 * u_char * snmp_build_var_op(
 * u_char *data      IN - pointer to the beginning of the output buffer
 * oid *var_name        IN - object id of variable
 * int *var_name_len    IN - length of object id
 * u_char var_val_type  IN - type of variable
 * int    var_val_len   IN - length of variable
 * u_char *var_val      IN - value of variable
 * int *listlength      IN/OUT - number of valid bytes left in
 * output buffer
 */

u_char         *
Priot_buildVarOp(u_char * data,
                  oid * var_name,
                  size_t * var_name_len,
                  u_char var_val_type,
                  size_t var_val_len,
                  u_char * var_val, size_t * listlength)
{
    size_t          dummyLen, headerLen;
    u_char         *dataPtr;

    dummyLen = *listlength;
    dataPtr = data;

    if (dummyLen < 4)
        return NULL;
    data += 4;
    dummyLen -= 4;

    headerLen = data - dataPtr;
    *listlength -= headerLen;
    DEBUG_DUMPHEADER("send", "Name");
    data = Asn01_buildObjid(data, listlength,
                           (u_char) (ASN01_UNIVERSAL | ASN01_PRIMITIVE |
                                     ASN01_OBJECT_ID), var_name,
                           *var_name_len);
    DEBUG_INDENTLESS();
    if (data == NULL) {
        IMPL_ERROR_MSG("Can't build OID for variable");
        return NULL;
    }
    DEBUG_DUMPHEADER("send", "Value");
    switch (var_val_type) {
    case ASN01_INTEGER:
        data = Asn01_buildInt(data, listlength, var_val_type,
                             (long *) var_val, var_val_len);
        break;
    case ASN01_GAUGE:
    case ASN01_COUNTER:
    case ASN01_TIMETICKS:
    case ASN01_UINTEGER:
        data = Asn01_buildUnsignedInt(data, listlength, var_val_type,
                                      (u_long *) var_val, var_val_len);
        break;
    case ASN01_OPAQUE_COUNTER64:
    case ASN01_OPAQUE_U64:
    case ASN01_COUNTER64:
        data = Asn01_buildUnsignedInt64(data, listlength, var_val_type,
                                        (Counter64 *) var_val,
                                        var_val_len);
        break;
    case ASN01_OCTET_STR:
    case ASN01_IPADDRESS:
    case ASN01_OPAQUE:
    case ASN01_NSAP:
        data = Asn01_buildString(data, listlength, var_val_type,
                                var_val, var_val_len);
        break;
    case ASN01_OBJECT_ID:
        data = Asn01_buildObjid(data, listlength, var_val_type,
                               (oid *) var_val, var_val_len / sizeof(oid));
        break;
    case ASN01_NULL:
        data = Asn01_buildNull(data, listlength, var_val_type);
        break;
    case ASN01_BIT_STR:
        data = Asn01_buildBitstring(data, listlength, var_val_type,
                                   var_val, var_val_len);
        break;
    case PRIOT_NOSUCHOBJECT:
    case PRIOT_NOSUCHINSTANCE:
    case PRIOT_ENDOFMIBVIEW:
        data = Asn01_buildNull(data, listlength, var_val_type);
        break;
    case ASN01_OPAQUE_FLOAT:
        data = Asn01_buildFloat(data, listlength, var_val_type,
                               (float *) var_val, var_val_len);
        break;
    case ASN01_OPAQUE_DOUBLE:
        data = Asn01_buildDouble(data, listlength, var_val_type,
                                (double *) var_val, var_val_len);
        break;
    case ASN01_OPAQUE_I64:
        data = Asn01_buildSignedInt64(data, listlength, var_val_type,
                                      (Counter64 *) var_val,
                                      var_val_len);
        break;
    default:
    {
    char error_buf[64];
    snprintf(error_buf, sizeof(error_buf),
        "wrong type in snmp_build_var_op: %d", var_val_type);
        IMPL_ERROR_MSG(error_buf);
        data = NULL;
    }
    }
    DEBUG_INDENTLESS();
    if (data == NULL) {
        return NULL;
    }
    dummyLen = (data - dataPtr) - headerLen;

    Asn01_buildSequence(dataPtr, &dummyLen,
                       (u_char) (ASN01_SEQUENCE | ASN01_CONSTRUCTOR),
                       dummyLen);
    return data;
}

int
Priot_reallocRbuildVarOp(u_char ** pkt, size_t * pkt_len,
                           size_t * offset, int allow_realloc,
                           const oid * var_name, size_t * var_name_len,
                           u_char var_val_type,
                           u_char * var_val, size_t var_val_len)
{
    size_t          start_offset = *offset;
    int             rc = 0;

    /*
     * Encode the value.
     */
    DEBUG_DUMPHEADER("send", "Value");

    switch (var_val_type) {
    case ASN01_INTEGER:
        rc = Asn01_reallocRbuildInt(pkt, pkt_len, offset, allow_realloc,
                                    var_val_type, (long *) var_val,
                                    var_val_len);
        break;

    case ASN01_GAUGE:
    case ASN01_COUNTER:
    case ASN01_TIMETICKS:
    case ASN01_UINTEGER:
        rc = Asn01_reallocRbuildUnsignedInt(pkt, pkt_len, offset,
                                             allow_realloc, var_val_type,
                                             (u_long *) var_val,
                                             var_val_len);
        break;

    case ASN01_OPAQUE_COUNTER64:
    case ASN01_OPAQUE_U64:
    case ASN01_COUNTER64:
        rc = Asn01_reallocRbuildUnsignedInt64(pkt, pkt_len, offset,
                                               allow_realloc, var_val_type,
                                               (Counter64 *)
                                               var_val, var_val_len);
        break;

    case ASN01_OCTET_STR:
    case ASN01_IPADDRESS:
    case ASN01_OPAQUE:
    case ASN01_NSAP:
        rc = Asn01_reallocRbuildString(pkt, pkt_len, offset, allow_realloc,
                                       var_val_type, var_val, var_val_len);
        break;

    case ASN01_OBJECT_ID:
        rc = Asn01_reallocRbuildObjid(pkt, pkt_len, offset, allow_realloc,
                                      var_val_type, (oid *) var_val,
                                      var_val_len / sizeof(oid));
        break;

    case ASN01_NULL:
        rc = Asn01_reallocRbuildNull(pkt, pkt_len, offset, allow_realloc,
                                     var_val_type);
        break;

    case ASN01_BIT_STR:
        rc = Asn01_reallocRbuildBitstring(pkt, pkt_len, offset,
                                          allow_realloc, var_val_type,
                                          var_val, var_val_len);
        break;

    case PRIOT_NOSUCHOBJECT:
    case PRIOT_NOSUCHINSTANCE:
    case PRIOT_ENDOFMIBVIEW:
        rc = Asn01_reallocRbuildNull(pkt, pkt_len, offset, allow_realloc,
                                     var_val_type);
        break;

    case ASN01_OPAQUE_FLOAT:
        rc = Asn01_reallocRbuildFloat(pkt, pkt_len, offset, allow_realloc,
                                      var_val_type, (float *) var_val,
                                      var_val_len);
        break;

    case ASN01_OPAQUE_DOUBLE:
        rc = Asn01_reallocRbuildDouble(pkt, pkt_len, offset, allow_realloc,
                                       var_val_type, (double *) var_val,
                                       var_val_len);
        break;

    case ASN01_OPAQUE_I64:
        rc = Asn01_reallocRbuildSignedInt64(pkt, pkt_len, offset,
                                             allow_realloc, var_val_type,
                                             (Counter64 *) var_val,
                                             var_val_len);
        break;
    default:
    {
    char error_buf[64];
    snprintf(error_buf, sizeof(error_buf),
        "wrong type in snmp_realloc_rbuild_var_op: %d", var_val_type);
        IMPL_ERROR_MSG(error_buf);
        rc = 0;
    }
    }
    DEBUG_INDENTLESS();

    if (rc == 0) {
        return 0;
    }

    /*
     * Build the OID.
     */

    DEBUG_DUMPHEADER("send", "Name");
    rc = Asn01_reallocRbuildObjid(pkt, pkt_len, offset, allow_realloc,
                                  (u_char) (ASN01_UNIVERSAL | ASN01_PRIMITIVE |
                                            ASN01_OBJECT_ID), var_name,
                                  *var_name_len);
    DEBUG_INDENTLESS();
    if (rc == 0) {
        IMPL_ERROR_MSG("Can't build OID for variable");
        return 0;
    }

    /*
     * Build the sequence header.
     */

    rc = Asn01_reallocRbuildSequence(pkt, pkt_len, offset, allow_realloc,
                                     (u_char) (ASN01_SEQUENCE |
                                               ASN01_CONSTRUCTOR),
                                     *offset - start_offset);
    return rc;
}

