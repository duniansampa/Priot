//! \brief This is the include for the Double entity.
#include "Double.h"

//Returns the value of the specified number as a _s64.
_s64 Double_bigLongValue(double value){
    if(value >= 0)
        return (_s64)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (_s64)(value - DOUBLE_EPSILON * 1E+1);
}

//Returns the value of this Double as a _ubyte (by casting to a _ubyte).
_ubyte Double_byteValue(double value){
    if(value >= 0)
        return (_ubyte)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (_ubyte)(value - DOUBLE_EPSILON * 1E+1);
}
//Compares the two specified double values.
_i32 Double_compare(double d1, double d2){

     return Double_compare( d1,  d2,  DOUBLE_EPSILON * 1E+1);
}
//Compares the two specified double values using epsilon
_i32	Double_compare(double d1, double d2, double epsilon){

	 if( (((d2 - epsilon ) < d1) && (d1 <( d2 + epsilon)))  )
		 return 0;
	 if( (d1 - d2) > epsilon )
		 return 1;
	 return -1;
}

//Returns the value of the specified number as a char.
_s8 Double_charValue(double value){
    if(value >= 0)
        return (_s8)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (_s8)(value - DOUBLE_EPSILON * 1E+1);
}


//Returns the float value of this Double object.
float Double_floatValue(double value){
    return (float)value;
}

//Returns the value of this Double as an int (by casting to type int).
_i32 Double_intValue(double value){
    if(value >= 0)
        return (_i32)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (_i32)(value - DOUBLE_EPSILON * 1E+1);
}

//Returns true if this Double value is infinitely large in magnitude, false otherwise.
_bool Double_isInfinite(double value){
    return isinf(value);
}

//Returns true if the specified number is a Not-a-Number (NaN) value, false otherwise.
_bool Double_isNaN(double v){
    return isnan(v);
}

//Returns the value of this Double as a long (by casting to type long).
_s32 Double_longValue(double value){
    if(value >= 0)
        return (_s32)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (_s32)(value - DOUBLE_EPSILON * 1E+1);
}

//Returns a new double initialized to the value represented by the specified String, as performed by the valueOf method of class Double.
double Double_parseDouble(const string & s){
	return (double) atof(s.c_str());
}

//Returns the value of this Double as a short (by casting to a short).
_s16 Double_shortValue(double value){
    if(value >= 0)
        return (_s16)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (_s16)(value - DOUBLE_EPSILON * 1E+1);
}

//Returns a string representation of the double argument as an Hexadecimal floating point
_s8 *  Double_toHexString(double d){
    return String_format("%A", d);
}

//Returns a string representation of the double argument.
_s8 * 	Double_toString(double d){
    return String_valueOf(d);
}

