/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/json.h"
#include "libromano/arena.h"
#include "libromano/logger.h"
#include "libromano/error.h"

#if defined(ROMANO_LINUX)
#include <errno.h>
#endif

extern ErrorCode g_current_error;

/********************/
/* JsonValue funcs */
/********************/

/*
 * First half (1 - 32) of the tag is for flags
 * Second half (33 - 64) of the tag is for sized elements (str, array, dict)
 */

typedef enum JsonTag {
    /* Value type */
    JsonTag_Null = BIT(0),
    JsonTag_U64 = BIT(1),
    JsonTag_I64 = BIT(2),
    JsonTag_F64 = BIT(3),
    JsonTag_Str = BIT(4),
    JsonTag_Array = BIT(5),
    JsonTag_Dict = BIT(6),

    /* Utility tag */
    JsonTag_ArrayElem = BIT(7),
    JsonTag_DictElem = BIT(8),

} JsonTag;

#define JSON_TAGS_MASK ((1 << 9) - 1)

#define JSON_SZ_MASK (0xFFFFFFFF << 32)

#define json_set_tags(tags, tag) \
    tags &= ~JSON_TAGS_MASK;     \
    tags &= tag;

#define json_set_sz(tags, sz) \
    tags &= ~JSON_SZ_MASK;    \
    tags &= ((sz && 0xFFFFFFFF) << 32);

#define json_get_sz(tags) ((tags >> 32) & 0xFFFFFFFF)

#define json_set_tags(tags, tag) \
    tags &= ~JSON_TAGS_MASK; \
    tags &= tag;

/* NULL */

bool json_is_null(JsonValue* value)
{
    return value->tags & JsonTag_Null;
}

JsonValue* json_null_new(Json* json)
{
    JsonValue* value;

    value = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
    memset(value, 0, sizeof(JsonValue));

    json_set_tags(value->tags, JsonTag_Null);

    return value;
}

void json_null_set(Json* json, JsonValue* value)
{
    json_set_tags(value->tags, JsonTag_Null);
}

/* U64 */

bool json_is_u64(JsonValue* value)
{
    return value->tags & JsonTag_U64;
}

JsonValue* json_u64_new(Json* json, uint64_t u64)
{
    JsonValue* value;

    value = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
    memset(value, 0, sizeof(JsonValue));

    json_set_tags(value->tags, JsonTag_U64);
    value->value.u64 = u64;

    return value;
}

uint64_t json_u64_get(JsonValue* value)
{
    if((value->tags & JsonTag_U64) == 0)
        return JSON_INVALID_U64;

    return value->value.u64;
}

void json_u64_set(Json* json, JsonValue* value, uint64_t u64)
{
    json_set_tags(value->tags, JsonTag_U64);
    value->value.u64 = u64;
}

/* I64 */

bool json_is_i64(JsonValue* value)
{
    return value->tags & JsonTag_I64;
}

JsonValue* json_i64_new(Json* json, int64_t i64)
{
    JsonValue* value;

    value = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
    memset(value, 0, sizeof(JsonValue));

    json_set_tags(value->tags, JsonTag_I64);
    value->value.i64 = i64;

    return value;
}

int64_t json_i64_get(JsonValue* value)
{
    if((value->tags & JsonTag_I64) == 0)
        return JSON_INVALID_I64;

    return value->value.i64;
}


void json_i64_set(Json* json, JsonValue* value, int64_t i64)
{
    json_set_tags(value->tags, JsonTag_I64);
    value->value.i64 = i64;
}

/* F64 */

bool json_is_f64(JsonValue* value)
{
    return value->tags & JsonTag_F64;
}

JsonValue* json_f64_new(Json* json, double f64)
{
    JsonValue* value;

    value = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
    memset(value, 0, sizeof(JsonValue));

    json_set_tags(value->tags, JsonTag_F64);
    value->value.f64 = f64;

    return value;
}

double json_f64_get(JsonValue* value)
{
    if((value->tags & JsonTag_F64) == 0)
        return JSON_INVALID_F64;

    return value->value.f64;
}

void json_f64_set(Json* json, JsonValue* value, double f64)
{
    json_set_tags(value->tags, JsonTag_F64);
    value->value.f64 = f64;
}

/* Str */

bool json_is_str(JsonValue* value)
{
    return value->tags & JsonTag_Str;
}

JsonValue* json_str_new(Json* json, const char* str)
{
    JsonValue* value;
    size_t str_sz;
    char* str_ptr;

    value = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
    memset(value, 0, sizeof(JsonValue));

    str_sz = strlen(str);
    str_ptr = arena_push(&json->string_arena, NULL, (str_sz + 1) * sizeof(char));
    memcpy(str_ptr, str, str_sz * sizeof(char));
    str_ptr[str_sz] = '\0';

    json_set_tags(value->tags, JsonTag_Str);
    value->value.str = str_ptr;

    return value;
}

const char* json_str_get(JsonValue* value)
{
    if((value->tags & JsonTag_Str) == 0)
        return NULL;

    return value->value.str;
}

size_t json_str_get_size(JsonValue* value)
{
    if((value->tags & JsonTag_Str) == 0)
        return 0;

    return (size_t)json_get_sz(value->tags);
}

void json_str_set(Json* json, JsonValue* value, const char* str)
{
    size_t str_sz;
    char* str_ptr;

    json_set_tags(value->tags, JsonTag_Str);
    
    str_sz = strlen(str);
    str_ptr = arena_push(&json->string_arena, NULL, str_sz * sizeof(char));
    memcpy(str_ptr, str, str_sz * sizeof(char));
    str_ptr[str_sz] = '\0';

    value->value.str = str_ptr;
}

/* Array */

bool json_is_array(JsonValue* value)
{
    return value->tags & JsonTag_Array;
}

JsonValue* json_array_new(Json* json)
{
    JsonValue* array;

    array = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
    memset(array, 0, sizeof(JsonValue));

    json_set_tags(array->tags, JsonTag_Array);

    return array;
}

JsonValue* json_array_get(JsonValue* value)
{
    if((value->tags & JsonTag_Array) == 0)
        return NULL;

    return value;
}

void json_array_append(Json* json, JsonValue* array, JsonValue* value)
{
    JsonArrayIterator* iterator;
    JsonArrayElement* new_element;
    JsonValue* new_value;

    iterator->current = (JsonArrayElement*)(array->value.ptr);

    while(iterator->current->next != NULL)
        iterator->current = iterator->current->next;

    new_value = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
    memcpy(new_value, value, sizeof(JsonValue));

    new_element = arena_push(&json->value_arena, NULL, sizeof(JsonArrayElement));
    new_element->value = new_value;
    new_element->next = NULL;

    iterator->current->next = new_element;
}

void json_array_pop(Json* json, JsonValue* array, size_t index)
{
    JsonArrayIterator* iterator;
    JsonArrayElement* previous;
    size_t i;

    if(index >= json_array_get_size(array))
        return;

    if(index == 0)
    {
        array->value.ptr = (void*)((JsonArrayElement*)(array->value.ptr))->next;
        return;
    }

    iterator->current = (JsonArrayElement*)(array->value.ptr);
    previous = NULL;
    i = 0;

    while(iterator->current->next != NULL && i < index)
    {
        previous = iterator->current;
        iterator->current = iterator->current->next;
        i++;
    }

    previous->next = iterator->current->next;
}

JsonValue* json_array_get_next(Json* json, JsonValue* array, JsonArrayIterator* iterator)
{
    if(iterator->current == NULL)
        iterator->current = (JsonArrayElement*)array->value.ptr;
    else
        iterator->current = iterator->current->next;

    if(iterator->current == NULL)
        return NULL;

    return iterator->current->value;
}

size_t json_array_get_size(JsonValue* value)
{
    if((value->tags & JsonTag_Array) == 0)
        return 0;

    return (size_t)json_get_sz(value->tags);
}

/* Dict */

bool json_is_dict(JsonValue* value)
{
    return value->tags & JsonTag_Dict;
}

JsonValue* json_dict_new(Json* json, JsonValue* value)
{
    JsonValue* dict;

    dict = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
    memset(dict, 0, sizeof(JsonValue));

    json_set_tags(dict->tags, JsonTag_Dict);

    return dict;
}

JsonValue* json_dict_get(JsonValue* value)
{
    if((value->tags & JsonTag_Dict) == 0)
        return NULL;

    return value;
}

void json_dict_append(Json* json, JsonValue* dict, const char* key, JsonValue* value)
{
    /* TODO */
}

JsonValue* json_dict_find(Json* json, JsonValue* dict, const char* key)
{
    /* TODO */
    return NULL;
}

void json_dict_pop(Json* json, JsonValue* dict, const char* key)
{
    /* TODO */
}

JsonKeyValue* json_dict_get_next(Json* json, JsonValue* dict, JsonDictIterator* iterator)
{
    /* TODO */
    return NULL;
}

size_t json_dict_get_size(JsonValue* value)
{
    if((value->tags & JsonTag_Dict) == 0)
        return 0;

    return (size_t)json_get_sz(value->tags);
}

/**************/
/* Json funcs */
/**************/

Json* json_new()
{
    Json* json = malloc(sizeof(Json));

    if(json == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    json->root = NULL;
    arena_init(&json->string_arena, 128 * 1024);
    arena_init(&json->value_arena, 1024 * sizeof(JsonValue));

    return json;
}

void json_set_root(Json* json, JsonValue* root)
{
    if(json != NULL)
        json->root = root;
}

Json* json_loads(const char* str, size_t len)
{
    if(str[0] != '{' && str[0] != '[')
    {
        logger_log_error("Error while trying to read json string. Expected [ or { at position 0, got %c",
                         str[0]);
        g_current_error = ErrorCode_JsonUnexpectedCharacter;
        return NULL;
    }

    Json* root = json_new();

    /* Error code is already set by json_new */
    if(root == NULL)
        return NULL;

    return root;
}

Json* json_loadf(const char* file_path)
{
    FILE* file = fopen(file_path, "rb");

    if(file == NULL)
    {
        g_current_error = error_get_last_from_system();

        logger_log_error("Error while trying to read json file: %s (%d)",
                         file_path,
                         (int)g_current_error);

        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* file_buffer = calloc(file_size, sizeof(char));

    if(file_buffer == NULL)
    {
        logger_log_error("Error while trying to allocate memory to read json file: %s", file_path);
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    size_t file_read_size = fread(file_buffer, sizeof(char), file_size, file);

    if(file_read_size != file_size)
    {
        g_current_error = error_get_last_from_system();
        logger_log_error("Error while trying to read json file: %s (%d)", file_path, g_current_error);

        free(file_buffer);

        return NULL;
    }

    Json* json = json_loads(file_buffer, file_size);

    free(file_buffer);

    return json;
}

char* json_dumps(Json* json, size_t indent_size, size_t* dumps_size)
{
    return NULL;
}

bool json_dumpf(Json* json, size_t indent_size, const char* file_path)
{
    return false;
}

void json_free(Json* json)
{
    arena_release(&json->string_arena);
    arena_release(&json->value_arena);

    free(json);
}
