
//! \brief This is the include for the String entity.
#include "String.h"

//Tests if this string begin with the specified prefix.
_bool String_beginsWith(const _s8 * s, const _s8 * prefix){

    _i32 lp, ls;

    if(s == NULL || prefix == NULL )
        return false;

    lp = strlen(prefix);
    ls = strlen(s);

    if(lp > ls)
        return false;

    return strncmp(s, prefix, lp) == 0;
}

//Appends a copy of the source string to the destination string.
//The terminating null character in destination is overwritten by the first character of source,
//and a null-character is included at the end of the new string formed by the concatenation of
//both in destination
_s8 * String_concat(_s8 * destination, const _s8 * source){


    int newSize;

    // Determine new size
    newSize = strlen(destination)  + strlen(source) + 1;

    // Allocate new buffer
    char * newBuffer = (char *)malloc(newSize);

    // do the copy and concat
    strcpy(newBuffer, destination);
    strcat(newBuffer, source); // or strncat

    // release old buffer
    free( (char *)destination);

    // store new pointer
    destination = newBuffer;

    return destination;
}

//Compares two strings lexicographically.
_i32 String_compare(const _s8 * s1, const _s8 * s2){

    return strcmp(s1, s2);

}

//Tests if this string ends with the specified suffix.
_bool String_endsWith(const _s8 * s, const _s8 * suffix){

    _i32 lp, ls, diff;

    if(s == NULL || suffix == NULL )
        return false;

    lp = strlen(suffix);
    ls = strlen(s);

    if(lp > ls)
        return false;

    diff = ls - lp;

    return strcmp(s + diff, suffix) == 0;
}

//Compares this String to another String, ignoring case considerations.
_bool String_equalsIgnoreCase(const _s8 * s1, const _s8 * s2){

   return strcasecmp(s1, s2) == 0;
}

//Compares this String to another String
_bool String_equals(const _s8 * s1, const _s8 * s2){
    return String_compare(s1, s2) == 0;
}

//Returns a formatted string using the specified locale, format string, and arguments.
_s8 * String_format(const _s8 * format, ...){
    char buffer[512];
    int size;
    va_list args;
    _s8 * s;
    va_start (args, format);
    if( (size = vsprintf (buffer,format, args)) < 0){
        s = NULL;
    }else{
        s = strndup(buffer, size);
    }
    va_end (args);

    return s;
}

//Converts all of the characters in this string to lower case.
_s8 * String_toLowerCase(const _s8 * s){
    _s8 * sA = strdup(s);
    while( *sA != 0 ){
        *sA  = tolower(*sA);
        sA++;
    }
    return sA;
}

//Returns a string representation of the string argument as an string in base 16.
_s8 * String_toHexString(const _s8 * s){

    _i32 size = 0;
    char buffer[512];
    while(*s != 0 ){
        size += sprintf(buffer + size , "%02X", *s);
        s++;
    }
    return strndup(buffer, size);
}

//Converts all of the characters in this String to upper;
_s8 *	String_toUpperCase(const _s8 * s){
    _s8 * sA = strdup(s);
    while( *sA != 0 ){
        *sA  = toupper(*sA);
        sA++;
    }
	return sA;
}

//Returns the string, with leading and trailing whitespace omitted.
// returned string is equal to s after function call.
_s8 * String_trim(_s8 * s){

    char *end;

    // Trim leading space
    while(isspace((unsigned char)*s)) s++;

    if(*s == 0)  // All spaces?
      return s;

    // Trim trailing space
    end = s + strlen(s) - 1;
    while(end > s && isspace((unsigned char)*end)) end--;

    // Write new null terminator
    *(end+1) = 0;

    return s;
}

//Returns the string representation of the bigLong argument.
_s8 * String_valueOf(_s64 b){

    return String_format("%lld", b);
}

//Returns the string representation of the boolean argument.
_s8 * String_valueOf(_bool b){
    return String_format("%d", b);
}

//Returns the string representation of the byte argument.
_s8 * String_valueOf(_ubyte b){
    return String_format("%d", b);
}

//Returns the string representation of the double argument.
_s8 * String_valueOf(double d){
    return String_format("%f", d);
}
//Returns the string representation of the float argument.
_s8 * String_valueOf(float f){
    return String_format("%f", f);
}

//Returns the string representation of the int argument.
_s8 * String_valueOf(_i32 i){
    return String_format("%d", i);
}

//Returns the string representation of the long argument.
_s8 * String_valueOf(_s32 l){
    return String_format("%ld", l);
}

//Returns the string representation of the short argument.
_s8 * 	String_valueOf(_s16 i){
    return String_format("%d", i);
}

