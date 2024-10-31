/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/strview.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define NUM_INTS_TO_PARSE 10
#define NUM_BOOLS_TO_PARSE 7
#define NUM_STRINGS_TO_FIND 7

int main(void)
{
    size_t count, i;

    static const char* test = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin malesuada eget"
                              "libero quis viverra. Interdum et malesuada fames ac ante ipsum primis in faucibus. "
                              "Curabitur eget ultrices nisl, eu interdum nisi. Morbi ultrices vulputate enim sit "
                              "amet facilisis. Nullam et ultrices ipsum, eget lobortis metus. Duis eleifend quis "
                              "metus ultrices lacinia. Morbi quis imperdiet augue. Vestibulum ornare laoreet "
                              "vulputate. Maecenas sollicitudin orci nec purus euismod, ut ultricies felis dapibus. "
                              "Mauris diam massa, elementum quis sodales in, pulvinar eu neque. Vivamus feugiat magna "
                              "nunc, at aliquet arcu feugiat sed. Nullam sagittis posuere sapien sed efficitur. "
                              "Donec maximus massa ut rhoncus dapibus.";

    strview_t sp = STRVIEW_NULL;

    while(strview_split(test, " ", &sp))
    {
        printf("Split : \""STRVIEW_FMT"\"\n", STRVIEW_ARG(sp));
    }

    const char* test_trim = "    trim this string  ";

    strview_t trim = strview_trim(test_trim);

    printf("Trimmed string : \""STRVIEW_FMT"\"\n", STRVIEW_ARG(trim));

    const char* test_split = "test this split";

    strview_t r;
    strview_t lsplit = strview_lsplit(test_split, " ", &r);

    printf("lsplit: \""STRVIEW_FMT"\", \""STRVIEW_FMT"\"\n", STRVIEW_ARG(lsplit), STRVIEW_ARG(r));

    strview_t l;
    strview_t rsplit = strview_rsplit(test_split, " ", &l);

    printf("rsplit: \""STRVIEW_FMT"\", \""STRVIEW_FMT"\"\n", STRVIEW_ARG(l), STRVIEW_ARG(rsplit));

    strview_t string_view_to_contain;
    string_view_to_contain.data = "this string may contain some substrings";
    string_view_to_contain.size = strlen(string_view_to_contain.data);

    char* const contains[NUM_STRINGS_TO_FIND] = {
        "this",
        "string",
        "may",
        "contrain",
        "some",
        "substrings",
        "wont find this string"
    };

    for(i = 0; i < NUM_STRINGS_TO_FIND; i++)
    {
        printf("String view \""STRVIEW_FMT"\" contains \"%s\": %d\n", STRVIEW_ARG(string_view_to_contain),
                                                                      contains[i], 
                                                                      strview_find(string_view_to_contain, contains[i], strlen(contains[i])));
    }

    strview_t startswith;
    startswith.data = "this phrase starts with";
    startswith.size = strlen(startswith.data);

    printf("\""STRVIEW_FMT"\" starts with: \"this\" %d\n", STRVIEW_ARG(startswith), strview_startswith(startswith, "this", 4));

    strview_t endswith;
    endswith.data = "this phrase ends with";
    endswith.size = strlen(endswith.data);

    printf("\""STRVIEW_FMT"\" ends with: \"with\" %d\n", STRVIEW_ARG(endswith), strview_endswith(endswith, "with", 4));

    char* const ints[NUM_INTS_TO_PARSE] = {
        "2147483647",
        "48190",
        "8192",
        "189401995839",
        "4839019",
        "571892",
        " 583940    ",
        " 57389 word",
        " 5738",
        "-483901"
    };

    for(i = 0; i < NUM_INTS_TO_PARSE; i++)
    {
        strview_t int_string_view;
        int_string_view.data = ints[i];
        int_string_view.size = strlen(ints[i]);

        int parsed_int = strview_parse_int(int_string_view);

        printf("Int as string: %s, Parsed int: %d\n", ints[i], parsed_int);
    }

    char* const bools[NUM_BOOLS_TO_PARSE] = {
        "1",
        "0",
        "true",
        "True",
        "false",
        "False",
        "not a bool"
    };

    for(i = 0; i < NUM_BOOLS_TO_PARSE; i++)
    {
        strview_t bool_string_view;
        bool_string_view.data = bools[i];
        bool_string_view.size = strlen(bools[i]);

        int parsed_bool = strview_parse_bool(bool_string_view);

        printf("Bool as a string: %s, Parsed bool %d\n", bools[i], parsed_bool);
    }

    return 0;
}
