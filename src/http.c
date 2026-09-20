/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/http.h"
#include "libromano/arena.h"
#include "libromano/common.h"
#include "libromano/error.h"
#include "libromano/buffer.h"

#include <string.h>

#if ROMANO_HTTP

#define HTTP_PORT 80
#define HTTPS_PORT 443

extern ErrorCode g_current_error;

/*
 * Returns NULL on unknown version
 */
const char* http_version_as_string(HTTPVersion version)
{
    switch(version)
    {
        case HTTPVersion_1_1: return "HTTP/1.1";
        case HTTPVersion_2: return "HTTP/2";
        case HTTPVersion_3: return "HTTP/3";
        default: return NULL;
    }
}

/*
 * Returns 0 on unknown version
 */
size_t http_version_as_strlen(HTTPVersion version)
{
    switch(version)
    {
        case HTTPVersion_1_1: return 8;
        case HTTPVersion_2:
        case HTTPVersion_3: return 6;
        default: return 0;
    }
}

HTTPVersion http_version_from_string(const char* str, size_t str_sz)
{
    if(str_sz < 6)
        return HTTPVersion_Invalid;

    if(strncmp(str, "HTTP/", 5) != 0)
        return HTTPVersion_Invalid;

    return (HTTPVersion)(str[5] - '0');
}

/*
 * Returns NULL on unknown method
 */
const char* http_method_as_string(HTTPMethod method)
{
    switch(method)
    {
        case HTTPMethod_POST: return "POST";
        case HTTPMethod_GET: return "GET";
        case HTTPMethod_DELETE: return "DELETE";
        case HTTPMethod_PUT: return "PUT";
        case HTTPMethod_PATCH: return "PATCH";
        case HTTPMethod_HEAD: return "HEAD";
        case HTTPMethod_OPTIONS: return "OPTIONS";
        case HTTPMethod_TRACE: return "TRACE";
        case HTTPMethod_CONNECT: return "CONNECT";
        default: return NULL;
    }
}

/*
 * Returns 0 on unknown method
 */
size_t http_method_as_strlen(HTTPMethod method)
{
    switch(method)
    {
        case HTTPMethod_POST: return 4;
        case HTTPMethod_GET: return 3;
        case HTTPMethod_DELETE: return 6;
        case HTTPMethod_PUT: return 3;
        case HTTPMethod_PATCH: return 5;
        case HTTPMethod_HEAD: return 4;
        case HTTPMethod_OPTIONS: return 7;
        case HTTPMethod_TRACE: return 5;
        case HTTPMethod_CONNECT: return 7;
        default: return 0;
    }
}

/*
 * Returns NULL on unknown content
 */
const char* http_content_type_as_string(HTTPContentType type)
{
    switch(type)
    {
        case HTTPContentType_ApplicationJson: return "application/json";
        default: return NULL;
    }
}

/***********/
/* HEADERS */
/***********/

bool http_header_init(HTTPHeader* header)
{
    ROMANO_ASSERT(header != NULL, "header is NULL");

    header->head = NULL;
    header->tail = NULL;

    if(!arena_init(&header->string_arena, 2048))
        return false;

    if(!arena_init(&header->entries_arena, 16 * sizeof(HTTPHeaderEntry)))
        return false;

    return true;
}

void http_header_iterator_init(HTTPHeaderIterator* iterator)
{
    ROMANO_ASSERT(iterator != NULL, "iterator is NULL");

    iterator->current = NULL;
}

void http_header_add_entry(HTTPHeader* header,
                           const char* key,
                           size_t key_sz,
                           const char* value,
                           size_t value_sz)
{
    HTTPHeaderEntry* entry;
    char* internal_key;
    char* internal_value;


    ROMANO_ASSERT(header != NULL, "header is NULL");
    ROMANO_ASSERT(key != NULL, "key is NULL");
    ROMANO_ASSERT(value != NULL, "value is NULL");

    entry = (HTTPHeaderEntry*)arena_push(&header->entries_arena,
                                         NULL,
                                         sizeof(HTTPHeaderEntry));

    if(key_sz == 0)
        key_sz = strlen(key);

    internal_key = (char*)arena_push(&header->string_arena,
                                     NULL,
                                     (key_sz + 1) * sizeof(char));
    memcpy(internal_key, key, key_sz * sizeof(char));
    internal_key[key_sz] = '\0';

    if(value_sz == 0)
        value_sz = strlen(value);

    internal_value = (char*)arena_push(&header->string_arena,
                                       NULL,
                                       (value_sz + 1) * sizeof(char));
    memcpy(internal_value, value, value_sz * sizeof(char));
    internal_value[value_sz] = '\0';

    entry->key = internal_key;
    entry->value = internal_value;
    entry->next = NULL;

    if(header->head == NULL)
        header->head = entry;

    if(header->tail != NULL)
        header->tail->next = entry;

    header->tail = entry;
}

HTTPHeaderEntry* http_header_get_next(HTTPHeader* header,
                                      HTTPHeaderIterator* iterator)
{
    ROMANO_ASSERT(header != NULL, "header is NULL");
    ROMANO_ASSERT(iterator != NULL, "iterator is NULL");

    if(iterator->current == NULL)
        iterator->current = header->head;
    else
        iterator->current = iterator->current->next;

    return iterator->current == NULL ? NULL : iterator->current;
}

HTTPHeaderEntry* http_header_find(HTTPHeader* header, const char* key)
{
    HTTPHeaderEntry* entry;
    HTTPHeaderIterator iterator;

    ROMANO_ASSERT(header != NULL, "header is NULL");

    http_header_iterator_init(&iterator);

    while((entry = http_header_get_next(header, &iterator)))
    {
        if(strcmp(key, entry->key) == 0)
            return entry;
    }

    return NULL;
}

void http_header_remove_entry(HTTPHeader* header, const char* key)
{
    HTTPHeaderEntry* entry;
    HTTPHeaderEntry* previous = NULL;

    ROMANO_ASSERT(header != NULL, "header is NULL");

    for(entry = header->head; entry != NULL; previous = entry, entry = entry->next)
        if(strcmp(entry->key, key) == 0)
            break;

    if(entry == NULL)
        return;

    if(previous == NULL)
        header->head = entry->next;
    else
        previous->next = entry->next;

    if(entry == header->tail)
        header->tail = previous;
}

void http_header_release(HTTPHeader* header)
{
    ROMANO_ASSERT(header != NULL, "header is NULL");

    header->head = NULL;
    header->tail = NULL;

    arena_release(&header->string_arena);
    arena_release(&header->entries_arena);
}

/***********/
/* BUILDER */
/***********/

bool http_builder_init(HTTPBuilder* builder, size_t capacity)
{
    ROMANO_ASSERT(builder != NULL, "builder is NULL");

    return buffer_init(builder, capacity * sizeof(char));
}

bool http_builder_is_empty(HTTPBuilder* builder)
{
    ROMANO_ASSERT(builder != NULL, "builder is NULL");

    return buffer_is_empty(builder);
}

size_t http_builder_size(HTTPBuilder* builder)
{
    ROMANO_ASSERT(builder != NULL, "builder is NULL");

    return builder->sz;
}

void http_builder_reset(HTTPBuilder* builder)
{
    ROMANO_ASSERT(builder != NULL, "builder is NULL");

    builder->sz = 0;
}

/*
 * Pass 0 to str_sz to let the function calculate it
 */
bool http_builder_append(HTTPBuilder* builder, const char* str, size_t str_sz)
{
    ROMANO_ASSERT(builder != NULL, "builder is NULL");

    if(str_sz == 0)
        str_sz = strlen(str);

    if(!buffer_append(builder, (const void*)str, str_sz * sizeof(char)))
        return false;

    return true;
}

void http_builder_release(HTTPBuilder* builder)
{
    ROMANO_ASSERT(builder != NULL, "builder is NULL");

    buffer_release(builder);
}

/***********/
/* REQUEST */
/***********/

bool http_request_init(HTTPRequest* request,
                       HTTPVersion version,
                       HTTPMethod method,
                       const char* endpoint)
{
    ROMANO_ASSERT(request != NULL, "request is NULL");

    request->version = version;
    request->method = method;
    request->endpoint = endpoint;

    if(!http_header_init(&request->headers))
        return false;

    return true;
}

size_t http_request_deduce_build_size(HTTPRequest* request)
{
    HTTPHeaderIterator header_iterator;
    HTTPHeaderEntry* header_entry;
    size_t sz;

    sz = 0;

    sz += http_version_as_strlen(request->version) + 1; /* version + space */
    sz += strlen(request->endpoint) + 1; /* endpoint + space */
    sz += http_method_as_strlen(request->method) + 2; /* method + CRLF */

    http_header_iterator_init(&header_iterator);

    while((header_entry = http_header_get_next(&request->headers, &header_iterator)))
    {
        /* key + ": " + value + CRLF */
        sz += strlen(header_entry->key);
        sz += 2;
        sz += strlen(header_entry->value);
        sz += 2;
    }

    sz += 2; /* final CRLF */

    sz++; /* null term */

    return sz;
}

/*
 * The request body will not be null-terminated, beware
 */
char* http_request_build(HTTPRequest* request, HTTPBuilder* builder)
{
    HTTPHeaderIterator header_iterator;
    HTTPHeaderEntry* header_entry;
    size_t deduced_request_build_sz;

    deduced_request_build_sz = http_request_deduce_build_size(request);

    if(!http_builder_is_empty(builder))
        return NULL;

    http_builder_append(builder,
                        http_method_as_string(request->method),
                        http_method_as_strlen(request->method));
    http_builder_append(builder, " ", 1);

    http_builder_append(builder, request->endpoint, 0);
    http_builder_append(builder, " ", 1);

    http_builder_append(builder,
                        http_version_as_string(request->version),
                        http_version_as_strlen(request->version));
    http_builder_append(builder, "\r\n", 2);

    http_header_iterator_init(&header_iterator);

    while((header_entry = http_header_get_next(&request->headers, &header_iterator)))
    {
        http_builder_append(builder, header_entry->key, 0);
        http_builder_append(builder, ": ", 2);
        http_builder_append(builder, header_entry->value, 0);
        http_builder_append(builder, "\r\n", 2);
    }

    http_builder_append(builder, "\r\n", 2);

    return (char*)builder->data;
}

void http_request_release(HTTPRequest* request)
{
    ROMANO_ASSERT(request != NULL, "request is NULL");

    http_header_release(&request->headers);
}

/************/
/* RESPONSE */
/************/

bool http_response_init(HTTPResponse* response)
{
    ROMANO_ASSERT(response != NULL, "response is NULL");

    if(!http_header_init(&response->headers))
        return false;

    response->code = 0;
    response->content = NULL;
    response->content_sz = 0;

    return true;
}

typedef struct HTTPReponseParser {
    const char* start;
    size_t current;
    size_t sz;
} HTTPResponseParser;

ROMANO_FORCE_INLINE bool http_response_parser_skip_whitespace(HTTPResponseParser* parser)
{
    while(parser->start[parser->current++] != ' ')
        if(parser->current >= parser->sz)
            return false;

    if(parser->current >= parser->sz)
        return false;

    return true;
}

ROMANO_FORCE_INLINE bool http_response_parser_skip_crlf(HTTPResponseParser* parser)
{
    while(parser->start[parser->current++] != '\r')
        if(parser->current >= parser->sz)
            return false;

    if(parser->current >= parser->sz)
        return false;

    if(!(parser->start[parser->current] == '\n'))
        return false;

    parser->current++;

    if(parser->current >= parser->sz)
        return false;

    return true;
}

ROMANO_FORCE_INLINE char* http_response_parser_parse_key(HTTPResponseParser* parser,
                                                         size_t* key_sz)
{
    char* key_start;

    if((parser->current + 2) >= parser->sz)
        return NULL;

    if(parser->start[parser->current] == '\r' &&
       parser->start[parser->current + 1] == '\n')
        return NULL;

    key_start = (char*)(parser->start + parser->current);
    *key_sz = 0;

    while(parser->current < parser->sz &&
          parser->start[parser->current++] != ':')
        (*key_sz)++;

    parser->current++;

    return key_start;
}

ROMANO_FORCE_INLINE char* http_response_parser_parse_value(HTTPResponseParser* parser,
                                                           size_t* value_sz)
{
    char* value_start;

    value_start = (char*)(parser->start + parser->current);
    *value_sz = 0;

    while(parser->current < parser->sz &&
          parser->start[parser->current++] != '\r')
        (*value_sz)++;

    parser->current++;

    return value_start;
}

bool http_response_parse(HTTPResponse* response, const char* body, size_t body_sz)
{
    char* key;
    size_t key_sz;
    char* value;
    size_t value_sz;

    HTTPResponseParser parser;
    parser.start = body;
    parser.current = 0;
    parser.sz = body_sz;

    response->version = http_version_from_string(parser.start, body_sz);

    if(response->version == HTTPVersion_Invalid)
        return false;

    if(!http_response_parser_skip_whitespace(&parser))
        return false;

    response->code = atoi(parser.start + parser.current);

    if(!http_response_parser_skip_crlf(&parser))
        return false;

    while(1)
    {
        key = http_response_parser_parse_key(&parser, &key_sz);

        if(key == NULL)
            break;

        if(parser.current >= parser.sz)
            return false;

        value = http_response_parser_parse_value(&parser, &value_sz);

        http_header_add_entry(&response->headers, key, key_sz, value, value_sz);
    }

    if(!http_response_parser_skip_crlf(&parser))
        return false;

    if(parser.current >= parser.sz)
        return true;

    response->content_sz = parser.sz - parser.current;
    response->content = calloc(response->content_sz + 1, sizeof(char));

    if(response->content == NULL)
    {
        g_current_error = ErrorCode_MemAllocError;
        return false;
    }

    memcpy(response->content,
           parser.start + parser.current,
           response->content_sz * sizeof(char));

    response->content[response->content_sz] = '\0';

    return true;
}

void http_response_release(HTTPResponse* response)
{
    ROMANO_ASSERT(response != NULL, "response is NULL");

    http_header_release(&response->headers);

    if(response->content != NULL)
        free(response->content);

    response->content = NULL;
    response->content_sz = 0;
}


/***********/
/* CONTEXT */
/***********/

bool http_context_connect(HTTPContext* ctx)
{
    ROMANO_ASSERT(ctx != NULL, "ctx is NULL");

    if(ctx->is_alive)
        return true;

    ctx->socket = socket_new(AF_INET, SOCK_STREAM, 0);

    if(ctx->socket == ROMANO_INVALID_SOCKET)
    {
        g_current_error = error_get_last_from_system();
        return false;
    }

    if(socket_connect(ctx->socket, (SockAddr*)&ctx->server, sizeof(SockAddr)) < 0)
    {
        g_current_error = error_get_last_from_system();
        socket_free(ctx->socket);
        return false;
    }

    ctx->is_alive = true;

    return true;
}

void http_context_disconnect(HTTPContext* ctx)
{
    ROMANO_ASSERT(ctx != NULL, "ctx is NULL");

    if(!ctx->is_alive)
        return;

    socket_shutdown(ctx->socket, SHUTDOWN_ALL);
    socket_free(ctx->socket);

    ctx->is_alive = false;
}

bool http_context_init(HTTPContext* ctx, const char* host, int port)
{
    DNSResolveResult res;

    socket_dns_result_init(&res);

    if(!socket_resolve_dns_ipv4(host, &res))
    {
        socket_dns_result_release(&res);
        return false;
    }

    if(socket_dns_result_get_count(&res) == 0)
    {
        socket_dns_result_release(&res);
        g_current_error = ErrorCode_DNSCantFindHost;
        return false;
    }

    ctx->host = host;
    ctx->port = port;
    ctx->keep_alive = true;
    ctx->is_alive = false;
    ctx->last_used_timestamp = 0;

    memset(&ctx->server, 0, sizeof(SockAddrIn));
    ctx->server.sin_family = AF_INET;
    ctx->server.sin_port = htons(port);

    memcpy(&ctx->server.sin_addr,
           &((const SockAddrIn*)socket_dns_result_get(&res, 0))->sin_addr,
           sizeof(InAddr));

    socket_dns_result_release(&res);

    if(!http_context_connect(ctx))
        return false;

    if(!http_builder_init(&ctx->builder, 4096))
    {
        http_context_disconnect(ctx);
        return false;
    }

    if(!buffer_init(&ctx->recv_buffer, 4096))
    {
        http_context_disconnect(ctx);
        http_builder_release(&ctx->builder);
        return false;
    }

    return true;
}

bool http_context_is_alive(HTTPContext* ctx)
{
    return ctx->is_alive;
}

#define HTTP_RESPONSE_BUFFER_SZ 512

static bool http_ascii_iequal(const char* a, const char* b, size_t size)
{
    size_t i;

    for(i = 0; i < size; i++)
    {
        const char ca = (a[i] >= 'A' && a[i] <= 'Z') ? (char)(a[i] - 'A' + 'a') : a[i];
        const char cb = (b[i] >= 'A' && b[i] <= 'Z') ? (char)(b[i] - 'A' + 'a') : b[i];

        if(ca != cb)
            return false;
    }

    return true;
}

/* Returns the Content-Length announced in the headers block, or -1 if there is none */
static long long http_find_content_length(const char* headers, size_t headers_sz)
{
    static const char name[] = "\r\ncontent-length:";
    const size_t name_sz = sizeof(name) - 1;
    size_t i;

    for(i = 0; i + name_sz <= headers_sz; i++)
    {
        if(http_ascii_iequal(headers + i, name, name_sz))
        {
            const char* value = headers + i + name_sz;

            while(*value == ' ' || *value == '\t')
                value++;

            return strtoll(value, NULL, 10);
        }
    }

    return -1;
}

static const char* http_find_headers_end(const char* data, size_t data_sz)
{
    size_t i;

    for(i = 0; i + 4 <= data_sz; i++)
        if(memcmp(data + i, "\r\n\r\n", 4) == 0)
            return data + i + 4;

    return NULL;
}

bool http_context_send_request(HTTPContext *ctx,
                               HTTPRequest *request,
                               HTTPResponse *response)
{
    char* request_body;
    size_t request_sz;
    size_t response_sz;
    ssize_t sent_sz;
    ssize_t recv_sz;
    const char* headers_end = NULL;
    long long content_length = -1;
    HTTPHeaderEntry* resp_entry;

    ROMANO_ASSERT(ctx != NULL, "ctx is NULL");
    ROMANO_ASSERT(request != NULL, "request is NULL");
    ROMANO_ASSERT(response != NULL, "response is NULL");

    if(!ctx->is_alive && !http_context_connect(ctx))
    {
        g_current_error = ErrorCode_HTTPContextNotAlive;
        return false;
    }

    if(http_header_find(&request->headers, "Host") == NULL)
        http_header_add_entry(&request->headers, "Host", 4, ctx->host, 0);

    http_builder_reset(&ctx->builder);

    request_body = http_request_build(request, &ctx->builder);
    request_sz = http_builder_size(&ctx->builder);

    if(request_body == NULL)
    {
        g_current_error = ErrorCode_HTTPInvalidRequest;
        return false;
    }

    sent_sz = socket_send(ctx->socket, request_body, request_sz, 0);

    if(sent_sz <= 0)
    {
        g_current_error = error_get_last_from_system();
        return false;
    }

    buffer_reset(&ctx->recv_buffer);

    while(1)
    {
        if(!buffer_prepare_emplace(&ctx->recv_buffer, HTTP_RESPONSE_BUFFER_SZ))
            return false;

        recv_sz = socket_recv(ctx->socket,
                              buffer_back(&ctx->recv_buffer),
                              HTTP_RESPONSE_BUFFER_SZ,
                              0);

        if(recv_sz < 0)
            return false;

        if(recv_sz == 0)
        {
            http_context_disconnect(ctx);
            break;
        }

        buffer_emplace_size(&ctx->recv_buffer, (size_t)recv_sz);

        if(headers_end == NULL)
        {
            headers_end = http_find_headers_end((const char*)buffer_front(&ctx->recv_buffer),
                                                buffer_size(&ctx->recv_buffer));

            if(headers_end != NULL)
                content_length = http_find_content_length((const char*)buffer_front(&ctx->recv_buffer),
                                                          (size_t)(headers_end - (const char*)buffer_front(&ctx->recv_buffer)));
        }
        else
        {
            /* The buffer may have been reallocated */
            headers_end = http_find_headers_end((const char*)buffer_front(&ctx->recv_buffer),
                                                buffer_size(&ctx->recv_buffer));
        }

        if(headers_end != NULL && content_length >= 0)
        {
            const size_t headers_sz = (size_t)(headers_end - (const char*)buffer_front(&ctx->recv_buffer));

            if(buffer_size(&ctx->recv_buffer) >= headers_sz + (size_t)content_length)
                break;
        }
    }

    response_sz = buffer_size(&ctx->recv_buffer);

    if(!buffer_append(&ctx->recv_buffer, "\0", 1))
        return false;

    if(!http_response_parse(response,
                            (const char*)buffer_front(&ctx->recv_buffer),
                            response_sz))
        return false;

    resp_entry = http_header_find(&response->headers, "Connection");

    if(resp_entry != NULL && strlen(resp_entry->value) == 5 && http_ascii_iequal(resp_entry->value, "close", 5))
        http_context_disconnect(ctx);

    return true;
}

void http_context_release(HTTPContext* ctx)
{
    ROMANO_ASSERT(ctx != NULL, "ctx is NULL");

    if(ctx->is_alive)
        http_context_disconnect(ctx);

    http_builder_release(&ctx->builder);

    buffer_release(&ctx->recv_buffer);
}

#endif /* ROMANO_HTTP */
