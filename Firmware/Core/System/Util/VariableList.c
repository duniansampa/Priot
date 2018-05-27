#include "System/Util/VariableList.h"
#include "Priot.h"
#include "TextualConvention.h"

int VariableList_checkType( const VariableList* var, int type )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( var->type != type ) {
        rc = PRIOT_ERR_WRONGTYPE;
    }

    return rc;
}

int VariableList_checkLength( const VariableList* var, size_t valueSize )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    else if ( var->valueLength != valueSize ) {
        rc = PRIOT_ERR_WRONGLENGTH;
    }

    return rc;
}

int VariableList_checkMaxLength( const VariableList* var, size_t maxValueSize )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    else if ( var->valueLength > maxValueSize ) {
        rc = PRIOT_ERR_WRONGLENGTH;
    }

    return rc;
}

int VariableList_checkIntegerRange( const VariableList* var, size_t low, size_t high )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( ( size_t )*var->value.integer < low ) || ( ( size_t )*var->value.integer > high ) ) {
        rc = PRIOT_ERR_WRONGVALUE;
    }

    return rc;
}

int VariableList_checkLengthRange( const VariableList* var,
    size_t low,
    size_t high )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    if ( ( var->valueLength < low ) || ( var->valueLength > high ) ) {
        rc = PRIOT_ERR_WRONGLENGTH;
    }

    return rc;
}

int VariableList_checkTypeAndLength( const VariableList* var, int type, size_t valueSize )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    rc = VariableList_checkType( var, type );

    if ( rc == PRIOT_ERR_NOERROR ) {
        rc = VariableList_checkLength( var, valueSize );
    }

    return rc;
}

int VariableList_checkTypeAndMaxLength( const VariableList* var, int type, size_t maxValueSize )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    rc = VariableList_checkType( var, type );

    if ( rc == PRIOT_ERR_NOERROR )
        rc = VariableList_checkMaxLength( var, maxValueSize );

    return rc;
}

int VariableList_checkOidMaxLength( const VariableList* var )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    rc = VariableList_checkType( var, ASN01_OBJECT_ID );

    if ( rc == PRIOT_ERR_NOERROR )
        rc = VariableList_checkMaxLength( var, TYPES_MAX_OID_LEN * sizeof( oid ) );

    return rc;
}

int VariableList_checkIntLength( const VariableList* var )
{
    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    return VariableList_checkTypeAndLength( var, ASN01_INTEGER, sizeof( long ) );
}

int VariableList_checkUIntLength( const VariableList* var )
{
    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    return VariableList_checkTypeAndLength( var, ASN01_UNSIGNED, sizeof( long ) );
}

int VariableList_checkIntLengthAndRange( const VariableList* var, int low, int high )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    rc = VariableList_checkTypeAndLength( var, ASN01_INTEGER, sizeof( long ) );

    if ( rc == PRIOT_ERR_NOERROR ) {
        if ( ( *var->value.integer < low ) || ( *var->value.integer > high ) ) {
            rc = PRIOT_ERR_WRONGVALUE;
        }
    }

    return rc;
}

int VariableList_checkBoolLengthAndValue( const VariableList* var )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    rc = VariableList_checkTypeAndLength( var, ASN01_INTEGER, sizeof( long ) );

    if ( rc == PRIOT_ERR_NOERROR )
        rc = VariableList_checkIntLengthAndRange( var, 1, 2 );

    return rc;
}

int VariableList_checkRowStatusLengthAndRange( const VariableList* var )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    rc = VariableList_checkTypeAndLength( var, ASN01_INTEGER, sizeof( long ) );

    if ( rc == PRIOT_ERR_NOERROR ) {
        if ( *var->value.integer == TC_RS_NOTREADY )
            rc = PRIOT_ERR_WRONGVALUE;
    }

    if ( rc == PRIOT_ERR_NOERROR ) {
        rc = VariableList_checkIntLengthAndRange( var, PRIOT_ROW_NONEXISTENT, PRIOT_ROW_DESTROY );
    }

    return rc;
}

int VariableList_checkRowStatusTransition( const VariableList* var, int oldValue )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    rc = VariableList_checkRowStatusLengthAndRange( var );

    if ( rc == PRIOT_ERR_NOERROR ) {
        rc = Tc_checkRowstatusTransition( oldValue, *var->value.integer );
    }

    return rc;
}

int VariableList_checkRowStatusWithStorageType( const VariableList* var, int oldValue, int oldStorage )
{
    register int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    rc = VariableList_checkRowStatusLengthAndRange( var );

    if ( rc == PRIOT_ERR_NOERROR ) {
        rc = Tc_checkRowstatusWithStoragetypeTransition( oldValue, *var->value.integer, oldStorage );
    }

    return rc;
}

int VariableList_checkStorageType( const VariableList* var, int old_value )
{
    int rc = PRIOT_ERR_NOERROR;

    if ( NULL == var )
        return PRIOT_ERR_GENERR;

    rc = VariableList_checkTypeAndLength( var, ASN01_INTEGER, sizeof( long ) );

    if ( rc == PRIOT_ERR_NOERROR ) {
        rc = VariableList_checkIntLengthAndRange( var, PRIOT_STORAGE_NONE, PRIOT_STORAGE_READONLY );
    }

    if ( rc == PRIOT_ERR_NOERROR ) {
        rc = Tc_checkStorageTransition( old_value, *var->value.integer );
    }

    return rc;
}
