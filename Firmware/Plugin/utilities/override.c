/** allows overriding of a given oid with a new type and value */

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

#include "override.h"
#include "AgentReadConfig.h"
#include "Client.h"
#include "System/Util/Trace.h"
#include "Instance.h"
#include "System/Util/Logger.h"
#include "Mib.h"
#include "ReadConfig.h"

typedef struct override_data_s {
    int type;
    void* value;
    size_t value_len;
    void* set_space;
    size_t set_len;
} override_data;

/** @todo: (optionally) save values persistently when configured for
 *  read-write */
int override_handler( MibHandler* handler,
    HandlerRegistration* reginfo,
    AgentRequestInfo* reqinfo,
    RequestInfo* requests )
{

    override_data* data = ( override_data* )handler->myvoid;
    void* tmpptr;

    if ( !data ) {
        Agent_setRequestError( reqinfo, requests, PRIOT_ERR_GENERR );
        return PRIOT_ERR_NOERROR;
    }

    switch ( reqinfo->mode ) {
    case MODE_GET:
        DEBUG_MSGTL( ( "override", "overriding oid " ) );
        DEBUG_MSGOID( ( "override", requests->requestvb->name,
            requests->requestvb->nameLength ) );
        DEBUG_MSG( ( "override", "\n" ) );
        Client_setVarTypedValue( requests->requestvb, ( u_char )data->type,
            ( u_char* )data->value, data->value_len );
        break;

    case MODE_SET_RESERVE1:
        if ( requests->requestvb->type != data->type )
            Agent_setRequestError( reqinfo, requests, PRIOT_ERR_WRONGTYPE );
        break;

    case MODE_SET_RESERVE2:
        data->set_space = Memory_memdup( requests->requestvb->value.string,
            requests->requestvb->valueLength );
        if ( !data->set_space )
            Agent_setRequestError( reqinfo, requests,
                PRIOT_ERR_RESOURCEUNAVAILABLE );
        break;

    case MODE_SET_FREE:
        MEMORY_FREE( data->set_space );
        break;

    case MODE_SET_ACTION:
        /* swap the values in */
        tmpptr = data->value;
        data->value = data->set_space;
        data->set_space = tmpptr;

        /* set the lengths */
        data->set_len = data->value_len;
        data->value_len = requests->requestvb->valueLength;
        break;

    case MODE_SET_UNDO:
        MEMORY_FREE( data->value );
        data->value = data->set_space;
        data->value_len = data->set_len;
        break;

    case MODE_SET_COMMIT:
        MEMORY_FREE( data->set_space );
        break;

    default:
        Logger_log( LOGGER_PRIORITY_ERR, "unsupported mode in override handler\n" );
        Agent_setRequestError( reqinfo, requests, PRIOT_ERR_GENERR );
        return PRIOT_ERR_GENERR;
    }
    return PRIOT_ERR_NOERROR;
}

#define MALLOC_OR_DIE( x )                                      \
    thedata->value = malloc( x );                               \
    thedata->value_len = x;                                     \
    if ( !thedata->value ) {                                    \
        free( thedata );                                        \
        ReadConfig_configPerror( "memory allocation failure" ); \
        return;                                                 \
    }

void netsnmp_parse_override( const char* token, char* line )
{
    char* cp;
    char buf[ UTILITIES_MAX_BUFFER ], namebuf[ UTILITIES_MAX_BUFFER ];
    int readwrite = 0;
    oid oidbuf[ ASN01_MAX_OID_LEN ];
    size_t oidbuf_len = ASN01_MAX_OID_LEN;
    int type;
    override_data* thedata;
    HandlerRegistration* the_reg;

    cp = ReadConfig_copyNword( line, namebuf, sizeof( namebuf ) - 1 );
    if ( strcmp( namebuf, "-rw" ) == 0 ) {
        readwrite = 1;
        cp = ReadConfig_copyNword( cp, namebuf, sizeof( namebuf ) - 1 );
    }

    if ( !cp ) {
        ReadConfig_configPerror( "no oid specified" );
        return;
    }

    if ( !Mib_parseOid( namebuf, oidbuf, &oidbuf_len ) ) {
        ReadConfig_configPerror( "illegal oid" );
        return;
    }
    cp = ReadConfig_copyNword( cp, buf, sizeof( buf ) - 1 );

    if ( !cp && strcmp( buf, "null" ) != 0 ) {
        ReadConfig_configPerror( "no variable value specified" );
        return;
    }

    {
        struct {
            const char* key;
            int value;
        } const strings[] = {
            { "counter", ASN01_COUNTER },
            { "counter64", ASN01_COUNTER64 },
            { "integer", ASN01_INTEGER },
            { "ipaddress", ASN01_IPADDRESS },
            { "nsap", ASN01_NSAP },
            { "null", ASN01_NULL },
            { "object_id", ASN01_OBJECT_ID },
            { "octet_str", ASN01_OCTET_STR },
            { "opaque", ASN01_OPAQUE },
            { "opaque_counter64", ASN01_OPAQUE_COUNTER64 },
            { "opaque_double", ASN01_OPAQUE_DOUBLE },
            { "opaque_float", ASN01_OPAQUE_FLOAT },
            { "opaque_i64", ASN01_OPAQUE_I64 },
            { "opaque_u64", ASN01_OPAQUE_U64 },
            { "timeticks", ASN01_TIMETICKS },
            { "uinteger", ASN01_GAUGE },
            { "unsigned", ASN01_UNSIGNED },
            { NULL, 0 }
        },
                *run;
        for ( run = strings; run->key && strcasecmp( run->key, buf ) < 0; ++run )
            ;
        if ( run->key && strcasecmp( run->key, buf ) == 0 )
            type = run->value;
        else {
            ReadConfig_configPerror( "unknown type specified" );
            return;
        }
    }

    if ( cp )
        ReadConfig_copyNword( cp, buf, sizeof( buf ) - 1 );
    else
        buf[ 0 ] = 0;

    thedata = MEMORY_MALLOC_TYPEDEF( override_data );
    if ( !thedata ) {
        ReadConfig_configPerror( "memory allocation failure" );
        return;
    }
    thedata->type = type;

    switch ( type ) {
    case ASN01_INTEGER:
        MALLOC_OR_DIE( sizeof( long ) );
        *( ( long* )thedata->value ) = strtol( buf, NULL, 0 );
        break;

    case ASN01_COUNTER:
    case ASN01_TIMETICKS:
    case ASN01_UNSIGNED:
        MALLOC_OR_DIE( sizeof( u_long ) );
        *( ( u_long* )thedata->value ) = strtoul( buf, NULL, 0 );
        break;

    case ASN01_OCTET_STR:
    case ASN01_BIT_STR:
        if ( buf[ 0 ] == '0' && buf[ 1 ] == 'x' ) {
            /*
             * hex 
             */
            thedata->value_len = Convert_hexStringToBinaryString2( ( u_char* )( buf + 2 ), strlen( buf ) - 2,
                ( char** )&thedata->value );
        } else {
            thedata->value = strdup( buf );
            thedata->value_len = strlen( buf );
        }
        break;

    case ASN01_OBJECT_ID:
        ReadConfig_readObjid( buf, ( oid** )&thedata->value,
            &thedata->value_len );
        /* We need the size of the value in bytes, not in oids */
        thedata->value_len *= sizeof( oid );
        break;

    case ASN01_NULL:
        thedata->value_len = 0;
        break;

    default:
        MEMORY_FREE( thedata );
        ReadConfig_configPerror( "illegal/unsupported type specified" );
        return;
    }

    if ( !thedata->value && thedata->type != ASN01_NULL ) {
        ReadConfig_configPerror( "memory allocation failure" );
        free( thedata );
        return;
    }

    the_reg = MEMORY_MALLOC_TYPEDEF( HandlerRegistration );
    if ( !the_reg ) {
        ReadConfig_configPerror( "memory allocation failure" );
        free( thedata );
        return;
    }

    the_reg->handlerName = strdup( namebuf );
    the_reg->priority = 255;
    the_reg->modes = ( readwrite ) ? HANDLER_CAN_RWRITE : HANDLER_CAN_RONLY;
    the_reg->handler = AgentHandler_createHandler( "override", override_handler );
    the_reg->rootoid = Api_duplicateObjid( oidbuf, oidbuf_len );
    the_reg->rootoid_len = oidbuf_len;
    if ( !the_reg->rootoid || !the_reg->handler || !the_reg->handlerName ) {
        if ( the_reg->handler )
            MEMORY_FREE( the_reg->handler->handler_name );
        MEMORY_FREE( the_reg->handler );
        MEMORY_FREE( the_reg->handlerName );
        MEMORY_FREE( the_reg );
        ReadConfig_configPerror( "memory allocation failure" );
        free( thedata );
        return;
    }
    the_reg->handler->myvoid = thedata;

    if ( Instance_registerInstance( the_reg ) ) {
        ReadConfig_configPerror( "oid registration failed within the agent" );
        MEMORY_FREE( thedata->value );
        free( thedata );
        return;
    }
}

void init_override( void )
{
    AgentReadConfig_priotdRegisterConfigHandler( "override", netsnmp_parse_override, NULL, /* XXX: free func */
        "[-rw] mibnode type value" );
}
