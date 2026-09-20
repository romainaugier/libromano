/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/fmt.h"
#include "libromano/bit.h"

#include <string.h>
#include <math.h>

/* https://lemire.me/blog/2021/06/03/computing-the-number-of-digits-of-an-integer-even-faster/ */

ROMANO_FORCE_INLINE int floor_log10_pow2(int e)
{
    return (e * 1262611) >> 22;
}

ROMANO_FORCE_INLINE int ceil_log10_pow2(int e)
{
    return e == 0 ? 0 : floor_log10_pow2(e) + 1;
}

typedef struct {
    uint64_t entry[64];
} digit_count_table_holder_t;

/* Function to generate digit count table */
void generate_digit_count_table(uint64_t* table)
{
    static const uint64_t pow10[] = {
        1ULL,
        10ULL,
        100ULL,
        1000ULL,
        10000ULL,
        100000ULL,
        1000000ULL,
        10000000ULL,
        100000000ULL,
        1000000000ULL,
        10000000000ULL,
        100000000000ULL,
        1000000000000ULL,
        10000000000000ULL,
        100000000000000ULL,
        1000000000000000ULL,
        10000000000000000ULL,
        100000000000000000ULL,
        1000000000000000000ULL,
        10000000000000000000ULL
    };

    for(int i = 0; i < 64; i++)
    {
        uint64_t ub = (uint64_t)ceil_log10_pow2(i);

        ROMANO_ASSERT(ub <= 19, "ub should be less than 19");

        table[i] = ((ub + 1) << 52) - (pow10[ub] >> (i / 4));
    }
}

static const uint64_t g_digit_count_table[64] = {
    4503599627370495,
    9007199254740982,
    9007199254740982,
    9007199254740982,
    13510798882111438,
    13510798882111438,
    13510798882111438,
    18014398509481484,
    18014398509481734,
    18014398509481734,
    22517998136849980,
    22517998136849980,
    22517998136851230,
    22517998136851230,
    27021597764210476,
    27021597764210476,
    27021597764216726,
    31525197391530972,
    31525197391530972,
    31525197391530972,
    36028797018651468,
    36028797018651468,
    36028797018651468,
    36028797018651468,
    40532396644771964,
    40532396644771964,
    40532396644771964,
    45035996258079960,
    45035996265892460,
    45035996265892460,
    49539595822950456,
    49539595822950456,
    49539595862012956,
    49539595862012956,
    54043195137820952,
    54043195137820952,
    54043195333133452,
    58546793202691448,
    58546793202691448,
    58546793202691448,
    63050385017561944,
    63050385017561944,
    63050385017561944,
    63050385017561944,
    67553945582432440,
    67553945582432440,
    67553945582432440,
    72057105756677936,
    72057349897302936,
    72057349897302936,
    76558752259048432,
    76558752259048432,
    76559972962173432,
    76559972962173432,
    81052586261418928,
    81052586261418928,
    81058689777043928,
    85507357763789424,
    85507357763789424,
    85507357763789424,
    89766816766159920,
    89766816766159920,
    89766816766159920,
    89766816766159920,
};

ROMANO_FORCE_INLINE int floor_log2(uint64_t n)
{
    return 63 ^ clz_u64(n);
}

ROMANO_FORCE_INLINE int count_digits(uint64_t n)
{
    return n == 0 ? 1 : ((int)((g_digit_count_table[floor_log2(n)] + (n >> (floor_log2(n) / 4))) >> 52));
}

static const char digit_pairs[200] = {
    "00010203040506070809"
    "10111213141516171819"
    "20212223242526272829"
    "30313233343536373839"
    "40414243444546474849"
    "50515253545556575859"
    "60616263646566676869"
    "70717273747576777879"
    "80818283848586878889"
    "90919293949596979899"
};

static char* utoa_fast(uint64_t val, char* buf)
{
    int ndigits;
    char* p_end;

    ndigits = count_digits(val);
    p_end = buf + ndigits - 1;

    while(val >= 100)
    {
        unsigned int r = val % 100;
        val /= 100;
        *p_end-- = digit_pairs[r * 2 + 1];
        *p_end-- = digit_pairs[r * 2];
    }

    if(val >= 10)
    {
        *p_end-- = digit_pairs[val * 2 + 1];
        *p_end = digit_pairs[val * 2];
    }
    else
    {
        *p_end = '0' + val;
    }

    return buf + ndigits;
}

int fmt_size_i64(int64_t i64)
{
    return count_digits(abs_i64(i64)) + (int)(i64 < 0);
}

int fmt_i64(char* buffer, int64_t i64)
{
    bool negative = i64 < 0;

    if(negative)
        *(buffer++) = '-';

    return (int)(utoa_fast(abs_i64(i64), buffer) - buffer) + (int)negative;
}

int fmt_size_u64(uint64_t u64)
{
    return count_digits(u64);
}

int fmt_u64(char* buffer, uint64_t u64)
{
    return (int)(utoa_fast(u64, buffer) - buffer);
}

static const uint64_t g_pow10_u64[18] = {
    1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, 1000000ULL, 10000000ULL, 100000000ULL,
    1000000000ULL, 10000000000ULL, 100000000000ULL, 1000000000000ULL, 10000000000000ULL,
    100000000000000ULL, 1000000000000000ULL, 10000000000000000ULL, 100000000000000000ULL
};

int fmt_f64(char* buffer, double f64, int precision)
{
    char* p = buffer;
    double int_part;
    double frac_part;
    uint64_t int_value;
    uint64_t frac_value;
    uint64_t scale;

    if(isnan(f64))
    {
        memcpy(p, "nan", 3);
        return 3;
    }

    if(signbit(f64))
    {
        *p++ = '-';
        f64 = -f64;
    }

    if(isinf(f64))
    {
        memcpy(p, "inf", 3);
        return (int)(p - buffer) + 3;
    }

    if(precision < 0)
        precision = 6;

    if(precision > 17)
        precision = 17;

    frac_part = modf(f64, &int_part);

    if(int_part >= 18446744073709551616.0)
    {
        memcpy(p, "ovf", 3);
        return (int)(p - buffer) + 3;
    }

    int_value = (uint64_t)int_part;
    scale = g_pow10_u64[precision];
    frac_value = (uint64_t)(frac_part * (double)scale + 0.5);

    if(frac_value >= scale)
    {
        frac_value -= scale;
        int_value++;
    }

    p = utoa_fast(int_value, p);

    if(precision > 0)
    {
        char digits[20];
        int digits_size = (int)(utoa_fast(frac_value, digits) - digits);

        *p++ = '.';

        memset(p, '0', (size_t)(precision - digits_size));
        p += precision - digits_size;

        memcpy(p, digits, (size_t)digits_size);
        p += digits_size;
    }

    return (int)(p - buffer);
}

int fmt_size_f64(double f64, int precision)
{
    char buffer[64];

    return fmt_f64(buffer, f64, precision);
}
