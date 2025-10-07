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

typedef enum JsonError : uint64_t {
    JsonError_Ok,
    JsonError_InvalidSyntax,
} JsonError;

typedef enum JsonTag : uint64_t {
    /* Value type */
    JsonTag_U64 = BIT(0),
    JsonTag_I64 = BIT(1),
    JsonTag_F64 = BIT(2),
    JsonTag_Str = BIT(3),
    JsonTag_Array = BIT(4),
    JsonTag_Dict = BIT(5),

    /* Value */
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

typedef struct Json {
    JsonValue* root;
    Arena string_arena;
    Arena value_arena;
} Json;

ROMANO_API Json* json_new();

ROMANO_API Json* json_from_string(const char* str, size_t len);

ROMANO_API Json* json_from_file(const char* file_path);

ROMANO_API void json_free(Json* json);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_JSON) */