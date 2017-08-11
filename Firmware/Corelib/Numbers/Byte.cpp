
//! \brief This is the include for the Byte entity.
#include "Byte.h"


//Parses the string argument as a decimal _ubyte.
_ubyte Byte_parseByte(const _s8 * s){
    return (_ubyte) atoi(s);
}

//Parses the string argument as a _ubyte in the base specified by the second argument.
_ubyte Byte_parseByte(const _s8 * s, _u8 base){
    return ((_ubyte)strtol (s,NULL, base));
}


//Returns a string representation of the '_ubyte' argument as an unsigned '_ubyte' in base 2.
_s8 *  Byte_toBinaryString(_ubyte b){
  return Byte_toString(b, 2);
}

//Returns a string representation of the '_ubyte' argument as an unsigned '_ubyte' in base 16.
_s8 *  Byte_toHexString(_ubyte b){
    return String_format("%X", b);
}

//Returns a new string object representing the specified _ubyte.
_s8 *  Byte_toString(_ubyte b){
    return String_valueOf(b);
}

//Returns a string representation of the first argument in the 'base' specified by the second argument.
_s8 * 	Byte_toString(_ubyte i, _u8 base){
    _ubyte q;
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

// Returns a BigInteger whose value is equivalent to this BigInteger with the designated bit cleared.
_ubyte Byte_clearBit(_ubyte b, _u8 n){
    return (_ubyte)(b & ~(1<<n));
}

// Returns a BigInteger whose value is equivalent to this BigInteger with the designated bit set.
_ubyte Byte_setBit(_ubyte b, _u8 n){
    return (_ubyte)(b | (1<<n));
}

// Returns true if and only if the designated bit is set.
_bool Byte_testBit(_ubyte b, _u8 n){
    return (b & (1<<n)) != 0;
}
double Byte_getValue(_ubyte * beginByte, _ubyte startBit, _ubyte numBit, double resolution, bool isSigned){

    _s64 val = 0;
    _s64 up;
    _s64 test = 1;
    _i32 numBytes;

	if(numBit > 31){
        up = (~((_s64)0)) << 31;
		up <<= (numBit - 31);
		test <<= 31;
		test <<= (numBit - 31 - 1);
	}else{
        up = (~((_s64)0)) << numBit;
		test= test << (numBit - 1);

	}
    numBytes = Double_intValue((numBit + startBit)/8.0);
    if(Double_compare((numBit + startBit)/8.0, numBytes ) != 0){
		numBytes = numBytes + 1;
	}
    for(_i32 i = numBytes ; i != 0; i--){
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

void Byte_setValue(double value, _ubyte * beginByte, _ubyte startBit, _ubyte numBit, double resolution){
    _s64 val =  Double_intValue(value / resolution);
    _s64 up;
    _s64 mask;
    _i32 numBytes;
    mask = (~((_s64)0));
	if(numBit > 31){
        up = (~((_s64)0)) << 31;
		up <<= (numBit - 31);

	}else{
        up = (~((_s64)0)) << numBit;
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
    for(_i32 i = 0 ; i != numBytes; i++){

        beginByte[i] &= (_ubyte)mask;
        beginByte[i] |= (_ubyte)val;
		val  >>= 8;
		mask >>= 8;
	}
}
void Byte_reflectValue(_ubyte * beginByte, _ubyte startBit, _ubyte numBit){

    double d = Byte_getValue(beginByte, startBit, numBit, 1, false);

    _u64 value   = d;
    _u64 reflect = 0;
    _u64 j = 1;

    for ( _i32 i = 0; i < numBit; i++) {
		reflect <<= 1;
		if ( value & j)
			reflect |= 1;
		j <<= 1;
	}
	d = reflect;

    Byte_setValue(reflect, beginByte, startBit, numBit, 1);
}
