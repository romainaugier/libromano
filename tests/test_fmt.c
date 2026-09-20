/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/fmt.h"

#include <ctype.h>
#include <float.h>

static void check_i64(int64_t value)
{
    char expected[32];
    char buffer[32];
    int expected_size = snprintf(expected, sizeof(expected), "%lld", (long long)value);
    int size = fmt_i64(buffer, value);

    TEST_CHECK_EQ_INT(fmt_size_i64(value), expected_size);
    TEST_CHECK_EQ_INT(size, expected_size);
    TEST_CHECK_MSG(size == expected_size && memcmp(buffer, expected, (size_t)size) == 0,
                   "fmt_i64(%s) = %.*s", expected, size, buffer);
}

static void check_u64(uint64_t value)
{
    char expected[32];
    char buffer[32];
    int expected_size = snprintf(expected, sizeof(expected), "%llu", (unsigned long long)value);
    int size = fmt_u64(buffer, value);

    TEST_CHECK_EQ_INT(fmt_size_u64(value), expected_size);
    TEST_CHECK_EQ_INT(size, expected_size);
    TEST_CHECK_MSG(size == expected_size && memcmp(buffer, expected, (size_t)size) == 0,
                   "fmt_u64(%s) = %.*s", expected, size, buffer);
}

static void test_integers_edge_cases(void)
{
    uint64_t power = 1;
    int i;

    check_i64(0);
    check_i64(1);
    check_i64(-1);
    check_i64(INT64_MAX);
    check_i64(INT64_MIN);
    check_u64(0);
    check_u64(UINT64_MAX);

    for(i = 0; i < 20; i++)
    {
        check_u64(power);
        check_u64(power - 1);
        check_u64(power + 1);
        check_i64((int64_t)power);
        check_i64(-(int64_t)power);
        check_i64((int64_t)power - 1);

        if(i < 19)
            power *= 10;
    }
}

static bool property_integers(FuzzSource* source, void* user_data)
{
    const int64_t i = fuzz_i64_special(source);
    const uint64_t u = fuzz_u64_special(source);
    char buffer[32];
    char expected[32];
    int size;

    ROMANO_UNUSED(user_data);

    snprintf(expected, sizeof(expected), "%lld", (long long)i);
    size = fmt_i64(buffer, i);
    TEST_FUZZ_CHECK_MSG((size_t)size == strlen(expected) && memcmp(buffer, expected, (size_t)size) == 0, "i64 %s", expected);
    TEST_FUZZ_CHECK(fmt_size_i64(i) == size);

    snprintf(expected, sizeof(expected), "%llu", (unsigned long long)u);
    size = fmt_u64(buffer, u);
    TEST_FUZZ_CHECK_MSG((size_t)size == strlen(expected) && memcmp(buffer, expected, (size_t)size) == 0, "u64 %s", expected);
    TEST_FUZZ_CHECK(fmt_size_u64(u) == size);

    return true;
}

static void test_fuzz_integers(void)
{
    test_fuzz_property("fmt_integers", 50000, property_integers, NULL);
}

static void test_f64_special_values(void)
{
    char buffer[320];
    int size;

    size = fmt_f64(buffer, NAN, 3);
    TEST_CHECK_EQ_INT(size, 3);
    TEST_CHECK_EQ_MEM(buffer, "nan", 3);
    TEST_CHECK_EQ_INT(fmt_size_f64(NAN, 3), 3);

    size = fmt_f64(buffer, HUGE_VAL, 3);
    TEST_CHECK_EQ_INT(size, 3);
    TEST_CHECK_EQ_MEM(buffer, "inf", 3);
    TEST_CHECK_EQ_INT(fmt_size_f64(HUGE_VAL, 3), 3);

    size = fmt_f64(buffer, -HUGE_VAL, 3);
    TEST_CHECK_EQ_INT(size, 4);
    TEST_CHECK_EQ_MEM(buffer, "-inf", 4);
    TEST_CHECK_EQ_INT(fmt_size_f64(-HUGE_VAL, 3), 4);

    size = fmt_f64(buffer, 0.0, 2);
    TEST_CHECK_EQ_INT(size, 4);
    TEST_CHECK_EQ_MEM(buffer, "0.00", 4);

    size = fmt_f64(buffer, -0.0, 1);
    TEST_CHECK_EQ_INT(size, 4);
    TEST_CHECK_EQ_MEM(buffer, "-0.0", 4);
    TEST_CHECK_EQ_INT(fmt_size_f64(-0.0, 1), 4);

    size = fmt_f64(buffer, 0.0, 0);
    TEST_CHECK_EQ_INT(size, 1);
    TEST_CHECK_EQ_INT(fmt_size_f64(0.0, 0), 1);

    size = fmt_f64(buffer, 0.0, -1);
    TEST_CHECK_EQ_INT(size, 8);
    TEST_CHECK_EQ_MEM(buffer, "0.000000", 8);

    size = fmt_f64(buffer, 9.99, 1);
    TEST_CHECK_EQ_MEM(buffer, "10.0", 4);
    TEST_CHECK_EQ_INT(size, 4);
    TEST_CHECK_EQ_INT(fmt_size_f64(9.99, 1), 4);

    size = fmt_f64(buffer, -9.96, 1);
    TEST_CHECK_EQ_MEM(buffer, "-10.0", 5);
    TEST_CHECK_EQ_INT(size, 5);

    size = fmt_f64(buffer, 51937.220206291094655, 6);
    TEST_CHECK_EQ_MEM(buffer, "51937.220206", 12);

    size = fmt_f64(buffer, 1e300, 2);
    TEST_CHECK_EQ_INT(size, 3);
    TEST_CHECK_EQ_MEM(buffer, "ovf", 3);
    TEST_CHECK_EQ_INT(fmt_size_f64(1e300, 2), 3);
}

static bool well_formed(const char* buffer, int size, int precision)
{
    int i = 0;
    int digits = 0;

    if(i < size && buffer[i] == '-')
        i++;

    while(i < size && isdigit((unsigned char)buffer[i]))
        i++, digits++;

    if(digits == 0)
        return false;

    if(precision == 0)
        return i == size;

    if(i >= size || buffer[i++] != '.')
        return false;

    for(digits = 0; i < size; i++, digits++)
        if(!isdigit((unsigned char)buffer[i]))
            return false;

    return digits == precision;
}

static bool property_f64(FuzzSource* source, void* user_data)
{
    const double value = fuzz_f64_finite(source);
    int precision = (int)fuzz_range_i64(source, -1, 20);
    const int effective_precision = precision < 0 ? 6 : (precision > 17 ? 17 : precision);
    char buffer[320];
    char* end;
    double parsed;
    double tolerance;
    int size;

    ROMANO_UNUSED(user_data);

    if(fabs(value) >= 1.8e19)
        return true;

    size = fmt_f64(buffer, value, precision);
    buffer[size] = '\0';

    TEST_FUZZ_CHECK_MSG(fmt_size_f64(value, precision) == size, "size of %.17g (precision %d): %d vs \"%s\"",
                        value, precision, fmt_size_f64(value, precision), buffer);
    TEST_FUZZ_CHECK_MSG(well_formed(buffer, size, effective_precision), "\"%s\" for %.17g (precision %d)",
                        buffer, value, precision);
    TEST_FUZZ_CHECK_MSG((buffer[0] == '-') == (signbit(value) != 0), "sign of \"%s\" for %.17g", buffer, value);

    parsed = strtod(buffer, &end);
    tolerance = 0.5 * pow(10.0, -effective_precision) + fabs(value) * 4.0 * DBL_EPSILON + 1e-17;

    TEST_FUZZ_CHECK_MSG(fabs(parsed - value) <= tolerance, "%.17g formatted as \"%s\" (precision %d)",
                        value, buffer, precision);

    return true;
}

static void test_fuzz_f64(void)
{
    test_fuzz_property("fmt_f64", 50000, property_f64, NULL);
}

TEST_MAIN(
    TEST(test_integers_edge_cases),
    TEST(test_fuzz_integers),
    TEST(test_f64_special_values),
    TEST(test_fuzz_f64),
)
