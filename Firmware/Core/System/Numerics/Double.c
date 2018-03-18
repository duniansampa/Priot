//! \brief This is the include for the Double entity.
#include "Double.h"

#include "Generals.h"

//Returns the value of the specified number as a int64.
int64 Double_bigLongValue(double value){
    if(value >= 0)
        return (int64)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (int64)(value - DOUBLE_EPSILON * 1E+1);
}

//Returns the value of this Double as a ubyte (by casting to a ubyte).
ubyte Double_byteValue(double value){
    if(value >= 0)
        return (ubyte)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (ubyte)(value - DOUBLE_EPSILON * 1E+1);
}
//Compares the two specified double values.
int Double_compare(double d1, double d2){

     return Double_compare2( d1,  d2,  DOUBLE_EPSILON * 1E+1);
}
//Compares the two specified double values using epsilon
int	Double_compare2(double d1, double d2, double epsilon){

	 if( (((d2 - epsilon ) < d1) && (d1 <( d2 + epsilon)))  )
		 return 0;
	 if( (d1 - d2) > epsilon )
		 return 1;
	 return -1;
}

//Returns the value of the specified number as a char.
char Double_charValue(double value){
    if(value >= 0)
        return (char)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (char)(value - DOUBLE_EPSILON * 1E+1);
}


//Returns the float value of this Double object.
float Double_floatValue(double value){
    return (float)value;
}

//Returns the value of this Double as an int (by casting to type int).
int Double_intValue(double value){
    if(value >= 0)
        return (int)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (int)(value - DOUBLE_EPSILON * 1E+1);
}

//Returns true if this Double value is infinitely large in magnitude, false otherwise.
bool Double_isInfinite(double value){
    return isinf(value);
}

//Returns true if the specified number is a Not-a-Number (NaN) value, false otherwise.
bool Double_isNaN(double v){
    return isnan(v);
}

//Returns the value of this Double as a long (by casting to type long).
long Double_longValue(double value){
    if(value >= 0)
        return (long)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (long)(value - DOUBLE_EPSILON * 1E+1);
}

//Returns a new double initialized to the value represented by the specified String, as performed by the valueOf method of class Double.
double Double_parseDouble(const char * s){
    return (double) atof(s);
}

//Returns the value of this Double as a short (by casting to a short).
short Double_shortValue(double value){
    if(value >= 0)
        return (short)(value + DOUBLE_EPSILON * 1E+1);
	else
        return (short)(value - DOUBLE_EPSILON * 1E+1);
}

//Returns a string representation of the double argument as an Hexadecimal floating point
char *  Double_toHexString(double d){
    return String_format("%A", d);
}

//Returns a string representation of the double argument.
char * 	Double_toString(double d){
    return String_valueOfD(d);
}

