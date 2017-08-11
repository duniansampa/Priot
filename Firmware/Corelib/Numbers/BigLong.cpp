
//! \brief This is the include for the BigLong entity.
#include "BigLong.h"

#include "String.h"

//Parses the string argument as a signed decimal _s64.
_s64 BigLong_parseLong(const _s8 * s){
    return (_s64) atol(s);
}

//Parses the string argument as a signed _s64 in the 'base' specified by the second argument.
_s64 BigLong_parseLong(const _s8 * s, _u8 base){
    return ((_s64)strtol (s,NULL, base));
}

//Returns a string representation of the _s64 argument as an unsigned integer in base 2.
_s8 * BigLong_toBinaryString(_s64 i){
    return BigLong_toString( i, 2);
}

//Returns a string representation of the _s64 argument as an unsigned integer in base 16.
_s8 * BigLong_toHexString(_s64 i){
    return String_format("%X", i);
}

//Returns a string representation of the _s64 argument as an unsigned integer in base 8.
_s8 * BigLong_toOctalString(_s64 i){
    return String_format("%o", i);
}

//Returns a string object representing the specified _s64.
_s8 * BigLong_toString(_s64 i){
    return String_valueOf(i);
}

//Returns a string representation of the first argument in the 'base' specified by the second argument.
_s8 *	BigLong_toString(_s64 i, _u8 base){
    _s64 q;
    _i32 r;
    const _u32 BUF_SIZE = 512;
    _s8 buffer[BUF_SIZE];
    _u32 index = 0;
    if (base < 1) return NULL;

	do{
		q =  i / base;
		r =  i % base;
		i = q;

        buffer[BUF_SIZE - 1 - index++] = (_s8)((r < 10)?('0' + r):('A' + r - 10));
	}while(i > 0 );

    return strndup(buffer + (BUF_SIZE - index) , index);
}

