/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/json.h"
#include "libromano/arena.h"
#include "libromano/common.h"
#include "libromano/logger.h"
#include "libromano/error.h"
#include "libromano/fmt.h"
#include "libromano/bit.h"

#include <ctype.h>
#include <math.h>
#include <string.h>

extern ErrorCode g_current_error;

/********************/
/* JsonValue funcs */
/********************/

/*
 * First half (1 - 32) of the tag is for flags
 * Second half (33 - 64) of the tag is for sized elements (str, array, dict)
 */

typedef enum JsonTag {
    JsonTag_Null = BIT(0),
    JsonTag_Bool = BIT(1),
    JsonTag_U64 = BIT(2),
    JsonTag_I64 = BIT(3),
    JsonTag_F64 = BIT(5),
    JsonTag_Str = BIT(6),
    JsonTag_Array = BIT(7),
    JsonTag_Dict = BIT(8),
} JsonTag;

#define JSON_TAGS_MASK ((1 << 9) - 1)

#define JSON_SZ_MASK (0xFFFFFFFFULL << 32)

#define json_set_tags(tags, tag) \
    tags &= ~JSON_TAGS_MASK;     \
    tags |= tag;

#define json_set_sz(tags, sz) \
    tags &= ~JSON_SZ_MASK;    \
    tags |= (((uint64_t)sz & 0xFFFFFFFFULL) << 32);

#define json_get_sz(tags) (((uint64_t)tags >> 32) & 0xFFFFFFFFULL)
#define json_incr_sz(tags) ((tags += (1ULL << 32)))
#define json_decr_sz(tags) ((tags -= (1ULL << 32)))

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
    ROMANO_UNUSED(json);

    json_set_tags(value->tags, JsonTag_Null);
}

/* Bool */

bool json_is_bool(JsonValue* value)
{
    return value->tags & JsonTag_Bool;
}

JsonValue* json_bool_new(Json* json, bool b)
{
    JsonValue* value;

    value = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
    memset(value, 0, sizeof(JsonValue));

    json_set_tags(value->tags, JsonTag_Bool);
    value->value.b = b;

    return value;
}

bool json_bool_get(JsonValue* value)
{
    if((value->tags & JsonTag_Bool) == 0)
        return false;

    return value->value.b;
}

void json_bool_set(Json* json, JsonValue* value, bool b)
{
    ROMANO_UNUSED(json);

    json_set_tags(value->tags, JsonTag_Bool);
    value->value.b = b;
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
    ROMANO_UNUSED(json);

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
    ROMANO_UNUSED(json);

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
    ROMANO_UNUSED(json);

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

    json_set_sz(value->tags, str_sz);

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
    str_ptr = arena_push(&json->string_arena, NULL, (str_sz + 1) * sizeof(char));
    memcpy(str_ptr, str, str_sz * sizeof(char));
    str_ptr[str_sz] = '\0';

    json_set_sz(value->tags, str_sz);
    value->value.str = str_ptr;
}

/* Array */

typedef struct JsonArrayInfo {
    JsonArrayElement* head;
    JsonArrayElement* tail;
} JsonArrayInfo;

bool json_is_array(JsonValue* value)
{
    return value->tags & JsonTag_Array;
}

JsonValue* json_array_new(Json* json)
{
    JsonValue* array;
    JsonArrayInfo* info;

    array = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
    memset(array, 0, sizeof(JsonValue));

    info = arena_push(&json->value_arena, NULL, sizeof(JsonArrayInfo));
    memset(info, 0, sizeof(JsonArrayInfo));

    json_set_tags(array->tags, JsonTag_Array);
    array->value.ptr = (void*)info;

    return array;
}

void json_array_append(Json* json, JsonValue* array, JsonValue* value, bool reference)
{
    JsonArrayElement* new_element;
    JsonArrayInfo* info;
    JsonValue* new_value;

    info = (JsonArrayInfo*)array->value.ptr;

    new_element = arena_push(&json->value_arena, NULL, sizeof(JsonArrayElement));
    new_element->next = NULL;

    if(info->head == NULL)
    {
        info->head = new_element;
        info->tail = new_element;
    }
    else
    {
        info->tail->next = new_element;
        info->tail = new_element;
    }

    if(!reference)
    {
        new_value = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
        memcpy(new_value, value, sizeof(JsonValue));
    }

    new_element->value = reference ? value : new_value;

    json_incr_sz(array->tags);
}

void json_array_pop(Json* json, JsonValue* array, size_t index)
{
    ROMANO_UNUSED(json);

    JsonArrayIterator iterator;
    JsonArrayInfo* info;
    JsonArrayElement* previous;
    size_t i;

    if(index >= json_array_get_size(array))
        return;

    info = (JsonArrayInfo*)array->value.ptr;

    if(index == 0)
    {
        info->head = info->head->next;

        if(info->head == NULL)
            info->tail = NULL;

        json_decr_sz(array->tags);
        return;
    }

    iterator.current = info->head;
    previous = NULL;
    i = 0;

    while(iterator.current->next != NULL && i < index)
    {
        previous = iterator.current;
        iterator.current = iterator.current->next;
        i++;
    }

    if(previous == NULL)
    {
        info->head = iterator.current;
    }
    else
    {
        previous->next = iterator.current->next;

        if(iterator.current == info->tail)
            info->tail = previous;
    }

    json_decr_sz(array->tags);
}

JsonValue* json_array_get_next(Json* json, JsonValue* array, JsonArrayIterator* iterator)
{
    if(iterator->current == NULL)
        iterator->current = ((JsonArrayInfo*)array->value.ptr)->head;
    else
        iterator->current = iterator->current->next;

    return iterator->current == NULL ? NULL : iterator->current->value;
}

size_t json_array_get_size(JsonValue* value)
{
    if((value->tags & JsonTag_Array) == 0)
        return 0;

    return (size_t)json_get_sz(value->tags);
}

/* Dict */

typedef struct JsonDictInfo {
    JsonDictElement* head;
    JsonDictElement* tail;
} JsonDictInfo;

bool json_is_dict(JsonValue* value)
{
    return value->tags & JsonTag_Dict;
}

JsonValue* json_dict_new(Json* json, JsonValue* value)
{
    JsonValue* dict;
    JsonDictInfo* info;

    ROMANO_UNUSED(value);

    dict = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
    memset(dict, 0, sizeof(JsonValue));

    info = arena_push(&json->value_arena, NULL, sizeof(JsonDictInfo));
    memset(info, 0, sizeof(JsonDictInfo));

    json_set_tags(dict->tags, JsonTag_Dict);
    dict->value.ptr = (void*)info;

    return dict;
}

void json_dict_append(Json* json, JsonValue* dict, const char* key, JsonValue* value, bool reference)
{
    JsonDictInfo* info;
    JsonDictElement* element;
    JsonKeyValue* new_key_value;
    JsonValue* new_value;
    size_t key_sz;
    char* new_key;

    info = (JsonDictInfo*)dict->value.ptr;

    element = (JsonDictElement*)arena_push(&json->value_arena, NULL, sizeof(JsonDictElement));
    element->next = NULL;

    if(info->head == NULL)
    {
        info->head = element;
        info->tail = element;
    }
    else
    {
        info->tail->next = element;
        info->tail = element;
    }

    key_sz = strlen(key);

    new_key = arena_push(&json->string_arena, NULL, (key_sz + 1) * sizeof(char));
    memcpy(new_key, key, key_sz);
    new_key[key_sz] = '\0';

    if(!reference)
    {
        new_value = arena_push(&json->value_arena, NULL, sizeof(JsonValue));
        memcpy(new_value, value, sizeof(JsonValue));
    }

    new_key_value = arena_push(&json->value_arena, NULL, sizeof(JsonKeyValue));
    new_key_value->key = new_key;
    new_key_value->value = reference ? value : new_value;

    element->key_value = new_key_value;

    json_incr_sz(dict->tags);
}

JsonValue* json_dict_find(Json* json, JsonValue* dict, const char* key)
{
    JsonDictInfo* info;
    JsonDictIterator iterator;

    ROMANO_UNUSED(json);

    info = (JsonDictInfo*)dict->value.ptr;

    iterator.current = info->head;

    while(iterator.current != NULL)
    {
        if(strcmp(iterator.current->key_value->key, key) == 0)
            break;

        iterator.current = iterator.current->next;
    }

    return iterator.current == NULL ? NULL : iterator.current->key_value->value;
}

void json_dict_pop(Json* json, JsonValue* dict, const char* key)
{
    JsonDictInfo* info;
    JsonDictIterator iterator;
    JsonDictElement* previous;
    bool found;

    ROMANO_UNUSED(json);

    found = false;
    info = (JsonDictInfo*)dict->value.ptr;

    iterator.current = (JsonDictElement*)(info->head);
    previous = NULL;

    while(iterator.current != NULL)
    {
        if(strcmp(iterator.current->key_value->key, key) == 0)
        {
            found = true;
            break;
        }

        previous = iterator.current;
        iterator.current = iterator.current->next;
    }

    if(!found)
        return;

    if(previous == NULL)
        info->head = iterator.current->next;
    else
        previous->next = iterator.current->next;

    if(iterator.current == info->tail)
        info->tail = previous;

    json_decr_sz(dict->tags);
}

JsonKeyValue* json_dict_get_next(Json* json, JsonValue* dict, JsonDictIterator* iterator)
{
    JsonDictInfo* info;

    ROMANO_UNUSED(json);

    info = (JsonDictInfo*)dict->value.ptr;

    if(iterator->current == NULL)
        iterator->current = info->head;
    else
        iterator->current = iterator->current->next;

    if(iterator->current == NULL)
        return NULL;

    return iterator->current->key_value;
}

size_t json_dict_get_size(JsonValue* value)
{
    if((value->tags & JsonTag_Dict) == 0)
        return 0;

    return (size_t)json_get_sz(value->tags);
}

/***************/
/* Json Parser */
/***************/

#define JSON_MAX_DEPTH 512

typedef struct JsonParser {
    const char* str;
    size_t pos;
    size_t len;
    Json* json;
    size_t depth;
} JsonParser;

JsonValue* json_parse_value(JsonParser* p);

ROMANO_FORCE_INLINE void json_skip_whitespace(JsonParser* p)
{
    while(p->pos < p->len)
    {
        char c = p->str[p->pos];

        if(c == ' ' || c == '\t' || c == '\n' || c == '\r')
            p->pos++;
        else
            break;
    }
}

static int json_hex_value(char c)
{
    if(c >= '0' && c <= '9')
        return c - '0';

    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

static bool json_parse_hex4(const char* str, uint32_t* out)
{
    uint32_t value = 0;
    int i;

    for(i = 0; i < 4; i++)
    {
        const int digit = json_hex_value(str[i]);

        if(digit < 0)
            return false;

        value = (value << 4) | (uint32_t)digit;
    }

    *out = value;

    return true;
}

static size_t json_encode_utf8(uint32_t codepoint, char* out)
{
    if(codepoint < 0x80)
    {
        out[0] = (char)codepoint;
        return 1;
    }

    if(codepoint < 0x800)
    {
        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        return 2;
    }

    if(codepoint < 0x10000)
    {
        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        return 3;
    }

    out[0] = (char)(0xF0 | (codepoint >> 18));
    out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
    out[3] = (char)(0x80 | (codepoint & 0x3F));
    return 4;
}

/* Decodes the escape sequence at str (after the backslash), returns the number of input chars consumed or 0 if invalid */
static size_t json_decode_escape(const char* str, size_t available, char* out, size_t* out_sz)
{
    uint32_t codepoint;
    uint32_t low;

    if(available == 0)
        return 0;

    switch(str[0])
    {
        case '"': *out = '"'; *out_sz = 1; return 1;
        case '\\': *out = '\\'; *out_sz = 1; return 1;
        case '/': *out = '/'; *out_sz = 1; return 1;
        case 'b': *out = '\b'; *out_sz = 1; return 1;
        case 'f': *out = '\f'; *out_sz = 1; return 1;
        case 'n': *out = '\n'; *out_sz = 1; return 1;
        case 'r': *out = '\r'; *out_sz = 1; return 1;
        case 't': *out = '\t'; *out_sz = 1; return 1;
        case 'u':
            break;
        default:
            return 0;
    }

    if(available < 5 || !json_parse_hex4(str + 1, &codepoint))
        return 0;

    if(codepoint >= 0xDC00 && codepoint <= 0xDFFF)
        return 0;

    if(codepoint >= 0xD800 && codepoint <= 0xDBFF)
    {
        if(available < 11 || str[5] != '\\' || str[6] != 'u' || !json_parse_hex4(str + 7, &low))
            return 0;

        if(low < 0xDC00 || low > 0xDFFF)
            return 0;

        codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
        *out_sz = json_encode_utf8(codepoint, out);
        return 11;
    }

    *out_sz = json_encode_utf8(codepoint, out);

    return 5;
}

JsonValue* json_parse_string(JsonParser* p)
{
    JsonValue* value;
    char* str;
    size_t start;
    size_t end;
    size_t i;
    size_t j;
    bool has_escape = false;

    if(p->pos >= p->len || p->str[p->pos] != '"')
        return NULL;

    p->pos++;
    start = p->pos;

    while(p->pos < p->len && p->str[p->pos] != '"')
    {
        const char c = p->str[p->pos];

        if((unsigned char)c < 0x20)
            return NULL;

        if(c == '\\')
        {
            has_escape = true;
            p->pos++;
        }

        p->pos++;
    }

    if(p->pos >= p->len)
        return NULL;

    end = p->pos;

    /* Decoded strings are never longer than their escaped form */
    str = arena_push(&p->json->string_arena, NULL, end - start + 1);
    value = arena_push(&p->json->value_arena, NULL, sizeof(JsonValue));

    if(str == NULL || value == NULL)
        return NULL;

    if(!has_escape)
    {
        memcpy(str, p->str + start, end - start);
        j = end - start;
    }
    else
    {
        j = 0;

        for(i = start; i < end;)
        {
            if(p->str[i] == '\\')
            {
                size_t decoded_sz = 0;
                size_t consumed = json_decode_escape(p->str + i + 1, end - i - 1, str + j, &decoded_sz);

                if(consumed == 0)
                    return NULL;

                i += 1 + consumed;
                j += decoded_sz;
            }
            else
            {
                str[j++] = p->str[i++];
            }
        }
    }

    str[j] = '\0';

    p->pos++;

    memset(value, 0, sizeof(JsonValue));
    json_set_tags(value->tags, JsonTag_Str);
    json_set_sz(value->tags, j);
    value->value.str = str;

    return value;
}

ROMANO_FORCE_INLINE bool is_digit(unsigned int c)
{
    return (c - 48) < 10;
}

JsonValue* json_parse_number(JsonParser* p)
{
    const size_t start = p->pos;
    bool is_negative = false;
    bool is_float = false;
    bool overflow = false;
    uint64_t int_val = 0;
    char buffer[128];
    size_t number_sz;

    if(p->pos < p->len && p->str[p->pos] == '-')
    {
        is_negative = true;
        p->pos++;
    }

    if(p->pos >= p->len || !is_digit(p->str[p->pos]))
        return NULL;

    if(p->str[p->pos] == '0' && p->pos + 1 < p->len && is_digit(p->str[p->pos + 1]))
        return NULL;

    while(p->pos < p->len && is_digit(p->str[p->pos]))
    {
        const uint64_t digit = (uint64_t)(p->str[p->pos] - '0');

        if(int_val > (UINT64_MAX - digit) / 10)
            overflow = true;
        else
            int_val = int_val * 10 + digit;

        p->pos++;
    }

    if(p->pos < p->len && p->str[p->pos] == '.')
    {
        is_float = true;
        p->pos++;

        if(p->pos >= p->len || !is_digit(p->str[p->pos]))
            return NULL;

        while(p->pos < p->len && is_digit(p->str[p->pos]))
            p->pos++;
    }

    if(p->pos < p->len && (p->str[p->pos] == 'e' || p->str[p->pos] == 'E'))
    {
        is_float = true;
        p->pos++;

        if(p->pos < p->len && (p->str[p->pos] == '-' || p->str[p->pos] == '+'))
            p->pos++;

        if(p->pos >= p->len || !is_digit(p->str[p->pos]))
            return NULL;

        while(p->pos < p->len && is_digit(p->str[p->pos]))
            p->pos++;
    }

    if(!is_float && !overflow)
    {
        if(!is_negative)
            return json_u64_new(p->json, int_val);

        if(int_val <= (uint64_t)INT64_MAX + 1)
            return json_i64_new(p->json, (int64_t)(0 - int_val));
    }

    number_sz = p->pos - start;

    if(number_sz < sizeof(buffer))
    {
        memcpy(buffer, p->str + start, number_sz);
        buffer[number_sz] = '\0';

        return json_f64_new(p->json, strtod(buffer, NULL));
    }
    else
    {
        char* heap_buffer = (char*)malloc(number_sz + 1);
        double value;

        if(heap_buffer == NULL)
            return NULL;

        memcpy(heap_buffer, p->str + start, number_sz);
        heap_buffer[number_sz] = '\0';
        value = strtod(heap_buffer, NULL);
        free(heap_buffer);

        return json_f64_new(p->json, value);
    }
}

JsonValue* json_parse_array(JsonParser* p)
{
    if(p->pos >= p->len || p->str[p->pos] != '[')
        return NULL;

    p->pos++;

    JsonValue* array = json_array_new(p->json);
    json_skip_whitespace(p);

    if(p->pos < p->len && p->str[p->pos] == ']')
    {
        p->pos++;
        return array;
    }

    while(p->pos < p->len)
    {
        JsonValue* element = json_parse_value(p);

        if(element == NULL)
            return NULL;

        json_array_append(p->json, array, element, true);
        json_skip_whitespace(p);

        if(p->pos >= p->len)
            return NULL;

        if(p->str[p->pos] == ',')
        {
            p->pos++;
            json_skip_whitespace(p);
        }
        else if (p->str[p->pos] == ']')
        {
            p->pos++;
            return array;
        }
        else
        {
            return NULL;
        }
    }

    return NULL;
}

JsonValue* parse_dict(JsonParser* p)
{
    if(p->pos >= p->len || p->str[p->pos] != '{')
        return NULL;

    p->pos++;

    JsonValue* dict = json_dict_new(p->json, NULL);
    json_skip_whitespace(p);

    if(p->pos < p->len && p->str[p->pos] == '}')
    {
        p->pos++;
        return dict;
    }

    while(p->pos < p->len)
    {
        json_skip_whitespace(p);

        if(p->pos >= p->len || p->str[p->pos] != '"')
        {
            g_current_error = ErrorCode_JsonExpectedKey;
            return NULL;
        }

        JsonValue* key_val = json_parse_string(p);

        if(key_val == NULL)
            return NULL;

        const char* key = json_str_get(key_val);

        json_skip_whitespace(p);

        if(p->pos >= p->len || p->str[p->pos] != ':')
        {
            g_current_error = ErrorCode_JsonExpectedColon;
            return NULL;
        }

        p->pos++;
        json_skip_whitespace(p);

        JsonValue* value = json_parse_value(p);

        if(value == NULL)
            return NULL;

        json_dict_append(p->json, dict, key, value, true);
        json_skip_whitespace(p);

        if(p->pos >= p->len)
            return NULL;

        if(p->str[p->pos] == ',')
        {
            p->pos++;
            json_skip_whitespace(p);
        }
        else if(p->str[p->pos] == '}')
        {
            p->pos++;
            return dict;
        }
        else
        {
            return NULL;
        }
    }

    return NULL;
}

JsonValue* json_parse_literal(JsonParser* p)
{
    if(p->pos + 4 <= p->len && memcmp(p->str + p->pos, "null", 4) == 0)
    {
        p->pos += 4;
        return json_null_new(p->json);
    }

    if(p->pos + 4 <= p->len && memcmp(p->str + p->pos, "true", 4) == 0)
    {
        p->pos += 4;
        return json_bool_new(p->json, true);
    }

    if(p->pos + 5 <= p->len && memcmp(p->str + p->pos, "false", 5) == 0)
    {
        p->pos += 5;
        return json_bool_new(p->json, false);
    }

    return NULL;
}

JsonValue* json_parse_value(JsonParser* p)
{
    JsonValue* nested;

    json_skip_whitespace(p);

    if(p->pos >= p->len)
        return NULL;

    char c = p->str[p->pos];

    if(c == '"')
        return json_parse_string(p);

    if(c == '{' || c == '[')
    {
        if(p->depth >= JSON_MAX_DEPTH)
            return NULL;

        p->depth++;
        nested = c == '{' ? parse_dict(p) : json_parse_array(p);
        p->depth--;

        return nested;
    }
    else if (c == '-' || isdigit(c))
        return json_parse_number(p);
    else if (c == 't' || c == 'f' || c == 'n')
        return json_parse_literal(p);

    return NULL;
}

Json* json_parse(const char* str, size_t str_sz)
{
    if(str == NULL || str_sz == 0)
        return NULL;

    Json* json = json_new();

    if(json == NULL)
        return NULL;

    JsonParser parser;
    parser.str = str;
    parser.pos = 0;
    parser.len = str_sz;
    parser.json = json;
    parser.depth = 0;

    JsonValue* root = json_parse_value(&parser);

    if(!root)
    {
        json_free(json);
        return NULL;
    }

    json_skip_whitespace(&parser);

    if(parser.pos != parser.len)
    {
        json_free(json);
        return NULL;
    }

    json_set_root(json, root);

    return json;
}

/***************/
/* Json writer */
/***************/

typedef struct JsonWriter {
    Json* json;
    char* str;
    size_t str_capacity;
    size_t str_sz;
    size_t indent_size;
    size_t indent;
} JsonWriter;

bool json_write_realloc(JsonWriter* writer, size_t needed_size)
{
    char* new_str;
    size_t total_needed_size;
    size_t new_capacity;

    total_needed_size = writer->str_sz + needed_size;
    new_capacity = writer->str_capacity;

    if(total_needed_size < new_capacity)
        return true;

    while(total_needed_size >= new_capacity)
        new_capacity <<= 1;

    new_str = realloc(writer->str, new_capacity * sizeof(char));

    if(new_str == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return false;
    }

    writer->str = new_str;
    writer->str_capacity = new_capacity;

    return true;
}

ROMANO_FORCE_INLINE bool json_write_char(JsonWriter* writer, char c)
{
    if(!json_write_realloc(writer, 1))
        return false;

    writer->str[writer->str_sz++] = c;

    return true;
}

ROMANO_FORCE_INLINE bool json_write_indent(JsonWriter* writer)
{
    if(!json_write_realloc(writer, writer->indent))
        return false;

    memset(writer->str + writer->str_sz, ' ', writer->indent * sizeof(char));
    writer->str_sz += writer->indent;

    return true;
}

bool json_write_value(JsonWriter* writer, JsonValue* value);

bool json_write_str(JsonWriter* writer, const char* str);

bool json_write_array(JsonWriter* writer, JsonValue* array)
{
    JsonArrayIterator iterator;
    JsonValue* array_element;
    size_t i;

    memset(&iterator, 0, sizeof(JsonArrayIterator));

    if(!json_write_char(writer, '['))
        return false;

    if(writer->indent_size > 0)
    {
        writer->indent += writer->indent_size;
    }

    i = 0;

    while((array_element = json_array_get_next(writer->json, array, &iterator)) != NULL)
    {
        if(i > 0)
        {
            if(!json_write_char(writer, ','))
                return false;
        }

        if(writer->indent_size > 0)
        {
            if(!json_write_char(writer, '\n'))
                return false;

            if(!json_write_indent(writer))
                return false;
        }

        if(!json_write_value(writer, array_element))
            return false;

        i++;
    }

    if(writer->indent_size > 0)
    {
        if(!json_write_char(writer, '\n'))
            return false;

        writer->indent -= writer->indent_size;

        if(!json_write_indent(writer))
            return false;
    }

    if(!json_write_char(writer, ']'))
        return false;

    return true;
}

bool json_write_dict(JsonWriter* writer, JsonValue* dict)
{
    JsonDictIterator iterator;
    JsonKeyValue* dict_element;
    size_t i;

    memset(&iterator, 0, sizeof(JsonDictIterator));

    if(!json_write_char(writer, '{'))
        return false;

    if(writer->indent_size > 0)
    {
        writer->indent += writer->indent_size;
    }

    i = 0;

    while((dict_element = json_dict_get_next(writer->json, dict, &iterator)) != NULL)
    {
        if(i > 0)
        {
            if(!json_write_char(writer, ','))
                return false;
        }

        if(writer->indent_size > 0)
        {
            if(!json_write_char(writer, '\n'))
                return false;

            if(!json_write_indent(writer))
                return false;
        }

        if(!json_write_str(writer, dict_element->key))
            return false;

        if(!json_write_char(writer, ':'))
            return false;

        if(!json_write_char(writer, ' '))
            return false;

        if(!json_write_value(writer, dict_element->value))
            return false;

        i++;
    }

    if(writer->indent_size > 0)
    {
        if(!json_write_char(writer, '\n'))
            return false;

        writer->indent -= writer->indent_size;

        if(!json_write_indent(writer))
            return false;
    }

    if(!json_write_char(writer, '}'))
        return false;

    return true;
}

bool json_write_literal(JsonWriter* writer, const char* lit, size_t lit_sz)
{
    if(!json_write_realloc(writer, lit_sz))
        return false;

    memcpy(writer->str + writer->str_sz, lit, lit_sz);

    writer->str_sz += lit_sz;

    return true;
}

bool json_write_u64(JsonWriter* writer, uint64_t u64)
{
    int buffer_sz;

    buffer_sz = fmt_size_u64(u64);

    if(buffer_sz < 0)
    {
        g_current_error = ErrorCode_FormattingError;
        return false;
    }

    if(!json_write_realloc(writer, (size_t)buffer_sz))
        return false;

    fmt_u64(writer->str + writer->str_sz, u64);

    writer->str_sz += buffer_sz;

    return true;
}

bool json_write_i64(JsonWriter* writer, int64_t i64)
{
    int buffer_sz;

    buffer_sz = fmt_size_i64(i64);

    if(buffer_sz < 0)
    {
        g_current_error = ErrorCode_FormattingError;
        return false;
    }

    if(!json_write_realloc(writer, (size_t)buffer_sz))
        return false;

    fmt_i64(writer->str + writer->str_sz, i64);

    writer->str_sz += buffer_sz;

    return true;
}

bool json_write_f64(JsonWriter* writer, double f64)
{
    char buffer[32];
    int buffer_sz;

    if(!isfinite(f64))
        return json_write_literal(writer, "null", 4);

    buffer_sz = snprintf(buffer, sizeof(buffer), "%.17g", f64);

    if(buffer_sz <= 0 || (size_t)buffer_sz >= sizeof(buffer) - 2)
    {
        g_current_error = ErrorCode_FormattingError;
        return false;
    }

    /* Keeps the value a float when read back */
    if(strpbrk(buffer, ".eE") == NULL)
    {
        buffer[buffer_sz++] = '.';
        buffer[buffer_sz++] = '0';
    }

    return json_write_literal(writer, buffer, (size_t)buffer_sz);
}

static size_t json_escape_char(unsigned char c, char* out)
{
    static const char hex[] = "0123456789abcdef";

    switch(c)
    {
        case '"': out[0] = '\\'; out[1] = '"'; return 2;
        case '\\': out[0] = '\\'; out[1] = '\\'; return 2;
        case '\n': out[0] = '\\'; out[1] = 'n'; return 2;
        case '\r': out[0] = '\\'; out[1] = 'r'; return 2;
        case '\t': out[0] = '\\'; out[1] = 't'; return 2;
        case '\b': out[0] = '\\'; out[1] = 'b'; return 2;
        case '\f': out[0] = '\\'; out[1] = 'f'; return 2;
        default:
            break;
    }

    if(c < 0x20)
    {
        memcpy(out, "\\u00", 4);
        out[4] = hex[c >> 4];
        out[5] = hex[c & 0xF];
        return 6;
    }

    out[0] = (char)c;

    return 1;
}

bool json_write_str(JsonWriter* writer, const char* str)
{
    char escaped[6];
    size_t escaped_sz;
    size_t i;

    if(!json_write_char(writer, '"'))
        return false;

    for(i = 0; str[i] != '\0'; i++)
    {
        escaped_sz = json_escape_char((unsigned char)str[i], escaped);

        if(!json_write_literal(writer, escaped, escaped_sz))
            return false;
    }

    return json_write_char(writer, '"');
}

bool json_write_value(JsonWriter* writer, JsonValue* value)
{
    size_t tag;

    tag = value->tags & JSON_TAGS_MASK;

    switch(tag)
    {
        case JsonTag_Null:
            return json_write_literal(writer, "null", 4);
        case JsonTag_Bool:
            if(value->value.b)
                return json_write_literal(writer, "true", 4);
            else
                return json_write_literal(writer, "false", 5);
        case JsonTag_U64:
            return json_write_u64(writer, value->value.u64);
        case JsonTag_I64:
            return json_write_i64(writer, value->value.i64);
        case JsonTag_F64:
            return json_write_f64(writer, value->value.f64);
        case JsonTag_Str:
            return json_write_str(writer, value->value.str);
        case JsonTag_Array:
            return json_write_array(writer, value);
        case JsonTag_Dict:
            return json_write_dict(writer, value);
    }

    return false;
}

char* json_write(Json* json, size_t indent_size, size_t* written_size)
{
    JsonWriter writer;

    if(json == NULL || json->root == NULL)
        return NULL;

    writer.json = json;
    writer.indent = 0;
    writer.indent_size = indent_size;
    writer.str_capacity = 4096;
    writer.str_sz = 0;
    writer.str = (char*)calloc(writer.str_capacity, sizeof(char));

    if(writer.str == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return NULL;
    }

    if(!json_write_value(&writer, json->root) || !json_write_realloc(&writer, 1))
    {
        free(writer.str);
        return NULL;
    }

    writer.str[writer.str_sz] = '\0';

    if(written_size != NULL)
        *written_size = writer.str_sz;

    return writer.str;
}

/**************/
/* Json funcs */
/**************/

Json* json_new(void)
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
    return json_parse(str, len);
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

    char* file_buffer = calloc(file_size + 1, sizeof(char));

    if(file_buffer == NULL)
    {
        logger_log_error("Error while trying to allocate memory to read json file: %s", file_path);
        g_current_error = ErrorCode_MemAllocError;
        fclose(file);
        return NULL;
    }

    size_t file_read_size = fread(file_buffer, sizeof(char), file_size, file);

    fclose(file);

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
    return json_write(json, indent_size, dumps_size);
}

bool json_dumpf(Json* json, size_t indent_size, const char* file_path)
{
    char* written;
    size_t written_sz;
    size_t fwritten_sz;
    FILE* file;

    written = json_write(json, indent_size, &written_sz);

    if(written == NULL)
        return false;

    file = fopen(file_path, "wb");

    if(file == NULL)
    {
        free(written);

        g_current_error = error_get_last_from_system();

        logger_log_error("Error while trying to write json file: %s (%d)",
                         file_path,
                         (int)g_current_error);

        return false;
    }

    fwritten_sz = fwrite(written, sizeof(char), written_sz, file);

    free(written);

    if(fclose(file) != 0 || fwritten_sz < written_sz)
    {
        g_current_error = error_get_last_from_system();

        logger_log_error("Error while trying to write json file: %s (%d)",
                         file_path,
                         (int)g_current_error);

        return false;
    }

    return true;
}

void json_free(Json* json)
{
    arena_release(&json->string_arena);
    arena_release(&json->value_arena);

    free(json);
}
