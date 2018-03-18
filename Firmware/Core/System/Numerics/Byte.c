
//! \brief This is the include for the Byte entity.
#include "Byte.h"
#include "Double.h"

//Parses the string argument as a decimal ubyte.
ubyte Byte_parseByte1(const char * s){
    return (ubyte) atoi(s);
}

//Parses the string argument as a ubyte in the base specified by the second argument.
ubyte Byte_parseByte2(const char * s, uchar base){
    return ((ubyte)strtol (s,NULL, base));
}


//Returns a string representation of the 'ubyte' argument as an unsigned 'ubyte' in base 2.
char *  Byte_toBinaryString(ubyte b){
  return Byte_toString2(b, 2);
}

//Returns a string representation of the 'ubyte' argument as an unsigned 'ubyte' in base 16.
char *  Byte_toHexString(ubyte b){
    return String_format("%X", b);
}

//Returns a new string object representing the specified ubyte.
char *  Byte_toString1(ubyte b){
    return String_valueOfBt(b);
}

//Returns a string representation of the first argument in the 'base' specified by the second argument.
char * 	Byte_toString2(ubyte i, uchar base){
    ubyte q;
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

// Returns a BigInteger whose value is equivalent to this BigInteger with the designated bit cleared.
ubyte Byte_clearBit(ubyte b, uchar n){
    return (ubyte)(b & ~(1<<n));
}

// Returns a BigInteger whose value is equivalent to this BigInteger with the designated bit set.
ubyte Byte_setBit(ubyte b, uchar n){
    return (ubyte)(b | (1<<n));
}

// Returns true if and only if the designated bit is set.
bool Byte_testBit(ubyte b, uchar n){
    return (b & (1<<n)) != 0;
}
double Byte_getValue(ubyte * beginByte, ubyte startBit, ubyte numBit, double resolution, bool isSigned){

    int64 val = 0;
    int64 up;
    int64 test = 1;
    int numBytes;

	if(numBit > 31){
        up = (~((int64)0)) << 31;
		up <<= (numBit - 31);
		test <<= 31;
		test <<= (numBit - 31 - 1);
	}else{
        up = (~((int64)0)) << numBit;
		test= test << (numBit - 1);

	}
    numBytes = Double_intValue((numBit + startBit)/8.0);
    if(Double_compare((numBit + startBit)/8.0, numBytes ) != 0){
		numBytes = numBytes + 1;
	}
    for(int i = numBytes ; i != 0; i--){
		val  <<= 8;
		val |=  beginByte[i-1];

	}
	val >>= startBit;
	val &= (~up);
    if(isSigned){
    	if( (val &  test) != 0){
    		val = val | up;
    	}
    }
	return (double)(val * resolution);
}

void Byte_setValue(double value, ubyte * beginByte, ubyte startBit, ubyte numBit, double resolution){
    int64 val =  Double_intValue(value / resolution);
    int64 up;
    int64 mask;
    int numBytes;
    mask = (~((int64)0));
	if(numBit > 31){
        up = (~((int64)0)) << 31;
		up <<= (numBit - 31);

	}else{
        up = (~((int64)0)) << numBit;
	}
	val &= (~up);
	mask &= (~up);
	val <<= startBit;
	mask <<= startBit;
	mask = ~mask;
    numBytes = Double_intValue((numBit + startBit)/8.0);
    if(Double_compare((numBit + startBit)/8.0, numBytes ) != 0){
		numBytes = numBytes + 1;
	}
    for(int i = 0 ; i != numBytes; i++){

        beginByte[i] &= (ubyte)mask;
        beginByte[i] |= (ubyte)val;
		val  >>= 8;
		mask >>= 8;
	}
}
void Byte_reflectValue(ubyte * beginByte, ubyte startBit, ubyte numBit){

    double d = Byte_getValue(beginByte, startBit, numBit, 1, 0);

    uint64 value   = d;
    uint64 reflect = 0;
    uint64 j = 1;

    for ( int i = 0; i < numBit; i++) {
		reflect <<= 1;
		if ( value & j)
			reflect |= 1;
		j <<= 1;
	}
	d = reflect;

    Byte_setValue(reflect, beginByte, startBit, numBit, 1);
}
