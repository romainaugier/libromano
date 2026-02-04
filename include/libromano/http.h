/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_HTTP)
#define __LIBROMANO_HTTP

#include "libromano/common.h"
#include "libromano/arena.h"
#include "libromano/socket.h"
#include "libromano/buffer.h"

#if ROMANO_HTTP

ROMANO_CPP_ENTER

typedef enum HTTPVersion {
    HTTPVersion_Invalid,
    HTTPVersion_1_1,
    HTTPVersion_2,
    HTTPVersion_3,
} HTTPVersion;

typedef enum HTTPMethod {
    HTTPMethod_POST,
    HTTPMethod_GET,
    HTTPMethod_DELETE,
    HTTPMethod_PUT,
    HTTPMethod_PATCH,
    HTTPMethod_HEAD,
    HTTPMethod_OPTIONS,
    HTTPMethod_TRACE,
    HTTPMethod_CONNECT,
} HTTPMethod;

typedef enum HTTPContentType {
    HTTPContentType_ApplicationJson,
} HTTPContentType;

/**********/
/* HEADER */
/**********/

typedef struct HTTPHeaderEntry {
    const char* key;
    const char* value;
    struct HTTPHeaderEntry* next;
} HTTPHeaderEntry;

typedef struct HTTPHeader {
    HTTPHeaderEntry* head;
    HTTPHeaderEntry* tail;

    Arena string_arena;
    Arena entries_arena;
} HTTPHeader;

typedef struct HTTPHeaderIterator {
    HTTPHeaderEntry* current;
} HTTPHeaderIterator;

ROMANO_API bool http_header_init(HTTPHeader* header);

ROMANO_API void http_header_add_entry(HTTPHeader* header,
                                      const char* key,
                                      size_t key_sz,
                                      const char* value,
                                      size_t value_sz);

ROMANO_API void http_header_iterator_init(HTTPHeaderIterator* iterator);

ROMANO_API HTTPHeaderEntry* http_header_get_next(HTTPHeader* header,
                                                 HTTPHeaderIterator* iterator);

ROMANO_API HTTPHeaderEntry* http_header_find(HTTPHeader* header,
                                             const char* key);

ROMANO_API void http_header_remove_entry(HTTPHeader* header, const char* key);

ROMANO_API void http_header_release(HTTPHeader* header);

/***********/
/* REQUEST */
/***********/

typedef struct HTTPRequest {
    HTTPVersion version;
    HTTPMethod method;
    const char* endpoint;
    HTTPHeader headers;
} HTTPRequest;

ROMANO_API bool http_request_init(HTTPRequest* request,
                                  HTTPVersion version,
                                  HTTPMethod method,
                                  const char* endpoint);

/*
 *
 */
ROMANO_API bool http_request_send(char* request_body,
                                  size_t request_body_sz,
                                  HTTPRequest* response);

ROMANO_API void http_request_release(HTTPRequest* request);

typedef Buffer HTTPBuilder;

/************/
/* RESPONSE */
/************/

typedef struct HTTPResponse {
    HTTPVersion version;
    HTTPHeader headers;
    int code;
    char* content;
    size_t content_sz;
} HTTPResponse;

/*
 * Returns false on failure (memory allocation error)
 */
ROMANO_API bool http_response_init(HTTPResponse* response);

/*
 *
 */
ROMANO_API void http_response_release(HTTPResponse* response);

/***********/
/* CONTEXT */
/***********/

typedef struct HTTPContext {
    HTTPBuilder builder;
    Buffer recv_buffer;
    Socket socket;
    const char* host;
    int port;
    SockAddrIn server;
    bool keep_alive;
    bool is_alive;
    size_t last_used_timestamp;
} HTTPContext;

/*
 * Initializes an http context to send request to the host.
 * Make sure socket_context_init is called before
 */
ROMANO_API bool http_context_init(HTTPContext* ctx, const char* host, int port);

ROMANO_API bool http_context_is_alive(HTTPContext* ctx);

/*
 * Make sure to initialize the response before passing it (http_response_init)
 */
ROMANO_API bool http_context_send_request(HTTPContext* ctx,
                                          HTTPRequest* request,
                                          HTTPResponse* response);

/*
 *
 */
ROMANO_API void http_context_release(HTTPContext* ctx);

ROMANO_CPP_END

#endif /* ROMANO_HTTP */

#endif /* !defined(__LIBROMANO_HTTP) */
