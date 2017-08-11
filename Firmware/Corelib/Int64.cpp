#include "Int64.h"
#include "Debug.h"
#include "Assert.h"
/**
 * Divide an unsigned 64-bit integer by 10.
 *
 * @param[in]  u64   Number to be divided.
 * @param[out] pu64Q Quotient.
 * @param[out] puR   Remainder.
 */
void Int64_divBy10(Int64_U64 u64, Int64_U64 * pu64Q, unsigned int *puR)
{
    unsigned long   ulT;
    unsigned long   ulQ;
    unsigned long   ulR;

    /*
     * top 16 bits
     */
    ulT = (u64.high >> 16) & 0x0ffff;
    ulQ = ulT / 10;
    ulR = ulT % 10;
    pu64Q->high = ulQ << 16;

    /*
     * next 16
     */
    ulT = (u64.high & 0x0ffff);
    ulT += (ulR << 16);
    ulQ = ulT / 10;
    ulR = ulT % 10;
    pu64Q->high = pu64Q->high | ulQ;

    /*
     * next 16
     */
    ulT = ((u64.low >> 16) & 0x0ffff) + (ulR << 16);
    ulQ = ulT / 10;
    ulR = ulT % 10;
    pu64Q->low = ulQ << 16;

    /*
     * final 16
     */
    ulT = (u64.low & 0x0ffff);
    ulT += (ulR << 16);
    ulQ = ulT / 10;
    ulR = ulT % 10;
    pu64Q->low = pu64Q->low | ulQ;

    *puR = (unsigned int) (ulR);
}

/**
 * Multiply an unsigned 64-bit integer by 10.
 *
 * @param[in]  u64   Number to be multiplied.
 * @param[out] pu64P Product.
 */
void Int64_multBy10(Int64_U64 u64, Int64_U64 * pu64P)
{
    unsigned long   ulT;
    unsigned long   ulP;
    unsigned long   ulK;

    /*
     * lower 16 bits
     */
    ulT = u64.low & 0x0ffff;
    ulP = ulT * 10;
    ulK = ulP >> 16;
    pu64P->low = ulP & 0x0ffff;

    /*
     * next 16
     */
    ulT = (u64.low >> 16) & 0x0ffff;
    ulP = (ulT * 10) + ulK;
    ulK = ulP >> 16;
    pu64P->low = (ulP & 0x0ffff) << 16 | pu64P->low;

    /*
     * next 16 bits
     */
    ulT = u64.high & 0x0ffff;
    ulP = (ulT * 10) + ulK;
    ulK = ulP >> 16;
    pu64P->high = ulP & 0x0ffff;

    /*
     * final 16
     */
    ulT = (u64.high >> 16) & 0x0ffff;
    ulP = (ulT * 10) + ulK;
    ulK = ulP >> 16;
    pu64P->high = (ulP & 0x0ffff) << 16 | pu64P->high;
}

/**
 * Add an unsigned 16-bit int to an unsigned 64-bit integer.
 *
 * @param[in,out] pu64 Number to be incremented.
 * @param[in]     u16  Amount to add.
 *
 */
void Int64_incrByU16(Int64_U64 * pu64, unsigned int u16)
{
    Int64_incrByU32(pu64, u16);
}

/**
 * Add an unsigned 32-bit int to an unsigned 64-bit integer.
 *
 * @param[in,out] pu64 Number to be incremented.
 * @param[in]     u32  Amount to add.
 *
 */
void Int64_incrByU32(Int64_U64 * pu64, unsigned int u32)
{
    uint32_t tmp;

    tmp = pu64->low;
    pu64->low = (uint32_t)(tmp + u32);
    if (pu64->low < tmp)
        pu64->high = (uint32_t)(pu64->high + 1);
}

/**
 * Subtract two 64-bit numbers.
 *
 * @param[in] pu64one Number to start from.
 * @param[in] pu64two Amount to subtract.
 * @param[out] pu64out pu64one - pu64two.
 */
void Int64_u64Subtract(const Int64_U64 * pu64one, const Int64_U64 * pu64two, Int64_U64 * pu64out)
{
    int carry;

    carry = pu64one->low < pu64two->low;
    pu64out->low = (uint32_t)(pu64one->low - pu64two->low);
    pu64out->high = (uint32_t)(pu64one->high - pu64two->high - carry);
}

/**
 * Add two 64-bit numbers.
 *
 * @param[in] pu64one Amount to add.
 * @param[in,out] pu64out pu64out += pu64one.
 */
void Int64_u64Incr(Int64_U64 * pu64out, const Int64_U64 * pu64one)
{
    pu64out->high = (uint32_t)(pu64out->high + pu64one->high);
    Int64_incrByU32(pu64out, pu64one->low);
}

/**
 * Add the difference of two 64-bit numbers to a 64-bit counter.
 *
 * @param[in] pu64one
 * @param[in] pu64two
 * @param[out] pu64out pu64out += (pu64one - pu64two)
 */
void Int64_u64UpdateCounter(Int64_U64 * pu64out, const Int64_U64 * pu64one, const Int64_U64 * pu64two)
{
    Int64_U64 tmp;

    Int64_u64Subtract(pu64one, pu64two, &tmp);
    Int64_u64Incr(pu64out, &tmp);
}

/**
 * Copy a 64-bit number.
 *
 * @param[in] pu64two Number to be copied.
 * @param[out] pu64one Where to store the copy - *pu64one = *pu64two.
 */
void Int64_u64Copy(Int64_U64 * pu64one, const Int64_U64 * pu64two)
{
    *pu64one = *pu64two;
}

/**
 * Set an unsigned 64-bit number to zero.
 *
 * @param[in] pu64 Number to be zeroed.
 */
void Int64_zeroU64(Int64_U64 * pu64)
{
    pu64->low = 0;
    pu64->high = 0;
}

/**
 * Check if an unsigned 64-bit number is zero.
 *
 * @param[in] pu64 Number to be checked.
 */
int Int64_isZeroU64(const Int64_U64 * pu64)
{
    return pu64->low == 0 && pu64->high == 0;
}

/**
 * check the old and new values of a counter64 for 32bit wrapping
 *
 * @param adjust : set to 1 to auto-increment new_val->high
 *                 if a 32bit wrap is detected.
 *
 * @param old_val
 * @param new_val
 *
 * @note
 * The old and new values must be be from within a time period
 * which would only allow the 32bit portion of the counter to
 * wrap once. i.e. if the 32bit portion of the counter could
 * wrap every 60 seconds, the old and new values should be compared
 * at least every 59 seconds (though I'd recommend at least every
 * 50 seconds to allow for timer inaccuracies).
 *
 * @retval 64 : 64bit wrap
 * @retval 32 : 32bit wrap
 * @retval  0 : did not wrap
 * @retval -1 : bad parameter
 * @retval -2 : unexpected high value (changed by more than 1)
 */
int Int64_c64CheckFor32bitWrap(Int64_U64 *oldVal, Int64_U64 *newVal, int adjust)
{
    if( (NULL == oldVal) || (NULL == newVal) )
        return -1;

    DEBUG_MSGTL(("9:c64:check_wrap", "check wrap 0x%0lx.0x%0lx 0x%0lx.0x%0lx\n",
                oldVal->high, oldVal->low, newVal->high, newVal->low));

    /*
     * check for wraps
     */
    if ((newVal->low >= oldVal->low) &&
        (newVal->high == oldVal->high)) {
        DEBUG_MSGTL(("9:c64:check_wrap", "no wrap\n"));
        return 0;
    }

    /*
     * low wrapped. did high change?
     */
    if (newVal->high == oldVal->high) {
        DEBUG_MSGTL(("c64:check_wrap", "32 bit wrap\n"));
        if (adjust)
            newVal->high = (uint32_t)(newVal->high + 1);
        return 32;
    }
    else if (newVal->high == (uint32_t)(oldVal->high + 1)) {
        DEBUG_MSGTL(("c64:check_wrap", "64 bit wrap\n"));
        return 64;
    }

    return -2;
}

/**
 * update a 64 bit value with the difference between two (possibly) 32 bit vals
 *
 * @param prev_val       : the 64 bit current counter
 * @param old_prev_val   : the (possibly 32 bit) previous value
 * @param new_val        : the (possible 32bit) new value
 * @param need_wrap_check: pointer to integer indicating if wrap check is needed
 *                         flag may be cleared if 64 bit counter is detected
 *
 * @note
 * The old_prev_val and new_val values must be be from within a time
 * period which would only allow the 32bit portion of the counter to
 * wrap once. i.e. if the 32bit portion of the counter could
 * wrap every 60 seconds, the old and new values should be compared
 * at least every 59 seconds (though I'd recommend at least every
 * 50 seconds to allow for timer inaccuracies).
 *
 * Suggested use:
 *
 *   static needwrapcheck = 1;
 *   static counter64 current, prev_val, new_val;
 *
 *   your_functions_to_update_new_value(&new_val);
 *   if (0 == needwrapcheck)
 *      memcpy(current, new_val, sizeof(new_val));
 *   else {
 *      netsnmp_c64_check32_and_update(&current,&new,&prev,&needwrapcheck);
 *      memcpy(prev_val, new_val, sizeof(new_val));
 *   }
 *
 *
 * @retval  0 : success
 * @retval -1 : error checking for 32 bit wrap
 * @retval -2 : look like we have 64 bit values, but sums aren't consistent
 */
int Int64_c64Check32AndUpdate(Int64_U64 *prevVal, Int64_U64 *newVal,
                              Int64_U64 *oldPrevVal, int *needWrapCheck)
{
    int rc;

    /*
     * counters are 32bit or unknown (which we'll treat as 32bit).
     * update the prev values with the difference between the
     * new stats and the prev old_stats:
     *    prev->stats += (new->stats - prev->old_stats)
     */
    if ((NULL == needWrapCheck) || (0 != *needWrapCheck)) {
        rc = Int64_c64CheckFor32bitWrap(oldPrevVal,newVal, 1);
        if (rc < 0) {
            DEBUG_MSGTL(("c64","32 bit check failed\n"));
            return -1;
        }
    }
    else
        rc = 0;

    /*
     * update previous values
     */
    (void) Int64_u64UpdateCounter(prevVal, newVal, oldPrevVal);

    /*
     * if wrap check was 32 bit, undo adjust, now that prev is updated
     */
    if (32 == rc) {
        /*
         * check wrap incremented high, so reset it. (Because having
         * high set for a 32 bit counter will confuse us in the next update).
         */
        Assert_assert(1 == newVal->high);
        newVal->high = 0;
    }
    else if (64 == rc) {
        /*
         * if we really have 64 bit counters, the summing we've been
         * doing for prev values should be equal to the new values.
         */
        if ((prevVal->low != newVal->low) ||
            (prevVal->high != newVal->high)) {
            DEBUG_MSGTL(("c64", "looks like a 64bit wrap, but prev!=new\n"));
            return -2;
        }
        else if (NULL != needWrapCheck)
            *needWrapCheck = 0;
    }

    return 0;
}

/** Convert an unsigned 64-bit number to ASCII. */
void Int64_printU64(char *buf, /* char [I64CHARSZ+1]; */
         const Int64_U64 * pu64)
{
    Int64_U64             u64a;
    Int64_U64             u64b;

    char            aRes[INT64_I64CHARSZ + 1];
    unsigned int    u;
    int             j;

    u64a = *pu64;
    aRes[INT64_I64CHARSZ] = 0;
    for (j = 0; j < INT64_I64CHARSZ; j++) {
        Int64_divBy10(u64a, &u64b, &u);
        aRes[(INT64_I64CHARSZ - 1) - j] = (char) ('0' + u);
        u64a = u64b;
        if (Int64_isZeroU64(&u64a))
            break;
    }
    strcpy(buf, &aRes[(INT64_I64CHARSZ - 1) - j]);
}

/** Convert a signed 64-bit number to ASCII. */
void Int64_printI64(char *buf, /* char [I64CHARSZ+1]; */
         const Int64_U64 * pu64)
{
    Int64_U64             u64a;

    if (pu64->high & 0x80000000) {
        u64a.high = (uint32_t) ~pu64->high;
        u64a.low = (uint32_t) ~pu64->low;
        Int64_incrByU32(&u64a, 1);    /* bit invert and incr by 1 to print 2s complement */
        buf[0] = '-';
        Int64_printU64(buf + 1, &u64a);
    } else {
        Int64_printU64(buf, pu64);
    }
}

/** Convert a signed 64-bit integer from ASCII to U64. */
int Int64_read64(Int64_U64 * i64, const char *str)
{
    Int64_U64             i64p;
    unsigned int    u;
    int             sign = 0;
    int             ok = 0;

    Int64_zeroU64(i64);
    if (*str == '-') {
        sign = 1;
        str++;
    }

    while (*str && isdigit((unsigned char)(*str))) {
        ok = 1;
        u = *str - '0';
        Int64_multBy10(*i64, &i64p);
        *i64 = i64p;
        Int64_incrByU16(i64, u);
        str++;
    }
    if (sign) {
        i64->high = (uint32_t) ~i64->high;
        i64->low = (uint32_t) ~i64->low;
        Int64_incrByU16(i64, 1);
    }
    return ok;
}
