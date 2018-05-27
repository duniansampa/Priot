
#include "Float.h"

//Compares the two specified float values.
int Float_compare( float f1, float f2 )
{

    return Float_compareEpsilon( f1, f2, FLOAT_EPSILON * 1E+1 );
}

//Compares the two specified float values using epsilon
int Float_compareEpsilon( float f1, float f2, float epsilon )
{

    if ( ( ( ( f2 - epsilon ) < f1 ) && ( f1 < ( f2 + epsilon ) ) ) )
        return 0;
    if ( ( f1 - f2 ) > epsilon )
        return 1;
    return -1;
}

//Returns true if the specified number is infinitely large in magnitude, false otherwise.
bool Float_isInfinite( float v )
{
    return isinf( v );
}

//Returns true if the specified number is a Not-a-Number (NaN) value, false otherwise.
bool Float_isNaN( float v )
{
    return isnan( v );
}
