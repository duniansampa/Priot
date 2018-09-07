#include "usmUser.h"
#include "AgentRegistry.h"
#include "Api.h"
#include "GetStatistic.h"
#include "Impl.h"
#include "System/Security/KeyTools.h"
#include "TextualConvention.h"
#include "VarStruct.h"
#include "usmUser.h"
#include "utilities/header_generic.h"

#include <stdlib.h>
#include <string.h>

int usmStatusCheck( struct Usm_User_s* uptr );

struct Variable4_s usmUser_variables[] = {
    { USMUSERSPINLOCK, asnINTEGER, IMPL_OLDAPI_RWRITE,
        var_usmUser, 1, { 1 } },
    { USMUSERSECURITYNAME, asnOCTET_STR, IMPL_OLDAPI_RONLY,
        var_usmUser, 3, { 2, 1, 3 } },
    { USMUSERCLONEFROM, asnOBJECT_ID, IMPL_OLDAPI_RWRITE,
        var_usmUser, 3, { 2, 1, 4 } },
    { USMUSERAUTHPROTOCOL, asnOBJECT_ID, IMPL_OLDAPI_RWRITE,
        var_usmUser, 3, { 2, 1, 5 } },
    { USMUSERAUTHKEYCHANGE, asnOCTET_STR, IMPL_OLDAPI_RWRITE,
        var_usmUser, 3, { 2, 1, 6 } },
    { USMUSEROWNAUTHKEYCHANGE, asnOCTET_STR, IMPL_OLDAPI_RWRITE,
        var_usmUser, 3, { 2, 1, 7 } },
    { USMUSERPRIVPROTOCOL, asnOBJECT_ID, IMPL_OLDAPI_RWRITE,
        var_usmUser, 3, { 2, 1, 8 } },
    { USMUSERPRIVKEYCHANGE, asnOCTET_STR, IMPL_OLDAPI_RWRITE,
        var_usmUser, 3, { 2, 1, 9 } },
    { USMUSEROWNPRIVKEYCHANGE, asnOCTET_STR, IMPL_OLDAPI_RWRITE,
        var_usmUser, 3, { 2, 1, 10 } },
    { USMUSERPUBLIC, asnOCTET_STR, IMPL_OLDAPI_RWRITE,
        var_usmUser, 3, { 2, 1, 11 } },
    { USMUSERSTORAGETYPE, asnINTEGER, IMPL_OLDAPI_RWRITE,
        var_usmUser, 3, { 2, 1, 12 } },
    { USMUSERSTATUS, asnINTEGER, IMPL_OLDAPI_RWRITE,
        var_usmUser, 3, { 2, 1, 13 } },

};

oid usmUser_variables_oid[] = { 1, 3, 6, 1, 6, 3, 15, 1, 2 };

/*
 * needed for the write_ functions to find the start of the index 
 */
#define USM_MIB_LENGTH 12

static unsigned int usmUserSpinLock = 0;

void init_usmUser( void )
{
    REGISTER_MIB( "snmpv3/usmUser", usmUser_variables, Variable4_s,
        usmUser_variables_oid );
}

void init_register_usmUser_context( const char* contextName )
{
    AgentRegistry_registerMibContext( "snmpv3/usmUser",
        ( struct Variable_s* )usmUser_variables,
        sizeof( struct Variable4_s ),
        sizeof( usmUser_variables ) / sizeof( struct Variable4_s ),
        usmUser_variables_oid,
        sizeof( usmUser_variables_oid ) / sizeof( oid ),
        DEFAULT_MIB_PRIORITY, 0, 0, NULL,
        contextName, -1, 0 );
}

/*******************************************************************-o-******
 * usm_generate_OID
 *
 * Parameters:
 *	*prefix		(I) OID prefix to the usmUser table entry.
 *	 prefixLen	(I)
 *	*uptr		(I) Pointer to a user in the user list.
 *	*length		(O) Length of generated index OID.
 *      
 * Returns:
 *	Pointer to the OID index for the user (uptr)  -OR-
 *	NULL on failure.
 *
 *
 * Generate the index OID for a given usmUser name.  'length' is set to
 * the length of the index OID.
 *
 * Index OID format is:
 *
 *    <...prefix>.<engineID_length>.<engineID>.<user_name_length>.<user_name>
 */
oid* usm_generate_OID( oid* prefix, size_t prefixLen, struct Usm_User_s* uptr,
    size_t* length )
{
    oid* indexOid;
    int i;

    *length = 2 + uptr->engineIDLen + strlen( uptr->name ) + prefixLen;
    indexOid = ( oid* )malloc( *length * sizeof( oid ) );
    if ( indexOid ) {
        memmove( indexOid, prefix, prefixLen * sizeof( oid ) );

        indexOid[ prefixLen ] = uptr->engineIDLen;
        for ( i = 0; i < ( int )uptr->engineIDLen; i++ )
            indexOid[ prefixLen + 1 + i ] = ( oid )uptr->engineID[ i ];

        indexOid[ prefixLen + uptr->engineIDLen + 1 ] = strlen( uptr->name );
        for ( i = 0; i < ( int )strlen( uptr->name ); i++ )
            indexOid[ prefixLen + uptr->engineIDLen + 2 + i ] = ( oid )uptr->name[ i ];
    }
    return indexOid;

} /* end usm_generate_OID() */

/*
 * usm_parse_oid(): parses an index to the usmTable to break it down into
 * a engineID component and a name component.  The results are stored in:
 * 
 * **engineID:   a newly malloced string.
 * *engineIDLen: The length of the malloced engineID string above.
 * **name:       a newly malloced string.
 * *nameLen:     The length of the malloced name string above.
 * 
 * returns 1 if an error is encountered, or 0 if successful.
 */
int usm_parse_oid( oid* oidIndex, size_t oidLen,
    unsigned char** engineID, size_t* engineIDLen,
    unsigned char** name, size_t* nameLen )
{
    int nameL;
    int engineIDL;
    int i;

    /*
     * first check the validity of the oid 
     */
    if ( ( oidLen <= 0 ) || ( !oidIndex ) ) {
        DEBUG_MSGTL( ( "usmUser",
            "parse_oid: null oid or zero length oid passed in\n" ) );
        return 1;
    }
    engineIDL = *oidIndex; /* initial engineID length */
    if ( ( int )oidLen < engineIDL + 2 ) {
        DEBUG_MSGTL( ( "usmUser",
            "parse_oid: invalid oid length: less than the engineIDLen\n" ) );
        return 1;
    }
    nameL = oidIndex[ engineIDL + 1 ]; /* the initial name length */
    if ( ( int )oidLen != engineIDL + nameL + 2 ) {
        DEBUG_MSGTL( ( "usmUser",
            "parse_oid: invalid oid length: length is not exact\n" ) );
        return 1;
    }

    /*
     * its valid, malloc the space and store the results 
     */
    if ( engineID == NULL || name == NULL ) {
        DEBUG_MSGTL( ( "usmUser",
            "parse_oid: null storage pointer passed in.\n" ) );
        return 1;
    }

    *engineID = ( unsigned char* )malloc( engineIDL );
    if ( *engineID == NULL ) {
        DEBUG_MSGTL( ( "usmUser",
            "parse_oid: malloc of the engineID failed\n" ) );
        return 1;
    }
    *engineIDLen = engineIDL;

    *name = ( unsigned char* )malloc( nameL + 1 );
    if ( *name == NULL ) {
        DEBUG_MSGTL( ( "usmUser", "parse_oid: malloc of the name failed\n" ) );
        free( *engineID );
        return 1;
    }
    *nameLen = nameL;

    for ( i = 0; i < engineIDL; i++ ) {
        if ( oidIndex[ i + 1 ] > 255 ) {
            goto UPO_parse_error;
        }
        engineID[ 0 ][ i ] = ( unsigned char )oidIndex[ i + 1 ];
    }

    for ( i = 0; i < nameL; i++ ) {
        if ( oidIndex[ i + 2 + engineIDL ] > 255 ) {
        UPO_parse_error:
            free( *engineID );
            free( *name );
            return 1;
        }
        name[ 0 ][ i ] = ( unsigned char )oidIndex[ i + 2 + engineIDL ];
    }
    name[ 0 ][ nameL ] = 0;

    return 0;

} /* end usm_parse_oid() */

/*******************************************************************-o-******
 * Usm_parseUser
 *
 * Parameters:
 *	*name		Complete OID indexing a given usmUser entry.
 *	 name_length
 *      
 * Returns:
 *	Pointer to a usmUser  -OR-
 *	NULL if name does not convert to a usmUser.
 * 
 * Convert an (full) OID and return a pointer to a matching user in the
 * user list if one exists.
 */
struct Usm_User_s*
Usm_parseUser( oid* name, size_t name_len )
{
    struct Usm_User_s* uptr;

    char* newName;
    u_char* engineID;
    size_t nameLen, engineIDLen;

    /*
     * get the name and engineID out of the incoming oid 
     */
    if ( usm_parse_oid( &name[ USM_MIB_LENGTH ], name_len - USM_MIB_LENGTH,
             &engineID, &engineIDLen, ( u_char** )&newName,
             &nameLen ) )
        return NULL;

    /*
     * Now see if a user exists with these index values 
     */
    uptr = Usm_getUser( engineID, engineIDLen, newName );
    free( engineID );
    free( newName );

    return uptr;

} /* end Usm_parseUser() */

/*******************************************************************-o-******
 * var_usmUser
 *
 * Parameters:
 *	  *vp	   (I)     Variable-binding associated with this action.
 *	  *name	   (I/O)   Input name requested, output name found.
 *	  *length  (I/O)   Length of input and output oid's.
 *	   exact   (I)     TRUE if an exact match was requested.
 *	  *var_len (O)     Length of variable or 0 if function returned.
 *	(**write_method)   Hook to name a write method (UNUSED).
 *      
 * Returns:
 *	Pointer to (char *) containing related data of length 'length'
 *	  (May be NULL.)
 *
 *
 * Call-back function passed to the agent in order to return information
 * for the USM MIB tree.
 *
 *
 * If this invocation is not for USMUSERSPINLOCK, lookup user name
 * in the usmUser list.
 *
 * If the name does not match any user and the request
 * is for an exact match, -or- if the usmUser list is empty, create a 
 * new list entry.
 *
 * Finally, service the given USMUSER* var-bind.  A NULL user generally
 * results in a NULL return value.
 */
u_char*
var_usmUser( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
    struct Usm_User_s *uptr = NULL, *nptr;
    int i, rtest, result;
    oid* indexOid;
    size_t len;

    /*
     * variables we may use later 
     */
    static long long_ret;
    static u_char string[ 1 ];
    static oid objid[ 2 ]; /* for .0.0 */

    if ( !vp || !name || !length || !var_len )
        return NULL;

    /* assume it isnt writable for the time being */
    *write_method = ( WriteMethodFT* )0;

    /* assume an integer and change later if not */
    *var_len = sizeof( long_ret );

    if ( vp->magic != USMUSERSPINLOCK ) {
        oid newname[ TYPES_MAX_OID_LEN ];
        len = ( *length < vp->namelen ) ? *length : vp->namelen;
        rtest = Api_oidCompare( name, len, vp->name, len );
        if ( rtest > 0 ||
            /*
             * (rtest == 0 && !exact && (int) vp->namelen+1 < (int) *length) || 
             */
            ( exact == 1 && rtest != 0 ) ) {
            if ( var_len )
                *var_len = 0;
            return NULL;
        }
        memset( newname, 0, sizeof( newname ) );
        if ( ( ( int )*length ) <= ( int )vp->namelen || rtest == -1 ) {
            /*
             * oid is not within our range yet 
             */
            /*
             * need to fail if not exact 
             */
            uptr = Usm_getUserList();

        } else {
            for ( nptr = Usm_getUserList(), uptr = NULL;
                  nptr != NULL; nptr = nptr->next ) {
                indexOid = usm_generate_OID( vp->name, vp->namelen, nptr, &len );
                result = Api_oidCompare( name, *length, indexOid, len );
                DEBUG_MSGTL( ( "usmUser", "Checking user: %s - ",
                    nptr->name ) );
                for ( i = 0; i < ( int )nptr->engineIDLen; i++ ) {
                    DEBUG_MSG( ( "usmUser", " %x", nptr->engineID[ i ] ) );
                }
                DEBUG_MSG( ( "usmUser", " - %d \n  -> OID: ", result ) );
                DEBUG_MSGOID( ( "usmUser", indexOid, len ) );
                DEBUG_MSG( ( "usmUser", "\n" ) );

                free( indexOid );

                if ( exact ) {
                    if ( result == 0 ) {
                        uptr = nptr;
                    }
                } else {
                    if ( result == 0 ) {
                        /*
                         * found an exact match.  Need the next one for !exact 
                         */
                        uptr = nptr->next;
                    } else if ( result == -1 ) {
                        uptr = nptr;
                        break;
                    }
                }
            }
        } /* endif -- name <= vp->name */

        /*
         * if uptr is NULL and exact we need to continue for creates 
         */
        if ( uptr == NULL && !exact )
            return ( NULL );

        if ( uptr ) {
            indexOid = usm_generate_OID( vp->name, vp->namelen, uptr, &len );
            *length = len;
            memmove( name, indexOid, len * sizeof( oid ) );

            DEBUG_MSGTL( ( "usmUser", "Found user: %s - ", uptr->name ) );
            for ( i = 0; i < ( int )uptr->engineIDLen; i++ ) {
                DEBUG_MSG( ( "usmUser", " %x", uptr->engineID[ i ] ) );
            }
            DEBUG_MSG( ( "usmUser", "\n  -> OID: " ) );
            DEBUG_MSGOID( ( "usmUser", indexOid, len ) );
            DEBUG_MSG( ( "usmUser", "\n" ) );

            free( indexOid );
        }
    } else {
        if ( header_generic( vp, name, length, exact, var_len, write_method ) )
            return NULL;
    } /* endif -- vp->magic != USMUSERSPINLOCK */

    switch ( vp->magic ) {
    case USMUSERSPINLOCK:
        *write_method = write_usmUserSpinLock;
        long_ret = usmUserSpinLock;
        return ( unsigned char* )&long_ret;

    case USMUSERSECURITYNAME:
        if ( uptr ) {
            *var_len = strlen( uptr->secName );
            return ( unsigned char* )uptr->secName;
        }
        return NULL;

    case USMUSERCLONEFROM:
        *write_method = write_usmUserCloneFrom;
        if ( uptr ) {
            objid[ 0 ] = 0; /* "When this object is read, the ZeroDotZero OID */
            objid[ 1 ] = 0; /*  is returned." */
            *var_len = sizeof( oid ) * 2;
            return ( unsigned char* )objid;
        }
        return NULL;

    case USMUSERAUTHPROTOCOL:
        *write_method = write_usmUserAuthProtocol;
        if ( uptr ) {
            *var_len = uptr->authProtocolLen * sizeof( oid );
            return ( u_char* )uptr->authProtocol;
        }
        return NULL;

    case USMUSERAUTHKEYCHANGE:
    case USMUSEROWNAUTHKEYCHANGE:
        /*
         * we treat these the same, and let the calling module
         * distinguish between them 
         */
        *write_method = write_usmUserAuthKeyChange;
        if ( uptr ) {
            *string = 0; /* always return a NULL string */
            *var_len = 0;
            return string;
        }
        return NULL;

    case USMUSERPRIVPROTOCOL:
        *write_method = write_usmUserPrivProtocol;
        if ( uptr ) {
            *var_len = uptr->privProtocolLen * sizeof( oid );
            return ( u_char* )uptr->privProtocol;
        }
        return NULL;

    case USMUSERPRIVKEYCHANGE:
    case USMUSEROWNPRIVKEYCHANGE:
        /*
         * we treat these the same, and let the calling module
         * distinguish between them 
         */
        *write_method = write_usmUserPrivKeyChange;
        if ( uptr ) {
            *string = 0; /* always return a NULL string */
            *var_len = 0;
            return string;
        }
        return NULL;

    case USMUSERPUBLIC:
        *write_method = write_usmUserPublic;
        if ( uptr ) {
            if ( uptr->userPublicString ) {
                *var_len = uptr->userPublicStringLen;
                return uptr->userPublicString;
            }
            *string = 0;
            *var_len = 0; /* return an empty string if the public
                                 * string hasn't been defined yet */
            return string;
        }
        return NULL;

    case USMUSERSTORAGETYPE:
        *write_method = write_usmUserStorageType;
        if ( uptr ) {
            long_ret = uptr->userStorageType;
            return ( unsigned char* )&long_ret;
        }
        return NULL;

    case USMUSERSTATUS:
        *write_method = write_usmUserStatus;
        if ( uptr ) {
            long_ret = uptr->userStatus;
            return ( unsigned char* )&long_ret;
        }
        return NULL;
    default:
        DEBUG_MSGTL( ( "priotd", "unknown sub-id %d in var_usmUser\n",
            vp->magic ) );
    }
    return NULL;

} /* end var_usmUser() */

/*
 * write_usmUserSpinLock(): called when a set is performed on the
 * usmUserSpinLock object 
 */
int write_usmUserSpinLock( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    /*
     * variables we may use later 
     */
    static long long_ret;

    if ( var_val_type != asnINTEGER ) {
        DEBUG_MSGTL( ( "usmUser",
            "write to usmUserSpinLock not ASN01_INTEGER\n" ) );
        return PRIOT_ERR_WRONGTYPE;
    }
    if ( var_val_len > sizeof( long_ret ) ) {
        DEBUG_MSGTL( ( "usmUser", "write to usmUserSpinLock: bad length\n" ) );
        return PRIOT_ERR_WRONGLENGTH;
    }
    long_ret = *( ( long* )var_val );
    if ( long_ret != ( long )usmUserSpinLock )
        return PRIOT_ERR_INCONSISTENTVALUE;
    if ( action == IMPL_COMMIT ) {
        if ( usmUserSpinLock == 2147483647 )
            usmUserSpinLock = 0;
        else
            usmUserSpinLock++;
    }
    return PRIOT_ERR_NOERROR;
} /* end write_usmUserSpinLock() */

/*******************************************************************-o-******
 * write_usmUserCloneFrom
 *
 * Parameters:
 *	 action
 *	*var_val
 *	 var_val_type
 *	 var_val_len
 *	*statP		(UNUSED)
 *	*name		OID of user to clone from.
 *	 name_len
 *      
 * Returns:
 *	PRIOT_ERR_NOERROR		On success  -OR-  If user exists
 *					  and has already been cloned.
 *	PRIOT_ERR_GENERR			Local function call failures.
 *	PRIOT_ERR_INCONSISTENTNAME	'name' does not exist in user list
 *					  -OR-  user to clone from != TC_RS_ACTIVE.
 *	PRIOT_ERR_WRONGLENGTH		OID length > than local buffer size.
 *	PRIOT_ERR_WRONGTYPE		ASN01_OBJECT_ID is wrong.
 *
 *
 * XXX:  should handle action=IMPL_UNDO's.
 */
int write_usmUserCloneFrom( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    struct Usm_User_s *uptr, *cloneFrom;

    if ( action == IMPL_RESERVE1 ) {
        if ( var_val_type != asnOBJECT_ID ) {
            DEBUG_MSGTL( ( "usmUser",
                "write to usmUserCloneFrom not ASN01_OBJECT_ID\n" ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len > USM_LENGTH_OID_MAX * sizeof( oid ) || var_val_len % sizeof( oid ) != 0 ) {
            DEBUG_MSGTL( ( "usmUser",
                "write to usmUserCloneFrom: bad length\n" ) );
            return PRIOT_ERR_WRONGLENGTH;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) == NULL ) {
            /*
             * We don't allow creations here.  
             */
            return PRIOT_ERR_INCONSISTENTNAME;
        }

        /*
         * Has the user already been cloned?  If so, writes to this variable
         * are defined to have no effect and to produce no error.  
         */
        if ( uptr->cloneFrom != NULL ) {
            return PRIOT_ERR_NOERROR;
        }

        cloneFrom = Usm_parseUser( ( oid* )var_val, var_val_len / sizeof( oid ) );
        if ( cloneFrom == NULL || cloneFrom->userStatus != PRIOT_ROW_ACTIVE ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        }
        uptr->cloneFrom = Api_duplicateObjid( ( oid* )var_val,
            var_val_len / sizeof( oid ) );
        Usm_cloneFromUser( cloneFrom, uptr );

        if ( usmStatusCheck( uptr ) && uptr->userStatus == PRIOT_ROW_NOTREADY ) {
            uptr->userStatus = PRIOT_ROW_NOTINSERVICE;
        }
    }

    return PRIOT_ERR_NOERROR;
}

/*******************************************************************-o-******
 * write_usmUserAuthProtocol
 *
 * Parameters:
 *	 action
 *	*var_val	OID of auth transform to set.
 *	 var_val_type
 *	 var_val_len
 *	*statP
 *	*name		OID of user upon which to perform set operation.
 *	 name_len
 *      
 * Returns:
 *	PRIOT_ERR_NOERROR		On success.
 *	PRIOT_ERR_GENERR
 *	PRIOT_ERR_INCONSISTENTVALUE
 *	PRIOT_ERR_NOSUCHNAME
 *	PRIOT_ERR_WRONGLENGTH
 *	PRIOT_ERR_WRONGTYPE
 */
int write_usmUserAuthProtocol( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    static oid* optr;
    static size_t olen;
    static int resetOnFail;
    struct Usm_User_s* uptr;

    if ( action == IMPL_RESERVE1 ) {
        resetOnFail = 0;
        if ( var_val_type != asnOBJECT_ID ) {
            DEBUG_MSGTL( ( "usmUser",
                "write to usmUserAuthProtocol not ASN01_OBJECT_ID\n" ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len > USM_LENGTH_OID_MAX * sizeof( oid ) || var_val_len % sizeof( oid ) != 0 ) {
            DEBUG_MSGTL( ( "usmUser",
                "write to usmUserAuthProtocol: bad length\n" ) );
            return PRIOT_ERR_WRONGLENGTH;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        }

        if ( uptr->userStatus == tcROW_STATUS_ACTIVE
            || uptr->userStatus == tcROW_STATUS_NOTREADY
            || uptr->userStatus == tcROW_STATUS_NOTINSERVICE ) {
            /*
             * The authProtocol is already set.  It is only legal to CHANGE it
             * to usm_noAuthProtocol...
             */
            if ( Api_oidCompare( ( oid* )var_val, var_val_len / sizeof( oid ),
                     usm_noAuthProtocol,
                     sizeof( usm_noAuthProtocol ) / sizeof( oid ) )
                == 0 ) {
                /*
                 * ... and then only if the privProtocol is equal to
                 * usm_noPrivProtocol.
                 */
                if ( Api_oidCompare( uptr->privProtocol, uptr->privProtocolLen,
                         usm_noPrivProtocol,
                         sizeof( usm_noPrivProtocol ) / sizeof( oid ) )
                    != 0 ) {
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
                optr = uptr->authProtocol;
                olen = uptr->authProtocolLen;
                resetOnFail = 1;
                uptr->authProtocol = Api_duplicateObjid( ( oid* )var_val,
                    var_val_len / sizeof( oid ) );
                if ( uptr->authProtocol == NULL ) {
                    return PRIOT_ERR_RESOURCEUNAVAILABLE;
                }
                uptr->authProtocolLen = var_val_len / sizeof( oid );
            } else if ( Api_oidCompare( ( oid* )var_val, var_val_len / sizeof( oid ),
                            uptr->authProtocol, uptr->authProtocolLen )
                == 0 ) {
                /*
                 * But it's also okay to set it to the same thing as it
                 * currently is.  
                 */
                return PRIOT_ERR_NOERROR;
            } else {
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
        } else {
            /*
             * This row is under creation.  It's okay to set
             * usmUserAuthProtocol to any valid authProtocol but it will be
             * overwritten when usmUserCloneFrom is set (so don't write it if
             * that has already been set).  
             */

            if ( Api_oidCompare( ( oid* )var_val, var_val_len / sizeof( oid ),
                     usm_noAuthProtocol,
                     sizeof( usm_noAuthProtocol ) / sizeof( oid ) )
                    == 0
                || Api_oidCompare( ( oid* )var_val,
                       var_val_len / sizeof( oid ),
                       usm_hMACMD5AuthProtocol,
                       sizeof( usm_hMACMD5AuthProtocol ) / sizeof( oid ) )
                    == 0
                || Api_oidCompare( ( oid* )var_val,
                       var_val_len / sizeof( oid ),
                       usm_hMACSHA1AuthProtocol,
                       sizeof( usm_hMACSHA1AuthProtocol ) / sizeof( oid ) )
                    == 0 ) {
                if ( uptr->cloneFrom != NULL ) {
                    optr = uptr->authProtocol;
                    olen = uptr->authProtocolLen;
                    resetOnFail = 1;
                    uptr->authProtocol = Api_duplicateObjid( ( oid* )var_val,
                        var_val_len / sizeof( oid ) );
                    if ( uptr->authProtocol == NULL ) {
                        return PRIOT_ERR_RESOURCEUNAVAILABLE;
                    }
                    uptr->authProtocolLen = var_val_len / sizeof( oid );
                }
            } else {
                /*
                 * Unknown authentication protocol.  
                 */
                return PRIOT_ERR_WRONGVALUE;
            }
        }
    } else if ( action == IMPL_COMMIT ) {
        MEMORY_FREE( optr );
    } else if ( action == IMPL_FREE || action == IMPL_UNDO ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) != NULL ) {
            if ( resetOnFail ) {
                MEMORY_FREE( uptr->authProtocol );
                uptr->authProtocol = optr;
                uptr->authProtocolLen = olen;
            }
        }
    }
    return PRIOT_ERR_NOERROR;
} /* end write_usmUserAuthProtocol() */

/*******************************************************************-o-******
 * write_usmUserAuthKeyChange
 *
 * Parameters:
 *	 action		
 *	*var_val	Octet string representing new KeyChange value.
 *	 var_val_type
 *	 var_val_len
 *	*statP		(UNUSED)
 *	*name		OID of user upon which to perform set operation.
 *	 name_len
 *      
 * Returns:
 *	PRIOT_ERR_NOERR		Success.
 *	PRIOT_ERR_WRONGTYPE
 *	PRIOT_ERR_WRONGLENGTH
 *	PRIOT_ERR_NOSUCHNAME
 *	PRIOT_ERR_GENERR
 *
 * Note: This function handles both the usmUserAuthKeyChange and
 *       usmUserOwnAuthKeyChange objects.  We are not passed the name
 *       of the user requseting the keychange, so we leave this to the
 *       calling module to verify when and if we should be called.  To
 *       change this would require a change in the mib module API to
 *       pass in the securityName requesting the change.
 *
 * XXX:  should handle action=IMPL_UNDO's.
 */
int write_usmUserAuthKeyChange( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    struct Usm_User_s* uptr;
    unsigned char buf[ UTILITIES_MAX_BUFFER_SMALL ];
    size_t buflen = UTILITIES_MAX_BUFFER_SMALL;
    const char fnAuthKey[] = "write_usmUserAuthKeyChange";
    const char fnOwnAuthKey[] = "write_usmUserOwnAuthKeyChange";
    const char* fname;
    static unsigned char* oldkey;
    static size_t oldkeylen;
    static int resetOnFail;

    if ( name[ USM_MIB_LENGTH - 1 ] == 6 ) {
        fname = fnAuthKey;
    } else {
        fname = fnOwnAuthKey;
    }

    if ( action == IMPL_RESERVE1 ) {
        resetOnFail = 0;
        if ( var_val_type != asnOCTET_STR ) {
            DEBUG_MSGTL( ( "usmUser", "write to %s not ASN01_OCTET_STR\n",
                fname ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len == 0 ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        } else {
            if ( Api_oidCompare( uptr->authProtocol, uptr->authProtocolLen,
                     usm_hMACMD5AuthProtocol,
                     sizeof( usm_hMACMD5AuthProtocol ) / sizeof( oid ) )
                == 0 ) {
                if ( var_val_len != 0 && var_val_len != 32 ) {
                    return PRIOT_ERR_WRONGLENGTH;
                }
            } else if ( Api_oidCompare( uptr->authProtocol, uptr->authProtocolLen,
                            usm_hMACSHA1AuthProtocol,
                            sizeof( usm_hMACSHA1AuthProtocol ) / sizeof( oid ) )
                == 0 ) {
                if ( var_val_len != 0 && var_val_len != 40 ) {
                    return PRIOT_ERR_WRONGLENGTH;
                }
            }
        }
    } else if ( action == IMPL_ACTION ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        }
        if ( uptr->cloneFrom == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        }
        if ( Api_oidCompare( uptr->authProtocol, uptr->authProtocolLen,
                 usm_noAuthProtocol,
                 sizeof( usm_noAuthProtocol ) / sizeof( oid ) )
            == 0 ) {
            /*
             * "When the value of the corresponding usmUserAuthProtocol is
             * usm_noAuthProtocol, then a set is successful, but effectively
             * is a no-op."  
             */
            DEBUG_MSGTL( ( "usmUser",
                "%s: noAuthProtocol keyChange... success!\n",
                fname ) );
            return PRIOT_ERR_NOERROR;
        }

        /*
         * Change the key.  
         */
        DEBUG_MSGTL( ( "usmUser", "%s: changing auth key for user %s\n",
            fname, uptr->secName ) );

        if ( KeyTools_decodeKeyChange( uptr->authProtocol, uptr->authProtocolLen,
                 uptr->authKey, uptr->authKeyLen,
                 var_val, var_val_len,
                 buf, &buflen )
            != ErrorCode_SUCCESS ) {
            DEBUG_MSGTL( ( "usmUser", "%s: ... failed\n", fname ) );
            return PRIOT_ERR_GENERR;
        }
        DEBUG_MSGTL( ( "usmUser", "%s: ... succeeded\n", fname ) );
        resetOnFail = 1;
        oldkey = uptr->authKey;
        oldkeylen = uptr->authKeyLen;
        uptr->authKey = ( u_char* )Memory_memdup( buf, buflen );
        if ( uptr->authKey == NULL ) {
            return PRIOT_ERR_RESOURCEUNAVAILABLE;
        }
        uptr->authKeyLen = buflen;
    } else if ( action == IMPL_COMMIT ) {
        MEMORY_FREE( oldkey );
    } else if ( action == IMPL_UNDO ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) != NULL && resetOnFail ) {
            MEMORY_FREE( uptr->authKey );
            uptr->authKey = oldkey;
            uptr->authKeyLen = oldkeylen;
        }
    }

    return PRIOT_ERR_NOERROR;
} /* end write_usmUserAuthKeyChange() */

int write_usmUserPrivProtocol( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    static oid* optr;
    static size_t olen;
    static int resetOnFail;
    struct Usm_User_s* uptr;

    if ( action == IMPL_RESERVE1 ) {
        resetOnFail = 0;
        if ( var_val_type != asnOBJECT_ID ) {
            DEBUG_MSGTL( ( "usmUser",
                "write to usmUserPrivProtocol not ASN01_OBJECT_ID\n" ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len > USM_LENGTH_OID_MAX * sizeof( oid ) || var_val_len % sizeof( oid ) != 0 ) {
            DEBUG_MSGTL( ( "usmUser",
                "write to usmUserPrivProtocol: bad length\n" ) );
            return PRIOT_ERR_WRONGLENGTH;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        }

        if ( uptr->userStatus == tcROW_STATUS_ACTIVE
            || uptr->userStatus == tcROW_STATUS_NOTREADY
            || uptr->userStatus == tcROW_STATUS_NOTINSERVICE ) {
            /*
             * The privProtocol is already set.  It is only legal to CHANGE it
             * to usm_noPrivProtocol.
             */
            if ( Api_oidCompare( ( oid* )var_val, var_val_len / sizeof( oid ),
                     usm_noPrivProtocol,
                     sizeof( usm_noPrivProtocol ) / sizeof( oid ) )
                == 0 ) {
                resetOnFail = 1;
                optr = uptr->privProtocol;
                olen = uptr->privProtocolLen;
                uptr->privProtocol = Api_duplicateObjid( ( oid* )var_val,
                    var_val_len / sizeof( oid ) );
                if ( uptr->privProtocol == NULL ) {
                    return PRIOT_ERR_RESOURCEUNAVAILABLE;
                }
                uptr->privProtocolLen = var_val_len / sizeof( oid );
            } else if ( Api_oidCompare( ( oid* )var_val, var_val_len / sizeof( oid ),
                            uptr->privProtocol, uptr->privProtocolLen )
                == 0 ) {
                /*
                 * But it's also okay to set it to the same thing as it
                 * currently is.  
                 */
                return PRIOT_ERR_NOERROR;
            } else {
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
        } else {
            /*
             * This row is under creation.  It's okay to set
             * usmUserPrivProtocol to any valid privProtocol with the proviso
             * that if usmUserAuthProtocol is set to usm_noAuthProtocol, it may
             * only be set to usm_noPrivProtocol.  The value will be overwritten
             * when usmUserCloneFrom is set (so don't write it if that has
             * already been set).  
             */
            if ( Api_oidCompare( uptr->authProtocol, uptr->authProtocolLen,
                     usm_noAuthProtocol,
                     sizeof( usm_noAuthProtocol ) / sizeof( oid ) )
                == 0 ) {
                if ( Api_oidCompare( ( oid* )var_val, var_val_len / sizeof( oid ),
                         usm_noPrivProtocol,
                         sizeof( usm_noPrivProtocol ) / sizeof( oid ) )
                    != 0 ) {
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
            } else {
                if ( Api_oidCompare( ( oid* )var_val, var_val_len / sizeof( oid ),
                         usm_noPrivProtocol,
                         sizeof( usm_noPrivProtocol ) / sizeof( oid ) )
                        != 0
                    && Api_oidCompare( ( oid* )var_val, var_val_len / sizeof( oid ),
                           usm_dESPrivProtocol,
                           sizeof( usm_dESPrivProtocol ) / sizeof( oid ) )
                        != 0
                    && Api_oidCompare( ( oid* )var_val, var_val_len / sizeof( oid ),
                           usm_aESPrivProtocol,
                           sizeof( usm_aESPrivProtocol ) / sizeof( oid ) )
                        != 0 ) {
                    return PRIOT_ERR_WRONGVALUE;
                }
            }
            resetOnFail = 1;
            optr = uptr->privProtocol;
            olen = uptr->privProtocolLen;
            uptr->privProtocol = Api_duplicateObjid( ( oid* )var_val,
                var_val_len / sizeof( oid ) );
            if ( uptr->privProtocol == NULL ) {
                return PRIOT_ERR_RESOURCEUNAVAILABLE;
            }
            uptr->privProtocolLen = var_val_len / sizeof( oid );
        }
    } else if ( action == IMPL_COMMIT ) {
        MEMORY_FREE( optr );
    } else if ( action == IMPL_FREE || action == IMPL_UNDO ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) != NULL ) {
            if ( resetOnFail ) {
                MEMORY_FREE( uptr->privProtocol );
                uptr->privProtocol = optr;
                uptr->privProtocolLen = olen;
            }
        }
    }

    return PRIOT_ERR_NOERROR;
} /* end write_usmUserPrivProtocol() */

/*
 * Note: This function handles both the usmUserPrivKeyChange and
 *       usmUserOwnPrivKeyChange objects.  We are not passed the name
 *       of the user requseting the keychange, so we leave this to the
 *       calling module to verify when and if we should be called.  To
 *       change this would require a change in the mib module API to
 *       pass in the securityName requesting the change.
 *
 */
int write_usmUserPrivKeyChange( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    struct Usm_User_s* uptr;
    unsigned char buf[ UTILITIES_MAX_BUFFER_SMALL ];
    size_t buflen = UTILITIES_MAX_BUFFER_SMALL;
    const char fnPrivKey[] = "write_usmUserPrivKeyChange";
    const char fnOwnPrivKey[] = "write_usmUserOwnPrivKeyChange";
    const char* fname;
    static unsigned char* oldkey;
    static size_t oldkeylen;
    static int resetOnFail;

    if ( name[ USM_MIB_LENGTH - 1 ] == 9 ) {
        fname = fnPrivKey;
    } else {
        fname = fnOwnPrivKey;
    }

    if ( action == IMPL_RESERVE1 ) {
        resetOnFail = 0;
        if ( var_val_type != asnOCTET_STR ) {
            DEBUG_MSGTL( ( "usmUser", "write to %s not ASN01_OCTET_STR\n",
                fname ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len == 0 ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        } else {
            if ( Api_oidCompare( uptr->privProtocol, uptr->privProtocolLen,
                     usm_dESPrivProtocol,
                     sizeof( usm_dESPrivProtocol ) / sizeof( oid ) )
                == 0 ) {
                if ( var_val_len != 0 && var_val_len != 32 ) {
                    return PRIOT_ERR_WRONGLENGTH;
                }
            }
            if ( Api_oidCompare( uptr->privProtocol, uptr->privProtocolLen,
                     usm_aESPrivProtocol,
                     sizeof( usm_aESPrivProtocol ) / sizeof( oid ) )
                == 0 ) {
                if ( var_val_len != 0 && var_val_len != 32 ) {
                    return PRIOT_ERR_WRONGLENGTH;
                }
            }
        }
    } else if ( action == IMPL_ACTION ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        }
        if ( uptr->cloneFrom == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        }
        if ( Api_oidCompare( uptr->privProtocol, uptr->privProtocolLen,
                 usm_noPrivProtocol,
                 sizeof( usm_noPrivProtocol ) / sizeof( oid ) )
            == 0 ) {
            /*
             * "When the value of the corresponding usmUserPrivProtocol is
             * usm_noPrivProtocol, then a set is successful, but effectively
             * is a no-op."  
             */
            DEBUG_MSGTL( ( "usmUser",
                "%s: noPrivProtocol keyChange... success!\n",
                fname ) );
            return PRIOT_ERR_NOERROR;
        }

        /*
         * Change the key. 
         */
        DEBUG_MSGTL( ( "usmUser", "%s: changing priv key for user %s\n",
            fname, uptr->secName ) );

        if ( KeyTools_decodeKeyChange( uptr->authProtocol, uptr->authProtocolLen,
                 uptr->privKey, uptr->privKeyLen,
                 var_val, var_val_len,
                 buf, &buflen )
            != ErrorCode_SUCCESS ) {
            DEBUG_MSGTL( ( "usmUser", "%s: ... failed\n", fname ) );
            return PRIOT_ERR_GENERR;
        }
        DEBUG_MSGTL( ( "usmUser", "%s: ... succeeded\n", fname ) );
        resetOnFail = 1;
        oldkey = uptr->privKey;
        oldkeylen = uptr->privKeyLen;
        uptr->privKey = ( u_char* )Memory_memdup( buf, buflen );
        if ( uptr->privKey == NULL ) {
            return PRIOT_ERR_RESOURCEUNAVAILABLE;
        }
        uptr->privKeyLen = buflen;
    } else if ( action == IMPL_COMMIT ) {
        MEMORY_FREE( oldkey );
    } else if ( action == IMPL_UNDO ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) != NULL && resetOnFail ) {
            MEMORY_FREE( uptr->privKey );
            uptr->privKey = oldkey;
            uptr->privKeyLen = oldkeylen;
        }
    }

    return PRIOT_ERR_NOERROR;
} /* end write_usmUserPrivKeyChange() */

int write_usmUserPublic( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    struct Usm_User_s* uptr = NULL;

    if ( var_val_type != asnOCTET_STR ) {
        DEBUG_MSGTL( ( "usmUser",
            "write to usmUserPublic not ASN01_OCTET_STR\n" ) );
        return PRIOT_ERR_WRONGTYPE;
    }
    if ( var_val_len > 32 ) {
        DEBUG_MSGTL( ( "usmUser", "write to usmUserPublic: bad length\n" ) );
        return PRIOT_ERR_WRONGLENGTH;
    }
    if ( action == IMPL_COMMIT ) {
        /*
         * don't allow creations here 
         */
        if ( ( uptr = Usm_parseUser( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_NOSUCHNAME;
        }
        if ( uptr->userPublicString )
            free( uptr->userPublicString );
        uptr->userPublicString = ( u_char* )malloc( var_val_len );
        if ( uptr->userPublicString == NULL ) {
            return PRIOT_ERR_GENERR;
        }
        memcpy( uptr->userPublicString, var_val, var_val_len );
        uptr->userPublicStringLen = var_val_len;
        DEBUG_MSG( ( "usmUser", "setting public string: %d - ", ( int )var_val_len ) );
        DEBUG_MSGHEX( ( "usmUser", uptr->userPublicString, var_val_len ) );
        DEBUG_MSG( ( "usmUser", "\n" ) );
    }
    return PRIOT_ERR_NOERROR;
} /* end write_usmUserPublic() */

int write_usmUserStorageType( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    long long_ret = *( ( long* )var_val );
    static long oldValue;
    struct Usm_User_s* uptr;
    static int resetOnFail;

    if ( action == IMPL_RESERVE1 ) {
        resetOnFail = 0;
        if ( var_val_type != asnINTEGER ) {
            DEBUG_MSGTL( ( "usmUser",
                "write to usmUserStorageType not ASN01_INTEGER\n" ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len != sizeof( long ) ) {
            DEBUG_MSGTL( ( "usmUser",
                "write to usmUserStorageType: bad length\n" ) );
            return PRIOT_ERR_WRONGLENGTH;
        }
        if ( long_ret < 1 || long_ret > 5 ) {
            return PRIOT_ERR_WRONGVALUE;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        }
        if ( ( long_ret == tcSTORAGE_TYPE_VOLATILE || long_ret == tcSTORAGE_TYPE_NONVOLATILE ) && ( uptr->userStorageType == tcSTORAGE_TYPE_VOLATILE || uptr->userStorageType == tcSTORAGE_TYPE_NONVOLATILE ) ) {
            oldValue = uptr->userStorageType;
            uptr->userStorageType = long_ret;
            resetOnFail = 1;
        } else {
            /*
             * From RFC2574:
             * 
             * "Note that any user who employs authentication or privacy must
             * allow its secret(s) to be updated and thus cannot be 'readOnly'.
             * 
             * If an initial set operation tries to set the value to 'readOnly'
             * for a user who employs authentication or privacy, then an
             * 'inconsistentValue' error must be returned.  Note that if the
             * value has been previously set (implicit or explicit) to any
             * value, then the rules as defined in the StorageType Textual
             * Convention apply.  
             */
            DEBUG_MSGTL( ( "usmUser",
                "long_ret %ld uptr->st %d uptr->status %d\n",
                long_ret, uptr->userStorageType,
                uptr->userStatus ) );

            if ( long_ret == tcSTORAGE_TYPE_READONLY && uptr->userStorageType != tcSTORAGE_TYPE_READONLY && ( uptr->userStatus == tcROW_STATUS_ACTIVE || uptr->userStatus == tcROW_STATUS_NOTINSERVICE ) ) {
                return PRIOT_ERR_WRONGVALUE;
            } else if ( long_ret == tcSTORAGE_TYPE_READONLY && ( Api_oidCompare( uptr->privProtocol, uptr->privProtocolLen,
                                                                     usm_noPrivProtocol,
                                                                     sizeof( usm_noPrivProtocol ) / sizeof( oid ) )
                                                                       != 0
                                                                   || Api_oidCompare( uptr->authProtocol,
                                                                          uptr->authProtocolLen,
                                                                          usm_noAuthProtocol,
                                                                          sizeof( usm_noAuthProtocol ) / sizeof( oid ) )
                                                                       != 0 ) ) {
                return PRIOT_ERR_INCONSISTENTVALUE;
            } else {
                return PRIOT_ERR_WRONGVALUE;
            }
        }
    } else if ( action == IMPL_UNDO || action == IMPL_FREE ) {
        if ( ( uptr = Usm_parseUser( name, name_len ) ) != NULL && resetOnFail ) {
            uptr->userStorageType = oldValue;
        }
    }
    return PRIOT_ERR_NOERROR;
} /* end write_usmUserStorageType() */

/*
 * Return 1 if enough objects have been set up to transition rowStatus to
 * notInService(2) or active(1).  
 */

int usmStatusCheck( struct Usm_User_s* uptr )
{
    if ( uptr == NULL ) {
        return 0;
    } else {
        if ( uptr->cloneFrom == NULL ) {
            return 0;
        }
    }
    return 1;
}

/*******************************************************************-o-******
 * write_usmUserStatus
 *
 * Parameters:
 *	 action
 *	*var_val
 *	 var_val_type
 *	 var_val_len
 *	*statP
 *	*name
 *	 name_len
 *      
 * Returns:
 *	PRIOT_ERR_NOERROR		On success.
 *	PRIOT_ERR_GENERR
 *	PRIOT_ERR_INCONSISTENTNAME
 *	PRIOT_ERR_INCONSISTENTVALUE
 *	PRIOT_ERR_WRONGLENGTH
 *	PRIOT_ERR_WRONGTYPE
 */
int write_usmUserStatus( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    /*
     * variables we may use later
     */
    static long long_ret;
    unsigned char* engineID;
    size_t engineIDLen;
    char* newName;
    size_t nameLen;
    struct Usm_User_s* uptr = NULL;

    if ( action == IMPL_RESERVE1 ) {
        if ( var_val_type != asnINTEGER ) {
            DEBUG_MSGTL( ( "usmUser",
                "write to usmUserStatus not ASN01_INTEGER\n" ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len != sizeof( long_ret ) ) {
            DEBUG_MSGTL( ( "usmUser",
                "write to usmUserStatus: bad length\n" ) );
            return PRIOT_ERR_WRONGLENGTH;
        }
        long_ret = *( ( long* )var_val );
        if ( long_ret == tcROW_STATUS_NOTREADY || long_ret < 1 || long_ret > 6 ) {
            return PRIOT_ERR_WRONGVALUE;
        }

        /*
         * See if we can parse the oid for engineID/name first.  
         */
        if ( usm_parse_oid( &name[ USM_MIB_LENGTH ], name_len - USM_MIB_LENGTH,
                 &engineID, &engineIDLen, ( u_char** )&newName,
                 &nameLen ) ) {
            DEBUG_MSGTL( ( "usmUser",
                "can't parse the OID for engineID or name\n" ) );
            return PRIOT_ERR_INCONSISTENTNAME;
        }

        if ( engineIDLen < 5 || engineIDLen > 32 || nameLen < 1
            || nameLen > 32 ) {
            MEMORY_FREE( engineID );
            MEMORY_FREE( newName );
            return PRIOT_ERR_NOCREATION;
        }

        /*
         * Now see if a user already exists with these index values. 
         */
        uptr = Usm_getUser( engineID, engineIDLen, newName );

        if ( uptr != NULL ) {
            if ( long_ret == tcROW_STATUS_CREATEANDGO || long_ret == tcROW_STATUS_CREATEANDWAIT ) {
                MEMORY_FREE( engineID );
                MEMORY_FREE( newName );
                long_ret = tcROW_STATUS_NOTREADY;
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
            MEMORY_FREE( engineID );
            MEMORY_FREE( newName );
        } else {
            if ( long_ret == tcROW_STATUS_ACTIVE || long_ret == tcROW_STATUS_NOTINSERVICE ) {
                MEMORY_FREE( engineID );
                MEMORY_FREE( newName );
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
            if ( long_ret == tcROW_STATUS_CREATEANDGO || long_ret == tcROW_STATUS_CREATEANDWAIT ) {
                if ( ( uptr = Usm_createUser() ) == NULL ) {
                    MEMORY_FREE( engineID );
                    MEMORY_FREE( newName );
                    return PRIOT_ERR_RESOURCEUNAVAILABLE;
                }
                uptr->engineID = engineID;
                uptr->name = newName;
                uptr->secName = strdup( uptr->name );
                if ( uptr->secName == NULL ) {
                    Usm_freeUser( uptr );
                    return PRIOT_ERR_RESOURCEUNAVAILABLE;
                }
                uptr->engineIDLen = engineIDLen;

                /*
                 * Set status to createAndGo or createAndWait so we can tell
                 * that this row is under creation.  
                 */

                uptr->userStatus = long_ret;

                /*
                 * Add to the list of users (we will take it off again
                 * later if something goes wrong).  
                 */

                Usm_addUser( uptr );
            } else {
                MEMORY_FREE( engineID );
                MEMORY_FREE( newName );
            }
        }
    } else if ( action == IMPL_ACTION ) {
        usm_parse_oid( &name[ USM_MIB_LENGTH ], name_len - USM_MIB_LENGTH,
            &engineID, &engineIDLen, ( u_char** )&newName,
            &nameLen );
        uptr = Usm_getUser( engineID, engineIDLen, newName );
        MEMORY_FREE( engineID );
        MEMORY_FREE( newName );

        if ( uptr != NULL ) {
            if ( long_ret == tcROW_STATUS_CREATEANDGO || long_ret == tcROW_STATUS_ACTIVE ) {
                if ( usmStatusCheck( uptr ) ) {
                    uptr->userStatus = tcROW_STATUS_ACTIVE;
                } else {
                    MEMORY_FREE( engineID );
                    MEMORY_FREE( newName );
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
            } else if ( long_ret == tcROW_STATUS_CREATEANDWAIT ) {
                if ( usmStatusCheck( uptr ) ) {
                    uptr->userStatus = tcROW_STATUS_NOTINSERVICE;
                } else {
                    uptr->userStatus = tcROW_STATUS_NOTREADY;
                }
            } else if ( long_ret == tcROW_STATUS_NOTINSERVICE ) {
                if ( uptr->userStatus == tcROW_STATUS_ACTIVE || uptr->userStatus == tcROW_STATUS_NOTINSERVICE ) {
                    uptr->userStatus = tcROW_STATUS_NOTINSERVICE;
                } else {
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
            }
        }
    } else if ( action == IMPL_COMMIT ) {
        usm_parse_oid( &name[ USM_MIB_LENGTH ], name_len - USM_MIB_LENGTH,
            &engineID, &engineIDLen, ( u_char** )&newName,
            &nameLen );
        uptr = Usm_getUser( engineID, engineIDLen, newName );
        MEMORY_FREE( engineID );
        MEMORY_FREE( newName );

        if ( uptr != NULL ) {
            if ( long_ret == tcROW_STATUS_DESTROY ) {
                Usm_removeUser( uptr );
                Usm_freeUser( uptr );
            }
        }
    } else if ( action == IMPL_UNDO || action == IMPL_FREE ) {
        if ( usm_parse_oid( &name[ USM_MIB_LENGTH ], name_len - USM_MIB_LENGTH,
                 &engineID, &engineIDLen, ( u_char** )&newName,
                 &nameLen ) ) {
            /* Can't extract engine info from the OID - nothing to undo */
            return PRIOT_ERR_NOERROR;
        }
        uptr = Usm_getUser( engineID, engineIDLen, newName );
        MEMORY_FREE( engineID );
        MEMORY_FREE( newName );

        if ( long_ret == tcROW_STATUS_CREATEANDGO || long_ret == tcROW_STATUS_CREATEANDWAIT ) {
            Usm_removeUser( uptr );
            Usm_freeUser( uptr );
        }
    }

    return PRIOT_ERR_NOERROR;
} /* write_usmUserStatus */
