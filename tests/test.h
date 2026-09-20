/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_TEST)
#define __LIBROMANO_TEST

#include "libromano/common.h"
#include "libromano/fuzz.h"
#include "libromano/logger.h"

#include <math.h>
#include <string.h>

/*
 * Usage: test_xxx [-v] [--list] [filter...]
 * Filters select the test cases whose name contains one of them.
 */

typedef void (*TestFunc)(void);

typedef struct TestCase {
    const char* name;
    TestFunc func;
} TestCase;

#define TEST(func) { #func, func }

#define TEST_MAIN(...)                                                                     \
    int main(int argc, char** argv)                                                        \
    {                                                                                      \
        static const TestCase test_cases[] = { __VA_ARGS__ };                              \
        return test_main(argc, argv, test_cases, sizeof(test_cases) / sizeof(TestCase));   \
    }

int test_main(int argc, char** argv, const TestCase* cases, size_t count);

bool test_check(bool ok, const char* file, int line, const char* expr);

bool test_checkf(bool ok, const char* file, int line, const char* expr, const char* format, ...);

bool test_check_int(int64_t a, int64_t b, const char* file, int line, const char* expr);

bool test_check_uint(uint64_t a, uint64_t b, const char* file, int line, const char* expr);

bool test_check_near(double a, double b, double epsilon, const char* file, int line, const char* expr);

bool test_check_str(const char* a, const char* b, const char* file, int line, const char* expr);

bool test_check_mem(const void* a, const void* b, size_t size, const char* file, int line, const char* expr);

/* Aborts the current test case */
void test_abort(void);

/* Non-fatal checks, return whether the check passed */
#define TEST_CHECK(cond) test_check(!!(cond), __FILE__, __LINE__, #cond)
#define TEST_CHECK_MSG(cond, ...) test_checkf(!!(cond), __FILE__, __LINE__, #cond, __VA_ARGS__)
#define TEST_CHECK_EQ_INT(a, b) test_check_int((int64_t)(a), (int64_t)(b), __FILE__, __LINE__, #a " == " #b)
#define TEST_CHECK_EQ_UINT(a, b) test_check_uint((uint64_t)(a), (uint64_t)(b), __FILE__, __LINE__, #a " == " #b)
#define TEST_CHECK_NEAR(a, b, eps) test_check_near((double)(a), (double)(b), (double)(eps), __FILE__, __LINE__, #a " ~= " #b)
#define TEST_CHECK_EQ_STR(a, b) test_check_str((a), (b), __FILE__, __LINE__, #a " == " #b)
#define TEST_CHECK_EQ_MEM(a, b, size) test_check_mem((a), (b), (size), __FILE__, __LINE__, #a " == " #b)

/* Fatal checks, abort the current test case on failure */
#define TEST_ASSERT(cond) do { if(!TEST_CHECK(cond)) test_abort(); } while(0)
#define TEST_ASSERT_MSG(cond, ...) do { if(!TEST_CHECK_MSG(cond, __VA_ARGS__)) test_abort(); } while(0)
#define TEST_ASSERT_EQ_INT(a, b) do { if(!TEST_CHECK_EQ_INT(a, b)) test_abort(); } while(0)
#define TEST_ASSERT_EQ_UINT(a, b) do { if(!TEST_CHECK_EQ_UINT(a, b)) test_abort(); } while(0)

/*
 * Checks for fuzz properties and targets: log the failure and return false, so the fuzzer can
 * report the seed and minimize the input
 */
bool test_fuzz_failure(const char* file, int line, const char* expr, const char* format, ...);

#define TEST_FUZZ_CHECK(cond) do { if(!(cond)) return test_fuzz_failure(__FILE__, __LINE__, #cond, NULL); } while(0)
#define TEST_FUZZ_CHECK_MSG(cond, ...) do { if(!(cond)) return test_fuzz_failure(__FILE__, __LINE__, #cond, __VA_ARGS__); } while(0)

/* Run a fuzzer and record a check failure if it fails */
bool test_fuzz_property(const char* name, uint64_t iterations, FuzzPropertyFunc property, void* user_data);

bool test_fuzz_property_with(const FuzzOptions* options, FuzzPropertyFunc property, void* user_data);

bool test_fuzz_input(const FuzzOptions* options, FuzzInputFunc target, void* user_data);

/* Returns a path inside the tests temporary directory, valid for the next 7 calls */
const char* test_tmp_path(const char* name);

/* Monotonic clock in milliseconds */
uint64_t test_now_ms(void);

void test_setenv(const char* name, const char* value);

void test_unsetenv(const char* name);

/* Scales iteration counts down in debug builds */
uint64_t test_scaled(uint64_t count);

#endif /* !defined(__LIBROMANO_TEST) */
