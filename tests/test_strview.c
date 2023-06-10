/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/strview.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    const char* test = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin malesuada eget"
                       "libero quis viverra. Interdum et malesuada fames ac ante ipsum primis in faucibus. "
                       "Curabitur eget ultrices nisl, eu interdum nisi. Morbi ultrices vulputate enim sit "
                       "amet facilisis. Nullam et ultrices ipsum, eget lobortis metus. Duis eleifend quis "
                       "metus ultrices lacinia. Morbi quis imperdiet augue. Vestibulum ornare laoreet "
                       "vulputate. Maecenas sollicitudin orci nec purus euismod, ut ultricies felis dapibus. "
                       "Mauris diam massa, elementum quis sodales in, pulvinar eu neque. Vivamus feugiat magna "
                       "nunc, at aliquet arcu feugiat sed. Nullam sagittis posuere sapien sed efficitur. "
                       "Donec maximus massa ut rhoncus dapibus.";
    size_t count;

    strview_t* views = strview_split(test, " ", &count);

    size_t i;

    printf("String view splits : \n");

    for(i = 0; i < count; i++)
    {
        printf("- \""STRVIEW_FMT"\"\n", STRVIEW_ARG(views[i]));
    }

    free(views);

    strview_t sp = STRVIEW_NULL;

    while(strview_split_yield(test, " ", &sp))
    {
        printf("Split yield : \""STRVIEW_FMT"\"\n", STRVIEW_ARG(sp));
    }

    const char* test_trim = "    trim this string  ";

    strview_t trim = strview_trim(test_trim);

    printf("Trimmed string : \""STRVIEW_FMT"\"\n", STRVIEW_ARG(trim));
}
