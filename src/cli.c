/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/cli.h"
#define __ROMANO_DEFINE_ERROR_EXTERNS
#include "libromano/error.h"

#include <ctype.h>
#include <string.h>
#include <stdio.h>

typedef struct CLIArg {
    size_t flags;

    union {
        int64_t i64;
        double f64;
        char* str;
        bool b;
    } data;

    size_t data_sz;
    char* help_text;
    char short_name;
    char* name;
} CLIArg;

#define CLI_ARG_GET_MODE(arg) ((arg).flags & 0x3)
#define CLI_ARG_SET_MODE(arg, mode) ((arg).flags = ((arg).flags & ~0x3) | ((mode) & 0x3))

#define CLI_ARG_GET_TYPE(arg) (((arg).flags >> 2) & 0x3)
#define CLI_ARG_SET_TYPE(arg, type) ((arg).flags = ((arg).flags & ~(0x3 << 2)) | (((type) & 0x3) << 2))

#define CLI_ARG_GET_ACTION(arg) (((arg).flags >> 4) & 0x7)
#define CLI_ARG_SET_ACTION(arg, action) ((arg).flags = ((arg).flags & ~(0x7 << 4)) | (((action) & 0x7) << 4))

#define CLI_PARG_GET_MODE(arg) ((arg)->flags & 0x3)
#define CLI_PARG_SET_MODE(arg, mode) ((arg)->flags = ((arg)->flags & ~0x3) | ((mode) & 0x3))

#define CLI_PARG_GET_TYPE(arg) (((arg)->flags >> 2) & 0x3)
#define CLI_PARG_SET_TYPE(arg, type) ((arg)->flags = ((arg)->flags & ~(0x3 << 2)) | (((type) & 0x3) << 2))

#define CLI_PARG_GET_ACTION(arg) (((arg)->flags >> 4) & 0x7)
#define CLI_PARG_SET_ACTION(arg, action) ((arg)->flags = ((arg)->flags & ~(0x7 << 4)) | (((action) & 0x7) << 4))

bool cli_parser_init(CLIParser* parser)
{
    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    parser->args_map = hashmap_new(16);

    if(parser->args_map == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        error_set_context(ROMANO_STRLIT_AND_SZ("Error during allocation of args_map"));
        return false;
    }

    parser->short_names_map = hashmap_new(8);

    if(parser->short_names_map == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        error_set_context(ROMANO_STRLIT_AND_SZ("Error during allocation of short_names_map"));
        hashmap_free(parser->args_map);
        return false;
    }

    vector_init(&parser->positional_args, 8, sizeof(CLIArg*));

    if(parser->positional_args.data == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        error_set_context(ROMANO_STRLIT_AND_SZ("Error during allocation of positional_args"));
        hashmap_free(parser->args_map);
        hashmap_free(parser->short_names_map);
        return false;
    }

    parser->program_name = NULL;
    parser->description = NULL;

    return true;
}

bool cli_parser_add_arg(CLIParser* parser,
                        const char* name,
                        size_t name_sz,
                        char short_name,
                        CLIArgMode mode,
                        CLIArgType type,
                        CLIArgAction action,
                        const char* help_text)
{
    CLIArg* arg;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(mode == CLIArgMode_Positional && action != CLIArgAction_Store)
    {
        g_current_error = ErrorCode_CLIInvalidArgumentAction;
        error_set_context(ROMANO_STRLIT_AND_SZ("Invalid argument action, a positional argument cannot do other action than store"));
        return false;
    }

    arg = (CLIArg*)calloc(1, sizeof(CLIArg));

    if(arg == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        error_set_context(ROMANO_STRLIT_AND_SZ("Invalid argument action, a positional argument cannot do other action than store"));
        return false;
    }

    memset(arg, 0, sizeof(CLIArg));
    CLI_PARG_SET_MODE(arg, mode);
    CLI_PARG_SET_TYPE(arg, type);
    CLI_PARG_SET_ACTION(arg, action);
    arg->data_sz = 0;

    if(name_sz == 0)
        name_sz = strlen(name);

    arg->name = (char*)calloc(name_sz + 1, sizeof(char));

    if(arg->name == NULL)
    {
        free(arg);
        g_current_error = ErrorCode_MemAllocError;
        return false;
    }

    memcpy(arg->name, name, name_sz);

    arg->short_name = short_name;

    if(short_name != CLI_NO_SHORT_NAME)
    {
        hashmap_insert(parser->short_names_map,
                       (const void*)&short_name,
                       (uint32_t)1,
                       &arg,
                       sizeof(CLIArg*));
    }

    if(help_text != NULL)
    {
        size_t help_sz = strlen(help_text);
        arg->help_text = (char*)calloc(help_sz + 1, sizeof(char));

        if(arg->help_text != NULL)
            memcpy(arg->help_text, help_text, help_sz);
    }

    if(mode == CLIArgMode_Positional)
    {
        vector_push_back(&parser->positional_args, &arg);
    }

    hashmap_insert(parser->args_map,
                   (const void*)name,
                   (uint32_t)name_sz,
                   &arg,
                   sizeof(CLIArg*));

    return true;
}

void cli_parser_print_help(CLIParser* parser)
{
    HashMapIterator it = 0;
    void* key;
    void* value;
    size_t i;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(parser->program_name != NULL)
        printf("Usage: %s", parser->program_name);
    else
        printf("Usage: program");

    for(i = 0; i < vector_size(&parser->positional_args); i++)
    {
        printf(" %s", (*(CLIArg**)vector_at(&parser->positional_args, i))->name);
    }

    printf(" [options]\n\n");

    if(parser->description != NULL)
        printf("%s\n\n", parser->description);

    if(vector_size(&parser->positional_args) > 0)
    {
        printf("Positional arguments:\n");

        for(i = 0; i < vector_size(&parser->positional_args); i++)
        {
            CLIArg* arg = *(CLIArg**)vector_at(&parser->positional_args, i);
            printf("  %-20s", arg->name);

            if(arg->help_text != NULL)
                printf("%s", arg->help_text);

            printf("\n");
        }

        printf("\n");
    }

    printf("Optional arguments:\n");
    printf("  -h, --help          Show this help message and exit\n");

    while(hashmap_iterate(parser->args_map, &it, &key, NULL, &value, NULL))
    {
        CLIArg* arg = *(CLIArg**)value;

        if(CLI_PARG_GET_MODE(arg) == CLIArgMode_Positional)
            continue;

        if(arg->short_name != CLI_NO_SHORT_NAME)
            printf("  -%c, --%-13s", arg->short_name, arg->name);
        else
            printf("  --%-17s", arg->name);

        if(arg->help_text != NULL)
            printf("%s", arg->help_text);

        printf("\n");
    }
}

void cli_parser_set_program_info(CLIParser* parser,
                                 const char* program_name,
                                 const char* description)
{
    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(program_name != NULL)
    {
        size_t name_sz = strlen(program_name);
        parser->program_name = (char*)calloc(name_sz + 1, sizeof(char));

        if(parser->program_name != NULL)
            memcpy(parser->program_name, program_name, name_sz);
    }

    if(description != NULL)
    {
        size_t desc_sz = strlen(description);
        parser->description = (char*)calloc(desc_sz + 1, sizeof(char));

        if(parser->description != NULL)
            memcpy(parser->description, description, desc_sz);
    }
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
            if(*arg_str != '\0' && (isdigit(*arg_str) || *arg_str == '-' || *arg_str == '+'))
                return true;

            return false;
        }
        case CLIArgType_Float:
        {
            if(*arg_str != '\0' && (isdigit(*arg_str) || *arg_str == '-' || *arg_str == '+' || *arg_str == '.'))
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

            return false;
        }
        default:
            return false;
    }
}

bool cli_parser_parse_bool(char* arg_str)
{
    if(strcmp(arg_str, "true") == 0 || strcmp(arg_str, "True") == 0)
        return true;

    return false;
}

CLIArg* cli_parser_find_arg(CLIParser* parser, char* arg_str, size_t* name_sz)
{
    CLIArg* arg = NULL;
    CLIArg** arg_ptr;
    char* arg_name;
    size_t arg_name_sz;
    size_t num_dashes = 0;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    while(*arg_str != '\0' && *arg_str == '-')
    {
        num_dashes += *arg_str == '-';
        arg_str++;
    }

    if(*arg_str == '\0')
        return NULL;

    arg_name = arg_str;
    arg_name_sz = 0;

    while(*arg_str != '\0' && *arg_str != '=' && *arg_str != ':')
    {
        arg_name_sz++;
        arg_str++;
    }

    if(num_dashes == 1)
    {
        arg_ptr = (CLIArg**)hashmap_get(parser->short_names_map,
                                        arg_name,
                                        (uint32_t)arg_name_sz,
                                        NULL);
    }
    else
    {
        arg_ptr = (CLIArg**)hashmap_get(parser->args_map,
                                        arg_name,
                                        (uint32_t)arg_name_sz,
                                        NULL);
    }

    if(arg_ptr != NULL)
        arg = *arg_ptr;

    if(name_sz != NULL)
        *name_sz = arg_name_sz;

    return arg;
}

bool cli_parser_parse_named_argument(CLIParser* parser, char* arg_str, int* next_arg_offset, char** next_arg)
{
    CLIArg* arg;
    char* value_str = arg_str;
    char* str_start;
    size_t str_sz;
    size_t type;
    size_t action;
    bool has_value = false;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    while(*value_str != '\0' && *value_str == '-')
        value_str++;

    if(*value_str == '\0')
    {
        g_current_error = ErrorCode_CLIMalformedArgument;
        return false;
    }

    while(*value_str != '\0' && *value_str != '=' && *value_str != ':')
        value_str++;

    if(*value_str == '=' || *value_str == ':')
    {
        has_value = true;
        value_str++;
    }

    arg = cli_parser_find_arg(parser, arg_str, NULL);

    if(arg == NULL)
    {
        g_current_error = ErrorCode_CLIUnknownArgument;
        error_set_context(arg_str, 0);
        return false;
    }

    type = CLI_PARG_GET_TYPE(arg);
    action = CLI_PARG_GET_ACTION(arg);

    if(action == CLIArgAction_StoreTrue)
    {
        arg->data.b = true;
        arg->data_sz = sizeof(bool);
        return true;
    }

    if(action == CLIArgAction_StoreFalse)
    {
        arg->data.b = false;
        arg->data_sz = sizeof(bool);
        return true;
    }

    if(action == CLIArgAction_Count)
    {
        arg->data.i64++;
        arg->data_sz = sizeof(int64_t);
        return true;
    }

    if(!has_value)
    {
        if(next_arg != NULL && *next_arg != NULL)
        {
            value_str = *next_arg;
            (*next_arg_offset)++;
        }
        else
        {
            g_current_error = ErrorCode_CLIMalformedArgument;
            return false;
        }
    }

    if(*value_str == '\0')
    {
        g_current_error = ErrorCode_CLIMalformedArgument;
        return false;
    }

    if(!cli_parser_validate_argument_type(type, value_str))
    {
        g_current_error = ErrorCode_CLIInvalidArgumentType;
        return false;
    }

    switch(type)
    {
        case CLIArgType_Str:
        {
            if(*value_str == '"')
                value_str++;

            str_sz = 0;
            str_start = value_str;

            while(*value_str != '\0' && *value_str != '"')
            {
                str_sz++;
                value_str++;
            }

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
            arg->data.i64 = strtoll(value_str, NULL, 10);
            arg->data_sz = sizeof(int64_t);
            break;
        }
        case CLIArgType_Float:
        {
            arg->data.f64 = strtod(value_str, NULL);
            arg->data_sz = sizeof(double);
            break;
        }
        case CLIArgType_Bool:
        {
            if(isdigit(*value_str))
                arg->data.b = (bool)(*value_str - '0');
            else
                arg->data.b = cli_parser_parse_bool(value_str);

            arg->data_sz = sizeof(bool);

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

bool cli_parser_parse_positional_argument(CLIParser* parser, char* arg_str, size_t pos_index)
{
    CLIArg* arg;
    char* str_start;
    size_t str_sz;
    size_t type;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(pos_index >= vector_size(&parser->positional_args))
    {
        g_current_error = ErrorCode_CLITooManyPositionalArgs;
        return false;
    }

    arg = *((CLIArg**)vector_at(&parser->positional_args, pos_index));
    type = CLI_PARG_GET_TYPE(arg);

    if(!cli_parser_validate_argument_type(type, arg_str))
    {
        g_current_error = ErrorCode_CLIInvalidArgumentType;
        return false;
    }

    switch(type)
    {
        case CLIArgType_Str:
        {
            if(*arg_str == '"')
                arg_str++;

            str_sz = 0;
            str_start = arg_str;

            while(*arg_str != '\0' && *arg_str != '"')
            {
                str_sz++;
                arg_str++;
            }

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
            arg->data_sz = sizeof(int64_t);
            break;
        }
        case CLIArgType_Float:
        {
            arg->data.f64 = strtod(arg_str, NULL);
            arg->data_sz = sizeof(double);
            break;
        }
        case CLIArgType_Bool:
        {
            if(isdigit(*arg_str))
                arg->data.b = (bool)(*arg_str - '0');
            else
                arg->data.b = cli_parser_parse_bool(arg_str);

            arg->data_sz = sizeof(bool);

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
    size_t pos_index = 0;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(parser->program_name == NULL && argc > 0)
        cli_parser_set_program_info(parser, argv[0], NULL);

    if(argc == 1)
        return true;

    for(i = 1; i < argc; i++)
    {
        /* Check for help flag */
        if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            cli_parser_print_help(parser);
            return false;
        }

        if(argv[i][0] == '-')
        {
            char* next_arg = (i + 1 < argc) ? argv[i + 1] : NULL;
            int offset = 0;

            if(!cli_parser_parse_named_argument(parser, argv[i], &offset, &next_arg))
                return false;

            i += offset;
        }
        else
        {
            if(!cli_parser_parse_positional_argument(parser, argv[i], pos_index))
                return false;

            pos_index++;
        }
    }

    if(pos_index < (vector_size(&parser->positional_args) - 1))
    {
        g_current_error = ErrorCode_CLIMissingPositionalArgs;
        return false;
    }

    HashMapIterator it = 0;
    void* key;
    void* value;

    while(hashmap_iterate(parser->args_map, &it, &key, NULL, &value, NULL))
    {
        CLIArg* arg = *(CLIArg**)value;

        if(CLI_PARG_GET_MODE(arg) == CLIArgMode_Named && arg->data_sz == 0)
        {
            g_current_error = ErrorCode_CLIMissingRequiredArg;
            return false;
        }
    }

    return true;
}

bool cli_parser_has_arg(CLIParser* parser,
                        const char* name,
                        size_t name_sz)
{
    CLIArg** arg_ptr;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(name_sz == 0)
        name_sz = strlen(name);

    arg_ptr = (CLIArg**)hashmap_get(parser->args_map,
                                    (const void*)name,
                                    (uint32_t)name_sz,
                                    NULL);

    if(arg_ptr == NULL)
        return false;

    return (*arg_ptr)->data_sz > 0;
}

int64_t* cli_parser_arg_get_i64(CLIParser* parser,
                                const char* name,
                                size_t name_sz)
{
    CLIArg** arg_ptr;
    CLIArg* arg;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(name_sz == 0)
        name_sz = strlen(name);

    arg_ptr = (CLIArg**)hashmap_get(parser->args_map,
                                    (const void*)name,
                                    (uint32_t)name_sz,
                                    NULL);

    if(arg_ptr == NULL)
        return NULL;

    arg = *arg_ptr;

    if(CLI_PARG_GET_TYPE(arg) != CLIArgType_Int)
        return NULL;

    return &arg->data.i64;
}

double* cli_parser_arg_get_f64(CLIParser* parser,
                               const char* name,
                               size_t name_sz)
{
    CLIArg** arg_ptr;
    CLIArg* arg;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(name_sz == 0)
        name_sz = strlen(name);

    arg_ptr = (CLIArg**)hashmap_get(parser->args_map,
                                    (const void*)name,
                                    (uint32_t)name_sz,
                                    NULL);

    if(arg_ptr == NULL)
        return NULL;

    arg = *arg_ptr;

    if(CLI_PARG_GET_TYPE(arg) != CLIArgType_Float)
        return NULL;

    return &arg->data.f64;
}

char* cli_parser_arg_get_str(CLIParser* parser,
                             const char* name,
                             size_t name_sz,
                             size_t* str_sz)
{
    CLIArg** arg_ptr;
    CLIArg* arg;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(name_sz == 0)
        name_sz = strlen(name);

    arg_ptr = (CLIArg**)hashmap_get(parser->args_map,
                                    (const void*)name,
                                    (uint32_t)name_sz,
                                    NULL);

    if(arg_ptr == NULL)
        return NULL;

    arg = *arg_ptr;

    if(CLI_PARG_GET_TYPE(arg) != CLIArgType_Str)
        return NULL;

    if(str_sz != NULL)
        *str_sz = arg->data_sz;

    return arg->data.str;
}

bool* cli_parser_arg_get_bool(CLIParser* parser,
                              const char* name,
                              size_t name_sz)
{
    CLIArg** arg_ptr;
    CLIArg* arg;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    if(name_sz == 0)
        name_sz = strlen(name);

    arg_ptr = (CLIArg**)hashmap_get(parser->args_map,
                                    (const void*)name,
                                    (uint32_t)name_sz,
                                    NULL);

    if(arg_ptr == NULL)
        return NULL;

    arg = *arg_ptr;

    if(CLI_PARG_GET_TYPE(arg) != CLIArgType_Bool)
        return NULL;

    return &arg->data.b;
}

void cli_parser_release(CLIParser* parser)
{
    HashMapIterator it = 0;
    void* key;
    void* value;

    ROMANO_ASSERT(parser != NULL, "parser is NULL");

    while(hashmap_iterate(parser->args_map, &it, &key, NULL, &value, NULL))
    {
        CLIArg* arg = *(CLIArg**)value;

        if(CLI_PARG_GET_TYPE(arg) == CLIArgType_Str && arg->data.str != NULL)
            free(arg->data.str);

        if(arg->help_text != NULL)
            free(arg->help_text);

        if(arg->name != NULL)
            free(arg->name);

        free(arg);
    }

    hashmap_free(parser->args_map);
    hashmap_free(parser->short_names_map);

    vector_release(&parser->positional_args);

    if(parser->program_name != NULL)
        free(parser->program_name);

    if(parser->description != NULL)
        free(parser->description);
}
