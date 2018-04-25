/*
 * SNMPv3 View-based Access Control Model
 */
/* Portions of this file are subject to the following copyright(s).  See
 * the Net-SNMP's COPYING file for more details and other copyrights
 * that may apply:
 */
/*
 * Portions of this file are copyrighted by:
 * Copyright © 2003 Sun Microsystems, Inc. All rights reserved.
 * Use is subject to license terms specified in the COPYING file
 * distributed with the Net-SNMP package.
 */
#include "vacm_vars.h"
#include "AgentRegistry.h"
#include "Impl.h"
#include "SysORTable.h"
#include "Tc.h"
#include "VarStruct.h"
#include "utilities/header_generic.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <malloc.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static unsigned int vacmViewSpinLock = 0;

void init_vacm_vars( void )
{

    oid reg[] = { PRIOT_OID_SNMPMODULES, 16, 2, 2, 1 };

#define PRIVRW ( NETSNMP_SNMPV2ANY | 0x5000 )

    struct Variable1_s vacm_sec2group[] = {
        { VACM_SECURITYGROUP, ASN01_OCTET_STR, IMPL_OLDAPI_RWRITE,
            var_vacm_sec2group, 1, { 3 } },
        { VACM_SECURITYSTORAGE, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
            var_vacm_sec2group, 1, { 4 } },
        { VACM_SECURITYSTATUS, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
            var_vacm_sec2group, 1, { 5 } },
    };

    struct Variable1_s vacm_access[] = {
        { VACM_ACCESSMATCH, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
            var_vacm_access, 1, { 4 } },
        { VACM_ACCESSREAD, ASN01_OCTET_STR, IMPL_OLDAPI_RWRITE,
            var_vacm_access, 1, { 5 } },
        { VACM_ACCESSWRITE, ASN01_OCTET_STR, IMPL_OLDAPI_RWRITE,
            var_vacm_access, 1, { 6 } },
        { VACM_ACCESSNOTIFY, ASN01_OCTET_STR, IMPL_OLDAPI_RWRITE,
            var_vacm_access, 1, { 7 } },
        { VACM_ACCESSSTORAGE, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
            var_vacm_access, 1, { 8 } },
        { VACM_ACCESSSTATUS, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
            var_vacm_access, 1, { 9 } },
    };

    struct Variable3_s vacm_view[] = {
        { VACM_VACMVIEWSPINLOCK, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
            var_vacm_view, 1, { 1 } },
        { VACM_VIEWMASK, ASN01_OCTET_STR, IMPL_OLDAPI_RWRITE,
            var_vacm_view, 3, { 2, 1, 3 } },
        { VACM_VIEWTYPE, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
            var_vacm_view, 3, { 2, 1, 4 } },
        { VACM_VIEWSTORAGE, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
            var_vacm_view, 3, { 2, 1, 5 } },
        { VACM_VIEWSTATUS, ASN01_INTEGER, IMPL_OLDAPI_RWRITE,
            var_vacm_view, 3, { 2, 1, 6 } },
    };

    /*
     * Define the OID pointer to the top of the mib tree that we're
     * registering underneath 
     */
    oid vacm_sec2group_oid[] = { OID_VACMGROUPENTRY };
    oid vacm_access_oid[] = { OID_VACMACCESSENTRY };
    oid vacm_view_oid[] = { OID_VACMMIBVIEWS };

    /*
     * we need to be called back later 
     */
    Callback_registerCallback( CALLBACK_LIBRARY, CALLBACK_STORE_DATA,
        Vacm_storeVacm, NULL );

    /*
     * register ourselves with the agent to handle our mib tree 
     */
    REGISTER_MIB( "mibII/vacm:sec2group", vacm_sec2group, Variable1_s,
        vacm_sec2group_oid );
    REGISTER_MIB( "mibII/vacm:access", vacm_access, Variable1_s,
        vacm_access_oid );
    REGISTER_MIB( "mibII/vacm:view", vacm_view, Variable3_s, vacm_view_oid );

    REGISTER_SYSOR_ENTRY( reg, "View-based Access Control Model for SNMP." );
}

u_char*
var_vacm_sec2group( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact,
    size_t* var_len, WriteMethodFT** write_method )
{
    struct Vacm_GroupEntry_s* gp;
    oid* groupSubtree;
    ssize_t groupSubtreeLen;
    int secmodel;
    char secname[ VACM_VACMSTRINGLEN ], *cp;

    /*
     * Set up write_method first, in case we return NULL before getting to
     * the switch (vp->magic) below.  In some of these cases, we still want
     * to call the appropriate write_method, if only to have it return the
     * appropriate error.  
     */

    switch ( vp->magic ) {
    case VACM_SECURITYGROUP:
        *write_method = write_vacmGroupName;
        break;
    case VACM_SECURITYSTORAGE:
        *write_method = write_vacmSecurityToGroupStorageType;
        break;
    case VACM_SECURITYSTATUS:
        *write_method = write_vacmSecurityToGroupStatus;
        break;
    default:
        *write_method = NULL;
    }

    *var_len = 0; /* assume 0 length until found */

    if ( memcmp( name, vp->name, sizeof( oid ) * vp->namelen ) != 0 ) {
        memcpy( name, vp->name, sizeof( oid ) * vp->namelen );
        *length = vp->namelen;
    }
    if ( exact ) {
        if ( *length < 13 )
            return NULL;

        secmodel = name[ 11 ];
        groupSubtree = name + 13;
        groupSubtreeLen = *length - 13;
        if ( name[ 12 ] != ( oid )groupSubtreeLen )
            return NULL; /* Either extra subids, or an incomplete string */
        cp = secname;
        while ( groupSubtreeLen-- > 0 ) {
            if ( *groupSubtree > 255 )
                return NULL; /* illegal value */
            if ( cp - secname > VACM_MAX_STRING )
                return NULL;
            *cp++ = ( char )*groupSubtree++;
        }
        *cp = 0;

        gp = Vacm_getGroupEntry( secmodel, secname );
    } else {
        secmodel = *length > 11 ? name[ 11 ] : 0;
        groupSubtree = name + 12;
        groupSubtreeLen = *length - 12;
        cp = secname;
        while ( groupSubtreeLen-- > 0 ) {
            if ( *groupSubtree > 255 )
                return NULL; /* illegal value */
            if ( cp - secname > VACM_MAX_STRING )
                return NULL;
            *cp++ = ( char )*groupSubtree++;
        }
        *cp = 0;
        Vacm_scanGroupInit();
        while ( ( gp = Vacm_scanGroupNext() ) != NULL ) {
            if ( gp->securityModel > secmodel || ( gp->securityModel == secmodel
                                                     && strcmp( gp->securityName, secname ) > 0 ) )
                break;
        }
        if ( gp ) {
            name[ 11 ] = gp->securityModel;
            *length = 12;
            cp = gp->securityName;
            while ( *cp ) {
                name[ ( *length )++ ] = *cp++;
            }
        }
    }

    if ( gp == NULL ) {
        return NULL;
    }

    *var_len = sizeof( vars_longReturn );
    switch ( vp->magic ) {
    case VACM_SECURITYMODEL:
        vars_longReturn = gp->securityModel;
        return ( u_char* )&vars_longReturn;

    case VACM_SECURITYNAME:
        *var_len = gp->securityName[ 0 ];
        return ( u_char* )&gp->securityName[ 1 ];

    case VACM_SECURITYGROUP:
        *var_len = strlen( gp->groupName );
        return ( u_char* )gp->groupName;

    case VACM_SECURITYSTORAGE:
        vars_longReturn = gp->storageType;
        return ( u_char* )&vars_longReturn;

    case VACM_SECURITYSTATUS:
        vars_longReturn = gp->status;
        return ( u_char* )&vars_longReturn;

    default:
        break;
    }

    return NULL;
}

u_char*
var_vacm_access( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
    struct Vacm_AccessEntry_s* gp;
    int secmodel, seclevel;
    char groupName[ VACM_VACMSTRINGLEN ] = { 0 };
    char contextPrefix[ VACM_VACMSTRINGLEN ] = { 0 };
    oid* op;
    unsigned long len, i = 0;
    char* cp;
    int cmp;

    /*
     * Set up write_method first, in case we return NULL before getting to
     * the switch (vp->magic) below.  In some of these cases, we still want
     * to call the appropriate write_method, if only to have it return the
     * appropriate error.  
     */

    switch ( vp->magic ) {
    case VACM_ACCESSMATCH:
        *write_method = write_vacmAccessContextMatch;
        break;
    case VACM_ACCESSREAD:
        *write_method = write_vacmAccessReadViewName;
        break;
    case VACM_ACCESSWRITE:
        *write_method = write_vacmAccessWriteViewName;
        break;
    case VACM_ACCESSNOTIFY:
        *write_method = write_vacmAccessNotifyViewName;
        break;
    case VACM_ACCESSSTORAGE:
        *write_method = write_vacmAccessStorageType;
        break;
    case VACM_ACCESSSTATUS:
        *write_method = write_vacmAccessStatus;
        break;
    default:
        *write_method = NULL;
    }

    *var_len = 0; /* assume 0 length until found */

    if ( memcmp( name, vp->name, sizeof( oid ) * vp->namelen ) != 0 ) {
        memcpy( name, vp->name, sizeof( oid ) * vp->namelen );
        *length = vp->namelen;
    }

    if ( exact ) {
        if ( *length < 15 )
            return NULL;

        /*
         * Extract the group name index from the requested OID ....
         */
        op = name + 11;
        len = *op++;
        if ( len > VACM_MAX_STRING )
            return NULL;
        cp = groupName;
        while ( len-- > 0 ) {
            if ( *op > 255 )
                return NULL; /* illegal value */
            *cp++ = ( char )*op++;
        }
        *cp = 0;

        /*
         * ... followed by the context index ...
         */
        len = *op++;
        if ( len > VACM_MAX_STRING )
            return NULL;
        cp = contextPrefix;
        while ( len-- > 0 ) {
            if ( *op > 255 )
                return NULL; /* illegal value */
            *cp++ = ( char )*op++;
        }
        *cp = 0;

        /*
         * ... and the secModel and secLevel index values.
         */
        secmodel = *op++;
        seclevel = *op++;
        if ( op != name + *length ) {
            return NULL;
        }

        gp = Vacm_getAccessEntry( groupName, contextPrefix, secmodel,
            seclevel );
        if ( gp && gp->securityLevel != seclevel )
            return NULL; /* This isn't strictly what was asked for */

    } else {
        secmodel = seclevel = 0;
        groupName[ 0 ] = 0;
        contextPrefix[ 0 ] = 0;
        op = name + 11;
        if ( op >= name + *length ) {
        } else {
            len = *op;
            if ( len > VACM_MAX_STRING )
                return NULL;
            cp = groupName;
            for ( i = 0; i <= len; i++ ) {
                if ( *op > 255 ) {
                    return NULL; /* illegal value */
                }
                *cp++ = ( char )*op++;
            }
            *cp = 0;
        }
        if ( op >= name + *length ) {
        } else {
            len = *op;
            if ( len > VACM_MAX_STRING )
                return NULL;
            cp = contextPrefix;
            for ( i = 0; i <= len; i++ ) {
                if ( *op > 255 ) {
                    return NULL; /* illegal value */
                }
                *cp++ = ( char )*op++;
            }
            *cp = 0;
        }
        if ( op >= name + *length ) {
        } else {
            secmodel = *op++;
        }
        if ( op >= name + *length ) {
        } else {
            seclevel = *op++;
        }
        Vacm_scanAccessInit();
        while ( ( gp = Vacm_scanAccessNext() ) != NULL ) {
            cmp = strcmp( gp->groupName, groupName );
            if ( cmp > 0 )
                break;
            if ( cmp < 0 )
                continue;
            cmp = strcmp( gp->contextPrefix, contextPrefix );
            if ( cmp > 0 )
                break;
            if ( cmp < 0 )
                continue;
            if ( gp->securityModel > secmodel )
                break;
            if ( gp->securityModel < secmodel )
                continue;
            if ( gp->securityLevel > seclevel )
                break;
        }
        if ( gp ) {
            *length = 11;
            cp = gp->groupName;
            do {
                name[ ( *length )++ ] = *cp++;
            } while ( *cp );
            cp = gp->contextPrefix;
            do {
                name[ ( *length )++ ] = *cp++;
            } while ( *cp );
            name[ ( *length )++ ] = gp->securityModel;
            name[ ( *length )++ ] = gp->securityLevel;
        }
    }

    if ( !gp ) {
        return NULL;
    }

    *var_len = sizeof( vars_longReturn );
    switch ( vp->magic ) {
    case VACM_ACCESSMATCH:
        vars_longReturn = gp->contextMatch;
        return ( u_char* )&vars_longReturn;

    case VACM_ACCESSLEVEL:
        vars_longReturn = gp->securityLevel;
        return ( u_char* )&vars_longReturn;

    case VACM_ACCESSMODEL:
        vars_longReturn = gp->securityModel;
        return ( u_char* )&vars_longReturn;

    case VACM_ACCESSPREFIX:
        *var_len = *gp->contextPrefix;
        return ( u_char* )&gp->contextPrefix[ 1 ];

    case VACM_ACCESSREAD:
        *var_len = strlen( gp->views[ VACM_VIEW_READ ] );
        return ( u_char* )gp->views[ VACM_VIEW_READ ];

    case VACM_ACCESSWRITE:
        *var_len = strlen( gp->views[ VACM_VIEW_WRITE ] );
        return ( u_char* )gp->views[ VACM_VIEW_WRITE ];

    case VACM_ACCESSNOTIFY:
        *var_len = strlen( gp->views[ VACM_VIEW_NOTIFY ] );
        return ( u_char* )gp->views[ VACM_VIEW_NOTIFY ];

    case VACM_ACCESSSTORAGE:
        vars_longReturn = gp->storageType;
        return ( u_char* )&vars_longReturn;

    case VACM_ACCESSSTATUS:
        vars_longReturn = gp->status;
        return ( u_char* )&vars_longReturn;
    }

    return NULL;
}

u_char*
var_vacm_view( struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{
    struct Vacm_ViewEntry_s* gp = NULL;
    char viewName[ VACM_VACMSTRINGLEN ] = { 0 };
    oid subtree[ TYPES_MAX_OID_LEN ] = { 0 };
    size_t subtreeLen = 0;
    oid *op, *op1;
    unsigned long len = 0, i = 0;
    char* cp;
    int cmp, cmp2;

    /*
     * Set up write_method first, in case we return NULL before getting to
     * the switch (vp->magic) below.  In some of these cases, we still want
     * to call the appropriate write_method, if only to have it return the
     * appropriate error.  
     */

    switch ( vp->magic ) {
    case VACM_VIEWMASK:
        *write_method = write_vacmViewMask;
        break;
    case VACM_VIEWTYPE:
        *write_method = write_vacmViewType;
        break;
    case VACM_VIEWSTORAGE:
        *write_method = write_vacmViewStorageType;
        break;
    case VACM_VIEWSTATUS:
        *write_method = write_vacmViewStatus;
        break;
    default:
        *write_method = NULL;
    }

    *var_len = sizeof( vars_longReturn );

    if ( vp->magic != VACM_VACMVIEWSPINLOCK ) {
        if ( memcmp( name, vp->name, sizeof( oid ) * vp->namelen ) != 0 ) {
            memcpy( name, vp->name, sizeof( oid ) * vp->namelen );
            *length = vp->namelen;
        }

        if ( exact ) {
            if ( *length < 15 )
                return NULL;

            /*
             * Extract the view name index from the requested OID ....
             */
            op = name + 12;
            len = *op++;
            if ( len > VACM_MAX_STRING )
                return NULL;
            cp = viewName;
            while ( len-- > 0 ) {
                if ( *op > 255 )
                    return NULL;
                *cp++ = ( char )*op++;
            }
            *cp = 0;

            /*
             * ... followed by the view OID index.
             */
            subtree[ 0 ] = len = *op++;
            subtreeLen = 1;
            if ( len > TYPES_MAX_OID_LEN )
                return NULL;
            if ( ( op + len ) != ( name + *length ) )
                return NULL; /* Declared length doesn't match what we actually got */
            op1 = &( subtree[ 1 ] );
            while ( len-- > 0 ) {
                *op1++ = *op++;
                subtreeLen++;
            }

            gp = Vacm_getViewEntry( viewName, &subtree[ 1 ], subtreeLen - 1,
                VACM_MODE_IGNORE_MASK );
            if ( gp != NULL ) {
                if ( gp->viewSubtreeLen != subtreeLen ) {
                    gp = NULL;
                }
            }
        } else {
            viewName[ 0 ] = 0;
            op = name + 12;
            if ( op >= name + *length ) {
            } else {
                len = *op;
                if ( len > VACM_MAX_STRING )
                    return NULL;
                cp = viewName;
                for ( i = 0; i <= len && op < name + *length; i++ ) {
                    if ( *op > 255 ) {
                        return NULL;
                    }
                    *cp++ = ( char )*op++;
                }
                *cp = 0;
            }
            if ( op >= name + *length ) {
            } else {
                len = *op++;
                op1 = subtree;
                *op1++ = len;
                subtreeLen++;
                for ( i = 0; i <= len && op < name + *length; i++ ) {
                    *op1++ = *op++;
                    subtreeLen++;
                }
            }
            Vacm_scanViewInit();
            while ( ( gp = Vacm_scanViewNext() ) != NULL ) {
                cmp = strcmp( gp->viewName, viewName );
                cmp2 = Api_oidCompare( gp->viewSubtree, gp->viewSubtreeLen,
                    subtree, subtreeLen );
                if ( cmp == 0 && cmp2 > 0 )
                    break;
                if ( cmp > 0 )
                    break;
            }
            if ( gp ) {
                *length = 12;
                cp = gp->viewName;
                do {
                    name[ ( *length )++ ] = *cp++;
                } while ( *cp );
                op1 = gp->viewSubtree;
                len = gp->viewSubtreeLen;
                while ( len-- > 0 ) {
                    name[ ( *length )++ ] = *op1++;
                }
            }
        }

        if ( gp == NULL ) {
            return NULL;
        }
    } else {
        if ( header_generic( vp, name, length, exact, var_len, write_method ) ) {
            return NULL;
        }
    } /*endif -- vp->magic != VACM_VACMVIEWSPINLOCK */

    switch ( vp->magic ) {
    case VACM_VACMVIEWSPINLOCK:
        *write_method = write_vacmViewSpinLock;
        vars_longReturn = vacmViewSpinLock;
        return ( u_char* )&vars_longReturn;

    case VACM_VIEWNAME:
        *var_len = gp->viewName[ 0 ];
        return ( u_char* )&gp->viewName[ 1 ];

    case VACM_VIEWSUBTREE:
        *var_len = gp->viewSubtreeLen * sizeof( oid );
        return ( u_char* )gp->viewSubtree;

    case VACM_VIEWMASK:
        *var_len = gp->viewMaskLen;
        return ( u_char* )gp->viewMask;

    case VACM_VIEWTYPE:
        vars_longReturn = gp->viewType;
        return ( u_char* )&vars_longReturn;

    case VACM_VIEWSTORAGE:
        vars_longReturn = gp->viewStorageType;
        return ( u_char* )&vars_longReturn;

    case VACM_VIEWSTATUS:
        vars_longReturn = gp->viewStatus;
        return ( u_char* )&vars_longReturn;
    }

    return NULL;
}

int sec2group_parse_oid( oid* oidIndex, size_t oidLen,
    int* model, unsigned char** name, size_t* nameLen )
{
    int nameL;
    int i;

    /*
     * first check the validity of the oid 
     */
    if ( ( oidLen <= 0 ) || ( !oidIndex ) ) {
        return 1;
    }
    nameL = oidIndex[ 1 ]; /* the initial name length */
    if ( ( int )oidLen != nameL + 2 ) {
        return 1;
    }

    /*
     * its valid, malloc the space and store the results 
     */
    if ( name == NULL ) {
        return 1;
    }

    *name = ( unsigned char* )malloc( nameL + 1 );
    if ( *name == NULL ) {
        return 1;
    }

    *model = oidIndex[ 0 ];
    *nameLen = nameL;

    for ( i = 0; i < nameL; i++ ) {
        if ( oidIndex[ i + 2 ] > 255 ) {
            free( *name );
            return 1;
        }
        name[ 0 ][ i ] = ( unsigned char )oidIndex[ i + 2 ];
    }
    name[ 0 ][ nameL ] = 0;

    return 0;
}

struct Vacm_GroupEntry_s*
sec2group_parse_groupEntry( oid* name, size_t name_len )
{
    struct Vacm_GroupEntry_s* geptr;

    char* newName;
    int model;
    size_t nameLen;

    /*
     * get the name and engineID out of the incoming oid 
     */
    if ( sec2group_parse_oid( &name[ SEC2GROUP_MIB_LENGTH ], name_len - SEC2GROUP_MIB_LENGTH,
             &model, ( u_char** )&newName, &nameLen ) )
        return NULL;

    /*
     * Now see if a user exists with these index values 
     */
    geptr = Vacm_getGroupEntry( model, newName );
    free( newName );

    return geptr;

} /* end vacm_parse_groupEntry() */

int write_vacmGroupName( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    static unsigned char string[ VACM_VACMSTRINGLEN ];
    struct Vacm_GroupEntry_s* geptr;
    static int resetOnFail;

    if ( action == IMPL_RESERVE1 ) {
        resetOnFail = 0;
        if ( var_val_type != ASN01_OCTET_STR ) {
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len < 1 || var_val_len > 32 ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( geptr = sec2group_parse_groupEntry( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        } else {
            resetOnFail = 1;
            memcpy( string, geptr->groupName, VACM_VACMSTRINGLEN );
            memcpy( geptr->groupName, var_val, var_val_len );
            geptr->groupName[ var_val_len ] = 0;
            if ( geptr->status == TC_RS_NOTREADY ) {
                geptr->status = TC_RS_NOTINSERVICE;
            }
        }
    } else if ( action == IMPL_FREE ) {
        /*
         * Try to undo the SET here (abnormal usage of IMPL_FREE clause)
         */
        if ( ( geptr = sec2group_parse_groupEntry( name, name_len ) ) != NULL && resetOnFail ) {
            memcpy( geptr->groupName, string, VACM_VACMSTRINGLEN );
        }
    }
    return PRIOT_ERR_NOERROR;
}

int write_vacmSecurityToGroupStorageType( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP,
    oid* name, size_t name_len )
{
    /*
     * variables we may use later 
     */
    static long long_ret;
    struct Vacm_GroupEntry_s* geptr;

    if ( var_val_type != ASN01_INTEGER ) {
        return PRIOT_ERR_WRONGTYPE;
    }
    if ( var_val_len > sizeof( long_ret ) ) {
        return PRIOT_ERR_WRONGLENGTH;
    }
    if ( action == IMPL_COMMIT ) {
        /*
         * don't allow creations here 
         */
        if ( ( geptr = sec2group_parse_groupEntry( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_NOSUCHNAME;
        }
        long_ret = *( ( long* )var_val );
        if ( ( long_ret == TC_ST_VOLATILE || long_ret == TC_ST_NONVOLATILE ) && ( geptr->storageType == TC_ST_VOLATILE || geptr->storageType == TC_ST_NONVOLATILE ) ) {
            geptr->storageType = long_ret;
        } else if ( long_ret == geptr->storageType ) {
            return PRIOT_ERR_NOERROR;
        } else {
            return PRIOT_ERR_INCONSISTENTVALUE;
        }
    }
    return PRIOT_ERR_NOERROR;
}

int write_vacmSecurityToGroupStatus( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP,
    oid* name, size_t name_len )
{
    static long long_ret;
    int model;
    char* newName;
    size_t nameLen;
    struct Vacm_GroupEntry_s* geptr;

    if ( action == IMPL_RESERVE1 ) {
        if ( var_val_type != ASN01_INTEGER ) {
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len != sizeof( long_ret ) ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
        long_ret = *( ( long* )var_val );
        if ( long_ret == TC_RS_NOTREADY || long_ret < 1 || long_ret > 6 ) {
            return PRIOT_ERR_WRONGVALUE;
        }

        /*
         * See if we can parse the oid for model/name first.  
         */

        if ( sec2group_parse_oid( &name[ SEC2GROUP_MIB_LENGTH ],
                 name_len - SEC2GROUP_MIB_LENGTH,
                 &model, ( u_char** )&newName, &nameLen ) ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        }

        if ( model < 1 || nameLen < 1 || nameLen > 32 ) {
            free( newName );
            return PRIOT_ERR_NOCREATION;
        }

        /*
         * Now see if a group already exists with these index values.  
         */
        geptr = Vacm_getGroupEntry( model, newName );

        if ( geptr != NULL ) {
            if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_CREATEANDWAIT ) {
                free( newName );
                long_ret = TC_RS_NOTREADY;
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
            if ( long_ret == TC_RS_DESTROY && geptr->storageType == TC_ST_PERMANENT ) {
                free( newName );
                return PRIOT_ERR_WRONGVALUE;
            }
        } else {
            if ( long_ret == TC_RS_ACTIVE || long_ret == TC_RS_NOTINSERVICE ) {
                free( newName );
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
            if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_CREATEANDWAIT ) {

                /*
                 * Generate a new group entry.  
                 */
                if ( ( geptr = Vacm_createGroupEntry( model, newName ) ) == NULL ) {
                    free( newName );
                    return PRIOT_ERR_GENERR;
                }

                /*
                 * Set defaults.  
                 */
                geptr->storageType = TC_ST_NONVOLATILE;
                geptr->status = TC_RS_NOTREADY;
            }
        }
        free( newName );
    } else if ( action == IMPL_ACTION ) {
        sec2group_parse_oid( &name[ SEC2GROUP_MIB_LENGTH ],
            name_len - SEC2GROUP_MIB_LENGTH,
            &model, ( u_char** )&newName, &nameLen );

        geptr = Vacm_getGroupEntry( model, newName );

        if ( geptr != NULL ) {
            if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_ACTIVE ) {
                /*
                 * Check that all the mandatory objects have been set by now,
                 * otherwise return inconsistentValue.  
                 */
                if ( geptr->groupName[ 0 ] == 0 ) {
                    free( newName );
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
                geptr->status = TC_RS_ACTIVE;
            } else if ( long_ret == TC_RS_CREATEANDWAIT ) {
                if ( geptr->groupName[ 0 ] != 0 ) {
                    geptr->status = TC_RS_NOTINSERVICE;
                }
            } else if ( long_ret == TC_RS_NOTINSERVICE ) {
                if ( geptr->status == TC_RS_ACTIVE ) {
                    geptr->status = TC_RS_NOTINSERVICE;
                } else if ( geptr->status == TC_RS_NOTREADY ) {
                    free( newName );
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
            }
        }
        free( newName );
    } else if ( action == IMPL_COMMIT ) {
        sec2group_parse_oid( &name[ SEC2GROUP_MIB_LENGTH ],
            name_len - SEC2GROUP_MIB_LENGTH,
            &model, ( u_char** )&newName, &nameLen );

        geptr = Vacm_getGroupEntry( model, newName );

        if ( geptr != NULL ) {
            if ( long_ret == TC_RS_DESTROY ) {
                Vacm_destroyGroupEntry( model, newName );
            }
        }
        free( newName );
    } else if ( action == IMPL_UNDO ) {
        if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_CREATEANDWAIT ) {
            sec2group_parse_oid( &name[ SEC2GROUP_MIB_LENGTH ],
                name_len - SEC2GROUP_MIB_LENGTH,
                &model, ( u_char** )&newName, &nameLen );

            geptr = Vacm_getGroupEntry( model, newName );

            if ( geptr != NULL ) {
                Vacm_destroyGroupEntry( model, newName );
            }
            free( newName );
        }
    }

    return PRIOT_ERR_NOERROR;
}

int access_parse_oid( oid* oidIndex, size_t oidLen,
    unsigned char** groupName, size_t* groupNameLen,
    unsigned char** contextPrefix, size_t* contextPrefixLen,
    int* model, int* level )
{
    int groupNameL, contextPrefixL;
    int i;

    /*
     * first check the validity of the oid 
     */
    if ( ( oidLen <= 0 ) || ( !oidIndex ) ) {
        return 1;
    }
    groupNameL = oidIndex[ 0 ];
    contextPrefixL = oidIndex[ groupNameL + 1 ]; /* the initial name length */
    if ( ( int )oidLen != groupNameL + contextPrefixL + 4 ) {
        return 1;
    }

    /*
     * its valid, malloc the space and store the results 
     */
    if ( contextPrefix == NULL || groupName == NULL ) {
        return 1;
    }

    *groupName = ( unsigned char* )malloc( groupNameL + 1 );
    if ( *groupName == NULL ) {
        return 1;
    }

    *contextPrefix = ( unsigned char* )malloc( contextPrefixL + 1 );
    if ( *contextPrefix == NULL ) {
        free( *groupName );
        return 1;
    }

    *contextPrefixLen = contextPrefixL;
    *groupNameLen = groupNameL;

    for ( i = 0; i < groupNameL; i++ ) {
        if ( oidIndex[ i + 1 ] > 255 ) {
            free( *groupName );
            free( *contextPrefix );
            return 1;
        }
        groupName[ 0 ][ i ] = ( unsigned char )oidIndex[ i + 1 ];
    }
    groupName[ 0 ][ groupNameL ] = 0;

    for ( i = 0; i < contextPrefixL; i++ ) {
        if ( oidIndex[ i + groupNameL + 2 ] > 255 ) {
            free( *groupName );
            free( *contextPrefix );
            return 1;
        }
        contextPrefix[ 0 ][ i ] = ( unsigned char )oidIndex[ i + groupNameL + 2 ];
    }
    contextPrefix[ 0 ][ contextPrefixL ] = 0;

    *model = oidIndex[ groupNameL + contextPrefixL + 2 ];
    *level = oidIndex[ groupNameL + contextPrefixL + 3 ];

    return 0;
}

struct Vacm_AccessEntry_s*
access_parse_accessEntry( oid* name, size_t name_len )
{
    struct Vacm_AccessEntry_s* aptr;

    char* newGroupName = NULL;
    char* newContextPrefix = NULL;
    int model, level;
    size_t groupNameLen, contextPrefixLen;

    /*
     * get the name and engineID out of the incoming oid 
     */
    if ( access_parse_oid( &name[ ACCESS_MIB_LENGTH ], name_len - ACCESS_MIB_LENGTH,
             ( u_char** )&newGroupName, &groupNameLen,
             ( u_char** )&newContextPrefix, &contextPrefixLen, &model,
             &level ) )
        return NULL;

    /*
     * Now see if a user exists with these index values 
     */
    aptr = Vacm_getAccessEntry( newGroupName, newContextPrefix, model, level );
    TOOLS_FREE( newContextPrefix );
    TOOLS_FREE( newGroupName );

    return aptr;

} /* end vacm_parse_accessEntry() */

int write_vacmAccessStatus( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    static long long_ret;
    int model, level;
    char *newGroupName = NULL, *newContextPrefix = NULL;
    size_t groupNameLen, contextPrefixLen;
    struct Vacm_AccessEntry_s* aptr = NULL;

    if ( action == IMPL_RESERVE1 ) {
        if ( var_val_type != ASN01_INTEGER ) {
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len != sizeof( long_ret ) ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
        long_ret = *( ( long* )var_val );
        if ( long_ret == TC_RS_NOTREADY || long_ret < 1 || long_ret > 6 ) {
            return PRIOT_ERR_WRONGVALUE;
        }

        /*
         * See if we can parse the oid for model/name first.  
         */
        if ( access_parse_oid( &name[ ACCESS_MIB_LENGTH ],
                 name_len - ACCESS_MIB_LENGTH,
                 ( u_char** )&newGroupName, &groupNameLen,
                 ( u_char** )&newContextPrefix,
                 &contextPrefixLen, &model, &level ) ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        }

        if ( model < 0 || groupNameLen < 1 || groupNameLen > 32 || contextPrefixLen > 32 ) {
            free( newGroupName );
            free( newContextPrefix );
            return PRIOT_ERR_NOCREATION;
        }

        /*
         * Now see if a group already exists with these index values.  
         */
        aptr = Vacm_getAccessEntry( newGroupName, newContextPrefix, model,
            level );

        if ( aptr != NULL ) {
            if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_CREATEANDWAIT ) {
                free( newGroupName );
                free( newContextPrefix );
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
            if ( long_ret == TC_RS_DESTROY && aptr->storageType == TC_ST_PERMANENT ) {
                free( newGroupName );
                free( newContextPrefix );
                return PRIOT_ERR_WRONGVALUE;
            }
        } else {
            if ( long_ret == TC_RS_ACTIVE || long_ret == TC_RS_NOTINSERVICE ) {
                free( newGroupName );
                free( newContextPrefix );
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
            if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_CREATEANDWAIT ) {
                if ( ( aptr = Vacm_createAccessEntry( newGroupName,
                           newContextPrefix,
                           model,
                           level ) )
                    == NULL ) {
                    free( newGroupName );
                    free( newContextPrefix );
                    return PRIOT_ERR_GENERR;
                }

                /*
                 * Set defaults.  
                 */
                aptr->contextMatch = 1; /*  exact(1) is the DEFVAL  */
                aptr->storageType = TC_ST_NONVOLATILE;
                aptr->status = TC_RS_NOTREADY;
            }
        }
        free( newGroupName );
        free( newContextPrefix );
    } else if ( action == IMPL_ACTION ) {
        access_parse_oid( &name[ ACCESS_MIB_LENGTH ],
            name_len - ACCESS_MIB_LENGTH,
            ( u_char** )&newGroupName, &groupNameLen,
            ( u_char** )&newContextPrefix, &contextPrefixLen,
            &model, &level );
        aptr = Vacm_getAccessEntry( newGroupName, newContextPrefix, model,
            level );

        if ( aptr != NULL ) {
            if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_ACTIVE ) {
                aptr->status = TC_RS_ACTIVE;
            } else if ( long_ret == TC_RS_CREATEANDWAIT ) {
                aptr->status = TC_RS_NOTINSERVICE;
            } else if ( long_ret == TC_RS_NOTINSERVICE ) {
                if ( aptr->status == TC_RS_ACTIVE ) {
                    aptr->status = TC_RS_NOTINSERVICE;
                } else if ( aptr->status == TC_RS_NOTREADY ) {
                    free( newGroupName );
                    free( newContextPrefix );
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
            }
        }
        free( newGroupName );
        free( newContextPrefix );
    } else if ( action == IMPL_COMMIT ) {
        access_parse_oid( &name[ ACCESS_MIB_LENGTH ],
            name_len - ACCESS_MIB_LENGTH,
            ( u_char** )&newGroupName, &groupNameLen,
            ( u_char** )&newContextPrefix, &contextPrefixLen,
            &model, &level );
        aptr = Vacm_getAccessEntry( newGroupName, newContextPrefix, model,
            level );

        if ( aptr ) {
            if ( long_ret == TC_RS_DESTROY ) {
                Vacm_destroyAccessEntry( newGroupName, newContextPrefix,
                    model, level );
            }
        }
        free( newGroupName );
        free( newContextPrefix );
    } else if ( action == IMPL_UNDO ) {
        if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_CREATEANDWAIT ) {
            access_parse_oid( &name[ ACCESS_MIB_LENGTH ],
                name_len - ACCESS_MIB_LENGTH,
                ( u_char** )&newGroupName, &groupNameLen,
                ( u_char** )&newContextPrefix,
                &contextPrefixLen, &model, &level );
            aptr = Vacm_getAccessEntry( newGroupName, newContextPrefix, model,
                level );
            if ( aptr != NULL ) {
                Vacm_destroyAccessEntry( newGroupName, newContextPrefix,
                    model, level );
            }
        }
        free( newGroupName );
        free( newContextPrefix );
    }

    return PRIOT_ERR_NOERROR;
}

int write_vacmAccessStorageType( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    /*
     * variables we may use later 
     */
    static long long_ret;
    struct Vacm_AccessEntry_s* aptr;

    if ( var_val_type != ASN01_INTEGER ) {
        DEBUG_MSGTL( ( "mibII/vacm_vars",
            "write to vacmSecurityToGroupStorageType not ASN01_INTEGER\n" ) );
        return PRIOT_ERR_WRONGTYPE;
    }
    if ( var_val_len > sizeof( long_ret ) ) {
        DEBUG_MSGTL( ( "mibII/vacm_vars",
            "write to vacmSecurityToGroupStorageType: bad length\n" ) );
        return PRIOT_ERR_WRONGLENGTH;
    }
    if ( action == IMPL_COMMIT ) {
        /*
         * don't allow creations here 
         */
        if ( ( aptr = access_parse_accessEntry( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_NOSUCHNAME;
        }
        long_ret = *( ( long* )var_val );
        /*
         * if ((long_ret == TC_ST_VOLATILE || long_ret == TC_ST_NONVOLATILE) &&
         * (aptr->storageType == TC_ST_VOLATILE ||
         * aptr->storageType == TC_ST_NONVOLATILE))
         */
        /*
         * This version only supports volatile storage
         */
        if ( long_ret == aptr->storageType ) {
            return PRIOT_ERR_NOERROR;
        } else {
            return PRIOT_ERR_INCONSISTENTVALUE;
        }
    }
    return PRIOT_ERR_NOERROR;
}

int write_vacmAccessContextMatch( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    /*
     * variables we may use later 
     */
    static long long_ret;
    struct Vacm_AccessEntry_s* aptr;

    if ( var_val_type != ASN01_INTEGER ) {
        DEBUG_MSGTL( ( "mibII/vacm_vars",
            "write to vacmAccessContextMatch not ASN01_INTEGER\n" ) );
        return PRIOT_ERR_WRONGTYPE;
    }
    if ( var_val_len > sizeof( long_ret ) ) {
        DEBUG_MSGTL( ( "mibII/vacm_vars",
            "write to vacmAccessContextMatch: bad length\n" ) );
        return PRIOT_ERR_WRONGLENGTH;
    }
    if ( action == IMPL_COMMIT ) {
        /*
         * don't allow creations here 
         */
        if ( ( aptr = access_parse_accessEntry( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_NOSUCHNAME;
        }
        long_ret = *( ( long* )var_val );
        if ( long_ret == CM_EXACT || long_ret == CM_PREFIX ) {
            aptr->contextMatch = long_ret;
        } else {
            return PRIOT_ERR_WRONGVALUE;
        }
    }
    return PRIOT_ERR_NOERROR;
}

int write_vacmAccessReadViewName( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    static unsigned char string[ VACM_VACMSTRINGLEN ];
    struct Vacm_AccessEntry_s* aptr = NULL;
    static int resetOnFail;

    if ( action == IMPL_RESERVE1 ) {
        resetOnFail = 0;
        if ( var_val_type != ASN01_OCTET_STR ) {
            DEBUG_MSGTL( ( "mibII/vacm_vars",
                "write to vacmAccessReadViewName not ASN01_OCTET_STR\n" ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len > 32 ) {
            DEBUG_MSGTL( ( "mibII/vacm_vars",
                "write to vacmAccessReadViewName: bad length\n" ) );
            return PRIOT_ERR_WRONGLENGTH;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( aptr = access_parse_accessEntry( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        } else {
            resetOnFail = 1;
            memcpy( string, aptr->views[ VACM_VIEW_READ ], VACM_VACMSTRINGLEN );
            memcpy( aptr->views[ VACM_VIEW_READ ], var_val, var_val_len );
            aptr->views[ VACM_VIEW_READ ][ var_val_len ] = 0;
        }
    } else if ( action == IMPL_FREE ) {
        /*
         * Try to undo the SET here (abnormal usage of IMPL_FREE clause)
         */
        if ( ( aptr = access_parse_accessEntry( name, name_len ) ) != NULL && resetOnFail ) {
            memcpy( aptr->views[ VACM_VIEW_READ ], string, var_val_len );
        }
    }
    return PRIOT_ERR_NOERROR;
}

int write_vacmAccessWriteViewName( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    static unsigned char string[ VACM_VACMSTRINGLEN ];
    struct Vacm_AccessEntry_s* aptr = NULL;
    static int resetOnFail;

    if ( action == IMPL_RESERVE1 ) {
        resetOnFail = 0;
        if ( var_val_type != ASN01_OCTET_STR ) {
            DEBUG_MSGTL( ( "mibII/vacm_vars",
                "write to vacmAccessWriteViewName not ASN01_OCTET_STR\n" ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len > 32 ) {
            DEBUG_MSGTL( ( "mibII/vacm_vars",
                "write to vacmAccessWriteViewName: bad length\n" ) );
            return PRIOT_ERR_WRONGLENGTH;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( aptr = access_parse_accessEntry( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        } else {
            resetOnFail = 1;
            memcpy( string, aptr->views[ VACM_VIEW_WRITE ], VACM_VACMSTRINGLEN );
            memcpy( aptr->views[ VACM_VIEW_WRITE ], var_val, var_val_len );
            aptr->views[ VACM_VIEW_WRITE ][ var_val_len ] = 0;
        }
    } else if ( action == IMPL_FREE ) {
        /*
         * Try to undo the SET here (abnormal usage of IMPL_FREE clause)
         */
        if ( ( aptr = access_parse_accessEntry( name, name_len ) ) != NULL && resetOnFail ) {
            memcpy( aptr->views[ VACM_VIEW_WRITE ], string, var_val_len );
        }
    }
    return PRIOT_ERR_NOERROR;
}

int write_vacmAccessNotifyViewName( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    static unsigned char string[ VACM_VACMSTRINGLEN ];
    struct Vacm_AccessEntry_s* aptr = NULL;
    static int resetOnFail;

    if ( action == IMPL_RESERVE1 ) {
        resetOnFail = 0;
        if ( var_val_type != ASN01_OCTET_STR ) {
            DEBUG_MSGTL( ( "mibII/vacm_vars",
                "write to vacmAccessNotifyViewName not ASN01_OCTET_STR\n" ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len > 32 ) {
            DEBUG_MSGTL( ( "mibII/vacm_vars",
                "write to vacmAccessNotifyViewName: bad length\n" ) );
            return PRIOT_ERR_WRONGLENGTH;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( aptr = access_parse_accessEntry( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        } else {
            resetOnFail = 1;
            memcpy( string, aptr->views[ VACM_VIEW_NOTIFY ], VACM_VACMSTRINGLEN );
            memcpy( aptr->views[ VACM_VIEW_NOTIFY ], var_val, var_val_len );
            aptr->views[ VACM_VIEW_NOTIFY ][ var_val_len ] = 0;
        }
    } else if ( action == IMPL_FREE ) {
        /*
         * Try to undo the SET here (abnormal usage of IMPL_FREE clause)
         */
        if ( ( aptr = access_parse_accessEntry( name, name_len ) ) != NULL && resetOnFail ) {
            memcpy( aptr->views[ VACM_VIEW_NOTIFY ], string, var_val_len );
        }
    }
    return PRIOT_ERR_NOERROR;
}

int view_parse_oid( oid* oidIndex, size_t oidLen,
    unsigned char** viewName, size_t* viewNameLen,
    oid** subtree, size_t* subtreeLen )
{
    int viewNameL, subtreeL, i;

    /*
     * first check the validity of the oid 
     */
    if ( ( oidLen <= 0 ) || ( !oidIndex ) ) {
        return PRIOT_ERR_INCONSISTENTNAME;
    }
    viewNameL = oidIndex[ 0 ];
    subtreeL = oidLen - viewNameL - 1; /* the initial name length */

    /*
     * its valid, malloc the space and store the results 
     */
    if ( viewName == NULL || subtree == NULL ) {
        return PRIOT_ERR_RESOURCEUNAVAILABLE;
    }

    if ( subtreeL < 0 ) {
        return PRIOT_ERR_NOCREATION;
    }

    *viewName = ( unsigned char* )malloc( viewNameL + 1 );

    if ( *viewName == NULL ) {
        return PRIOT_ERR_RESOURCEUNAVAILABLE;
    }

    *subtree = ( oid* )malloc( subtreeL * sizeof( oid ) );
    if ( *subtree == NULL ) {
        free( *viewName );
        return PRIOT_ERR_RESOURCEUNAVAILABLE;
    }

    *subtreeLen = subtreeL;
    *viewNameLen = viewNameL;

    for ( i = 0; i < viewNameL; i++ ) {
        if ( oidIndex[ i + 1 ] > 255 ) {
            free( *viewName );
            free( *subtree );
            return PRIOT_ERR_INCONSISTENTNAME;
        }
        viewName[ 0 ][ i ] = ( unsigned char )oidIndex[ i + 1 ];
    }
    viewName[ 0 ][ viewNameL ] = 0;

    for ( i = 0; i < subtreeL; i++ ) {
        subtree[ 0 ][ i ] = ( oid )oidIndex[ i + viewNameL + 1 ];
    }

    return 0;
}

struct Vacm_ViewEntry_s*
view_parse_viewEntry( oid* name, size_t name_len )
{
    struct Vacm_ViewEntry_s* vptr;

    char* newViewName;
    oid* newViewSubtree;
    size_t viewNameLen, viewSubtreeLen;

    if ( view_parse_oid( &name[ VIEW_MIB_LENGTH ], name_len - VIEW_MIB_LENGTH,
             ( u_char** )&newViewName, &viewNameLen,
             ( oid** )&newViewSubtree, &viewSubtreeLen ) )
        return NULL;

    vptr = Vacm_getViewEntry( newViewName, &newViewSubtree[ 1 ], viewSubtreeLen - 1,
        VACM_MODE_IGNORE_MASK );
    free( newViewName );
    free( newViewSubtree );

    return vptr;

} /* end vacm_parse_viewEntry() */

int write_vacmViewStatus( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    static long long_ret;
    char* newViewName;
    oid* newViewSubtree;
    size_t viewNameLen, viewSubtreeLen;
    struct Vacm_ViewEntry_s* vptr;
    int rc = 0;

    if ( action == IMPL_RESERVE1 ) {
        if ( var_val_type != ASN01_INTEGER ) {
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len != sizeof( long_ret ) ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
        long_ret = *( ( long* )var_val );
        if ( long_ret == TC_RS_NOTREADY || long_ret < 1 || long_ret > 6 ) {
            return PRIOT_ERR_WRONGVALUE;
        }

        /*
         * See if we can parse the oid for model/name first.  
         */
        if ( ( rc = view_parse_oid( &name[ VIEW_MIB_LENGTH ],
                   name_len - VIEW_MIB_LENGTH,
                   ( u_char** )&newViewName, &viewNameLen,
                   ( oid** )&newViewSubtree, &viewSubtreeLen ) ) ) {
            return rc;
        }

        if ( viewNameLen < 1 || viewNameLen > 32 ) {
            free( newViewName );
            free( newViewSubtree );
            return PRIOT_ERR_NOCREATION;
        }

        /*
         * Now see if a group already exists with these index values.  
         */
        vptr = Vacm_getViewEntry( newViewName, &newViewSubtree[ 1 ], viewSubtreeLen - 1,
            VACM_MODE_IGNORE_MASK );
        if ( vptr && Api_oidEquals( vptr->viewSubtree + 1, vptr->viewSubtreeLen - 1, newViewSubtree + 1, viewSubtreeLen - 1 ) != 0 ) {
            vptr = NULL;
        }
        if ( vptr != NULL ) {
            if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_CREATEANDWAIT ) {
                free( newViewName );
                free( newViewSubtree );
                long_ret = TC_RS_NOTREADY;
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
            if ( long_ret == TC_RS_DESTROY && vptr->viewStorageType == TC_ST_PERMANENT ) {
                free( newViewName );
                free( newViewSubtree );
                return PRIOT_ERR_WRONGVALUE;
            }
        } else {
            if ( long_ret == TC_RS_ACTIVE || long_ret == TC_RS_NOTINSERVICE ) {
                free( newViewName );
                free( newViewSubtree );
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
            if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_CREATEANDWAIT ) {

                /*
                 * Generate a new group entry.  
                 */
                if ( ( vptr = Vacm_createViewEntry( newViewName, &newViewSubtree[ 1 ],
                           viewSubtreeLen - 1 ) )
                    == NULL ) {
                    free( newViewName );
                    free( newViewSubtree );
                    return PRIOT_ERR_GENERR;
                }

                /*
                 * Set defaults.  
                 */
                vptr->viewStorageType = TC_ST_NONVOLATILE;
                vptr->viewStatus = TC_RS_NOTREADY;
                vptr->viewType = PRIOT_VIEW_INCLUDED;
            }
        }
        free( newViewName );
        free( newViewSubtree );
    } else if ( action == IMPL_ACTION ) {
        view_parse_oid( &name[ VIEW_MIB_LENGTH ], name_len - VIEW_MIB_LENGTH,
            ( u_char** )&newViewName, &viewNameLen,
            ( oid** )&newViewSubtree, &viewSubtreeLen );

        vptr = Vacm_getViewEntry( newViewName, &newViewSubtree[ 1 ], viewSubtreeLen - 1,
            VACM_MODE_IGNORE_MASK );

        if ( vptr != NULL ) {
            if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_ACTIVE ) {
                vptr->viewStatus = TC_RS_ACTIVE;
            } else if ( long_ret == TC_RS_CREATEANDWAIT ) {
                vptr->viewStatus = TC_RS_NOTINSERVICE;
            } else if ( long_ret == TC_RS_NOTINSERVICE ) {
                if ( vptr->viewStatus == TC_RS_ACTIVE ) {
                    vptr->viewStatus = TC_RS_NOTINSERVICE;
                } else if ( vptr->viewStatus == TC_RS_NOTREADY ) {
                    free( newViewName );
                    free( newViewSubtree );
                    return PRIOT_ERR_INCONSISTENTVALUE;
                }
            }
        }
        free( newViewName );
        free( newViewSubtree );
    } else if ( action == IMPL_COMMIT ) {
        view_parse_oid( &name[ VIEW_MIB_LENGTH ], name_len - VIEW_MIB_LENGTH,
            ( u_char** )&newViewName, &viewNameLen,
            ( oid** )&newViewSubtree, &viewSubtreeLen );

        vptr = Vacm_getViewEntry( newViewName, &newViewSubtree[ 1 ], viewSubtreeLen - 1,
            VACM_MODE_IGNORE_MASK );

        if ( vptr != NULL ) {
            if ( long_ret == TC_RS_DESTROY ) {
                Vacm_destroyViewEntry( newViewName, newViewSubtree,
                    viewSubtreeLen );
            }
        }
        free( newViewName );
        free( newViewSubtree );
    } else if ( action == IMPL_UNDO ) {
        if ( long_ret == TC_RS_CREATEANDGO || long_ret == TC_RS_CREATEANDWAIT ) {
            view_parse_oid( &name[ VIEW_MIB_LENGTH ],
                name_len - VIEW_MIB_LENGTH,
                ( u_char** )&newViewName, &viewNameLen,
                ( oid** )&newViewSubtree, &viewSubtreeLen );

            vptr = Vacm_getViewEntry( newViewName, &newViewSubtree[ 1 ],
                viewSubtreeLen - 1, VACM_MODE_IGNORE_MASK );

            if ( vptr != NULL ) {
                Vacm_destroyViewEntry( newViewName, newViewSubtree,
                    viewSubtreeLen );
            }
            free( newViewName );
            free( newViewSubtree );
        }
    }

    return PRIOT_ERR_NOERROR;
}

int write_vacmViewStorageType( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    long newValue = *( ( long* )var_val );
    static long oldValue;
    struct Vacm_ViewEntry_s* vptr = NULL;

    if ( action == IMPL_RESERVE1 ) {
        if ( var_val_type != ASN01_INTEGER ) {
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len != sizeof( long ) ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( vptr = view_parse_viewEntry( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        } else {
            if ( ( newValue == TC_ST_VOLATILE || newValue == TC_ST_NONVOLATILE ) && ( vptr->viewStorageType == TC_ST_VOLATILE || vptr->viewStorageType == TC_ST_NONVOLATILE ) ) {
                oldValue = vptr->viewStorageType;
                vptr->viewStorageType = newValue;
            } else if ( newValue == vptr->viewStorageType ) {
                return PRIOT_ERR_NOERROR;
            } else {
                return PRIOT_ERR_INCONSISTENTVALUE;
            }
        }
    } else if ( action == IMPL_UNDO ) {
        if ( ( vptr = view_parse_viewEntry( name, name_len ) ) != NULL ) {
            vptr->viewStorageType = oldValue;
        }
    }

    return PRIOT_ERR_NOERROR;
}

int write_vacmViewMask( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    static unsigned char string[ VACM_VACMSTRINGLEN ];
    static long length;
    struct Vacm_ViewEntry_s* vptr = NULL;

    if ( action == IMPL_RESERVE1 ) {
        if ( var_val_type != ASN01_OCTET_STR ) {
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len > 16 ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( vptr = view_parse_viewEntry( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        } else {
            memcpy( string, vptr->viewMask, vptr->viewMaskLen );
            length = vptr->viewMaskLen;
            memcpy( vptr->viewMask, var_val, var_val_len );
            vptr->viewMaskLen = var_val_len;
        }
    } else if ( action == IMPL_FREE ) {
        if ( ( vptr = view_parse_viewEntry( name, name_len ) ) != NULL ) {
            memcpy( vptr->viewMask, string, length );
            vptr->viewMaskLen = length;
        }
    }
    return PRIOT_ERR_NOERROR;
}

int write_vacmViewType( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    long newValue = *( ( long* )var_val );
    static long oldValue;
    struct Vacm_ViewEntry_s* vptr = NULL;

    if ( action == IMPL_RESERVE1 ) {
        if ( var_val_type != ASN01_INTEGER ) {
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len != sizeof( long ) ) {
            return PRIOT_ERR_WRONGLENGTH;
        }
        if ( newValue < 1 || newValue > 2 ) {
            return PRIOT_ERR_WRONGVALUE;
        }
    } else if ( action == IMPL_RESERVE2 ) {
        if ( ( vptr = view_parse_viewEntry( name, name_len ) ) == NULL ) {
            return PRIOT_ERR_INCONSISTENTNAME;
        } else {
            oldValue = vptr->viewType;
            vptr->viewType = newValue;
        }
    } else if ( action == IMPL_UNDO ) {
        if ( ( vptr = view_parse_viewEntry( name, name_len ) ) != NULL ) {
            vptr->viewType = oldValue;
        }
    }

    return PRIOT_ERR_NOERROR;
}

int write_vacmViewSpinLock( int action,
    u_char* var_val,
    u_char var_val_type,
    size_t var_val_len,
    u_char* statP, oid* name, size_t name_len )
{
    static long long_ret;

    if ( action == IMPL_RESERVE1 ) {
        if ( var_val_type != ASN01_INTEGER ) {
            DEBUG_MSGTL( ( "mibII/vacm_vars",
                "write to vacmViewSpinLock not ASN01_INTEGER\n" ) );
            return PRIOT_ERR_WRONGTYPE;
        }
        if ( var_val_len != sizeof( long_ret ) ) {
            DEBUG_MSGTL( ( "mibII/vacm_vars",
                "write to vacmViewSpinLock: bad length\n" ) );
            return PRIOT_ERR_WRONGLENGTH;
        }
        long_ret = *( ( long* )var_val );
        if ( long_ret != ( long )vacmViewSpinLock ) {
            return PRIOT_ERR_INCONSISTENTVALUE;
        }
    } else if ( action == IMPL_COMMIT ) {
        if ( vacmViewSpinLock == 2147483647 ) {
            vacmViewSpinLock = 0;
        } else {
            vacmViewSpinLock++;
        }
    }
    return PRIOT_ERR_NOERROR;
}
