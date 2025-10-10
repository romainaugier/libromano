/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/json.h"
#include "libromano/arena.h"
#include "libromano/logger.h"

#if defined(ROMANO_LINUX)
#include <errno.h>
#endif

Json* json_new()
{
    Json* json = malloc(sizeof(Json));

    json->root = NULL;
    arena_init(&json->string_arena, 128 * 1024);
    arena_init(&json->value_arena, 1024 * sizeof(JsonValue));

    return json;
}

Json* json_from_string(const char* str, size_t len)
{
    if(str[0] != '{' && str[0] != '[')
    {
        return NULL;
    }

    Json* root = json_new();
}

Json* json_from_file(const char* file_path)
{
    FILE* file = fopen(file_path, "rb");

    if(file == NULL)
    {
        logger_log_error("Error while trying to read json file: %s (%d)", file_path, errno);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* file_buffer = calloc(file_size, sizeof(char));

    if(file_buffer == NULL)
    {
        logger_log_error("Error while trying to allocate memory to read json file: %s", file_path);
        return NULL;
    }

    size_t file_read_size = fread(file_buffer, sizeof(char), file_size, file);

    if(file_read_size != file_size)
    {
        logger_log_error("Error while trying to read json file: %s (%d)", file_path, errno);
        free(file_buffer);
        return NULL;
    }

    Json* json = json_from_string(file_buffer, file_size);

    free(file_buffer);

    return json;
}

void json_free(Json* json)
{
    arena_release(&json->string_arena);
    arena_release(&json->value_arena);

    free(json);
}