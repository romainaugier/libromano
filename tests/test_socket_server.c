/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/atomic.h"
#include "libromano/socket.h"
#include "libromano/socket_server.h"
#include "libromano/thread.h"

#if defined(ROMANO_WIN)
#include <WS2tcpip.h>
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
#include <arpa/inet.h>
#endif /* defined(ROMANO_WIN) */

#define SERVER_PORT 50111

static Atomic64 g_received_bytes = 0;
static Atomic32 g_callback_calls = 0;
static Atomic32 g_second_callback_calls = 0;

static void count_callback(char* data, size_t data_size)
{
    ROMANO_UNUSED(data);
    atomic_add_64(&g_received_bytes, (int64_t)data_size, MemoryOrder_SeqCst);
    atomic_add_32(&g_callback_calls, 1, MemoryOrder_SeqCst);
}

static void second_callback(char* data, size_t data_size)
{
    ROMANO_UNUSED(data);
    ROMANO_UNUSED(data_size);
    atomic_add_32(&g_second_callback_calls, 1, MemoryOrder_SeqCst);
}

static void log_callback(int32_t code, char* message)
{
    logger_log(code == 0 ? LogLevel_Debug : LogLevel_Warning, "socket server: %s (%d)", message, code);
}

static Socket connect_to_server(void)
{
    Socket client = socket_new(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SockAddrIn addr;

    if(client == ROMANO_INVALID_SOCKET)
        return client;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    socket_inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if(socket_connect(client, (SockAddr*)&addr, sizeof(addr)) == ROMANO_SOCKET_ERROR)
    {
        socket_free(client);
        return ROMANO_INVALID_SOCKET;
    }

    socket_set_timeout(client, 5000);

    return client;
}

/* Sends the payload and returns the size the server reports back, or -1 */
static long send_payload(size_t size, bool shutdown_send)
{
    Socket client = connect_to_server();
    char* payload;
    char reply[64];
    size_t reply_size = 0;
    ssize_t received;
    size_t i;

    if(client == ROMANO_INVALID_SOCKET)
        return -1;

    payload = (char*)malloc(size + 1);

    for(i = 0; i < size; i++)
        payload[i] = (char)('A' + i % 26);

    if(size > 0 && socket_send(client, payload, size, 0) != (ssize_t)size)
    {
        free(payload);
        socket_free(client);
        return -1;
    }

    free(payload);

    if(shutdown_send)
        socket_shutdown(client, SHUTDOWN_SEND);

    memset(reply, 0, sizeof(reply));

    while(reply_size < sizeof(reply) - 1 &&
          (received = socket_recv(client, reply + reply_size, sizeof(reply) - 1 - reply_size, 0)) > 0)
        reply_size += (size_t)received;

    socket_free(client);

    return reply_size > 0 ? strtol(reply, NULL, 10) : -1;
}

static bool wait_until_running(SocketServer* server)
{
    int i;

    for(i = 0; i < 500 && !socket_server_is_running(server); i++)
        thread_sleep(10);

    return socket_server_is_running(server);
}

static void test_server(void)
{
    static const size_t sizes[] = { 1, 100, 1023, 1024, 1025, 5000, 70000 };
    SocketServer* server;
    size_t expected_bytes = 0;
    size_t i;

    TEST_ASSERT(socket_context_init());

    server = socket_server_new(SERVER_PORT, 4, SocketServerFlags_IpMode_LocalHost);
    TEST_ASSERT(server != NULL);

    socket_server_set_log_callback(server, log_callback);
    TEST_CHECK(socket_server_push_callback(server, count_callback));
    TEST_CHECK(socket_server_push_callback(server, second_callback));

    TEST_CHECK(!socket_server_is_running(server));
    socket_server_start(server);
    TEST_ASSERT(wait_until_running(server));

    for(i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++)
    {
        TEST_CHECK_MSG(send_payload(sizes[i], true) == (long)sizes[i], "payload of %zu bytes", sizes[i]);
        expected_bytes += sizes[i];
    }

    TEST_CHECK_EQ_INT(atomic_load_32(&g_callback_calls, MemoryOrder_SeqCst), sizeof(sizes) / sizeof(sizes[0]));
    TEST_CHECK_EQ_INT(atomic_load_32(&g_second_callback_calls, MemoryOrder_SeqCst), sizeof(sizes) / sizeof(sizes[0]));
    TEST_CHECK_EQ_UINT(atomic_load_64(&g_received_bytes, MemoryOrder_SeqCst), expected_bytes);

    /* A client that never shuts down its side makes the server hit its receive timeout */
    TEST_CHECK(send_payload(10, false) >= 0);
    TEST_CHECK(send_payload(0, false) <= 0);
    TEST_CHECK(socket_server_is_running(server));
    TEST_CHECK_EQ_INT(send_payload(42, true), 42);

    socket_server_get_last_error(server);
    TEST_CHECK_EQ_INT(socket_server_get_last_error(server), 0);

    socket_server_stop(server);
    TEST_CHECK(!socket_server_is_running(server));
    socket_server_free(server);

    socket_context_release();
}

TEST_MAIN(
    TEST(test_server),
)
