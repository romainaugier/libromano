/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/regex.h"

#define MAX_STATES 32
#define MAX_SYMBOLS 128
#define MAX_TRANSITIONS (MAX_STATES * MAX_STATES)

typedef struct 
{
    uint32_t num_states;
    uint32_t start_state;
} NFA;

void nfa_init(NFA* nfa)
{

}

bool regex_compile(Regex* regex, const char* pattern)
{
    return false;
}

bool regex_match(Regex* regex, const char* string)
{
    return false;
}