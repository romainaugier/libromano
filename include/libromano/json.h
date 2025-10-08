/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_JSON)
#define __LIBROMANO_JSON

#include "libromano/common.h"
#include "libromano/bit.h"
#include "libromano/arena.h"

ROMANO_CPP_ENTER

typedef enum JsonTag {
    /* Value type */
    JsonTag_U64 = BIT(0),
    JsonTag_I64 = BIT(1),
    JsonTag_F64 = BIT(2),
    JsonTag_Str = BIT(3),
    JsonTag_Array = BIT(4),
    JsonTag_Dict = BIT(5),

    /* Utility tag */
    JsonTag_ArrayEnd = BIT(6),
    JsonTag_DictEnd = BIT(7),
} JsonTag;

typedef union JsonValueUnion {
    uint64_t u64;
    int64_t i64;
    double f64;
    const char* str;
    void* ptr;
} JsonValueUnion;

typedef struct JsonObject {
    uint64_t tags;
    JsonValueUnion value;
} JsonValue;

typedef struct JsonKeyValue {
    JsonValue key;
    JsonValue value;
} JsonKeyValue;

typedef struct Json {
    JsonValue* root;
    Arena string_arena;
    Arena value_arena;
} Json;

/*
 * Creates a new Json document. Returns NULL on error
 */
ROMANO_API Json* json_new();

/*
 * Reads a Json document from a string. Returns NULL on error
 */
ROMANO_API Json* json_from_string(const char* str, size_t len);

/*
 * Reads a Json document from a file. Returns NULL on error
 */
ROMANO_API Json* json_from_file(const char* file_path);

/*
 * Releases a Json document
 */
ROMANO_API void json_free(Json* json);

/*
 * Returns the JsonValue corresponding to key. If key cannot be found, returns NULL
 */
ROMANO_API JsonValue* json_value_get(JsonValue* value, const char* key);

/*
 * Returns the uint64_t value. If the JsonValue is not an uint64_t, returns 0
 */
ROMANO_API uint64_t json_get_u64(JsonValue* value);

/*
 * Returns the int64_t value. If the JsonValue is not an int64_t, returns 0
 */
ROMANO_API int64_t json_get_i64(JsonValue* value);

/*
 * Returns the double value. If the JsonValue is not a double, returns 0.0
 */
ROMANO_API double json_get_f64(JsonValue* value);

/*
 * Returns the str value. If the JsonValue is not an str, returns NULL
 */
ROMANO_API const char* json_get_str(JsonValue* value);

/*
 * Returns the ptr to the array. If the JsonValue is not an array, returns NULL
 */
ROMANO_API JsonValue* json_get_array(JsonValue* value);

/*
 * Returns the size of the array. If the JsonValue is not an array, returns 0
 */
ROMANO_API size_t json_get_array_sz(JsonValue* value);

/*
 * Returns the ptr to the dict. If the JsonValue is not a dict, returns NULL
 */
ROMANO_API JsonKeyValue* json_get_dict(JsonValue* value);

/*
 * Returns the size of the dict. If the JsonValue is not a dict, returns 0
 */
ROMANO_API size_t json_get_dict_sz(JsonValue* value);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_JSON) */