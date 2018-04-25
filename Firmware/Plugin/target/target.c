
#include "target.h"
#include "Debug.h"
#include "Impl.h"
#include "Logger.h"
#include "ReadConfig.h"
#include "Tc.h"
#include "snmpTargetAddrEntry.h"
#include "snmpTargetParamsEntry.h"

#define MAX_TAGS 128

Types_Session* get_target_sessions( char* taglist, TargetFilterFunction* filterfunct,
    void* filterArg )
{
    Types_Session *ret = NULL, thissess;
    struct targetAddrTable_struct* targaddrs;
    char buf[ IMPL_SPRINT_MAX_LEN ];
    char tags[ MAX_TAGS ][ IMPL_SPRINT_MAX_LEN ], *cp;
    int numtags = 0, i;

    static struct targetParamTable_struct* param;

    DEBUG_MSGTL( ( "target_sessions", "looking for: %s\n", taglist ) );
    for ( cp = taglist; cp && numtags < MAX_TAGS; ) {
        cp = ReadConfig_copyNword( cp, tags[ numtags ], sizeof( tags[ numtags ] ) );
        DEBUG_MSGTL( ( "target_sessions", " for: %d=%s\n", numtags,
            tags[ numtags ] ) );
        numtags++;
    }

    for ( targaddrs = get_addrTable(); targaddrs;
          targaddrs = targaddrs->next ) {

        /*
         * legal row? 
         */
        if ( targaddrs->tDomain == NULL || targaddrs->tAddress == NULL || targaddrs->rowStatus != PRIOT_ROW_ACTIVE ) {
            DEBUG_MSGTL( ( "target_sessions", "  which is not ready yet\n" ) );
            continue;
        }

        if ( Transport_tdomainSupport( targaddrs->tDomain, targaddrs->tDomainLen, NULL, NULL ) == 0 ) {
            Logger_log( LOGGER_PRIORITY_ERR,
                "unsupported domain for target address table entry %s\n",
                targaddrs->nameData );
        }

        /*
         * check tag list to see if we match 
         */
        if ( targaddrs->tagList ) {
            int matched = 0;

            /*
             * loop through tag list looking for requested tags 
             */
            for ( cp = targaddrs->tagList; cp && !matched; ) {
                cp = ReadConfig_copyNword( cp, buf, sizeof( buf ) );
                for ( i = 0; i < numtags && !matched; i++ ) {
                    if ( strcmp( buf, tags[ i ] ) == 0 ) {
                        /*
                         * found a valid target table entry 
                         */
                        DEBUG_MSGTL( ( "target_sessions", "found one: %s\n",
                            tags[ i ] ) );

                        if ( targaddrs->params ) {
                            param = get_paramEntry( targaddrs->params );
                            if ( !param
                                || param->rowStatus != PRIOT_ROW_ACTIVE ) {
                                /*
                                 * parameter entry must exist and be active 
                                 */
                                continue;
                            }
                        } else {
                            /*
                             * parameter entry must be specified 
                             */
                            continue;
                        }

                        /*
                         * last chance for caller to opt-out.  Call
                         * filtering function 
                         */
                        if ( filterfunct && ( *( filterfunct ) )( targaddrs, param,
                                                filterArg ) ) {
                            continue;
                        }

                        /*
                         * Only one notification per TargetAddrEntry,
                         * rather than one per tag
                         */
                        matched = 1;

                        if ( targaddrs->storageType != TC_ST_READONLY && targaddrs->sess && param->updateTime >= targaddrs->sessionCreationTime ) {
                            /*
                             * parameters have changed, nuke the old session 
                             */
                            Api_close( targaddrs->sess );
                            targaddrs->sess = NULL;
                        }

                        /*
                         * target session already exists? 
                         */
                        if ( targaddrs->sess == NULL ) {
                            /*
                             * create an appropriate snmp session and add
                             * it to our return list 
                             */
                            Transport_Transport* t = NULL;

                            t = Transport_tdomainTransportOid( targaddrs->tDomain,
                                targaddrs->tDomainLen,
                                targaddrs->tAddress,
                                targaddrs->tAddressLen,
                                0 );
                            if ( t == NULL ) {
                                DEBUG_MSGTL( ( "target_sessions",
                                    "bad dest \"" ) );
                                DEBUG_MSGOID( ( "target_sessions",
                                    targaddrs->tDomain,
                                    targaddrs->tDomainLen ) );
                                DEBUG_MSG( ( "target_sessions", "\", \"" ) );
                                DEBUG_MSGHEX( ( "target_sessions",
                                    targaddrs->tAddress,
                                    targaddrs->tAddressLen ) );
                                DEBUG_MSG( ( "target_sessions", "\n" ) );
                                continue;
                            } else {
                                char* dst_str = t->f_fmtaddr( t, NULL, 0 );
                                if ( dst_str != NULL ) {
                                    DEBUG_MSGTL( ( "target_sessions",
                                        "  to: %s\n", dst_str ) );
                                    free( dst_str );
                                }
                            }

                            memset( &thissess, 0, sizeof( thissess ) );
                            thissess.timeout = ( targaddrs->timeout ) * 10000;
                            thissess.retries = targaddrs->retryCount;
                            DEBUG_MSGTL( ( "target_sessions",
                                "timeout: %d -> %ld\n",
                                targaddrs->timeout,
                                thissess.timeout ) );

                            if ( param->mpModel == PRIOT_VERSION_3 && param->secModel != PRIOT_SEC_MODEL_USM && param->secModel != PRIOT_SEC_MODEL_TSM ) {
                                Logger_log( LOGGER_PRIORITY_ERR,
                                    "unsupported mpModel/secModel combo %d/%d for target %s\n",
                                    param->mpModel, param->secModel,
                                    targaddrs->nameData );
                                /*
                                 * XXX: memleak 
                                 */
                                Transport_free( t );
                                continue;
                            }
                            thissess.paramName = strdup( param->paramName );
                            thissess.version = param->mpModel;
                            if ( param->mpModel == PRIOT_VERSION_3 ) {
                                thissess.securityName = strdup( param->secName );
                                thissess.securityNameLen = strlen( thissess.securityName );
                                thissess.securityLevel = param->secLevel;
                                thissess.securityModel = param->secModel;
                            }

                            thissess.flags |= API_FLAGS_DONT_PROBE;
                            targaddrs->sess = Api_add( &thissess, t,
                                NULL, NULL );
                            thissess.flags &= ~API_FLAGS_DONT_PROBE;
                            targaddrs->sessionCreationTime = time( NULL );
                        }
                        if ( targaddrs->sess ) {
                            if ( NULL == targaddrs->sess->paramName )
                                targaddrs->sess->paramName = strdup( param->paramName );

                            targaddrs->sess->next = ret;
                            ret = targaddrs->sess;
                        } else {
                            Api_sessPerror( "target session", &thissess );
                        }
                    }
                }
            }
        }
    }
    return ret;
}
