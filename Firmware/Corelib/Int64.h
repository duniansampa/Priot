#ifndef INT64_H
#define INT64_H

#include "Asn01.h"

typedef Asn01_Counter64 Int64_U64;

#define INT64_I64CHARSZ 21

void            Int64_divBy10(Int64_U64, Int64_U64 *, unsigned int *);
void            Int64_multBy10(Int64_U64, Int64_U64 *);
void            Int64_incrByU16(Int64_U64 *, unsigned int);
void            Int64_incrByU32(Int64_U64 *, unsigned int);

void            Int64_zeroU64(Int64_U64 *);
int             Int64_isZeroU64(const Int64_U64 *);

void            Int64_printU64(char *, const Int64_U64 *);

void            Int64_printI64(char *, const Int64_U64 *);
int             Int64_read64(Int64_U64 *, const char *);

void            Int64_u64Subtract(const Int64_U64 * pu64one, const Int64_U64 * pu64two, Int64_U64 * pu64out);

void            Int64_u64Incr(Int64_U64 * pu64out, const Int64_U64 * pu64one);

void            Int64_u64UpdateCounter(Int64_U64 * pu64out, const Int64_U64 * pu64one, const Int64_U64 * pu64two);

void            Int64_u64Copy(Int64_U64 * pu64one, const Int64_U64 * pu64two);

int             Int64_c64CheckFor32bitWrap(Int64_U64 *oldVal, Int64_U64 *newVal, int adjust);

int             Int64_c64Check32AndUpdate(Int64_U64 *prevVal, Int64_U64 *newVal,
                                          Int64_U64 *oldPrevVal, int *needWrapCheck);

#endif // INT64_H
