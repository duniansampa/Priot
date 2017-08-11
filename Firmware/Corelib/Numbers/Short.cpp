//! \brief This is the include for the Short entity.
#include "Short.h"


//Parses the string argument as a signed decimal short.
short Short_parseShort(const _s8 * s){
    return (short) atoi(s);
}

//Parses the string argument as a signed short in the base specified by the second argument.
short Short_parseShort(const _s8 * s, int base){
    return ((short)strtol (s,NULL, base));
}


//Returns a string representation of the 'short' argument as an unsigned '_ubyte' in base 16.
string Short_toHexString(short sh){
    return String_format("%X", sh);
}

//Returns a new String object representing the specified short.
string Short_toString(short sh){
    return String_valueOf(sh);
}

