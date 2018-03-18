#ifndef DATATYPE_H
#define DATATYPE_H

enum{

    //Priot boolean data type
    DATATYPE_BOOLEAN = 0,

    //Priot BITS data type, which is the same as OCTETSTRING
    DATATYPE_BITS,

    //Priot BIT STRING data type
    DATATYPE_BITSTRING,

    //Priot 32-bit counter data type
    DATATYPE_COUNTER32,

    //Priot 64-bit counter data type
    DATATYPE_COUNTER64,

    //Priot 32-bit gauge data type
    DATATYPE_GAUGE32,

    //Priot Integer data type
    DATATYPE_INTEGER,

    //Priot ip address data type
    DATATYPE_IPADDRESS,

    //Priot NoSuchInstance data type
    DATATYPE_NO_SUCH_INSTANCE,

    //Priot NoSuchObject data type
    DATATYPE_NO_SUCH_OBJECT,

    //Priot null data type
    DATATYPE_NUULLL,

    //Priot octet string data type
    DATATYPE_OCTETSTRING,

    //Priot object identifier data type
    DATATYPE_OID,

    //Priot Opaque data type
    DATATYPE_OPAQUE,

    //Priot PDU data type
    DATATYPE_PDU,

    //
    DATATYPE_SEQUENCE,

    //Priot time ticks data type
    DATATYPE_TIMETICKS,

    //Priot unsigned 32-bit integer data type
    DATATYPE_UNSIGNED32,

    //Real Opaque data type
    DATATYPE_REALS
};

#endif // DATATYPE_H
