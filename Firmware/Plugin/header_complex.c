/*
 * header complex:  More complex storage and data sorting for mib modules 
 */

#include "header_complex.h"
#include "System/Util/Trace.h"
#include "System/Util/Logger.h"

int header_complex_generate_varoid( VariableList* var )
{
    int i;

    if ( var->name == NULL ) {
        /*
         * assume cached value is correct 
         */
        switch ( var->type ) {
        case ASN01_INTEGER:
        case ASN01_COUNTER:
        case ASN01_GAUGE:
        case ASN01_TIMETICKS:
            var->nameLength = 1;
            var->name = ( oid* )malloc( sizeof( oid ) );
            if ( var->name == NULL )
                return ErrorCode_GENERR;
            var->name[ 0 ] = *( var->value.integer );
            break;

        case ASN01_PRIV_IMPLIED_OBJECT_ID:
            var->nameLength = var->valueLength / sizeof( oid );
            var->name = ( oid* )malloc( sizeof( oid ) * ( var->nameLength ) );
            if ( var->name == NULL )
                return ErrorCode_GENERR;

            for ( i = 0; i < ( int )var->nameLength; i++ )
                var->name[ i ] = var->value.objectId[ i ];
            break;

        case ASN01_OBJECT_ID:
            var->nameLength = var->valueLength / sizeof( oid ) + 1;
            var->name = ( oid* )malloc( sizeof( oid ) * ( var->nameLength ) );
            if ( var->name == NULL )
                return ErrorCode_GENERR;

            var->name[ 0 ] = var->nameLength - 1;
            for ( i = 0; i < ( int )var->nameLength - 1; i++ )
                var->name[ i + 1 ] = var->value.objectId[ i ];
            break;

        case ASN01_PRIV_IMPLIED_OCTET_STR:
            var->nameLength = var->valueLength;
            var->name = ( oid* )malloc( sizeof( oid ) * ( var->nameLength ) );
            if ( var->name == NULL )
                return ErrorCode_GENERR;

            for ( i = 0; i < ( int )var->valueLength; i++ )
                var->name[ i ] = ( oid )var->value.string[ i ];
            break;

        case ASN01_OPAQUE:
        case ASN01_OCTET_STR:
            var->nameLength = var->valueLength + 1;
            var->name = ( oid* )malloc( sizeof( oid ) * ( var->nameLength ) );
            if ( var->name == NULL )
                return ErrorCode_GENERR;

            var->name[ 0 ] = ( oid )var->valueLength;
            for ( i = 0; i < ( int )var->valueLength; i++ )
                var->name[ i + 1 ] = ( oid )var->value.string[ i ];
            break;

        default:
            DEBUG_MSGTL( ( "header_complex_generate_varoid",
                "invalid asn type: %d\n", var->type ) );
            return ErrorCode_GENERR;
        }
    }
    if ( var->nameLength > ASN01_MAX_OID_LEN ) {
        DEBUG_MSGTL( ( "header_complex_generate_varoid",
            "Something terribly wrong, namelen = %d\n",
            ( int )var->nameLength ) );
        return ErrorCode_GENERR;
    }

    return ErrorCode_SUCCESS;
}

/*
 * header_complex_parse_oid(): parses an index to the usmTable to
 * break it down into a engineID component and a name component.
 * The results are stored in the data pointer, as a varbindlist:
 * 
 * 
 * returns 1 if an error is encountered, or 0 if successful.
 */
int header_complex_parse_oid( oid* oidIndex, size_t oidLen,
    VariableList* data )
{
    VariableList* var = data;
    int i, itmp;

    while ( var && oidLen > 0 ) {
        switch ( var->type ) {
        case ASN01_INTEGER:
        case ASN01_COUNTER:
        case ASN01_GAUGE:
        case ASN01_TIMETICKS:
            var->value.integer = ( long* )calloc( 1, sizeof( long ) );
            if ( var->value.string == NULL )
                return ErrorCode_GENERR;

            *var->value.integer = ( long )*oidIndex++;
            var->valueLength = sizeof( long );
            oidLen--;
            DEBUG_MSGTL( ( "header_complex_parse_oid",
                "Parsed int(%d): %ld\n", var->type,
                *var->value.integer ) );
            break;

        case ASN01_OBJECT_ID:
        case ASN01_PRIV_IMPLIED_OBJECT_ID:
            if ( var->type == ASN01_PRIV_IMPLIED_OBJECT_ID ) {
                itmp = oidLen;
            } else {
                itmp = ( long )*oidIndex++;
                oidLen--;
                if ( itmp > ( int )oidLen )
                    return ErrorCode_GENERR;
            }

            if ( itmp == 0 )
                break; /* zero length strings shouldn't malloc */

            var->valueLength = itmp * sizeof( oid );
            var->value.objectId = ( oid* )calloc( 1, var->valueLength );
            if ( var->value.objectId == NULL )
                return ErrorCode_GENERR;

            for ( i = 0; i < itmp; i++ )
                var->value.objectId[ i ] = ( u_char )*oidIndex++;
            oidLen -= itmp;

            DEBUG_MSGTL( ( "header_complex_parse_oid", "Parsed oid: " ) );
            DEBUG_MSGOID( ( "header_complex_parse_oid", var->value.objectId,
                var->valueLength / sizeof( oid ) ) );
            DEBUG_MSG( ( "header_complex_parse_oid", "\n" ) );
            break;

        case ASN01_OPAQUE:
        case ASN01_OCTET_STR:
        case ASN01_PRIV_IMPLIED_OCTET_STR:
            if ( var->type == ASN01_PRIV_IMPLIED_OCTET_STR ) {
                itmp = oidLen;
            } else {
                itmp = ( long )*oidIndex++;
                oidLen--;
                if ( itmp > ( int )oidLen )
                    return ErrorCode_GENERR;
            }

            if ( itmp == 0 )
                break; /* zero length strings shouldn't malloc */

            /*
             * malloc by size+1 to allow a null to be appended. 
             */
            var->valueLength = itmp;
            var->value.string = ( u_char* )calloc( 1, itmp + 1 );
            if ( var->value.string == NULL )
                return ErrorCode_GENERR;

            for ( i = 0; i < itmp; i++ )
                var->value.string[ i ] = ( u_char )*oidIndex++;
            var->value.string[ itmp ] = '\0';
            oidLen -= itmp;

            DEBUG_MSGTL( ( "header_complex_parse_oid",
                "Parsed str(%d): %s\n", var->type,
                var->value.string ) );
            break;

        default:
            DEBUG_MSGTL( ( "header_complex_parse_oid",
                "invalid asn type: %d\n", var->type ) );
            return ErrorCode_GENERR;
        }
        var = var->next;
    }
    if ( var != NULL || oidLen > 0 )
        return ErrorCode_GENERR;
    return ErrorCode_SUCCESS;
}

void header_complex_generate_oid( oid* name, /* out */
    size_t* length, /* out */
    oid* prefix,
    size_t prefix_len,
    VariableList* data )
{

    oid* oidptr;
    VariableList* var;

    if ( prefix ) {
        memcpy( name, prefix, prefix_len * sizeof( oid ) );
        oidptr = ( name + ( prefix_len ) );
        *length = prefix_len;
    } else {
        oidptr = name;
        *length = 0;
    }

    for ( var = data; var != NULL; var = var->next ) {
        header_complex_generate_varoid( var );
        memcpy( oidptr, var->name, sizeof( oid ) * var->nameLength );
        oidptr = oidptr + var->nameLength;
        *length += var->nameLength;
    }

    DEBUG_MSGTL( ( "header_complex_generate_oid", "generated: " ) );
    DEBUG_MSGOID( ( "header_complex_generate_oid", name, *length ) );
    DEBUG_MSG( ( "header_complex_generate_oid", "\n" ) );
}

/*
 * finds the data in "datalist" stored at "index" 
 */
void* header_complex_get( struct header_complex_index* datalist,
    VariableList* index )
{
    oid searchfor[ ASN01_MAX_OID_LEN ];
    size_t searchfor_len;

    header_complex_generate_oid( searchfor, /* out */
        &searchfor_len, /* out */
        NULL, 0, index );
    return header_complex_get_from_oid( datalist, searchfor, searchfor_len );
}

void* header_complex_get_from_oid( struct header_complex_index* datalist,
    oid* searchfor, size_t searchfor_len )
{
    struct header_complex_index* nptr;
    for ( nptr = datalist; nptr != NULL; nptr = nptr->next ) {
        if ( Api_oidEquals( searchfor, searchfor_len,
                 nptr->name, nptr->namelen )
            == 0 )
            return nptr->data;
    }
    return NULL;
}

void* header_complex( struct header_complex_index* datalist,
    struct Variable_s* vp,
    oid* name,
    size_t* length,
    int exact, size_t* var_len, WriteMethodFT** write_method )
{

    struct header_complex_index *nptr, *found = NULL;
    oid indexOid[ ASN01_MAX_OID_LEN ];
    size_t len;
    int result;

    /*
     * set up some nice defaults for the user 
     */
    if ( write_method )
        *write_method = NULL;
    if ( var_len )
        *var_len = sizeof( long );

    for ( nptr = datalist; nptr != NULL && found == NULL; nptr = nptr->next ) {
        if ( vp ) {
            memcpy( indexOid, vp->name, vp->namelen * sizeof( oid ) );
            memcpy( indexOid + vp->namelen, nptr->name,
                nptr->namelen * sizeof( oid ) );
            len = vp->namelen + nptr->namelen;
        } else {
            memcpy( indexOid, nptr->name, nptr->namelen * sizeof( oid ) );
            len = nptr->namelen;
        }
        result = Api_oidCompare( name, *length, indexOid, len );
        DEBUG_MSGTL( ( "header_complex", "Checking: " ) );
        DEBUG_MSGOID( ( "header_complex", indexOid, len ) );
        DEBUG_MSG( ( "header_complex", "\n" ) );

        if ( exact ) {
            if ( result == 0 ) {
                found = nptr;
            }
        } else {
            if ( result == 0 ) {
                /*
                 * found an exact match.  Need the next one for !exact 
                 */
                if ( nptr->next )
                    found = nptr->next;
            } else if ( result == -1 ) {
                found = nptr;
            }
        }
    }
    if ( found ) {
        if ( vp ) {
            memcpy( name, vp->name, vp->namelen * sizeof( oid ) );
            memcpy( name + vp->namelen, found->name,
                found->namelen * sizeof( oid ) );
            *length = vp->namelen + found->namelen;
        } else {
            memcpy( name, found->name, found->namelen * sizeof( oid ) );
            *length = found->namelen;
        }
        return found->data;
    }

    return NULL;
}

struct header_complex_index*
header_complex_maybe_add_data( struct header_complex_index** thedata,
    VariableList* var, void* data,
    int dont_allow_duplicates )
{
    oid newoid[ ASN01_MAX_OID_LEN ];
    size_t newoid_len;
    struct header_complex_index* ret;

    if ( thedata == NULL || var == NULL || data == NULL )
        return NULL;

    header_complex_generate_oid( newoid, &newoid_len, NULL, 0, var );
    ret = header_complex_maybe_add_data_by_oid( thedata, newoid, newoid_len, data,
        dont_allow_duplicates );
    /*
     * free the variable list, but not the enclosed data!  it's not ours! 
     */
    Api_freeVarbind( var );
    return ( ret );
}

struct header_complex_index*
header_complex_add_data( struct header_complex_index** thedata,
    VariableList* var, void* data )
{
    return header_complex_maybe_add_data( thedata, var, data, 0 );
}

struct header_complex_index*
_header_complex_add_between( struct header_complex_index** thedata,
    struct header_complex_index* hciptrp,
    struct header_complex_index* hciptrn,
    oid* newoid, size_t newoid_len, void* data )
{
    struct header_complex_index* ourself;

    /*
     * nptr should now point to the spot that we need to add ourselves
     * in front of, and pptr should be our new 'prev'. 
     */

    /*
     * create ourselves 
     */
    ourself = ( struct header_complex_index* )
        MEMORY_MALLOC_STRUCT( header_complex_index );
    if ( ourself == NULL )
        return NULL;

    /*
     * change our pointers 
     */
    ourself->prev = hciptrp;
    ourself->next = hciptrn;

    if ( ourself->next )
        ourself->next->prev = ourself;

    if ( ourself->prev )
        ourself->prev->next = ourself;

    ourself->data = data;
    ourself->name = Api_duplicateObjid( newoid, newoid_len );
    ourself->namelen = newoid_len;

    /*
     * rewind to the head of the list and return it (since the new head
     * could be us, we need to notify the above routine who the head now is. 
     */
    for ( hciptrp = ourself; hciptrp->prev != NULL;
          hciptrp = hciptrp->prev )
        ;

    *thedata = hciptrp;
    DEBUG_MSGTL( ( "header_complex_add_data", "adding something...\n" ) );

    return hciptrp;
}

struct header_complex_index*
header_complex_maybe_add_data_by_oid( struct header_complex_index** thedata,
    oid* newoid, size_t newoid_len, void* data,
    int dont_allow_duplicates )
{
    struct header_complex_index *hciptrn, *hciptrp;
    int rc;

    if ( thedata == NULL || newoid == NULL || data == NULL )
        return NULL;

    for ( hciptrn = *thedata, hciptrp = NULL;
          hciptrn != NULL; hciptrp = hciptrn, hciptrn = hciptrn->next ) {
        /*
         * XXX: check for == and error (overlapping table entries) 
         * 8/2005 rks Ok, I added duplicate entry check, but only log
         *            warning and continue, because it seems that nobody
         *            that calls this fucntion does error checking!.
         */
        rc = Api_oidCompare( hciptrn->name, hciptrn->namelen,
            newoid, newoid_len );
        if ( rc > 0 )
            break;
        else if ( 0 == rc ) {
            Logger_log( LOGGER_PRIORITY_WARNING, "header_complex_add_data_by_oid with "
                                                 "duplicate index.\n" );
            if ( dont_allow_duplicates )
                return NULL;
        }
    }

    return _header_complex_add_between( thedata, hciptrp, hciptrn,
        newoid, newoid_len, data );
}

struct header_complex_index*
header_complex_add_data_by_oid( struct header_complex_index** thedata,
    oid* newoid, size_t newoid_len, void* data )
{
    return header_complex_maybe_add_data_by_oid( thedata, newoid, newoid_len,
        data, 0 );
}

/*
 * extracts an entry from the storage space (removing it from future
 * accesses) and returns the data stored there
 * 
 * Modifies "thetop" pointer as needed (and if present) if were
 * extracting the first node.
 */

void* header_complex_extract_entry( struct header_complex_index** thetop,
    struct header_complex_index* thespot )
{
    struct header_complex_index *hciptrp, *hciptrn;
    void* retdata;

    if ( thespot == NULL ) {
        DEBUG_MSGTL( ( "header_complex_extract_entry",
            "Null pointer asked to be extracted\n" ) );
        return NULL;
    }

    retdata = thespot->data;

    hciptrp = thespot->prev;
    hciptrn = thespot->next;

    if ( hciptrp )
        hciptrp->next = hciptrn;
    else if ( thetop )
        *thetop = hciptrn;

    if ( hciptrn )
        hciptrn->prev = hciptrp;

    if ( thespot->name )
        free( thespot->name );

    free( thespot );
    return retdata;
}

/*
 * wipe out a single entry 
 */
void header_complex_free_entry( struct header_complex_index* theentry,
    HeaderComplexCleaner* cleaner )
{
    void* data;
    data = header_complex_extract_entry( NULL, theentry );
    ( *cleaner )( data );
}

/*
 * completely wipe out all entries in our data store 
 */
void header_complex_free_all( struct header_complex_index* thestuff,
    HeaderComplexCleaner* cleaner )
{
    struct header_complex_index *hciptr, *hciptrn;

    for ( hciptr = thestuff; hciptr != NULL; hciptr = hciptrn ) {
        hciptrn = hciptr->next; /* need to extract this before deleting it */
        header_complex_free_entry( hciptr, cleaner );
    }
}

struct header_complex_index*
header_complex_find_entry( struct header_complex_index* thestuff,
    void* theentry )
{
    struct header_complex_index* hciptr;

    for ( hciptr = thestuff; hciptr != NULL && hciptr->data != theentry;
          hciptr = hciptr->next )
        ;
    return hciptr;
}
