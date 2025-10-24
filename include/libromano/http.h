/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_HTTP)
#define __LIBROMANO_HTTP

#include "libromano/common.h"

ROMANO_CPP_ENTER

typedef enum HTTPVersion {
   HTTPVersion_1_1,
   HTTPVersion_2,
   HTTPVersion_3,
} HTTPVersion;

typedef enum HTTPRequestMethod {
    HTTPRequestMethod_POST,
    HTTPRequestMethod_GET,
    HTTPRequestMethod_DELETE,
    HTTPRequestMethod_PUT,
    HTTPRequestMethod_PATCH,
    HTTPRequestMethod_HEAD,
    HTTPRequestMethod_OPTIONS,
    HTTPRequestMethod_TRACE,
    HTTPRequestMethod_CONNECT,
} HTTPRequestMethod;

typedef enum HTTPContentType {
    HTTPContentType_ApplicationJson,
} HTTPContentType;

typedef struct HTTPHeaderEntry {
    const char* key;
    const char* value;
    struct HTTPHeaderEntry* next;
} HTTPHeaderEntry;

ROMANO_API void http_header_entry_init(HTTPHeaderEntry* entry,
                                       const char* key,
                                       const char* value);

typedef struct HTTPRequest {
    HTTPVersion version;
    HTTPRequestMethod method;
    const char* endpoint;
} HTTPRequest;

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_HTTP) */
