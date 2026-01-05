/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/cli.h"
#include "libromano/error.h"

#include <ctype.h>
#include <string.h>

extern ErrorCode g_current_error;

typedef struct CLIArg {
    CLIArgType type;
    CLIArgAction action;

    union {
        int64_t i64;
        double f64;
        char* str;
        bool b;
    } data;

    size_t data_sz;
} CLIArg;

bool cli_parser_init(CLIParser* parser)
{
    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    parser->args_map = hashmap_new(128);

    if(parser->args_map == NULL)
        return false;

    return true;
}

bool cli_parser_add_arg(CLIParser* parser,
                        const char* name,
                        size_t name_sz,
                        CLIArgType type,
                        CLIArgAction action)
{
    CLIArg arg;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    memset(&arg, 0, sizeof(CLIArg));
    arg.type = type;
    arg.action = action;

    if(name_sz == 0)
        name_sz = strlen(name);

    hashmap_insert(parser->args_map,
                   (const void*)name,
                   (uint32_t)name_sz,
                   &arg,
                   sizeof(CLIArg));

    return true;
}

/* TODO: optimize the function */
bool cli_parser_validate_argument_type(CLIArgType type, char* arg_str)
{
    switch(type)
    {
        case CLIArgType_Str:
        {
            return true;
        }
        case CLIArgType_Int:
        {
            if(*arg_str != '\0' && isdigit(*arg_str))
                return true;

            return false;
        }
        case CLIArgType_Float:
        {
            if(*arg_str != '\0' && isdigit(*arg_str))
                return true;

            return false;
        }
        case CLIArgType_Bool:
        {
            if(*arg_str != '\0' && (*arg_str == '0' || *arg_str == '1'))
                return true;

            if(strcmp(arg_str, "true") == 0 ||
               strcmp(arg_str, "false") == 0 ||
               strcmp(arg_str, "True") == 0 ||
               strcmp(arg_str, "False") == 0)
                return true;

            return true;
        }
        default:
            return false;
    }
}

bool cli_parser_parse_bool(char* arg_str)
{
    if(strcmp(arg_str, "true") || strcmp(arg_str, "True"))
        return true;

    return false;
}

bool cli_parser_parse_argument(CLIParser* parser, char* arg_str)
{
    CLIArg* arg;
    char* arg_name;
    size_t arg_name_sz;
    char* str_start;
    size_t str_sz;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    while(*arg_str != '\0' && *arg_str == '-')
        arg_str++;

    if(*arg_str == '\0')
    {
        g_current_error = ErrorCode_CLIMalformedArgument;
        return false;
    }

    arg_name = arg_str;
    arg_name_sz = 0;

    while(*arg_str != '\0' && *(arg_str++) != '=')
        arg_name_sz++;

    if(*arg_str == '\0')
    {
        g_current_error = ErrorCode_CLIMalformedArgument;
        return false;
    }

    arg = (CLIArg*)hashmap_get(parser->args_map,
                               arg_name,
                               (uint32_t)arg_name_sz,
                               NULL);

    if(arg == NULL)
    {
        g_current_error = ErrorCode_CLIUnknownArgument;
        return false;
    }

    if(!cli_parser_validate_argument_type(arg->type, arg_str))
    {
        g_current_error = ErrorCode_CLIInvalidArgumentType;
        return false;
    }

    switch(arg->type)
    {
        case CLIArgType_Str:
        {
            if(*arg_str == '"')
                arg_str++;

            str_sz = 0;

            str_start = arg_str;

            while(*arg_str != '\0' && *(arg_str++) != '"')
                str_sz++;

            arg->data.str = (char*)calloc(str_sz + 1, sizeof(char));

            if(arg->data.str == NULL)
            {
                g_current_error = ErrorCode_MemAllocError;
                return false;
            }

            memcpy(arg->data.str, str_start, str_sz * sizeof(char));

            arg->data_sz = str_sz;

            break;
        }
        case CLIArgType_Int:
        {
            arg->data.i64 = strtoll(arg_str, NULL, 10);
            break;
        }
        case CLIArgType_Float:
        {
            arg->data.f64 = strtod(arg_str, NULL);
            break;
        }
        case CLIArgType_Bool:
        {
            if(isdigit(*arg_str))
                arg->data.b = (bool)(*arg_str - '0');
            else
                arg->data.b = cli_parser_parse_bool(arg_str);

            break;
        }
        default:
        {
            g_current_error = ErrorCode_CLIInvalidArgumentType;
            return false;
        }
    }

    return true;
}

bool cli_parser_parse(CLIParser* parser, int argc, char** argv)
{
    int i;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(argc == 1)
        return true;

    for(i = 1; i < argc; i++)
        if(!cli_parser_parse_argument(parser, argv[i]))
            return false;

    return true;
}

bool cli_parser_has_arg(CLIParser* parser,
                        const char* name,
                        size_t name_sz)
{
    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    return hashmap_get(parser->args_map,
                       (const void*)name,
                       (uint32_t)name_sz,
                       NULL) != NULL;
}

int64_t* cli_parser_arg_get_i64(CLIParser* parser,
                                const char* name,
                                size_t name_sz)
{
    CLIArg* arg;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(name_sz == 0)
        name_sz = strlen(name);

    arg = (CLIArg*)hashmap_get(parser->args_map,
                               (const void*)name,
                               (uint32_t)name_sz,
                               NULL);

    if(arg == NULL)
        return NULL;

    if(arg->type != CLIArgType_Int)
        return NULL;

    return &arg->data.i64;
}

double* cli_parser_arg_get_f64(CLIParser* parser,
                               const char* name,
                               size_t name_sz)
{
    CLIArg* arg;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(name_sz == 0)
        name_sz = strlen(name);

    arg = (CLIArg*)hashmap_get(parser->args_map,
                               (const void*)name,
                               (uint32_t)name_sz,
                               NULL);

    if(arg == NULL)
        return NULL;

    if(arg->type != CLIArgType_Float)
        return NULL;

    return &arg->data.f64;
}

char* cli_parser_arg_get_str(CLIParser* parser,
                             const char* name,
                             size_t name_sz,
                             size_t* str_sz)
{
    CLIArg* arg;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(name_sz == 0)
        name_sz = strlen(name);

    arg = (CLIArg*)hashmap_get(parser->args_map,
                               (const void*)name,
                               (uint32_t)name_sz,
                               NULL);

    if(arg == NULL)
        return NULL;

    if(arg->type != CLIArgType_Str)
        return NULL;

    return arg->data.str;
}

bool* cli_parser_arg_get_bool(CLIParser* parser,
                              const char* name,
                              size_t name_sz)
{
    CLIArg* arg;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(name_sz == 0)
        name_sz = strlen(name);

    arg = (CLIArg*)hashmap_get(parser->args_map,
                               (const void*)name,
                               (uint32_t)name_sz,
                               NULL);

    if(arg == NULL)
        return NULL;

    if(arg->type != CLIArgType_Bool)
        return NULL;

    return &arg->data.b;
}

void cli_parser_release(CLIParser* parser)
{
    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    HashMapIterator it = 0;
    void* key;
    void* value;

    while(hashmap_iterate(parser->args_map, &it, &key, NULL, &value, NULL))
    {
        CLIArg* arg = (CLIArg*)value;

        if(arg->type == CLIArgType_Str)
            free(arg->data.str);
    }

    hashmap_free(parser->args_map);
}
