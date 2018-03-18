//! \brief This is the include for the Short entity.
#include "Short.h"


//Parses the string argument as a signed decimal short.
short Short_parseShort1(const char * s){
    return (short) atoi(s);
}

//Parses the string argument as a signed short in the base specified by the second argument.
short Short_parseShort2(const char * s, int base){
    return ((short)strtol (s,NULL, base));
}


//Returns a string representation of the 'short' argument as an unsigned 'ubyte' in base 16.
char * Short_toHexString(short sh){
    return String_format("%X", sh);
}

//Returns a new String object representing the specified short.
char * Short_toString(short sh){
    return String_valueOfS16(sh);
}

