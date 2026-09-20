/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/simd.h"

#if defined(ROMANO_X86_64) || defined(ROMANO_AARCH64)

typedef struct Lanes {
    float values[8];
    float t[8];
    float sum4;
    float sum8;
    float min4;
    float max4;
    float min8;
    float max8;
} Lanes;

static void lanes_generate(FuzzSource* source, Lanes* lanes)
{
    size_t i;

    for(i = 0; i < 8; i++)
    {
        /* Multiples of 1/8 keep every sum exact whatever the reduction order */
        lanes->values[i] = (float)fuzz_range_i64(source, -1000, 1000) / 8.0f;
        lanes->t[i] = (float)fuzz_f64_unit(source);
    }

    lanes->sum4 = lanes->sum8 = 0.0f;
    lanes->min4 = lanes->max4 = lanes->min8 = lanes->max8 = lanes->values[0];

    for(i = 0; i < 8; i++)
    {
        const float v = lanes->values[i];

        if(i < 4)
        {
            lanes->sum4 += v;
            lanes->min4 = v < lanes->min4 ? v : lanes->min4;
            lanes->max4 = v > lanes->max4 ? v : lanes->max4;
        }

        lanes->sum8 += v;
        lanes->min8 = v < lanes->min8 ? v : lanes->min8;
        lanes->max8 = v > lanes->max8 ? v : lanes->max8;
    }
}

static bool lerp_matches(const float* result, const Lanes* lanes, size_t count)
{
    size_t i;

    for(i = 0; i < count; i++)
    {
        const float expected = (1.0f - lanes->t[i]) * lanes->values[i] + lanes->t[i];

        if(fabsf(result[i] - expected) > 1e-3f)
            return false;
    }

    return true;
}

static void test_modes(void)
{
    const VectorizationMode initial = simd_get_vectorization_mode();
    int mode;

    TEST_CHECK_EQ_STR(simd_get_vectorization_mode_as_string(VectorizationMode_Scalar), "Scalar");
    TEST_CHECK_EQ_STR(simd_get_vectorization_mode_as_string((VectorizationMode)42), "Unknown");

    for(mode = 0; mode < VectorizationMode_COUNT; mode++)
        TEST_CHECK(strcmp(simd_get_vectorization_mode_as_string((VectorizationMode)mode), "Unknown") != 0);

    simd_force_vectorization_mode(VectorizationMode_Scalar);
    TEST_CHECK_EQ_INT(simd_get_vectorization_mode(), VectorizationMode_Scalar);

    simd_force_vectorization_mode((VectorizationMode)(VectorizationMode_COUNT + 1));
    TEST_CHECK_EQ_INT(simd_get_vectorization_mode(), VectorizationMode_Scalar);

    simd_force_vectorization_mode(initial);
    TEST_CHECK_EQ_INT(simd_get_vectorization_mode(), initial);

    logger_log_info("vectorization mode: %s", simd_get_vectorization_mode_as_string(initial));
}

static void test_env_override(void)
{
    const char* env = getenv("LIBROMANO_VECTORIZATION");
    const VectorizationMode mode = simd_get_vectorization_mode();

    if(env == NULL)
    {
        logger_log_info("LIBROMANO_VECTORIZATION is not set, nothing to check");
        return;
    }

    if(strlen(env) != 1 || env[0] < '0' || env[0] >= '0' + VectorizationMode_COUNT)
        TEST_CHECK_EQ_INT(mode, VectorizationMode_Scalar);
    else
        TEST_CHECK((int)mode <= env[0] - '0');

    simd_force_vectorization_mode(VectorizationMode_COUNT);
    TEST_CHECK_EQ_INT(simd_get_vectorization_mode(), mode);
}

#endif /* defined(ROMANO_X86_64) || defined(ROMANO_AARCH64) */

#if defined(ROMANO_X86_64)

static void test_features(void)
{
    const VectorizationMode mode = simd_get_vectorization_mode();

    TEST_CHECK(!simd_has_avx512() || simd_has_avx256());
    TEST_CHECK(!simd_has_avx256() || simd_has_avx());
    TEST_CHECK(!simd_has_avx() || simd_has_sse());

    TEST_CHECK(mode != VectorizationMode_AVX512 || simd_has_avx512());
    TEST_CHECK(mode != VectorizationMode_AVX256 || simd_has_avx256());
    TEST_CHECK(mode != VectorizationMode_AVX || simd_has_avx());
    TEST_CHECK(mode != VectorizationMode_SSE || simd_has_sse());

    if(getenv("LIBROMANO_VECTORIZATION") == NULL)
        TEST_CHECK(simd_has_sse());
}

static bool property_sse(FuzzSource* source, void* user_data)
{
    Lanes lanes;
    float lerp[4];
    __m128 x;

    ROMANO_UNUSED(user_data);

    lanes_generate(source, &lanes);
    x = _mm_loadu_ps(lanes.values);

    TEST_FUZZ_CHECK(_mm_hsum_ps(x) == lanes.sum4);
    TEST_FUZZ_CHECK(_mm_hmean_ps(x) == lanes.sum4 / 4.0f);
    TEST_FUZZ_CHECK_MSG(_mm_hmin_ps(x) == lanes.min4, "hmin(%g %g %g %g) = %g",
                        lanes.values[0], lanes.values[1], lanes.values[2], lanes.values[3], _mm_hmin_ps(x));
    TEST_FUZZ_CHECK_MSG(_mm_hmax_ps(x) == lanes.max4, "hmax(%g %g %g %g) = %g",
                        lanes.values[0], lanes.values[1], lanes.values[2], lanes.values[3], _mm_hmax_ps(x));

    _mm_storeu_ps(lerp, _mm_lerp_ps(x, _mm_set1_ps(1.0f), _mm_loadu_ps(lanes.t)));
    TEST_FUZZ_CHECK(lerp_matches(lerp, &lanes, 4));

    return true;
}

static bool property_avx(FuzzSource* source, void* user_data)
{
    Lanes lanes;
    float lerp[8];
    __m256 x;

    ROMANO_UNUSED(user_data);

    lanes_generate(source, &lanes);
    x = _mm256_loadu_ps(lanes.values);

    TEST_FUZZ_CHECK(_mm256_hsum_ps(x) == lanes.sum8);
    TEST_FUZZ_CHECK(_mm256_hmean_ps(x) == lanes.sum8 / 8.0f);
    TEST_FUZZ_CHECK(_mm256_hmin_ps(x) == lanes.min8);
    TEST_FUZZ_CHECK(_mm256_hmax_ps(x) == lanes.max8);

    _mm256_storeu_ps(lerp, _mm256_lerp_ps(x, _mm256_set1_ps(1.0f), _mm256_loadu_ps(lanes.t)));
    TEST_FUZZ_CHECK(lerp_matches(lerp, &lanes, 8));

    return true;
}

static void test_fuzz_helpers(void)
{
    test_fuzz_property("simd_sse_helpers", 5000, property_sse, NULL);

    if(!simd_has_avx())
    {
        logger_log_warning("AVX is not supported by this cpu, skipping the 256 bits helpers");
        return;
    }

    test_fuzz_property("simd_avx_helpers", 5000, property_avx, NULL);
}

#define PLATFORM_TESTS TEST(test_modes), TEST(test_env_override), TEST(test_features), TEST(test_fuzz_helpers),

#elif defined(ROMANO_AARCH64)

static void test_features(void)
{
    const VectorizationMode mode = simd_get_vectorization_mode();

    TEST_CHECK(mode != VectorizationMode_NEON || simd_has_neon());

    if(getenv("LIBROMANO_VECTORIZATION") == NULL)
        TEST_CHECK_EQ_INT(mode, simd_has_neon() ? VectorizationMode_NEON : VectorizationMode_Scalar);
}

static bool property_neon(FuzzSource* source, void* user_data)
{
    Lanes lanes;
    float lerp[4];
    float constants[4];
    float32x4_t x;

    ROMANO_UNUSED(user_data);

    lanes_generate(source, &lanes);
    x = vld1q_f32(lanes.values);

    TEST_FUZZ_CHECK(vhsumq_f32(x) == lanes.sum4);
    TEST_FUZZ_CHECK(vhmean_f32(x) == lanes.sum4 * 0.25f);

    vst1q_f32(lerp, vlerpq_f32(x, voneq_f32(), vld1q_f32(lanes.t)));
    TEST_FUZZ_CHECK(lerp_matches(lerp, &lanes, 4));

    vst1q_f32(constants, vaddq_f32(vzeroq_f32(), voneq_f32()));
    TEST_FUZZ_CHECK(constants[0] == 1.0f && constants[3] == 1.0f);

    return true;
}

static void test_fuzz_helpers(void)
{
    test_fuzz_property("simd_neon_helpers", 5000, property_neon, NULL);
}

#define PLATFORM_TESTS TEST(test_modes), TEST(test_env_override), TEST(test_features), TEST(test_fuzz_helpers),

#else

static void test_unsupported_platform(void)
{
    logger_log_warning("libromano has no simd support on this platform, nothing to test");
}

#define PLATFORM_TESTS TEST(test_unsupported_platform),

#endif /* defined(ROMANO_X86_64) */

TEST_MAIN(
    PLATFORM_TESTS
)
