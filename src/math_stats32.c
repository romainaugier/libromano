#include "libromano/math/stats32.h"
#include "libromano/math/common32.h"
#include "libromano/simd.h"

/* Sum */

float stats_sum(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    float sum;
    float c;
    float y;
    float z;
    float t;

    sum = 0.0f;
    c = 0.0f;

    ROMANO_NO_VECTORIZATION
    for(i = 0; i < n; i++)
    {
        y = array[i] - c;
        t = sum + y;
        z = t - sum;
        c = z - y;
        sum = t;
    }

    return sum;
}

/* Mean */

float __stats_mean_scalar(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    float mean;

    mean = 0.0f;

    ROMANO_NO_VECTORIZATION
    for(i = 0; i < n; i++)
    {
        mean = mathf_lerp(mean, array[i], 1.0f / (float)(i + 1));
    }

    return mean;
}

float __stats_mean_sse(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    size_t sse_loop;
    float mean;
    __m128 m;
    __m128 v;
    __m128 t;

    sse_loop = n % 4;
    mean = 0.0f;

    m = _mm_setzero_ps();

    for(i = 0; i < sse_loop; i += 4)
    {
        v = _mm_loadu_ps(&array[i]);
        t = _mm_set1_ps(1.0f / (float)(i + 1));
        m = _mm_lerp_ps(m, v, t);
    }

    mean = _mm_hmean_ps(m);

    for(; i < n; i++)
    {
        mean = mathf_lerp(mean, array[i], 1.0f / (float)(i + 1));
    }

    return mean;
}

float __stats_mean_avx(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    size_t avx_loop;
    float mean;
    __m256 m;
    __m256 v;
    __m256 t;

    avx_loop = n % 8;
    mean = 0.0f;

    m = _mm256_setzero_ps();

    for(i = 0; i < avx_loop; i += 8)
    {
        v = _mm256_loadu_ps(&array[i]);
        t = _mm256_set1_ps(1.0f / (float)(i + 1));
        m = _mm256_lerp_ps(m, v, t);
    }

    mean = _mm256_hmean_ps(m);

    for(; i < n; i++)
    {
        mean = mathf_lerp(mean, array[i], 1.0f / (float)(i + 1));
    }

    return mean;
}

typedef float (*stats_mean_func)(const float* ROMANO_RESTRICT, size_t);

stats_mean_func __stats_mean_funcs[5] = {
    __stats_mean_scalar,
    __stats_mean_sse,
    __stats_mean_avx,
    __stats_mean_avx,
    __stats_mean_avx,
};

float stats_mean(const float* ROMANO_RESTRICT array, size_t n)
{
    return __stats_mean_funcs[simd_get_vectorization_mode()](array, n);
}

/* STD */

float stats_std(const float* ROMANO_RESTRICT array, size_t n)
{
    return mathf_sqrt(stats_variance(array, n));
}

/* Variance */

float stats_variance(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    float _n;
    float m;
    float sum;
    float variance;

    variance = 0.0f;

    _n = 1.0f;
    m = array[0];
    sum = 0.0f;

    ROMANO_NO_VECTORIZATION
    for(i = 1; i < n; i++)
    {
        _n += 1.0f;
        m += array[i];
        sum += (1.0f / (_n * (_n - 1.0f))) * mathf_sqr(_n * array[i] - m);
    }

    return sum / (float)n;
}

/* Min */

float __stats_min_scalar(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    float min;

    min = array[0];

    ROMANO_NO_VECTORIZATION
    for(i = 1; i < n; i++)
    {
        min = mathf_min(min, array[i]);
    }

    return min;
}

ROMANO_FORCE_INLINE float __stats_hmin_sse(__m128 v)
{
    v = _mm_min_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 0)));
    v = _mm_min_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 0, 0)));
    return _mm_cvtss_f32(v);
}

float __stats_min_sse(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    size_t sse_loop;
    __m128 v;
    float min;

    min = array[0];
    sse_loop = n % 4;

    for(i = 0; i < sse_loop; i += 4)
    {
        v = _mm_loadu_ps(&array[i]);
        min = mathf_min(min, __stats_hmin_sse(v));
    }

    for(; i < n; i++)
    {
        min = mathf_min(min, array[i]);
    }

    return min;
}

ROMANO_FORCE_INLINE float __stats_hmin_avx(__m256 v)
{
    __m256 perm_halves = _mm256_permute2f128_ps(v, v, 1);
    __m256 m0 = _mm256_max_ps(perm_halves, v);

    __m256 perm0 = _mm256_permute_ps(m0, 0b01001110);
    __m256 m1 = _mm256_max_ps(m0, perm0);

    __m256 perm1 = _mm256_permute_ps(m1, 0b10110001);
    __m256 m2 = _mm256_max_ps(perm1, m1);

    return _mm256_cvtss_f32(m2);
}

float __stats_min_avx(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    size_t avx_loop;
    __m256 v;
    float min;

    min = array[0];
    avx_loop = n % 8;

    for(i = 0; i < avx_loop; i += 8)
    {
        v = _mm256_loadu_ps(&array[i]);
        min = mathf_min(min, __stats_hmin_avx(v));
    }

    for(; i < n; i++)
    {
        min = mathf_min(min, array[i]);
    }

    return min;
}

float __stats_min_avx5(const float* ROMANO_RESTRICT array, size_t n)
{
    /* TODO: implement avx512 version using _mm512_reduce_min_ps */
    return __stats_min_avx(array, n);
}

typedef float (*min_func)(const float* ROMANO_RESTRICT,size_t);

min_func __stats_min_funcs[5] = {
    __stats_min_scalar,
    __stats_min_sse,
    __stats_min_avx,
    __stats_min_avx,
    __stats_min_avx5,
};

float stats_min(const float* ROMANO_RESTRICT array, size_t n)
{
    return __stats_min_funcs[simd_get_vectorization_mode()](array, n);
}

/* Max */

float __stats_max_scalar(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    float max;

    max = array[0];

    ROMANO_NO_VECTORIZATION
    for(i = 1; i < n; i++)
    {
        max = mathf_max(max, array[i]);
    }

    return max;
}

ROMANO_FORCE_INLINE float __stats_hmax_sse(__m128 v)
{
    v = _mm_max_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 0)));
    v = _mm_max_ps(v, _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 0, 0)));
    return _mm_cvtss_f32(v);
}

float __stats_max_sse(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    size_t sse_loop;
    __m128 v;
    float max;

    max = array[0];
    sse_loop = n % 4;

    for(i = 0; i < sse_loop; i += 4)
    {
        v = _mm_loadu_ps(&array[i]);
        max = mathf_max(max, __stats_hmax_sse(v));
    }

    for(; i < n; i++)
    {
        max = mathf_max(max, array[i]);
    }

    return max;
}

ROMANO_FORCE_INLINE float __stats_hmax_avx(__m256 v)
{
    __m256 perm_halves = _mm256_permute2f128_ps(v, v, 1);
    __m256 m0 = _mm256_max_ps(perm_halves, v);

    __m256 perm0 = _mm256_permute_ps(m0, 0b01001110);
    __m256 m1 = _mm256_max_ps(m0, perm0);

    __m256 perm1 = _mm256_permute_ps(m1, 0b10110001);
    __m256 m2 = _mm256_max_ps(perm1, m1);

    return _mm256_cvtss_f32(m2);
}

float __stats_max_avx(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    size_t avx_loop;
    __m256 v;
    float max;

    max = array[0];
    avx_loop = n % 8;

    for(i = 0; i < avx_loop; i += 8)
    {
        v = _mm256_loadu_ps(&array[i]);
        max = mathf_max(max, __stats_hmax_avx(v));
    }

    for(; i < n; i++)
    {
        max = mathf_max(max, array[i]);
    }

    return max;
}

float __stats_max_avx5(const float* ROMANO_RESTRICT array, size_t n)
{
    /* TODO: implement avx512 version using _mm512_reduce_max_ps */
    return __stats_max_avx(array, n);
}

typedef float (*max_func)(const float* ROMANO_RESTRICT,size_t);

max_func __stats_max_funcs[5] = {
    __stats_max_scalar,
    __stats_max_sse,
    __stats_max_avx,
    __stats_max_avx,
    __stats_max_avx5,
};

float stats_max(const float* ROMANO_RESTRICT array, size_t n)
{
    return __stats_max_funcs[simd_get_vectorization_mode()](array, n);
}

/* Range */

float __stats_range_scalar(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    float min;
    float max;

    min = array[0];
    max = array[0];

    ROMANO_NO_VECTORIZATION
    for(i = 1; i < n; i++)
    {
        min = mathf_min(min, array[i]); 
        max = mathf_max(max, array[i]); 
    }

    return mathf_abs(max - min);
}

float __stats_range_sse(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    size_t sse_loop;
    __m128 v;
    float min;
    float max;

    min = array[0];
    max = array[0];
    sse_loop = n % 4;

    for(i = 0; i < sse_loop; i += 4)
    {
        v = _mm_loadu_ps(&array[i]);
        min = mathf_min(min, __stats_hmin_sse(v));
        max = mathf_max(max, __stats_hmax_sse(v));
    }

    for(; i < n; i++)
    {
        min = mathf_min(min, array[i]);
        max = mathf_max(max, array[i]);
    }

    return mathf_abs(max - min);
}

float __stats_range_avx(const float* ROMANO_RESTRICT array, size_t n)
{
    size_t i;
    size_t avx_loop;
    __m256 v;
    float min;
    float max;

    min = array[0];
    max = array[0];
    avx_loop = n % 8;

    for(i = 0; i < avx_loop; i += 8)
    {
        v = _mm256_loadu_ps(&array[i]);
        min = mathf_min(min, __stats_hmin_avx(v));
        max = mathf_max(max, __stats_hmax_avx(v));
    }

    for(; i < n; i++)
    {
        min = mathf_min(min, array[i]);
        max = mathf_max(max, array[i]);
    }

    return mathf_abs(max - min);
}

float __stats_range_avx5(const float* ROMANO_RESTRICT array, size_t n)
{
    /* TODO: implement avx512 version using _mm512_reduce_min/max_ps */
    return __stats_range_avx(array, n);
}

typedef float (*range_func)(const float* ROMANO_RESTRICT,size_t);

range_func __stats_range_funcs[5] = {
    __stats_range_scalar,
    __stats_range_sse,
    __stats_range_avx,
    __stats_range_avx,
    __stats_range_avx5,
};

float stats_range(const float* ROMANO_RESTRICT array, size_t n)
{
    return __stats_range_funcs[simd_get_vectorization_mode()](array, n);
}