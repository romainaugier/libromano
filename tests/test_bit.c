/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/bit.h"

static void test_macros(void)
{
    uint32_t x32 = 0;
    uint64_t x64 = 0;

    TEST_CHECK_EQ_UINT(BIT(5), 32);
    TEST_CHECK_EQ_UINT(BIT32(31), 0x80000000u);
    TEST_CHECK_EQ_UINT(BIT64(63), 0x8000000000000000ULL);

    SET_BIT32(x32, 3);
    TEST_CHECK(HAS_BIT32(x32, 3));
    TOGGLE_BIT32(x32, 4);
    TEST_CHECK_EQ_UINT(x32, 0x18);
    UNSET_BIT32(x32, 3);
    TEST_CHECK_EQ_UINT(x32, 0x10);

    SET_BIT64(x64, 40);
    TEST_CHECK(HAS_BIT64(x64, 40));
    TOGGLE_BIT64(x64, 41);
    TEST_CHECK_EQ_UINT(x64, BIT64(40) | BIT64(41));
    UNSET_BIT64(x64, 40);
    TEST_CHECK_EQ_UINT(x64, BIT64(41));
}

static void test_next_pow2(void)
{
    TEST_CHECK_EQ_UINT(round_u32_to_next_pow2(1), 1);
    TEST_CHECK_EQ_UINT(round_u32_to_next_pow2(7), 8);
    TEST_CHECK_EQ_UINT(round_u32_to_next_pow2(8), 8);
    TEST_CHECK_EQ_UINT(round_u32_to_next_pow2(1234), 2048);
    TEST_CHECK_EQ_UINT(round_u32_to_next_pow2(42000), 65536);
    TEST_CHECK_EQ_UINT(round_u64_to_next_pow2(1), 1);
    TEST_CHECK_EQ_UINT(round_u64_to_next_pow2(30000), 32768);
    TEST_CHECK_EQ_UINT(round_u64_to_next_pow2((1ULL << 40) + 1), 1ULL << 41);
}

static uint64_t reference_popcount(uint64_t x)
{
    uint64_t count = 0;

    for(; x != 0; x >>= 1)
        count += x & 1;

    return count;
}

static uint64_t reference_pext(uint64_t x, uint64_t mask)
{
    uint64_t result = 0;
    uint32_t out_bit = 0;
    uint32_t bit;

    for(bit = 0; bit < 64; bit++)
    {
        if(mask & BIT64(bit))
        {
            if(x & BIT64(bit))
                result |= BIT64(out_bit);

            out_bit++;
        }
    }

    return result;
}

static bool property_bit_ops(FuzzSource* source, void* user_data)
{
    uint64_t x = fuzz_u64_special(source);
    uint64_t y = fuzz_u64_special(source);
    uint32_t x32 = (uint32_t)x;
    int64_t s64 = fuzz_i64_special(source);
    int32_t s32 = fuzz_i32_special(source);
    uint32_t p = (uint32_t)fuzz_range(source, 1, 1u << 31);
    uint64_t p64 = fuzz_range(source, 1, 1ULL << 63);
    uint32_t bit;

    ROMANO_UNUSED(user_data);

    TEST_FUZZ_CHECK(popcount_u64(x) == reference_popcount(x));
    TEST_FUZZ_CHECK(popcount_u32(x32) == reference_popcount(x32));
    TEST_FUZZ_CHECK(pext_u64(x, y) == reference_pext(x, y));
    TEST_FUZZ_CHECK(pext_u32(x32, (uint32_t)y) == reference_pext(x32, (uint32_t)y));

    if(x != 0)
    {
        for(bit = 63; !(x & BIT64(bit)); bit--);

        TEST_FUZZ_CHECK(clz_u64(x) == 63 - bit);

        for(bit = 0; !(x & BIT64(bit)); bit++);

        TEST_FUZZ_CHECK(ctz_u64(x) == bit);
        TEST_FUZZ_CHECK(lsb_u64(x) == BIT64(bit));
        TEST_FUZZ_CHECK(clsb_u64(x) == (x & ~BIT64(bit)));
    }

    TEST_FUZZ_CHECK(abs_i64(s64) == (s64 < 0 ? 0 - (uint64_t)s64 : (uint64_t)s64));
    TEST_FUZZ_CHECK(abs_i32(s32) == (s32 < 0 ? 0 - (uint32_t)s32 : (uint32_t)s32));
    TEST_FUZZ_CHECK(abs_i16((int16_t)s32) == (uint16_t)((int16_t)s32 < 0 ? -(int32_t)(int16_t)s32 : (int16_t)s32));
    TEST_FUZZ_CHECK(abs_u8((int8_t)s32) == (uint8_t)((int8_t)s32 < 0 ? -(int32_t)(int8_t)s32 : (int8_t)s32));

    {
        uint32_t r = round_u32_to_next_pow2(p);
        uint64_t r64 = round_u64_to_next_pow2(p64);

        TEST_FUZZ_CHECK_MSG(popcount_u32(r) == 1 && r >= p && r / 2 < p, "p = %u, r = %u", p, r);
        TEST_FUZZ_CHECK(popcount_u64(r64) == 1 && r64 >= p64 && r64 / 2 < p64);
    }

    return true;
}

static void test_fuzz_bit_ops(void)
{
    test_fuzz_property("bit_ops", 20000, property_bit_ops, NULL);
}

TEST_MAIN(
    TEST(test_macros),
    TEST(test_next_pow2),
    TEST(test_fuzz_bit_ops),
)
