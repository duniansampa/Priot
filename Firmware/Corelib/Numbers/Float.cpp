
//! \brief This is the include for the Float entity.
#include "Float.h"

//Returns the value of the specified number as a _s64.
_s64 Float_bigLongValue(float value){
    if(value >= 0)
        return (_s64)(value + FLOAT_EPSILON * 1E+1);
	else
        return (_s64)(value - FLOAT_EPSILON * 1E+1);
}

//Returns the value of this Float as a _ubyte (by casting to a _ubyte).
_ubyte Float_byteValue(float value){
    if(value >= 0)
        return (_ubyte)(value + FLOAT_EPSILON * 1E+1);
	else
        return (_ubyte)(value - FLOAT_EPSILON * 1E+1);
}

//Compares the two specified float values.
_i32 Float_compare(float f1, float f2){

    return Float_compare(f1, f2, FLOAT_EPSILON * 1E+1);
}

//Compares the two specified float values using epsilon
_i32	Float_compare(float f1, float f2, float epsilon){

	 if( (((f2 - epsilon ) < f1) && (f1 <( f2 + epsilon)))  )
		 return 0;
	 if( (f1 - f2) > epsilon )
		 return 1;
	 return -1;
}

//Returns the value of the specified number as a char.
_s8 Float_charValue(float value){
    if(value >= 0)
        return (_s8)(value + FLOAT_EPSILON * 1E+1);
	else
        return (_s8)(value - FLOAT_EPSILON * 1E+1);
}

//Returns the double value of this Float object.
double	Float_doubleValue(float value){
    return (double)value;
}


//Returns the value of this Float as an int (by casting to type int).
_i32 Float_intValue(float value){
    if(value >= 0)
        return (_i32)(value + FLOAT_EPSILON * 1E+1);
	else
        return (_i32)(value - FLOAT_EPSILON * 1E+1);
}

//Returns true if the specified number is infinitely large in magnitude, false otherwise.
_bool Float_isInfinite(float v){
	return isinf(v);
}


//Returns true if the specified number is a Not-a-Number (NaN) value, false otherwise.
_bool Float_isNaN(float v){
    return isnan(v);
}

//Returns value of this Float as a long (by casting to type long).
_s32 Float_longValue(float value){
    if(value >= 0)
        return (_s32)(value + FLOAT_EPSILON * 1E+1);
	else
        return (_s32)(value - FLOAT_EPSILON * 1E+1);
}

//Returns a new float initialized to the value represented by the specified String, as performed by the valueOf method of class Float.
float Float_parseFloat(const _s8  * s) {
    return (float) atof(s);
}

//Returns the value of this Float as a short (by casting to a short).
_s16 Float_shortValue(float value){
    if(value >= 0)
        return (_s16)(value + FLOAT_EPSILON * 1E+1);
	else
        return (_s16)(value - FLOAT_EPSILON * 1E+1);
}

//Returns a string representation of the float argument as an Hexadecimal floating point.
_s8 *  Float_toHexString(float f){
    return String_format("%A", f);
}



//Returns a string representation of the float argument.
_s8 *  Float_toString(float f){
    return String_valueOf(f);
}
