/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/math/linalg32.h"
#include "libromano/logger.h"

#include <stdio.h>

#define N 5
#define M 9 

int main(void)
{
    logger_init();

    matrixf_t m = matrixf_create(N, M);

    for(int i = 0; i < N; i++)
    {
        for(int j = 0; j < M; j++)
        {
            matrixf_set_at(&m, (float)j, i, j);

            const float v = matrixf_get_at(&m, i, j);
            printf("%f, ", v);
        }

        printf("\n");
    }

    logger_log(LogLevel_Info, "Transpose");

    matrixf_transpose(&m);

    for(int i = 0; i < M; i++)
    {
        for(int j = 0; j < N; j++)
        {
            const float v = matrixf_get_at(&m, i, j);
            printf("%f, ", v);
        }

        printf("\n");
    }

    matrixf_destroy(&m);

    return 0;
}