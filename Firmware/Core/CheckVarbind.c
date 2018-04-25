#include "CheckVarbind.h"
#include "Priot.h"
#include "Tc.h"

int CheckVarbind_type( const Types_VariableList* var, int type )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( var->type != type ) {
        rc = PRIOT_ERR_WRONGTYPE;
    }

    return rc;
}

int CheckVarbind_size( const Types_VariableList* var, size_t size )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    else if ( var->valLen != size ) {
        rc = PRIOT_ERR_WRONGLENGTH;
    }

    return rc;
}

int CheckVarbind_maxSize( const Types_VariableList* var, size_t size )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    else if ( var->valLen > size ) {
        rc = PRIOT_ERR_WRONGLENGTH;
    }

    return rc;
}

int CheckVarbind_range( const Types_VariableList* var,
    size_t low,
    size_t high )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( ( size_t )*var->val.integer < low ) || ( ( size_t )*var->val.integer > high ) ) {
        rc = PRIOT_ERR_WRONGVALUE;
    }

    return rc;
}

int CheckVarbind_sizeRange( const Types_VariableList* var,
    size_t low,
    size_t high )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( var->valLen < low ) || ( var->valLen > high ) ) {
        rc = PRIOT_ERR_WRONGLENGTH;
    }

    return rc;
}

int CheckVarbind_typeAndSize( const Types_VariableList* var,
    int type,
    size_t size )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( rc = CheckVarbind_type( var, type ) ) )
        ;
    else
        rc = CheckVarbind_size( var, size );

    return rc;
}

int CheckVarbind_typeAndMaxSize( const Types_VariableList* var,
    int type,
    size_t size )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( rc = CheckVarbind_type( var, type ) ) )
        ;
    else
        rc = CheckVarbind_maxSize( var, size );

    return rc;
}

int CheckVarbind_oid( const Types_VariableList* var )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( rc = CheckVarbind_type( var, ASN01_OBJECT_ID ) ) )
        ;
    else
        rc = CheckVarbind_maxSize( var, TYPES_MAX_OID_LEN * sizeof( oid ) );

    return rc;
}

int CheckVarbind_int( const Types_VariableList* var )
{
    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    return CheckVarbind_typeAndSize( var, ASN01_INTEGER, sizeof( long ) );
}

int CheckVarbind_uint( const Types_VariableList* var )
{
    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    return CheckVarbind_typeAndSize( var, ASN01_UNSIGNED, sizeof( long ) );
}

int CheckVarbind_intRange( const Types_VariableList* var, int low, int high )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( rc = CheckVarbind_typeAndSize( var, ASN01_INTEGER, sizeof( long ) ) ) )
        return rc;

    if ( ( *var->val.integer < low ) || ( *var->val.integer > high ) ) {
        rc = PRIOT_ERR_WRONGVALUE;
    }

    return rc;
}

int CheckVarbind_truthValue( const Types_VariableList* var )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( rc = CheckVarbind_typeAndSize( var, ASN01_INTEGER, sizeof( long ) ) ) )
        return rc;

    return CheckVarbind_intRange( var, 1, 2 );
}

int CheckVarbind_rowStatusValue( const Types_VariableList* var )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( rc = CheckVarbind_typeAndSize( var, ASN01_INTEGER, sizeof( long ) ) ) )
        return rc;

    if ( *var->val.integer == TC_RS_NOTREADY )
        return PRIOT_ERR_WRONGVALUE;

    return CheckVarbind_intRange( var, PRIOT_ROW_NONEXISTENT, PRIOT_ROW_DESTROY );
}

int CheckVarbind_rowStatus( const Types_VariableList* var, int old_value )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( rc = CheckVarbind_rowStatusValue( var ) ) )
        return rc;

    return Tc_checkRowstatusTransition( old_value, *var->val.integer );
}

int CheckVarbind_rowStatusWithStorageType( const Types_VariableList* var,
    int old_value,
    int old_storage )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( rc = CheckVarbind_rowStatusValue( var ) ) )
        return rc;

    return Tc_checkRowstatusWithStoragetypeTransition(
        old_value, *var->val.integer, old_storage );
}

int CheckVarbind_storageType( const Types_VariableList* var, int old_value )
{
    int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( rc = CheckVarbind_typeAndSize( var, ASN01_INTEGER, sizeof( long ) ) ) )
        return rc;

    if ( ( rc = CheckVarbind_intRange( var, PRIOT_STORAGE_NONE,
               PRIOT_STORAGE_READONLY ) ) )
        return rc;

    return Tc_checkStorageTransition( old_value, *var->val.integer );
}
