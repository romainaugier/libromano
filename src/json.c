/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/json.h"
#include "libromano/arena.h"
#include "libromano/logger.h"
#include "libromano/error.h"
#include "libromano/fmt.h"
#include "libromano/bit.h"

#if defined(ROMANO_LINUX)
#include <errno.h>
#endif

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
    tags &= (((uint64_t)sz && 0xFFFFFFFFULL) << 32);

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
            info->tail = iterator.current->next;
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
        memcpy(new_value, value, sizeof(JsonKeyValue));
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

    found = false;
    info = (JsonDictInfo*)dict->value.ptr;

    iterator.current = (JsonDictElement*)(dict->value.ptr);
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
    {
        info->head = iterator.current;
    }
    else
    {
        previous->next = iterator.current->next;

        if(iterator.current == info->tail)
            info->tail = iterator.current->next;
    }

    json_decr_sz(dict->tags);
}

JsonKeyValue* json_dict_get_next(Json* json, JsonValue* dict, JsonDictIterator* iterator)
{
    JsonDictInfo* info;

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

typedef struct JsonParser {
    const char* str;
    size_t pos;
    size_t len;
    Json* json;
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

JsonValue* json_parse_string(JsonParser* p)
{
    if(p->pos >= p->len || p->str[p->pos] != '"')
        return NULL;

    p->pos++;

    size_t start = p->pos;
    size_t len = 0;
    bool has_escape = false;

    while(p->pos < p->len)
    {
        char c = p->str[p->pos];

        if(c == '"')
        {
            break;
        }
        else if(c == '\\')
        {
            has_escape = true;
            p->pos++;

            if(p->pos >= p->len)
                return NULL;

            char escape = p->str[p->pos];

            if(escape == 'u')
            {
                p->pos += 4;

                if(p->pos >= p->len)
                    return NULL;
            }
        }
        else if((unsigned char)c < 0x20)
        {
            return NULL;
        }

        p->pos++;
        len++;
    }

    if(p->pos >= p->len)
        return NULL;

    JsonValue* value = arena_push(&p->json->value_arena, NULL, sizeof(JsonValue));
    char* str;

    if(!has_escape)
    {
        str = arena_push(&p->json->string_arena, NULL, len + 1);
        memcpy(str, p->str + start, len);
        str[len] = '\0';
    }
    else
    {
        str = arena_push(&p->json->string_arena, NULL, len + 1);
        size_t j = 0;

        for(size_t i = start; i < p->pos; i++)
        {
            if(p->str[i] == '\\')
            {
                i++;

                switch(p->str[i])
                {
                    case '"': str[j++] = '"'; break;
                    case '\\': str[j++] = '\\'; break;
                    case '/': str[j++] = '/'; break;
                    case 'b': str[j++] = '\b'; break;
                    case 'f': str[j++] = '\f'; break;
                    case 'n': str[j++] = '\n'; break;
                    case 'r': str[j++] = '\r'; break;
                    case 't': str[j++] = '\t'; break;
                    case 'u':
                    {
                        /* TODO: unicode handling */
                        i += 4;
                        str[j++] = '?';
                        break;
                    }
                }
            }
            else
            {
                str[j++] = p->str[i];
            }
        }

        str[j] = '\0';
        len = j;
    }

    p->pos++;
    json_set_tags(value->tags, JsonTag_Str);
    value->value.str = str;

    return value;
}

ROMANO_FORCE_INLINE bool is_digit(unsigned int c)
{
    return (c - 48) < 10;
}

static const double pow10_table[] = {
    1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
    1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
    1e20, 1e21, 1e22
};

static const uint64_t pow10_int_table[] = {
    1ULL, 10ULL, 100ULL, 1000ULL, 10000ULL, 100000ULL, 1000000ULL,
    10000000ULL, 100000000ULL, 1000000000ULL, 10000000000ULL,
    100000000000ULL, 1000000000000ULL, 10000000000000ULL,
    100000000000000ULL, 1000000000000000ULL, 10000000000000000ULL,
    100000000000000000ULL, 1000000000000000000ULL, 10000000000000000000ULL
};

#define POW10_TABLE_SIZE (sizeof(pow10_table) / sizeof(pow10_table[0]))
#define POW10_INT_TABLE_SIZE (sizeof(pow10_int_table) / sizeof(pow10_int_table[0]))

JsonValue* json_parse_number(JsonParser* p)
{
    size_t start = p->pos;
    bool is_negative = false;
    bool is_float = false;
    int64_t int_val = 0;
    double float_val = 0.0;
    double fraction = 0.0;
    int exponent = 0;
    bool exp_negative = false;
    if(p->pos < p->len && p->str[p->pos] == '-')
    {
        is_negative = true;
        p->pos++;
    }
    if(p->pos >= p->len || !is_digit(p->str[p->pos]))
        return NULL;
    while(p->pos < p->len && is_digit(p->str[p->pos]))
    {
        int_val = int_val * 10 + (p->str[p->pos] - '0');
        float_val = float_val * 10 + (p->str[p->pos] - '0');
        p->pos++;
    }
    if(p->pos < p->len && p->str[p->pos] == '.')
    {
        is_float = true;
        p->pos++;
        if (p->pos >= p->len || !is_digit(p->str[p->pos]))
            return NULL;
        int frac_digits = 0;
        uint64_t frac_int = 0;
        while(p->pos < p->len && is_digit(p->str[p->pos]) && frac_digits < POW10_INT_TABLE_SIZE)
        {
            frac_int = frac_int * 10 + (p->str[p->pos] - '0');
            frac_digits++;
            p->pos++;
        }
        while(p->pos < p->len && is_digit(p->str[p->pos]))
            p->pos++;
        if(frac_digits > 0 && frac_digits < POW10_TABLE_SIZE)
            fraction = (double)frac_int / pow10_table[frac_digits];
        else if(frac_digits > 0)
            fraction = (double)frac_int / pow(10.0, frac_digits);
        float_val += fraction;
    }
    if(p->pos < p->len && (p->str[p->pos] == 'e' || p->str[p->pos] == 'E'))
    {
        is_float = true;
        p->pos++;
        if(p->pos < p->len && p->str[p->pos] == '-')
        {
            exp_negative = true;
            p->pos++;
        }
        else if(p->pos < p->len && p->str[p->pos] == '+')
        {
            p->pos++;
        }
        if(p->pos >= p->len || !is_digit(p->str[p->pos]))
            return NULL;
        while(p->pos < p->len && is_digit(p->str[p->pos]))
        {
            exponent = exponent * 10 + (p->str[p->pos] - '0');
            p->pos++;
        }
    }
    if(is_float)
    {
        if(exp_negative)
            exponent = -exponent;
        if(exponent >= -(int)POW10_TABLE_SIZE && exponent < (int)POW10_TABLE_SIZE)
        {
            if(exponent >= 0)
                float_val *= pow10_table[exponent];
            else
                float_val /= pow10_table[-exponent];
        }
        else
        {
            float_val *= pow(10.0, exponent);
        }
        if(is_negative)
            float_val = -float_val;
        return json_f64_new(p->json, float_val);
    }
    else
    {
        if(is_negative)
        {
            return json_i64_new(p->json, -int_val);
        }
        else
        {
            return json_u64_new(p->json, (uint64_t)int_val);
        }
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
    json_skip_whitespace(p);

    if(p->pos >= p->len)
        return NULL;

    char c = p->str[p->pos];

    if(c == '"')
        return json_parse_string(p);
    else if (c == '{')
        return parse_dict(p);
    else if (c == '[')
        return json_parse_array(p);
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
    int buffer_sz;

    buffer_sz = fmt_size_f64(f64, 3);

    if(buffer_sz < 0)
    {
        g_current_error = ErrorCode_FormattingError;
        return false;
    }

    if(!json_write_realloc(writer, (size_t)buffer_sz))
        return false;

    fmt_f64(writer->str + writer->str_sz, f64, 3);

    writer->str_sz += buffer_sz;

    return true;
}

ROMANO_FORCE_INLINE bool json_char_is_escaped(char c)
{
    switch(c)
    {
        case '\'':
        case '\"':
        case '\\':
        case '\n':
        case '\r':
        case '\t':
        case '\b':
        case '\f':
        case '\v':
            return true;
        default:
            return false;
    }
}

ROMANO_FORCE_INLINE char json_escaped_to_char(char c)
{
    switch(c)
    {
        case '\'': return 0x27;
        case '\"': return '"';
        case '\\': return '\\';
        case '\n': return 'n';
        case '\r': return 'r';
        case '\t': return 't';
        case '\b': return 'b';
        case '\f': return 'f';
        case '\v': return 'v';
        default: return c;
    }
}

bool json_write_str(JsonWriter* writer, const char* str)
{
    size_t str_sz;
    size_t i;

    str_sz = 0;
    i = 0;

    while(str[i] != '\0')
    {
        if(json_char_is_escaped(str[i]))
            str_sz += 2;

        str_sz++;
        i++;
    }

    if(!json_write_realloc(writer, str_sz + 2))
        return false;

    i = 0;

    writer->str[writer->str_sz++] = '"';

    while(str[i] != '\0')
    {
        if(json_char_is_escaped(str[i]))
        {
            if(str[i] != '\'')
                writer->str[writer->str_sz++] = '\\';

            writer->str[writer->str_sz++] = json_escaped_to_char(str[i]);
        }
        else
        {
            writer->str[writer->str_sz++] = str[i];
        }

        i++;
    }

    writer->str[writer->str_sz++] = '"';

    return true;
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

    if(!json_write_value(&writer, json->root))
        return NULL;

    *written_size = writer.str_sz;

    return writer.str;
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

    if(fwritten_sz < written_sz)
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
