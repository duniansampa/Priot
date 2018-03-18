
//! \brief This is the include for the BigLong entity.
#include "BigLong.h"

#include "../String.h"

//Parses the string argument as a signed decimal int64.
int64 BigLong_parseLong1(const char * s){
    return (int64) atol(s);
}

//Parses the string argument as a signed int64 in the 'base' specified by the second argument.
int64 BigLong_parseLong2(const char * s, uchar base){
    return ((int64)strtol (s,NULL, base));
}

//Returns a string representation of the int64 argument as an unsigned integer in base 2.
char * BigLong_toBinaryString3(int64 i){
    return BigLong_toString2( i, 2);
}

//Returns a string representation of the int64 argument as an unsigned integer in base 16.
char * BigLong_toHexString(int64 i){
    return String_format("%X", i);
}

//Returns a string representation of the int64 argument as an unsigned integer in base 8.
char * BigLong_toOctalString1(int64 i){
    return String_format("%o", i);
}

//Returns a string object representing the specified int64.
char * BigLong_toString(int64 i){
    return String_valueOfS64(i);
}

//Returns a string representation of the first argument in the 'base' specified by the second argument.
char *	BigLong_toString2(int64 i, uchar base){
    int64 q;
    int r;
    const uint BUF_SIZE = 512;
    char buffer[BUF_SIZE];
    uint index = 0;
    if (base < 1) return NULL;

	do{
		q =  i / base;
		r =  i % base;
		i = q;

        buffer[BUF_SIZE - 1 - index++] = (char)((r < 10)?('0' + r):('A' + r - 10));
	}while(i > 0 );

    return strndup(buffer + (BUF_SIZE - index) , index);
}

