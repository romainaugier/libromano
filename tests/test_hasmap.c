/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#define ROMANO_ENABLE_PROFILING

#include "libromano/hashmap.h"
#include "libromano/profiling.h"


int main(void)
{
    hashmap_t* hashmap = hashmap_new();


    hashmap_free(hashmap);

    return 0;
}
