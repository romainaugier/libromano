/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#if ROMANO_HTTP

#include "libromano/http.h"
#include "libromano/thread.h"

#if defined(ROMANO_WIN)
#include <WS2tcpip.h>
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
#include <arpa/inet.h>
#endif /* defined(ROMANO_WIN) */

#define NUM_REQUESTS 3
#define BIG_BODY_SIZE 3000

typedef struct LocalServer {
    Socket listener;
    uint16_t port;
    char requests[NUM_REQUESTS][1024];
    char big_body[BIG_BODY_SIZE];
} LocalServer;

static bool read_request(Socket connection, char* out, size_t capacity)
{
    size_t size = 0;

    while(size < capacity - 1)
    {
        ssize_t received = socket_recv(connection, out + size, 1, 0);

        if(received <= 0)
            return false;

        size++;
        out[size] = '\0';

        if(size >= 4 && memcmp(out + size - 4, "\r\n\r\n", 4) == 0)
            return true;
    }

    return false;
}

static void send_all(Socket connection, const char* data, size_t size)
{
    while(size > 0)
    {
        ssize_t sent = socket_send(connection, data, size, 0);

        if(sent <= 0)
            return;

        data += sent;
        size -= (size_t)sent;
    }
}

static void* serve(void* arg)
{
    LocalServer* server = (LocalServer*)arg;
    Socket connection = socket_accept(server->listener, NULL, NULL);
    char header[256];
    int length;

    if(connection == ROMANO_INVALID_SOCKET)
        return NULL;

    socket_set_timeout(connection, 5000);

    /* Body larger than the client buffer, sent in delayed pieces */
    if(!read_request(connection, server->requests[0], sizeof(server->requests[0])))
        goto end;

    length = snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: %d\r\n\r\n", BIG_BODY_SIZE);
    send_all(connection, header, (size_t)length);
    send_all(connection, server->big_body, 1000);
    thread_sleep(20);
    send_all(connection, server->big_body + 1000, 1000);
    thread_sleep(20);
    send_all(connection, server->big_body + 2000, BIG_BODY_SIZE - 2000);

    /* Same connection, the server then closes it */
    if(!read_request(connection, server->requests[1], sizeof(server->requests[1])))
        goto end;

    {
        static const char reply[] = "HTTP/1.1 404 Not Found\r\nConnection: close\r\nContent-Length: 5\r\n\r\nnope!";
        send_all(connection, reply, sizeof(reply) - 1);
    }
    socket_shutdown(connection, SHUTDOWN_ALL);
    socket_free(connection);

    /* The client reconnects, body without length is read until the connection closes */
    connection = socket_accept(server->listener, NULL, NULL);

    if(connection == ROMANO_INVALID_SOCKET)
        return NULL;

    socket_set_timeout(connection, 5000);

    if(!read_request(connection, server->requests[2], sizeof(server->requests[2])))
        goto end;

    {
        static const char reply[] = "HTTP/1.1 201 Created\r\nX-Custom: value\r\n\r\nuntil close";
        send_all(connection, reply, sizeof(reply) - 1);
    }
    socket_shutdown(connection, SHUTDOWN_SEND);

end:
    socket_free(connection);
    return NULL;
}

static bool start_server(LocalServer* server)
{
    SockAddrIn addr;
    SockLen addr_size = sizeof(addr);
    size_t i;

    memset(server, 0, sizeof(LocalServer));

    for(i = 0; i < BIG_BODY_SIZE; i++)
        server->big_body[i] = (char)('a' + i % 26);

    server->listener = socket_new(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(server->listener == ROMANO_INVALID_SOCKET)
        return false;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    socket_inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if(socket_bind(server->listener, (SockAddr*)&addr, sizeof(addr)) == ROMANO_SOCKET_ERROR ||
       getsockname(server->listener, (SockAddr*)&addr, &addr_size) != 0 ||
       socket_listen(server->listener, 2) == ROMANO_SOCKET_ERROR)
    {
        socket_free(server->listener);
        return false;
    }

    server->port = ntohs(addr.sin_port);

    return true;
}

static size_t count_occurrences(const char* haystack, const char* needle)
{
    size_t count = 0;

    while((haystack = strstr(haystack, needle)) != NULL)
    {
        count++;
        haystack++;
    }

    return count;
}

static void test_headers(void)
{
    HTTPHeader header;
    HTTPHeaderIterator iterator;
    HTTPHeaderEntry* entry;
    size_t count = 0;

    TEST_ASSERT(http_header_init(&header));

    http_header_add_entry(&header, "Accept", 6, "*/*", 3);
    http_header_add_entry(&header, "User-Agent", 0, "libromano", 0);
    http_header_add_entry(&header, "X-Trimmed-Key", 7, "value-trimmed", 5);

    TEST_ASSERT(http_header_find(&header, "Accept") != NULL);
    TEST_CHECK_EQ_STR(http_header_find(&header, "Accept")->value, "*/*");
    TEST_CHECK_EQ_STR(http_header_find(&header, "User-Agent")->value, "libromano");
    TEST_ASSERT(http_header_find(&header, "X-Trimm") != NULL);
    TEST_CHECK_EQ_STR(http_header_find(&header, "X-Trimm")->value, "value");
    TEST_CHECK(http_header_find(&header, "Missing") == NULL);

    http_header_iterator_init(&iterator);

    while((entry = http_header_get_next(&header, &iterator)) != NULL)
        count++;

    TEST_CHECK_EQ_UINT(count, 3);

    http_header_remove_entry(&header, "Accept");
    http_header_remove_entry(&header, "X-Trimm");
    http_header_remove_entry(&header, "Missing");
    TEST_CHECK(http_header_find(&header, "Accept") == NULL);

    http_header_add_entry(&header, "After", 5, "removal", 7);
    TEST_CHECK(http_header_find(&header, "After") != NULL);

    count = 0;
    http_header_iterator_init(&iterator);

    while((entry = http_header_get_next(&header, &iterator)) != NULL)
        count++;

    TEST_CHECK_EQ_UINT(count, 2);

    http_header_release(&header);
}

static void test_requests_against_local_server(void)
{
    LocalServer server;
    HTTPContext context;
    HTTPRequest request;
    HTTPResponse response;
    Thread* thread;
    char host[16] = "127.0.0.1";

    TEST_ASSERT(socket_context_init());
    TEST_ASSERT(start_server(&server));

    thread = thread_create(serve, &server);
    thread_start(thread);

    TEST_ASSERT(http_context_init(&context, host, server.port));
    TEST_CHECK(http_context_is_alive(&context));

    TEST_ASSERT(http_request_init(&request, HTTPVersion_1_1, HTTPMethod_GET, "/get"));
    http_header_add_entry(&request.headers, "User-Agent", 10, "zclient", 7);
    http_header_add_entry(&request.headers, "Accept", 6, "*/*", 3);

    TEST_ASSERT(http_response_init(&response));
    TEST_ASSERT(http_context_send_request(&context, &request, &response));
    TEST_CHECK_EQ_INT(response.code, 200);
    TEST_CHECK_EQ_UINT(response.content_sz, BIG_BODY_SIZE);
    TEST_CHECK_EQ_MEM(response.content, server.big_body, BIG_BODY_SIZE);
    TEST_ASSERT(http_header_find(&response.headers, "Content-Type") != NULL);
    TEST_CHECK_EQ_STR(http_header_find(&response.headers, "Content-Type")->value, "text/plain");
    TEST_CHECK(http_context_is_alive(&context));
    http_response_release(&response);

    TEST_ASSERT(http_response_init(&response));
    TEST_ASSERT(http_context_send_request(&context, &request, &response));
    TEST_CHECK_EQ_INT(response.code, 404);
    TEST_CHECK_EQ_UINT(response.content_sz, 5);
    TEST_CHECK_EQ_STR(response.content, "nope!");
    TEST_CHECK(!http_context_is_alive(&context));
    http_response_release(&response);
    http_request_release(&request);

    TEST_ASSERT(http_request_init(&request, HTTPVersion_1_1, HTTPMethod_POST, "/items"));
    TEST_ASSERT(http_response_init(&response));
    TEST_ASSERT(http_context_send_request(&context, &request, &response));
    TEST_CHECK_EQ_INT(response.code, 201);
    TEST_CHECK_EQ_STR(response.content, "until close");
    TEST_ASSERT(http_header_find(&response.headers, "X-Custom") != NULL);
    TEST_CHECK_EQ_STR(http_header_find(&response.headers, "X-Custom")->value, "value");
    http_response_release(&response);
    http_request_release(&request);

    http_context_release(&context);
    thread_join(thread);
    socket_free(server.listener);

    TEST_CHECK(strncmp(server.requests[0], "GET /get HTTP/1.1\r\n", 19) == 0);
    TEST_CHECK(strstr(server.requests[0], "\r\nUser-Agent: zclient\r\n") != NULL);
    TEST_CHECK(strstr(server.requests[0], "\r\nHost: 127.0.0.1\r\n") != NULL);
    TEST_CHECK_EQ_UINT(count_occurrences(server.requests[1], "Host:"), 1);
    TEST_CHECK(strncmp(server.requests[2], "POST /items HTTP/1.1\r\n", 22) == 0);

    socket_context_release();
}

static void test_unreachable_host(void)
{
    HTTPContext context;

    TEST_ASSERT(socket_context_init());
    TEST_CHECK(!http_context_init(&context, "does-not-exist.invalid", 80));
    socket_context_release();
}

TEST_MAIN(
    TEST(test_headers),
    TEST(test_requests_against_local_server),
    TEST(test_unreachable_host),
)

#else

static void test_http_disabled(void)
{
    logger_log_info("libromano was built without HTTP support, nothing to test");
}

TEST_MAIN(
    TEST(test_http_disabled),
)

#endif /* ROMANO_HTTP */
