//! \brief This is the include for the Integer entity.
#include "Integer.h"

//Parses the string argument as a signed decimal integer.
int	Integer_parseInt(const  _s8 * s){
    return (int) atoi(s);
}

//Parses the string argument as a signed integer in the base specified by the second argument.
int	Integer_parseInt(const  _s8 * s, int base){
    return ((int)strtol (s,NULL, base));
}

//Parses the string argument as signed integer in the base specified by the third argument.
//sub is an object of type string, whose value is set by the function to the substring that starts to next character
//in s after the numerical value.
int	Integer_parseInt(const  _s8 * s,  _s8 * sub, int base){

    int r =  (int)strtol (s, &sub, base);

    return r;
}


//Returns a string representation of the integer argument as an unsigned integer in base 2.
string Integer_toBinaryString(int i){
	unsigned int numBit  = sizeof(int) * 8;
	string ret;
	for(unsigned j = 0; j< numBit ; j++){
	  ret.push_back( ((i >>(numBit - 1 -j))&0x01) + '0');
	}
	return ret;
}

//Returns a string representation of the integer argument as an unsigned integer in base 16.
string Integer_toHexString(int i){
    return String_format("%X", i);
}

//Returns a string representation of the integer argument as an unsigned integer in base 8.
string	Integer_toOctalString(int i){
    return String_format("%o", i);
}

//Returns a String object representing the specified integer.
string	Integer_toString(int i){
    return String_valueOf(i);
}

// Returns a string representation of the first argument in the base specified by the second argument.
string	Integer_toString(unsigned int i, int base){

	int q;
	int r;
	string s;
	if (base < 1) return "";
	do{
		q =  i / base;
		r =  i % base;
		i = q;
        s.insert(0, 1, (char) ((r < 10)?('0' + r):('A' + r - 10)));
	}while(i > 0 );

	return s;
}

// Returns a Integer whose value is equivalent to this Integer with the designated bit cleared.
int Integer_clearBit(int value, int n){
    return value & ~(1<<n);
}

// Returns a Integer whose value is equivalent to this Integer with the designated bit set.
int Integer_setBit(int value, int n){
    return value | (1<<n);
}

// Returns true if and only if the designated bit is set.
bool Integer_testBit(int value, int n){
    return (value & (1<<n)) != 0;
}
