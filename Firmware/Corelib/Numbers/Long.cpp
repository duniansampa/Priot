//! \brief This is the include for the Long entity.
#include "Long.h"

//Parses the string argument as a signed decimal long.
long Long_parseLong(const _s8 *  s){
    return (long) atol(s);
}

//Parses the string argument as a signed long in the 'base' specified by the second argument.
long Long_parseLong(const _s8 * s, int base){
    return ((long)strtol (s,NULL, base));
}


//Returns a string representation of the long argument as an unsigned integer in base 2.
string Long_toBinaryString(long i){
    return Long_toString( i, 2);
}

//Returns a string representation of the long argument as an unsigned integer in base 16.
string Long_toHexString(long i){
    return String_format("%X", i);
}

//Returns a string representation of the long argument as an unsigned integer in base 8.
string Long_toOctalString(long i){
    return String_format("%o", i);
}


//Returns a string object representing the specified long.
string Long_toString(long i){
    return String_valueOf(i);
}

//Returns a string representation of the first argument in the 'base' specified by the second argument.
string	Long_toString(unsigned long i, int base){
	long q;
	long r;
	string buffer;
	if (base < 1) return "";
	do{

		q =  i / base;
		r =  i % base;
		i = q;

        buffer.insert(0, 1, (char)((r < 10)?('0' + r):('A' + r - 10)));
	}while(i > 0 );

	return buffer;
}
