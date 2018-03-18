
//! \brief This is the include for the String entity.
#include "String.h"
#include <ctype.h>

const struct String_Compare_s STRING_COMPARE = {-1, 0, -1};

//Tests if this string begin with the specified prefix.
bool String_beginsWith(const char * s, const char * prefix){

    int lp, ls;

    if(s == NULL || prefix == NULL )
        return 0;

    lp = strlen(prefix);
    ls = strlen(s);

    if(lp > ls)
        return 0;

    return strncmp(s, prefix, lp) == 0;
}

//Appends a copy of the source string to the destination string.
//The terminating null character in destination is overwritten by the first character of source,
//and a null-character is included at the end of the new string formed by the concatenation of
//both in destination
char * String_concat(char * destination, const char * source){


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
int String_compare(const char * s1, const char * s2){

    return strcmp(s1, s2);

}

//Tests if this string ends with the specified suffix.
bool String_endsWith(const char * s, const char * suffix){

    int lp, ls, diff;

    if(s == NULL || suffix == NULL )
        return 0;

    lp = strlen(suffix);
    ls = strlen(s);

    if(lp > ls)
        return 0;

    diff = ls - lp;

    return strcmp(s + diff, suffix) == 0;
}

//Compares this String to another String, ignoring case considerations.
bool String_equalsIgnoreCase(const char * s1, const char * s2){

   return strcasecmp(s1, s2) == 0;
}

//Compares this String to another String
bool String_equals(const char * s1, const char * s2){
    return String_compare(s1, s2) == 0;
}

//Returns a formatted string using the specified locale, format string, and arguments.
char * String_format(const char * format, ...){
    char buffer[512];
    int size;
    va_list args;
    char * s;
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
char * String_toLowerCase(const char * s){
    char * sA = strdup(s);
    while( *sA != 0 ){
        *sA  = tolower(*sA);
        sA++;
    }
    return sA;
}

//Returns a string representation of the string argument as an string in base 16.
char * String_toHexString(const char * s){

    int size = 0;
    char buffer[512];
    while(*s != 0 ){
        size += sprintf(buffer + size , "%02X", *s);
        s++;
    }
    return strndup(buffer, size);
}

//Converts all of the characters in this String to upper;
char *	String_toUpperCase(const char * s){
    char * sA = strdup(s);
    while( *sA != 0 ){
        *sA  = toupper(*sA);
        sA++;
    }
	return sA;
}

//Returns the string, with leading and trailing whitespace omitted.
// returned string is equal to s after function call.
char * String_trim(char * s){

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
char * String_valueOfS64(int64 b){

    return String_format("%lld", b);
}

//Returns the string representation of the boolean argument.
char * String_valueOfB(bool b){
    return String_format("%d", b);
}

//Returns the string representation of the byte argument.
char * String_valueOfBt(ubyte b){
    return String_format("%d", b);
}

//Returns the string representation of the double argument.
char * String_valueOfD(double d){
    return String_format("%f", d);
}
//Returns the string representation of the float argument.
char * String_valueOfF(float f){
    return String_format("%f", f);
}

//Returns the string representation of the int argument.
char * String_valueOfI32(int i){
    return String_format("%d", i);
}

//Returns the string representation of the long argument.
char * String_valueOfS32(long l){
    return String_format("%ld", l);
}

//Returns the string representation of the short argument.
char * 	String_valueOfS16(short i){
    return String_format("%d", i);
}

