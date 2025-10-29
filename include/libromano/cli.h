/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_CLI)
#define __LIBROMANO_CLI

#include "libromano/common.h"
#include "libromano/hashmap.h"

ROMANO_CPP_ENTER

typedef enum {
    CLIArgType_Str,
    CLIArgType_Int,
    CLIArgType_Float,
    CLIArgType_Bool,
} CLIArgType;

typedef enum {
    CLIArgAction_Store,
    CLIArgAction_StoreTrue,
    CLIArgAction_StoreFalse,
    CLIArgAction_Append,
    CLIArgAction_Count,
} CLIArgAction;

typedef struct CLIParser {
    HashMap* args_map;
} CLIParser;

ROMANO_API bool cli_parser_init(CLIParser* parser);

ROMANO_API bool cli_parser_add_arg(CLIParser* parser,
                                   const char* name,
                                   size_t name_sz,
                                   CLIArgType type,
                                   CLIArgAction action);

ROMANO_API bool cli_parser_parse(CLIParser* parser, int argc, char** argv);

ROMANO_API bool cli_parser_has_arg(CLIParser* parser,
                                   const char* name,
                                   size_t name_sz);

ROMANO_API int64_t* cli_parser_arg_get_i64(CLIParser* parser,
                                           const char* name,
                                           size_t name_sz);

ROMANO_API double* cli_parser_arg_get_f64(CLIParser* parser,
                                          const char* name,
                                          size_t name_sz);

ROMANO_API char* cli_parser_arg_get_str(CLIParser* parser,
                                        const char* name,
                                        size_t name_sz,
                                        size_t* str_sz);

ROMANO_API bool* cli_parser_arg_get_bool(CLIParser* parser,
                                         const char* name,
                                         size_t name_sz);

ROMANO_API void cli_parser_release(CLIParser* parser);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_CLI) */
