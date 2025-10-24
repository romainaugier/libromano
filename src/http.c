/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/http.h"

#define HTTP_PORT 80
#define HTTPS_PORT 443

const char* http_version_as_string(HTTPVersion version)
{
    switch(version)
    {
        case HTTPVersion_1_1: return "HTTP/1.1";
        case HTTPVersion_2: return "HTTP/2";
        case HTTPVersion_3: return "HTTP/3";
        default: return "HTTP/?";
    }
}
