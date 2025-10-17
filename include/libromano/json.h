/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_JSON)
#define __LIBROMANO_JSON

#include "libromano/common.h"
#include "libromano/arena.h"

ROMANO_CPP_ENTER

#define JSON_INVALID_U64 ((uint64_t)(0xFFFFFFFFFFFFFFFF))
#define JSON_INVALID_I64 ((int64_t)(0x7FFFFFFFFFFFFFFF))
#define JSON_INVALID_F64 ((double)(0xFFFFFFFF7FF7FFFF))

typedef union JsonValueUnion {
    bool b;
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
    const char* key;
    JsonValue* value;
} JsonKeyValue;

typedef struct JsonArrayElement {
    JsonValue* value;
    struct JsonArrayElement* next;
} JsonArrayElement;

typedef struct JsonArrayIterator {
    JsonArrayElement* current;
} JsonArrayIterator;

typedef struct JsonDictElement {
    JsonKeyValue* key_value;
    struct JsonDictElement* next;
} JsonDictElement;

typedef struct JsonDictIterator {
    JsonDictElement* current;
} JsonDictIterator;

typedef struct Json {
    JsonValue* root;
    Arena string_arena;
    Arena value_arena;
} Json;

/*******************/
/* JsonValue funcs */
/*******************/

/* NULL */

/*
 */
ROMANO_API bool json_is_null(JsonValue* value);

/*
 */
ROMANO_API JsonValue* json_null_new(Json* json);

/*
 */
ROMANO_API void json_null_set(Json* json, JsonValue* value);

/* Bool */

/*
 */
ROMANO_API bool json_is_bool(JsonValue* value);

/*
 */
ROMANO_API JsonValue* json_bool_new(Json* json, bool b);

/*
 */
ROMANO_API bool json_bool_get(JsonValue* value);

/*
 */
ROMANO_API void json_bool_set(Json* json, JsonValue* value, bool b);

/* U64 */

/*
 */
ROMANO_API bool json_is_u64(JsonValue* value);

/*
 */
ROMANO_API JsonValue* json_u64_new(Json* json, uint64_t u64);

/*
 * Returns the uint64_t value.
 * If the JsonValue is not an uint64_t, returns JSON_INVALID_U64 (U64_MAX)
 */
ROMANO_API uint64_t json_u64_get(JsonValue* value);

/*
 */
ROMANO_API void json_u64_set(Json* json, JsonValue* value, uint64_t u64);

/* I64 */

/*
 */
ROMANO_API bool json_is_i64(JsonValue* value);

/*
 */
ROMANO_API JsonValue* json_i64_new(Json* json, int64_t i64);

/*
 * Returns the int64_t value.
 * If the JsonValue is not an int64_t, returns JSON_INVALID_I64 (I64_MAX)
 */
ROMANO_API int64_t json_i64_get(JsonValue* value);

/*
 */
ROMANO_API void json_i64_set(Json* json, JsonValue* value, int64_t i64);

/* F64 */

/*
 */
ROMANO_API bool json_is_f64(JsonValue* value);

/*
 */
ROMANO_API JsonValue* json_f64_new(Json* json, double f64);

/*
 * Returns the double value.
 * If the JsonValue is not a double, returns JSON_INVALID_F64 (nan)
 */
ROMANO_API double json_f64_get(JsonValue* value);

/*
 */
ROMANO_API void json_f64_set(Json* json, JsonValue* value, double f64);

/* Str */

/*
 */
ROMANO_API bool json_is_str(JsonValue* value);

/*
 */
ROMANO_API JsonValue* json_str_new(Json* json, const char* str);

/*
 * Returns the str value. If the JsonValue is not an str, returns NULL
 */
ROMANO_API const char* json_str_get(JsonValue* value);

/*
 */
ROMANO_API size_t json_str_get_size(JsonValue* value);

/*
 */
ROMANO_API void json_str_set(Json* json, JsonValue* value, const char* str);

/* Array */

/*
 */
ROMANO_API bool json_is_array(JsonValue* value);

/*
 */
ROMANO_API JsonValue* json_array_new(Json* json);

/*
 */
ROMANO_API void json_array_append(Json* json, JsonValue* array, JsonValue* value, bool reference);

/*
 */
ROMANO_API void json_array_pop(Json* json, JsonValue* array, size_t index);

/*
 * Returns the next element of the array. If there is no element left, returns NULL
 */
ROMANO_API JsonValue* json_array_get_next(Json* json, JsonValue* array, JsonArrayIterator* iter);

/*
 * Returns the size of the array. If the JsonValue is not an array, returns 0
 */
ROMANO_API size_t json_array_get_size(JsonValue* value);

/* Dict */

/*
 */
ROMANO_API bool json_is_dict(JsonValue* value);

/*
 */
ROMANO_API JsonValue* json_dict_new(Json* json, JsonValue* value);

/*
 */
ROMANO_API void json_dict_append(Json* json, JsonValue* dict, const char* key, JsonValue* value, bool reference);

/*
 * Returns a ptr to the associated value. If not found, returns NULL
 */
ROMANO_API JsonValue* json_dict_find(Json* json, JsonValue* dict, const char* key);

/*
 */
ROMANO_API void json_dict_pop(Json* json, JsonValue* dict, const char* key);

/*
 */
ROMANO_API JsonKeyValue* json_dict_get_next(Json* json, JsonValue* dict, JsonDictIterator* iterator);

/*
 * Returns the size of the dict. If the JsonValue is not a dict, returns 0
 */
ROMANO_API size_t json_dict_get_size(JsonValue* value);

/**************/
/* Json funcs */
/**************/

/*
 * Creates a new Json document. Returns NULL on error, otherwise returns a heap-allocated Json
 * document
 */
ROMANO_API Json* json_new();

/*
 */
ROMANO_API void json_set_root(Json* json, JsonValue* root);

/*
 * Reads a Json document from a string. Returns NULL on error, otherwise returns a heap-allocated
 * Json document
 */
ROMANO_API Json* json_loads(const char* str, size_t len);

/*
 * Reads a Json document from a file. Returns NULL on error, otherwise returns a heap-allocated
 * Json document
 */
ROMANO_API Json* json_loadf(const char* file_path);

/*
 */
ROMANO_API char* json_dumps(Json* json, size_t indent_size, size_t* dumps_size);

/*
 */
ROMANO_API bool json_dumpf(Json* json, size_t indent_size, const char* file_path);

/*
 * Releases a Json document
 */
ROMANO_API void json_free(Json* json);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_JSON) */
