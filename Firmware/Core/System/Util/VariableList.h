#ifndef IOT_VARIABLELIST_H
#define IOT_VARIABLELIST_H

/** \file Map.h
 *  @brief  Assorted convience routines to check the contents of a
 *          VariableList instance.
 *
 *  \author Dunian Coutinho Sampa (duniansampa)
 *  \bug    No known bugs.
 */

#include "Asn01.h"
#include "Generals.h"

/** The value of ASN supported type*/
typedef union VarData_u {
    long* integer;
    u_char* string;
    oid* objectId;
    u_char* bitString;
    Counter64* counter64;
    float* floatValue;
    double* doubleValue;
} VarData;

/**
 * @struct VariableList_s
 * The VariableList list binding structure
 */
typedef struct VariableList_s {
    /** NULL for last variable */
    struct VariableList_s* next;
    /** Object identifier of variable */
    oid* name;
    /** number of subid's in name */
    size_t nameLength;
    /** ASN type of variable */
    u_char type;
    /** value of variable */
    VarData value;
    /** the length of the value to be copied into buf */
    size_t valueLength;
    /** buffer to hold the OID */
    oid nameLoc[ TYPES_MAX_OID_LEN ];
    /** 90 percentile < 40. */
    u_char buffer[ 40 ];
    /** (Opaque) hook for additional data */
    void* data;
    /** callback to free above */
    void ( *dataFreeHook )( void* );
    int index;
} VariableList;

/**
 * @brief VariableList_checkType
 *         checks if the variable @p var is type @p type,
 *         comparing the @c var->type with the @p type.
 *
 * @param var - variable to be analyzed.
 * @param type -  the type, for example:
 *                 ASN01_INTEGER, ASN01_BOOLEAN, etc.
 *
 * @retval PRIOT_ERR_NOERROR : on success;
 * @retval PRIOT_ERR_GENERR : in case of error (if var == NULL)
 * @retval PRIOT_ERR_WRONGTYPE: if var->type != type.
 */
int VariableList_checkType( const VariableList* var, int type );

/**
 * @brief VariableList_checkLength
 *        checks if @c var->valueLength is equal to valueSize.
 *
 * @param var - variable to be analyzed.
 * @param valueSize - the length to be used in the comparison
 *
 * @retval PRIOT_ERR_NOERROR : on success;
 * @retval PRIOT_ERR_GENERR : in case of error (if var == NULL)
 * @retval PRIOT_ERR_WRONGLENGTH: if var->valueLength != valueSize.
 */
int VariableList_checkLength( const VariableList* var, size_t valueSize );

/**
 * @brief VariableList_checkMaxLength
 *        checks if @c var->valueLength is greater than maxValueSize.
 *
 * @param var - variable to be analyzed.
 * @param maxValueSize - the length to be used in the comparison
 *
 * @retval PRIOT_ERR_NOERROR : on success;
 * @retval PRIOT_ERR_GENERR : in case of error (if var == NULL)
 * @retval PRIOT_ERR_WRONGLENGTH: if var->valueLength > maxValueSize.
 */
int VariableList_checkMaxLength( const VariableList* var, size_t maxValueSize );

/**
 * @brief VariableList_checkIntegerRange
 *        checks whether the value of variable @c var->value.integer is within the
 *        range @p low and @p high, that is, var->value.integer >= low and var->value.integer <= high.
 *
 * @param var - variable to be analyzed.
 * @param low - the minimum value
 * @param high - the maximum value
 *
 * @retval PRIOT_ERR_NOERROR : on success;
 * @retval PRIOT_ERR_GENERR : in case of error (if var == NULL)
 * @retval PRIOT_ERR_WRONGVALUE: if var->value.integer < low or var->value.integer > high.
 */
int VariableList_checkIntegerRange( const VariableList* var, size_t low, size_t high );

/**
 * @brief VariableList_checkLengthRange
 *        checks whether the value of variable @c var->valueLength is within the
 *        range @p low and @p high, that is, var->valueLength >= low and var->valueLength <= high.
 *
 * @param var - variable to be analyzed.
 * @param low - the minimum value
 * @param high - the maximum value
 *
 * @retval PRIOT_ERR_NOERROR : on success;
 * @retval PRIOT_ERR_GENERR : in case of error (if var == NULL)
 * @retval PRIOT_ERR_WRONGLENGTH: if var->value.integer < low or var->value.integer > high.
 */
int VariableList_checkLengthRange( const VariableList* var, size_t low, size_t high );

/**
 * @brief VariableList_checkTypeAndLength
 *        checks if @c var->type is equal to @p type and
 *        checks if @c var->valueLength is equal to  @p valueSize.
 *
 * @param var - variable to be analyzed.
 * @param type -  the type, for example:
 *                 ASN01_INTEGER, ASN01_BOOLEAN, etc.
 * @param valueSize - the length to be used in the comparison
 *
 * @retval PRIOT_ERR_NOERROR : on success;
 * @retval PRIOT_ERR_GENERR : in case of error (if var == NULL)
 * @retval PRIOT_ERR_WRONGTYPE: if var->type != type
 * @retval PRIOT_ERR_WRONGLENGTH: if var->type == type and var->valueLength != valueSize.
 */
int VariableList_checkTypeAndLength( const VariableList* var, int type, size_t valueSize );

/**
 * @brief VariableList_checkTypeAndMaxLength
 *        checks if @c var->type is equal to @p type and
 *        checks if @c var->valueLength is greater than maxValueSize.
 *
 *
 * @param var - variable to be analyzed.
 * @param type -  the type, for example:
 *                 ASN01_INTEGER, ASN01_BOOLEAN, etc.
 * @param maxValueSize - the length to be used in the comparison
 *
 * @retval PRIOT_ERR_NOERROR : on success;
 * @retval PRIOT_ERR_GENERR : in case of error (if var == NULL)
 * @retval PRIOT_ERR_WRONGTYPE: if var->type != type
 * @retval PRIOT_ERR_WRONGLENGTH: if var->type == type and var->valueLength > maxValueSize.
 */
int VariableList_checkTypeAndMaxLength( const VariableList* var, int type, size_t maxValueSize );

/**
 * @brief VariableList_checkOidMaxLength
 *        checks if @c var->type is equal to ASN01_OBJECT_ID and
 *        checks if @c var->valueLength is greater than TYPES_MAX_OID_LEN * sizeof( oid ).
 *
 *
 * @param var - variable to be analyzed.
 *
 * @retval PRIOT_ERR_NOERROR : on success;
 * @retval PRIOT_ERR_GENERR : in case of error (if var == NULL)
 * @retval PRIOT_ERR_WRONGTYPE: if var->type != ASN01_OBJECT_ID
 * @retval PRIOT_ERR_WRONGLENGTH: if var->type == ASN01_OBJECT_ID
 *                                 and var->valueLength > TYPES_MAX_OID_LEN * sizeof( oid ).
 */
int VariableList_checkOidMaxLength( const VariableList* var );

/**
 * @brief VariableList_checkIntLength
 *        checks if @c var->type is equal to ASN01_INTEGER and
 *        checks if @c var->valueLength is equal to  sizeof( long ).
 *
 * @param var - variable to be analyzed.
 *
 * @retval PRIOT_ERR_NOERROR : on success;
 * @retval PRIOT_ERR_GENERR : in case of error (if var == NULL)
 * @retval PRIOT_ERR_WRONGTYPE: if var->type != type
 * @retval PRIOT_ERR_WRONGLENGTH: if var->type == ASN01_INTEGER and var->valueLength != sizeof( long ) .
 */
int VariableList_checkIntLength( const VariableList* var );

/**
 * @brief VariableList_checkUIntLength
 *        checks if @c var->type is equal to ASN01_UNSIGNED and
 *        checks if @c var->valueLength is equal to  sizeof( long ).
 *
 * @param var - variable to be analyzed.
 *
 * @retval PRIOT_ERR_NOERROR : on success;
 * @retval PRIOT_ERR_GENERR : in case of error (if var == NULL)
 * @retval PRIOT_ERR_WRONGTYPE: if var->type != type
 * @retval PRIOT_ERR_WRONGLENGTH: if var->type == ASN01_UNSIGNED and var->valueLength != sizeof( long ) .
 */
int VariableList_checkUIntLength( const VariableList* var );

/**
 * @brief VariableList_checkIntLengthAndRange
 *        checks if @c var->type is equal to ASN01_UNSIGNED,
 *        checks if @c var->valueLength is equal to  sizeof( long ,
 *        checks whether the value of variable @c var->value.integer is within the
 *        range @p low and @p high, that is, var->value.integer >= low and var->value.integer <= high.
 *
 * @param var - variable to be analyzed.
 * @param low - the minimum value
 * @param high - the maximum value
 *
 * @retval PRIOT_ERR_NOERROR : on success;
 * @retval PRIOT_ERR_GENERR : in case of error (if var == NULL)
 * @retval PRIOT_ERR_WRONGTYPE: if var->type != type
 * @retval PRIOT_ERR_WRONGLENGTH: if var->type == ASN01_UNSIGNED and var->valueLength != sizeof( long ).
 * @retval PRIOT_ERR_WRONGVALUE:  if var->value.integer < low or var->value.integer > high.
 */
int VariableList_checkIntLengthAndRange( const VariableList* var, int low, int high );

/**
 * @brief VariableList_checkBoolLengthAndValue
 *        returns PRIOT_ERR_NOERROR if the results of both VariableList_checkTypeAndLength
 *        and VariableList_checkIntLengthAndRange are PRIOT_ERR_NOERROR. Otherwise, returns the other errors.
 *
 *
 * @param var - variable to be analyzed.
 *
 * @see VariableList_checkTypeAndLength
 * @see VariableList_checkIntLengthAndRange
 */
int VariableList_checkBoolLengthAndValue( const VariableList* var );

/**
 * @brief VariableList_checkRowStatusLengthAndRange
 *        ruturns PRIOT_ERR_NOERROR if the results of
 *        both VariableList_checkTypeAndLength
 *        and VariableList_checkIntLengthAndRange
 *        are PRIOT_ERR_NOERROR. Otherwise, returns the other errors.
 *
 *
 * @param var - variable to be analyzed.
 *
 * @see VariableList_checkTypeAndLength
 * @see VariableList_checkIntLengthAndRange
 */
int VariableList_checkRowStatusLengthAndRange( const VariableList* var );

/**
 * @brief VariableList_checkRowStatusTransition
 *        ruturns PRIOT_ERR_NOERROR if the results of both
 *        VariableList_checkRowStatusLengthAndRange
 *        and Tc_checkRowstatusTransition are
 *        PRIOT_ERR_NOERROR. Otherwise, returns the other errors.
 *
 *
 * @param var - variable to be analyzed.
 * @param oldValue - the previous value
 *
 * @see VariableList_checkRowStatusLengthAndRange
 * @see Tc_checkRowstatusTransition
 */
int VariableList_checkRowStatusTransition( const VariableList* var, int oldValue );

/**
 * @brief VariableList_rowStatusWithStorageType
 *        ruturns PRIOT_ERR_NOERROR if the results of both
 *        VariableList_checkRowStatusLengthAndRange
 *        and Tc_checkRowstatusWithStoragetypeTransition
 *        are PRIOT_ERR_NOERROR. Otherwise, returns the other errors.
 *
 * @param var - variable to be analyzed.
 * @param oldValue - the previous value
 * @param oldStorage - the previous storage value
 *
 * @see VariableList_checkRowStatusLengthAndRange
 * @see Tc_checkRowstatusWithStoragetypeTransition
 */
int VariableList_checkRowStatusWithStorageType( const VariableList* var, int oldValue, int oldStorage );

/**
 * @brief VariableList_storageType
 *        ruturns PRIOT_ERR_NOERROR if the results of both
 *        VariableList_checkTypeAndLength
 *        and VariableList_checkIntLengthAndRange
 *        and Tc_checkStorageTransition
 *        are PRIOT_ERR_NOERROR. Otherwise, returns the other errors.
 *
 * @param var - variable to be analyzed.
 * @param oldValue - the previous value
 *
 * @see VariableList_checkTypeAndLength
 * @see VariableList_checkIntLengthAndRange
 * @see Tc_checkStorageTransition
 */
int VariableList_checkStorageType( const VariableList* var, int oldValue );

#endif // IOT_VARIABLELIST_H
