
//! \brief This is the include for the Float entity.
#include "Float.h"

//Returns the value of the specified number as a int64.
int64 Float_bigLongValue(float value){
    if(value >= 0)
        return (int64)(value + FLOAT_EPSILON * 1E+1);
	else
        return (int64)(value - FLOAT_EPSILON * 1E+1);
}

//Returns the value of this Float as a ubyte (by casting to a ubyte).
ubyte Float_byteValue(float value){
    if(value >= 0)
        return (ubyte)(value + FLOAT_EPSILON * 1E+1);
	else
        return (ubyte)(value - FLOAT_EPSILON * 1E+1);
}

//Compares the two specified float values.
int Float_compare1(float f1, float f2){

    return Float_compare2(f1, f2, FLOAT_EPSILON * 1E+1);
}

//Compares the two specified float values using epsilon
int	Float_compare2(float f1, float f2, float epsilon){

	 if( (((f2 - epsilon ) < f1) && (f1 <( f2 + epsilon)))  )
		 return 0;
	 if( (f1 - f2) > epsilon )
		 return 1;
	 return -1;
}

//Returns the value of the specified number as a char.
char Float_charValue(float value){
    if(value >= 0)
        return (char)(value + FLOAT_EPSILON * 1E+1);
	else
        return (char)(value - FLOAT_EPSILON * 1E+1);
}

//Returns the double value of this Float object.
double	Float_doubleValue(float value){
    return (double)value;
}


//Returns the value of this Float as an int (by casting to type int).
int Float_intValue(float value){
    if(value >= 0)
        return (int)(value + FLOAT_EPSILON * 1E+1);
	else
        return (int)(value - FLOAT_EPSILON * 1E+1);
}

//Returns true if the specified number is infinitely large in magnitude, false otherwise.
bool Float_isInfinite(float v){
	return isinf(v);
}


//Returns true if the specified number is a Not-a-Number (NaN) value, false otherwise.
bool Float_isNaN(float v){
    return isnan(v);
}

//Returns value of this Float as a long (by casting to type long).
long Float_longValue(float value){
    if(value >= 0)
        return (long)(value + FLOAT_EPSILON * 1E+1);
	else
        return (long)(value - FLOAT_EPSILON * 1E+1);
}

//Returns a new float initialized to the value represented by the specified String, as performed by the valueOf method of class Float.
float Float_parseFloat(const char  * s) {
    return (float) atof(s);
}

//Returns the value of this Float as a short (by casting to a short).
short Float_shortValue(float value){
    if(value >= 0)
        return (short)(value + FLOAT_EPSILON * 1E+1);
	else
        return (short)(value - FLOAT_EPSILON * 1E+1);
}

//Returns a string representation of the float argument as an Hexadecimal floating point.
char *  Float_toHexString(float f){
    return String_format("%A", f);
}



//Returns a string representation of the float argument.
char *  Float_toString(float f){
    return String_valueOfF(f);
}
