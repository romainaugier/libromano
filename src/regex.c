/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/* https://swtch.com/~rsc/regexp/regexp1.html */

#include "libromano/regex.h"

typedef struct
{
    int c;
    State* out;
    State* out1;
    int last_list;
} State;

typedef union 
{
    PtrList* next;
    State* s;
} PtrList;

bool regex_compile(Regex* regex, const char* pattern)
{
    return false;
}

bool regex_match(Regex* regex, const char* string)
{
    return false;
}