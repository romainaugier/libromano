/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

/* https://swtch.com/~rsc/regexp/regexp1.html */

#include "libromano/regex.h"

struct _State 
{
    int c;
    struct _State* out;
    struct _State* out1;
    int last_list;
};

typedef struct _State State;

union _PtrList
{
    union _PtrList* next;
    State* s;
};

typedef union _PtrList PtrList;

bool regex_compile(Regex* regex, const char* pattern)
{
    return false;
}

bool regex_match(Regex* regex, const char* string)
{
    return false;
}