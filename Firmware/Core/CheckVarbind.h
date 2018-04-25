#ifndef CHECKVARBIND_H
#define CHECKVARBIND_H

#include "Types.h"
/*
 * Assorted convience routines to check the contents of a
 * Types_VariableList instance.
 */

int CheckVarbind_type( const Types_VariableList *var, int type );

int CheckVarbind_size( const Types_VariableList *var, size_t size );

int CheckVarbind_maxSize( const Types_VariableList *var, size_t size );

int CheckVarbind_range( const Types_VariableList *var, size_t low,
                        size_t high );

int CheckVarbind_sizeRange( const Types_VariableList *var, size_t low,
                            size_t high );

int CheckVarbind_typeAndSize( const Types_VariableList *var, int type,
                              size_t size );

int CheckVarbind_typeAndMaxSize( const Types_VariableList *var, int type,
                                 size_t size );

int CheckVarbind_oid( const Types_VariableList *var );

int CheckVarbind_int( const Types_VariableList *var );

int CheckVarbind_uint( const Types_VariableList *var );

int CheckVarbind_intRange( const Types_VariableList *var, int low, int high );

int CheckVarbind_truthValue( const Types_VariableList *var );

int CheckVarbind_rowStatusValue( const Types_VariableList *var );

int CheckVarbind_rowStatus( const Types_VariableList *var, int old_val );

int CheckVarbind_rowStatusWithStorageType( const Types_VariableList *var,
                                           int old_val, int old_storage );

int CheckVarbind_storageType( const Types_VariableList *var, int old_val );

#endif // CHECKVARBIND_H
