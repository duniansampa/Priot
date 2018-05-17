#include "Utilities.h"

int Utilities_addrStringHton( char* ptr, size_t len )
{
    char tmp[ 8 ];

    if ( 8 == len ) {
        tmp[ 0 ] = ptr[ 6 ];
        tmp[ 1 ] = ptr[ 7 ];
        tmp[ 2 ] = ptr[ 4 ];
        tmp[ 3 ] = ptr[ 5 ];
        tmp[ 4 ] = ptr[ 2 ];
        tmp[ 5 ] = ptr[ 3 ];
        tmp[ 6 ] = ptr[ 0 ];
        tmp[ 7 ] = ptr[ 1 ];
        memcpy( ptr, &tmp, 8 );
    } else if ( 32 == len ) {
        Utilities_addrStringHton( ptr, 8 );
        Utilities_addrStringHton( ptr + 8, 8 );
        Utilities_addrStringHton( ptr + 16, 8 );
        Utilities_addrStringHton( ptr + 24, 8 );
    } else
        return -1;

    return 0;
}
