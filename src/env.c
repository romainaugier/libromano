/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/env.h"

#include <string.h>
#include <stdlib.h>

#if defined(ROMANO_WIN)
#include <Windows.h>
#elif defined(ROMANO_APPLE)
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#else
extern char** environ;
#endif /* defined(ROMANO_WIN) */

const char* env_get(const char* name)
{
    ROMANO_ASSERT(name != NULL, "name is NULL");

    return getenv(name);
}

bool env_set(const char* name, const char* value)
{
    ROMANO_ASSERT(name != NULL && value != NULL, "name or value is NULL");

#if defined(ROMANO_WIN)
    return _putenv_s(name, value) == 0;
#else
    return setenv(name, value, 1) == 0;
#endif /* defined(ROMANO_WIN) */
}

bool env_unset(const char* name)
{
    ROMANO_ASSERT(name != NULL, "name is NULL");

#if defined(ROMANO_WIN)
    return _putenv_s(name, "") == 0;
#else
    return unsetenv(name) == 0;
#endif /* defined(ROMANO_WIN) */
}

char** env_list_new(void)
{
#if defined(ROMANO_WIN)
    char** env = _environ;
#else
    char** env = environ;
#endif /* defined(ROMANO_WIN) */
    char** list;
    size_t count = 0;
    size_t i;

    while(env != NULL && env[count] != NULL)
        count++;

    list = (char**)calloc(count + 1, sizeof(char*));

    if(list == NULL)
        return NULL;

    for(i = 0; i < count; i++)
    {
        size_t sz = strlen(env[i]);

        list[i] = (char*)malloc(sz + 1);

        if(list[i] == NULL)
        {
            env_list_free(list);
            return NULL;
        }

        memcpy(list[i], env[i], sz + 1);
    }

    return list;
}

void env_list_free(char** list)
{
    size_t i;

    if(list == NULL)
        return;

    for(i = 0; list[i] != NULL; i++)
        free(list[i]);

    free(list);
}

char env_path_separator(void)
{
#if defined(ROMANO_WIN)
    return ';';
#else
    return ':';
#endif /* defined(ROMANO_WIN) */
}
