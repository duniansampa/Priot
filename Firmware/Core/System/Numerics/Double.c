#include "Double.h"
#include "Generals.h"

int Double_compare( double d1, double d2 )
{

    return Double_compareEpsilon( d1, d2, DOUBLE_EPSILON * 1E+1 );
}

int Double_compareEpsilon( double d1, double d2, double epsilon )
{

    if ( ( ( ( d2 - epsilon ) < d1 ) && ( d1 < ( d2 + epsilon ) ) ) )
        return 0;
    if ( ( d1 - d2 ) > epsilon )
        return 1;
    return -1;
}

bool Double_isInfinite( double value )
{
    return isinf( value );
}

bool Double_isNaN( double v )
{
    return isnan( v );
}
