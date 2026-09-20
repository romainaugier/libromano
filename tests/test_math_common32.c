/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/math/common32.h"

#include <float.h>

static bool near_rel(float got, double expected, double tolerance)
{
    if(isnan(expected))
        return isnan(got);

    if(isinf(expected))
        return got == (float)expected;

    return fabs((double)got - expected) <= tolerance * (fabs(expected) > 1.0 ? fabs(expected) : 1.0);
}

static void test_constants(void)
{
    TEST_CHECK(INF > MAX_FLOAT);
    TEST_CHECK(NEGINF < -MAX_FLOAT);
    TEST_CHECK(isinf(INF) && isinf(NEGINF));
    TEST_CHECK_NEAR(PI, 3.14159265358979, 1e-6);
    TEST_CHECK_NEAR(PI_OVER_TWO * 2.0f, PI, 1e-6);
    TEST_CHECK_NEAR(ONE_OVER_PI * PI, 1.0, 1e-6);
    TEST_CHECK_NEAR(SQRT2 * ONE_OVER_SQRT2, 1.0, 1e-6);
    TEST_CHECK_NEAR(logf(E), 1.0, 1e-6);
    TEST_CHECK_NEAR(LN2 * LOG2E, 1.0, 1e-6);
    TEST_CHECK_NEAR(LN10 * LOG10E, 1.0, 1e-6);
    TEST_CHECK(MIN_FLOAT == FLT_MIN && MAX_FLOAT == FLT_MAX);
}

static void test_classification(void)
{
    const float nan_value = NAN;

    TEST_CHECK(mathf_isinf(INF) && mathf_isinf(NEGINF));
    TEST_CHECK(!mathf_isinf(0.0f) && !mathf_isinf(1.0f) && !mathf_isinf(nan_value));
    TEST_CHECK(mathf_isnan(nan_value) && !mathf_isnan(1.0f) && !mathf_isnan(INF));
    TEST_CHECK(mathf_isfinite(0.0f) && mathf_isfinite(-MAX_FLOAT) && mathf_isfinite(FLT_MIN / 4.0f));
    TEST_CHECK(!mathf_isfinite(INF) && !mathf_isfinite(NEGINF) && !mathf_isfinite(nan_value));

    TEST_CHECK(mathf_float_eq(1.5f, 1.5f) && !mathf_float_eq(1.5f, 1.25f) && !mathf_float_eq(nan_value, nan_value));
    TEST_CHECK(mathf_float_gt(2.0f, 1.0f) && !mathf_float_gt(1.0f, 2.0f) && !mathf_float_gt(nan_value, 1.0f));
    TEST_CHECK(mathf_float_lt(1.0f, 2.0f) && !mathf_float_lt(2.0f, 1.0f) && !mathf_float_lt(nan_value, 1.0f));
}

static void test_helpers(void)
{
    TEST_CHECK_EQ_INT(mathf_to_int(3.9f), 3);
    TEST_CHECK_EQ_INT(mathf_to_int(-3.9f), -3);
    TEST_CHECK(mathf_to_float(7) == 7.0f);
    TEST_CHECK(mathf_sqr(-3.0f) == 9.0f);
    TEST_CHECK_NEAR(mathf_smax(1.0f, 3.0f, 0.0f), 3.0, 1e-6);
    TEST_CHECK_NEAR(mathf_fit(5.0f, 0.0f, 10.0f, 100.0f, 200.0f), 150.0, 1e-4);
    TEST_CHECK_NEAR(mathf_fit01(0.25f, 10.0f, 20.0f), 12.5, 1e-5);
    TEST_CHECK_NEAR(mathf_lerp(10.0f, 20.0f, 0.5f), 15.0, 1e-6);
    TEST_CHECK(mathf_clamp(5.0f, 0.0f, 1.0f) == 1.0f);
    TEST_CHECK(mathf_clamp(-5.0f, 0.0f, 1.0f) == 0.0f);
    TEST_CHECK(mathf_clamp(0.5f, 0.0f, 1.0f) == 0.5f);
    TEST_CHECK(mathf_clampz(-1.0f, 2.0f) == 0.0f);
    TEST_CHECK(mathf_clampz(3.0f, 2.0f) == 2.0f);
    TEST_CHECK_NEAR(mathf_deg2rad(180.0f), PI, 1e-6);
    TEST_CHECK_NEAR(mathf_rad2deg(PI_OVER_TWO), 90.0, 1e-4);
    TEST_CHECK_NEAR(mathf_logN(8.0f, 2.0f), 3.0, 1e-6);
    TEST_CHECK_NEAR(mathf_frac(-1.25f), 0.75, 1e-6);
    TEST_CHECK(mathf_rcp_safe(4.0f) == 0.25f);
}

static bool property_matches_libm(FuzzSource* source, void* user_data)
{
    const float x = (float)fuzz_range_i64(source, -100000, 100000) / 1000.0f;
    const float y = (float)fuzz_range_i64(source, -100000, 100000) / 1000.0f;
    const float z = (float)fuzz_range_i64(source, -100000, 100000) / 1000.0f;
    const float positive = fabsf(x) + 1e-3f;
    const float unit = (float)fuzz_f64_unit(source) * 2.0f - 1.0f;

    ROMANO_UNUSED(user_data);

    TEST_FUZZ_CHECK_MSG(near_rel(mathf_rcp(positive), 1.0 / positive, 1e-6), "rcp(%g) = %g", positive, mathf_rcp(positive));
    TEST_FUZZ_CHECK_MSG(near_rel(mathf_rsqrt(positive), 1.0 / sqrt(positive), 1e-5), "rsqrt(%g) = %g", positive, mathf_rsqrt(positive));
    TEST_FUZZ_CHECK(mathf_abs(x) == fabsf(x));
    TEST_FUZZ_CHECK(near_rel(mathf_sqrt(positive), sqrt(positive), 1e-6));
    TEST_FUZZ_CHECK(mathf_min(x, y) == (x < y ? x : y));
    TEST_FUZZ_CHECK(mathf_max(x, y) == (x > y ? x : y));
    TEST_FUZZ_CHECK(near_rel(mathf_exp(x / 10.0f), exp(x / 10.0f), 1e-5));
    TEST_FUZZ_CHECK(near_rel(mathf_log(positive), log(positive), 1e-5));
    TEST_FUZZ_CHECK(near_rel(mathf_log2(positive), log2(positive), 1e-5));
    TEST_FUZZ_CHECK(near_rel(mathf_log10(positive), log10(positive), 1e-5));
    TEST_FUZZ_CHECK(near_rel(mathf_pow(positive, y / 50.0f), pow(positive, y / 50.0f), 1e-4));
    TEST_FUZZ_CHECK(mathf_floor(x) == floorf(x) && mathf_ceil(x) == ceilf(x));
    TEST_FUZZ_CHECK(near_rel(mathf_frac(x), x - floor(x), 1e-6));
    TEST_FUZZ_CHECK(y == 0.0f || near_rel(mathf_fmod(x, y), fmod(x, y), 1e-5));
    TEST_FUZZ_CHECK(near_rel(mathf_cos(x), cos(x), 1e-5) && near_rel(mathf_sin(x), sin(x), 1e-5));
    TEST_FUZZ_CHECK(fabs(cos(x)) < 1e-3 || near_rel(mathf_tan(x), tan(x), 1e-3));
    TEST_FUZZ_CHECK(near_rel(mathf_acos(unit), acos(unit), 1e-5) && near_rel(mathf_asin(unit), asin(unit), 1e-5));
    TEST_FUZZ_CHECK(near_rel(mathf_atan(x), atan(x), 1e-5) && near_rel(mathf_atan2(y, x), atan2(y, x), 1e-5));
    TEST_FUZZ_CHECK(near_rel(mathf_cosh(x / 10.0f), cosh(x / 10.0f), 1e-5));
    TEST_FUZZ_CHECK(near_rel(mathf_sinh(x / 10.0f), sinh(x / 10.0f), 1e-5));
    TEST_FUZZ_CHECK(near_rel(mathf_tanh(x), tanh(x), 1e-5));
    TEST_FUZZ_CHECK_MSG(near_rel(mathf_madd(x, y, z), (double)x * y + z, 1e-5), "madd(%g, %g, %g)", x, y, z);
    TEST_FUZZ_CHECK(near_rel(mathf_msub(x, y, z), (double)x * y - z, 1e-5));
    TEST_FUZZ_CHECK(near_rel(mathf_nmadd(x, y, z), -(double)x * y + z, 1e-5));
    TEST_FUZZ_CHECK(near_rel(mathf_nmsub(x, y, z), -(double)x * y - z, 1e-5));

    return true;
}

static void test_fuzz_matches_libm(void)
{
    test_fuzz_property("common32_vs_libm", 20000, property_matches_libm, NULL);
}

TEST_MAIN(
    TEST(test_constants),
    TEST(test_classification),
    TEST(test_helpers),
    TEST(test_fuzz_matches_libm),
)
