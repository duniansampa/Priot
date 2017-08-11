#ifndef MD5MD5_H
#define MD5MD5_H

#include "Generals.h"

/*
 * MDstruct is the data structure for a message digest computation.
 */
typedef struct {
    unsigned int    buffer[4];      /* Holds 4-word result of MD computation */
    unsigned char   count[8];       /* Number of bits processed so far */
    unsigned int    done;   /* Nonzero means MD computation finished */
} Md5_MdStruct     , *Md5_MdPTR;

/*
 * Md5_begin(MD)
 * ** Input: MD -- an Md5_MdPTR
 * ** Initialize the MDstruct prepatory to doing a message digest computation.
 */
void Md5_begin(Md5_MdPTR);

/*
 * Md5_update(MD,X,count)
 * ** Input: MD -- an Md5_MdPTR
 * **        X -- a pointer to an array of unsigned characters.
 * **        count -- the number of bits of X to use (an unsigned int).
 * ** Updates MD using the first ``count'' bits of X.
 * ** The array pointed to by X is not modified.
 * ** If count is not a multiple of 8, MDupdate uses high bits of last byte.
 * ** This is the basic input routine for a user.
 * ** The routine terminates the MD computation when count < 512, so
 * ** every MD computation should end with one call to MDupdate with a
 * ** count less than 512.  Zero is OK for a count.
 */
int Md5_update(Md5_MdPTR, const unsigned char *, unsigned int);

/*
 * Md5_print(MD)
 * ** Input: MD -- an Md5_MdPTR
 * ** Prints message digest buffer MD as 32 hexadecimal digits.
 * ** Order is from low-order byte of buffer[0] to high-order byte of buffer[3].
 * ** Each byte is printed with high-order hexadecimal digit first.
 */
void Md5_print(Md5_MdPTR);

int  Md5_checksum(const u_char * data, size_t len, u_char * mac, size_t maclen);

int  Md5_sign(const u_char * data, size_t len, u_char * mac,
                       size_t maclen, const u_char * secret,
                       size_t secretlen);

void Md5_get(Md5_MdStruct * MD, u_char * buf, size_t buflen);


#endif // MD5MD5_H
