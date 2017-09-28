#include "Md5.h"

#include <stdint.h>

/*
 * ** To use MD5:
 * **   -- Include Md5.h in your program
 * **   -- Declare an Md5_MdStruct MD to hold the state of the digest computation.
 * **   -- Initialize MD using Md5_begin(&MD)
 * **   -- For each full block (64 bytes) X you wish to process, call
 * **          Md5_update(&MD,X,512)
 * **      (512 is the number of bits in a full block.)
 * **   -- For the last block (less than 64 bytes) you wish to process,
 * **          Md5_update(&MD,X,n)
 * **      where n is the number of bits in the partial block. A partial
 * **      block terminates the computation, so every MD computation should
 * **      terminate by processing a partial block, even if it has n = 0.
 * **   -- The message digest is available in MD.buffer[0] ... MD.buffer[3].
 * **      (Least-significant byte of each word should be output first.)
 * **   -- You can print out the digest using MDprint(&MD)
 */

/*
 * Implementation notes:
 * ** This implementation assumes that ints are 32-bit quantities.
 * ** If the machine stores the least-significant byte of an int in the
 * ** least-addressed byte (eg., VAX and 8086), then LOWBYTEFIRST should be
 * ** set to TRUE.  Otherwise (eg., SUNS), LOWBYTEFIRST should be set to
 * ** FALSE.  Note that on machines with LOWBYTEFIRST FALSE the routine
 * ** MDupdate modifies has a side-effect on its input array (the order of bytes
 * ** in each word are reversed).  If this is undesired a call to MDreverse(X) can
 * ** reverse the bytes of X back into order after each call to MDupdate.
 */

/*
 * code uses WORDS_BIGENDIAN defined by configure now  -- WH 9/27/95
 */


/*
 * Compile-time includes
 */

#include "Tools.h"

/*
 * Compile-time declarations of MD5 ``magic constants''.
 */
#define MD5_I0  0x67452301          /* Initial values for MD buffer */
#define MD5_I1  0xefcdab89
#define MD5_I2  0x98badcfe
#define MD5_I3  0x10325476
#define MD5_FS1  7                  /* round 1 shift amounts */
#define MD5_FS2 12
#define MD5_FS3 17
#define MD5_FS4 22
#define MD5_GS1  5                  /* round 2 shift amounts */
#define MD5_GS2  9
#define MD5_GS3 14
#define MD5_GS4 20
#define MD5_HS1  4                  /* round 3 shift amounts */
#define MD5_HS2 11
#define MD5_HS3 16
#define MD5_HS4 23
#define MD5_IS1  6                  /* round 4 shift amounts */
#define MD5_IS2 10
#define MD5_IS3 15
#define MD5_IS4 21


/*
 * Compile-time macro declarations for MD5.
 * ** Note: The ``rot'' operator uses the variable ``tmp''.
 * ** It assumes tmp is declared as unsigned int, so that the >>
 * ** operator will shift in zeros rather than extending the sign bit.
 */
#define	MD5_F(X,Y,Z)             ((X&Y) | ((~X)&Z))
#define	MD5_G(X,Y,Z)             ((X&Z) | (Y&(~Z)))
#define MD5_H(X,Y,Z)             (X^Y^Z)
#define MD5_I_(X,Y,Z)            (Y ^ ((X) | (~Z)))
#define MD5_ROT(X,S)             (tmp=X,(tmp<<S) | (tmp>>(32-S)))
#define MD5_FF(A,B,C,D,i,s,lp)   A = MD5_ROT((A + MD5_F(B,C,D) + X[i] + lp),s) + B
#define MD5_GG(A,B,C,D,i,s,lp)   A = MD5_ROT((A + MD5_G(B,C,D) + X[i] + lp),s) + B
#define MD5_HH(A,B,C,D,i,s,lp)   A = MD5_ROT((A + MD5_H(B,C,D) + X[i] + lp),s) + B
#define MD5_II(A,B,C,D,i,s,lp)   A = MD5_ROT((A + MD5_I_(B,C,D) + X[i] + lp),s) + B

#define MD5_UNS(num) num##U

void Md5_reverse(unsigned int *);
static void     Md5_block(Md5_MdPTR, const unsigned int *);


/*
 * MDbegin(MDp)
 * ** Initialize message digest buffer MDp.
 * ** This is a user-callable routine.
 */
void Md5_begin(Md5_MdPTR MDp)
{
    int             i;
    MDp->buffer[0] = MD5_I0;
    MDp->buffer[1] = MD5_I1;
    MDp->buffer[2] = MD5_I2;
    MDp->buffer[3] = MD5_I3;
    for (i = 0; i < 8; i++)
        MDp->count[i] = 0;
    MDp->done = 0;
}

/*
 * Md5_reverse(X)
 * ** Reverse the byte-ordering of every int in X.
 * ** Assumes X is an array of 16 ints.
 * ** The macro revx reverses the byte-ordering of the next word of X.
 */
#define MD5_REVX { t = (*X << 16) | (*X >> 16); \
           *X++ = ((t & 0xFF00FF00) >> 8) | ((t & 0x00FF00FF) << 8); }

void Md5_reverse(unsigned int *X)
{
    register unsigned int t;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
    MD5_REVX;
}

/*
 * Md5_block(MDp,X)
 * ** Update message digest buffer MDp->buffer using 16-word data block X.
 * ** Assumes all 16 words of X are full of data.
 * ** Does not update MDp->count.
 * ** This routine is not user-callable.
 */
static void Md5_block(Md5_MdPTR MDp, const unsigned int *X)
{
    register unsigned int tmp, A, B, C, D;      /* hpux sysv sun */
    A = MDp->buffer[0];
    B = MDp->buffer[1];
    C = MDp->buffer[2];
    D = MDp->buffer[3];

    /*
     * Update the message digest buffer
     */
    MD5_FF(A, B, C, D, 0,  MD5_FS1, MD5_UNS(3614090360));    /* Round 1 */
    MD5_FF(D, A, B, C, 1,  MD5_FS2, MD5_UNS(3905402710));
    MD5_FF(C, D, A, B, 2,  MD5_FS3, MD5_UNS(606105819));
    MD5_FF(B, C, D, A, 3,  MD5_FS4, MD5_UNS(3250441966));
    MD5_FF(A, B, C, D, 4,  MD5_FS1, MD5_UNS(4118548399));
    MD5_FF(D, A, B, C, 5,  MD5_FS2, MD5_UNS(1200080426));
    MD5_FF(C, D, A, B, 6,  MD5_FS3, MD5_UNS(2821735955));
    MD5_FF(B, C, D, A, 7,  MD5_FS4, MD5_UNS(4249261313));
    MD5_FF(A, B, C, D, 8,  MD5_FS1, MD5_UNS(1770035416));
    MD5_FF(D, A, B, C, 9,  MD5_FS2, MD5_UNS(2336552879));
    MD5_FF(C, D, A, B, 10, MD5_FS3, MD5_UNS(4294925233));
    MD5_FF(B, C, D, A, 11, MD5_FS4, MD5_UNS(2304563134));
    MD5_FF(A, B, C, D, 12, MD5_FS1, MD5_UNS(1804603682));
    MD5_FF(D, A, B, C, 13, MD5_FS2, MD5_UNS(4254626195));
    MD5_FF(C, D, A, B, 14, MD5_FS3, MD5_UNS(2792965006));
    MD5_FF(B, C, D, A, 15, MD5_FS4, MD5_UNS(1236535329));
    MD5_GG(A, B, C, D, 1,  MD5_GS1, MD5_UNS(4129170786));    /* Round 2 */
    MD5_GG(D, A, B, C, 6,  MD5_GS2, MD5_UNS(3225465664));
    MD5_GG(C, D, A, B, 11, MD5_GS3, MD5_UNS(643717713));
    MD5_GG(B, C, D, A, 0,  MD5_GS4, MD5_UNS(3921069994));
    MD5_GG(A, B, C, D, 5,  MD5_GS1, MD5_UNS(3593408605));
    MD5_GG(D, A, B, C, 10, MD5_GS2, MD5_UNS(38016083));
    MD5_GG(C, D, A, B, 15, MD5_GS3, MD5_UNS(3634488961));
    MD5_GG(B, C, D, A, 4,  MD5_GS4, MD5_UNS(3889429448));
    MD5_GG(A, B, C, D, 9,  MD5_GS1, MD5_UNS(568446438));
    MD5_GG(D, A, B, C, 14, MD5_GS2, MD5_UNS(3275163606));
    MD5_GG(C, D, A, B, 3,  MD5_GS3, MD5_UNS(4107603335));
    MD5_GG(B, C, D, A, 8,  MD5_GS4, MD5_UNS(1163531501));
    MD5_GG(A, B, C, D, 13, MD5_GS1, MD5_UNS(2850285829));
    MD5_GG(D, A, B, C, 2,  MD5_GS2, MD5_UNS(4243563512));
    MD5_GG(C, D, A, B, 7,  MD5_GS3, MD5_UNS(1735328473));
    MD5_GG(B, C, D, A, 12, MD5_GS4, MD5_UNS(2368359562));
    MD5_HH(A, B, C, D, 5,  MD5_HS1, MD5_UNS(4294588738));    /* Round 3 */
    MD5_HH(D, A, B, C, 8,  MD5_HS2, MD5_UNS(2272392833));
    MD5_HH(C, D, A, B, 11, MD5_HS3, MD5_UNS(1839030562));
    MD5_HH(B, C, D, A, 14, MD5_HS4, MD5_UNS(4259657740));
    MD5_HH(A, B, C, D, 1,  MD5_HS1, MD5_UNS(2763975236));
    MD5_HH(D, A, B, C, 4,  MD5_HS2, MD5_UNS(1272893353));
    MD5_HH(C, D, A, B, 7,  MD5_HS3, MD5_UNS(4139469664));
    MD5_HH(B, C, D, A, 10, MD5_HS4, MD5_UNS(3200236656));
    MD5_HH(A, B, C, D, 13, MD5_HS1, MD5_UNS(681279174));
    MD5_HH(D, A, B, C, 0,  MD5_HS2, MD5_UNS(3936430074));
    MD5_HH(C, D, A, B, 3,  MD5_HS3, MD5_UNS(3572445317));
    MD5_HH(B, C, D, A, 6,  MD5_HS4, MD5_UNS(76029189));
    MD5_HH(A, B, C, D, 9,  MD5_HS1, MD5_UNS(3654602809));
    MD5_HH(D, A, B, C, 12, MD5_HS2, MD5_UNS(3873151461));
    MD5_HH(C, D, A, B, 15, MD5_HS3, MD5_UNS(530742520));
    MD5_HH(B, C, D, A, 2,  MD5_HS4, MD5_UNS(3299628645));
    MD5_II(A, B, C, D, 0,  MD5_IS1, MD5_UNS(4096336452));    /* Round 4 */
    MD5_II(D, A, B, C, 7,  MD5_IS2, MD5_UNS(1126891415));
    MD5_II(C, D, A, B, 14, MD5_IS3, MD5_UNS(2878612391));
    MD5_II(B, C, D, A, 5,  MD5_IS4, MD5_UNS(4237533241));
    MD5_II(A, B, C, D, 12, MD5_IS1, MD5_UNS(1700485571));
    MD5_II(D, A, B, C, 3,  MD5_IS2, MD5_UNS(2399980690));
    MD5_II(C, D, A, B, 10, MD5_IS3, MD5_UNS(4293915773));
    MD5_II(B, C, D, A, 1,  MD5_IS4, MD5_UNS(2240044497));
    MD5_II(A, B, C, D, 8,  MD5_IS1, MD5_UNS(1873313359));
    MD5_II(D, A, B, C, 15, MD5_IS2, MD5_UNS(4264355552));
    MD5_II(C, D, A, B, 6,  MD5_IS3, MD5_UNS(2734768916));
    MD5_II(B, C, D, A, 13, MD5_IS4, MD5_UNS(1309151649));
    MD5_II(A, B, C, D, 4,  MD5_IS1, MD5_UNS(4149444226));
    MD5_II(D, A, B, C, 11, MD5_IS2, MD5_UNS(3174756917));
    MD5_II(C, D, A, B, 2,  MD5_IS3, MD5_UNS(718787259));
    MD5_II(B, C, D, A, 9,  MD5_IS4, MD5_UNS(3951481745));

    MDp->buffer[0] += A;
    MDp->buffer[1] += B;
    MDp->buffer[2] += C;
    MDp->buffer[3] += D;
}

/*
 * Md5_update(MDp,X,count)
 * ** Input: MDp -- an Md5_MdPTR
 * **        X -- a pointer to an array of unsigned characters.
 * **        count -- the number of bits of X to use.
 * **                 (if not a multiple of 8, uses high bits of last byte.)
 * ** Update MDp using the number of bits of X given by count.
 * ** This is the basic input routine for an MD5 user.
 * ** The routine completes the MD computation when count < 512, so
 * ** every MD computation should end with one call to MDupdate with a
 * ** count less than 512.  A call with count 0 will be ignored if the
 * ** MD has already been terminated (done != 0), so an extra call with count
 * ** 0 can be given as a ``courtesy close'' to force termination if desired.
 * ** Returns : 0 if processing succeeds or was already done;
 * **          -1 if processing was already done
 * **          -2 if count was too large
 */
int Md5_update(Md5_MdPTR MDp, const unsigned char *X, unsigned int count)
{
    unsigned int    i, tmp, bit, byte, mask;
    unsigned char   XX[64];
    unsigned char  *p;
    /*
     * return with no error if this is a courtesy close with count
     * ** zero and MDp->done is true.
     */
    if (count == 0 && MDp->done)
        return 0;
    /*
     * check to see if MD is already done and report error
     */
    if (MDp->done) {
        return -1;
    }
    /*
     * if (MDp->done) { fprintf(stderr,"\nError: MDupdate MD already done."); return; }
     */
    /*
     * Add count to MDp->count
     */
    tmp = count;
    p = MDp->count;
    while (tmp) {
        tmp += *p;
        *p++ = tmp;
        tmp = tmp >> 8;
    }
    /*
     * Process data
     */
    if (count == 512) {         /* Full block of data to handle */
        Md5_block(MDp, (const unsigned int *) X);
    } else if (count > 512)     /* Check for count too large */
        return -2;
    /*
     * { fprintf(stderr,"\nError: MDupdate called with illegal count value %d.",count);
     * return;
     * }
     */
    else {                      /* partial block -- must be last block so finish up */
        /*
         * Find out how many bytes and residual bits there are
         */
        int             copycount;
        byte = count >> 3;
        bit = count & 7;
        copycount = byte;
        if (bit)
            copycount++;
        /*
         * Copy X into XX since we need to modify it
         */
        memset(XX, 0, sizeof(XX));
        memcpy(XX, X, copycount);

        /*
         * Add padding '1' bit and low-order zeros in last byte
         */
        mask = ((unsigned long) 1) << (7 - bit);
        XX[byte] = (XX[byte] | mask) & ~(mask - 1);
        /*
         * If room for bit count, finish up with this block
         */
        if (byte <= 55) {
            for (i = 0; i < 8; i++)
                XX[56 + i] = MDp->count[i];
            Md5_block(MDp, (unsigned int *) XX);
        } else {                /* need to do two blocks to finish up */
            Md5_block(MDp, (unsigned int *) XX);
            for (i = 0; i < 56; i++)
                XX[i] = 0;
            for (i = 0; i < 8; i++)
                XX[56 + i] = MDp->count[i];
            Md5_block(MDp, (unsigned int *) XX);
        }
        /*
         * Set flag saying we're done with MD computation
         */
        MDp->done = 1;
    }
    return 0;
}

/*
 * Md5_checksum(data, len, MD5): do a checksum on an arbirtrary amount of data
 */
int Md5_checksum(const u_char * data, size_t len, u_char * mac, size_t maclen)
{
    Md5_MdStruct        md;
    Md5_MdStruct       *MD = &md;
    int             rc = 0;

    Md5_begin(MD);
    while (len >= 64) {
        rc = Md5_update(MD, data, 64 * 8);
        if (rc)
            goto check_end;
        data += 64;
        len -= 64;
    }
    rc = Md5_update(MD, data, len * 8);
    if (rc)
        goto check_end;

    /*
     * copy the checksum to the outgoing data (all of it that is requested).
     */
    Md5_get(MD, mac, maclen);

  check_end:
    memset(&md, 0, sizeof(md));
    return rc;
}


/*
 * Md5_sign(data, len, MD5): do a checksum on an arbirtrary amount
 * of data, and prepended with a secret in the standard fashion
 */
int Md5_sign(const u_char * data, size_t len, u_char * mac, size_t maclen,
       const u_char * secret, size_t secretlen)
{
#define HASHKEYLEN 64

    Md5_MdStruct        MD;
    u_char          K1[HASHKEYLEN];
    u_char          K2[HASHKEYLEN];
    u_char          extendedAuthKey[HASHKEYLEN];
    u_char          buf[HASHKEYLEN];
    size_t          i;
    const u_char   *cp;
    u_char         *newdata = NULL;
    int             rc = 0;

    /*
     * memset(K1,0,HASHKEYLEN);
     * memset(K2,0,HASHKEYLEN);
     * memset(buf,0,HASHKEYLEN);
     * memset(extendedAuthKey,0,HASHKEYLEN);
     */

    if (secretlen != 16 || secret == NULL || mac == NULL || data == NULL ||
        len <= 0 || maclen <= 0) {
        /*
         * DEBUGMSGTL(("md5","MD5 signing not properly initialized"));
         */
        return -1;
    }

    memset(extendedAuthKey, 0, HASHKEYLEN);
    memcpy(extendedAuthKey, secret, secretlen);
    for (i = 0; i < HASHKEYLEN; i++) {
        K1[i] = extendedAuthKey[i] ^ 0x36;
        K2[i] = extendedAuthKey[i] ^ 0x5c;
    }

    Md5_begin(&MD);
    rc = Md5_update(&MD, K1, HASHKEYLEN * 8);
    if (rc)
        goto update_end;

    i = len;
    if (((uintptr_t) data) % sizeof(long) != 0) {
        /*
         * this relies on the ability to use integer math and thus we
         * must rely on data that aligns on 32-bit-word-boundries
         */
        newdata = (u_char *)Tools_memdup(data, len);
        cp = newdata;
    } else {
        cp = data;
    }

    while (i >= 64) {
        rc = Md5_update(&MD, cp, 64 * 8);
        if (rc)
            goto update_end;
        cp += 64;
        i -= 64;
    }

    rc = Md5_update(&MD, cp, i * 8);
    if (rc)
        goto update_end;

    memset(buf, 0, HASHKEYLEN);
    Md5_get(&MD, buf, HASHKEYLEN);

    Md5_begin(&MD);
    rc = Md5_update(&MD, K2, HASHKEYLEN * 8);
    if (rc)
        goto update_end;
    rc = Md5_update(&MD, buf, 16 * 8);
    if (rc)
        goto update_end;

    /*
     * copy the sign checksum to the outgoing pointer
     */
    Md5_get(&MD, mac, maclen);

  update_end:
    memset(buf, 0, HASHKEYLEN);
    memset(K1, 0, HASHKEYLEN);
    memset(K2, 0, HASHKEYLEN);
    memset(extendedAuthKey, 0, HASHKEYLEN);
    memset(&MD, 0, sizeof(MD));

    if (newdata)
        free(newdata);
    return rc;
}

void Md5_get(Md5_MdStruct * MD, u_char * buf, size_t buflen)
{
    int             i, j;

    /*
     * copy the checksum to the outgoing data (all of it that is requested).
     */
    for (i = 0; i < 4 && i * 4 < (int) buflen; i++)
        for (j = 0; j < 4 && i * 4 + j < (int) buflen; j++)
            buf[i * 4 + j] = (MD->buffer[i] >> j * 8) & 0xff;
}
