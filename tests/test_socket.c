/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "test.h"

#include "libromano/socket.h"
#include "libromano/thread.h"

#if defined(ROMANO_WIN)
#include <WS2tcpip.h>
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
#include <arpa/inet.h>
#endif /* defined(ROMANO_WIN) */

static Socket bind_loopback(int type, uint16_t* port)
{
    Socket s = socket_new(AF_INET, type, type == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP);
    SockAddrIn addr;
    SockLen addr_size = sizeof(addr);

    if(s == ROMANO_INVALID_SOCKET)
        return s;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    socket_inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if(socket_bind(s, (SockAddr*)&addr, sizeof(addr)) == ROMANO_SOCKET_ERROR ||
       getsockname(s, (SockAddr*)&addr, &addr_size) != 0)
    {
        socket_free(s);
        return ROMANO_INVALID_SOCKET;
    }

    *port = ntohs(addr.sin_port);

    return s;
}

static SockAddrIn loopback_address(uint16_t port)
{
    SockAddrIn addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    socket_inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    return addr;
}

static void test_address_conversions(void)
{
    InAddr ipv4;
    unsigned char ipv6[16];
    char buffer[64];

    TEST_CHECK_EQ_INT(socket_inet_pton(AF_INET, "192.168.1.42", &ipv4), 1);
    TEST_CHECK(socket_inet_ntop(AF_INET, &ipv4, buffer, sizeof(buffer)) != NULL);
    TEST_CHECK_EQ_STR(buffer, "192.168.1.42");

    TEST_CHECK_EQ_INT(socket_inet_pton(AF_INET6, "::1", ipv6), 1);
    TEST_CHECK(socket_inet_ntop(AF_INET6, ipv6, buffer, sizeof(buffer)) != NULL);
    TEST_CHECK_EQ_STR(buffer, "::1");

    TEST_CHECK_EQ_INT(socket_inet_pton(AF_INET, "256.1.1.1", &ipv4), 0);
    TEST_CHECK_EQ_INT(socket_inet_pton(AF_INET, "not an ip", &ipv4), 0);
    TEST_CHECK(socket_inet_ntop(AF_INET, &ipv4, buffer, 4) == NULL);
}

typedef struct EchoServer {
    Socket listener;
    size_t received;
} EchoServer;

static void* echo_once(void* arg)
{
    EchoServer* server = (EchoServer*)arg;
    Socket connection = socket_accept(server->listener, NULL, NULL);
    char buffer[4096];
    ssize_t received;

    if(connection == ROMANO_INVALID_SOCKET)
        return NULL;

    socket_set_timeout(connection, 5000);

    while((received = socket_recv(connection, buffer, sizeof(buffer), 0)) > 0)
    {
        server->received += (size_t)received;
        socket_send(connection, buffer, (size_t)received, 0);
    }

    socket_shutdown(connection, SHUTDOWN_ALL);
    socket_free(connection);

    return NULL;
}

static void test_tcp_echo(void)
{
    EchoServer server = { ROMANO_INVALID_SOCKET, 0 };
    char message[3000];
    char echoed[3000];
    size_t total = 0;
    uint16_t port = 0;
    SockAddrIn addr;
    Thread* thread;
    Socket client;
    size_t i;

    for(i = 0; i < sizeof(message); i++)
        message[i] = (char)('a' + i % 26);

    server.listener = bind_loopback(SOCK_STREAM, &port);
    TEST_ASSERT(server.listener != ROMANO_INVALID_SOCKET);
    TEST_ASSERT(socket_listen(server.listener, 4) != ROMANO_SOCKET_ERROR);

    thread = thread_create(echo_once, &server);
    thread_start(thread);

    client = socket_new(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    TEST_ASSERT(client != ROMANO_INVALID_SOCKET);
    socket_set_timeout(client, 5000);

    addr = loopback_address(port);
    TEST_ASSERT(socket_connect(client, (SockAddr*)&addr, sizeof(addr)) != ROMANO_SOCKET_ERROR);

    TEST_CHECK_EQ_INT(socket_send(client, message, sizeof(message), 0), sizeof(message));
    TEST_CHECK(socket_shutdown(client, SHUTDOWN_SEND) != ROMANO_SOCKET_ERROR);

    while(total < sizeof(echoed))
    {
        ssize_t received = socket_recv(client, echoed + total, sizeof(echoed) - total, 0);

        if(received <= 0)
            break;

        total += (size_t)received;
    }

    TEST_CHECK_EQ_UINT(total, sizeof(message));
    TEST_CHECK_EQ_MEM(echoed, message, sizeof(message));

    socket_free(client);
    thread_join(thread);
    socket_free(server.listener);

    TEST_CHECK_EQ_UINT(server.received, sizeof(message));
}

static void test_udp(void)
{
    uint16_t port_a = 0;
    uint16_t port_b = 0;
    Socket a = bind_loopback(SOCK_DGRAM, &port_a);
    Socket b = bind_loopback(SOCK_DGRAM, &port_b);
    SockAddrIn to;
    SockAddrStorage from;
    SockLen from_size = sizeof(from);
    char buffer[64];
    FdSet read_set;
    struct timeval timeout;

    TEST_ASSERT(a != ROMANO_INVALID_SOCKET && b != ROMANO_INVALID_SOCKET);

    to = loopback_address(port_b);
    TEST_CHECK_EQ_INT(socket_sendto(a, "datagram", 8, 0, (SockAddr*)&to, sizeof(to)), 8);

    FD_ZERO(&read_set);
    FD_SET(b, &read_set);
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    TEST_CHECK_EQ_INT(socket_select((int)b + 1, &read_set, NULL, NULL, &timeout), 1);

    memset(buffer, 0, sizeof(buffer));
    TEST_CHECK_EQ_INT(socket_recvfrom(b, buffer, sizeof(buffer), 0, (SockAddr*)&from, &from_size), 8);
    TEST_CHECK_EQ_STR(buffer, "datagram");
    TEST_CHECK_EQ_UINT(ntohs(((SockAddrIn*)&from)->sin_port), port_a);

    socket_set_nonblocking(b, true);
    TEST_CHECK(socket_recv(b, buffer, sizeof(buffer), 0) < 0);
    socket_set_nonblocking(b, false);

    socket_free(a);
    socket_free(b);
}

static void test_connection_refused(void)
{
    uint16_t port = 0;
    Socket placeholder = bind_loopback(SOCK_STREAM, &port);
    Socket client;
    SockAddrIn addr;

    TEST_ASSERT(placeholder != ROMANO_INVALID_SOCKET);

    /* Bound but not listening: connecting must fail */
    client = socket_new(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    addr = loopback_address(port);

    TEST_CHECK(socket_connect(client, (SockAddr*)&addr, sizeof(addr)) == ROMANO_SOCKET_ERROR);
    TEST_CHECK(socket_get_error() != 0);

    socket_free(client);
    socket_free(placeholder);
}

static void test_dns(void)
{
    DNSResolveResult result;
    char address[64];
    size_t i;
    bool found_loopback = false;

    socket_dns_result_init(&result);
    TEST_CHECK_EQ_UINT(socket_dns_result_get_count(&result), 0);

    TEST_ASSERT(socket_resolve_dns_ipv4("localhost", &result));
    TEST_ASSERT(socket_dns_result_get_count(&result) > 0);

    for(i = 0; i < socket_dns_result_get_count(&result); i++)
    {
        socket_addr_to_string(socket_dns_result_get(&result, i), address, sizeof(address));
        logger_log_debug("localhost -> %s", address);
        found_loopback |= strncmp(address, "127.", 4) == 0;
    }

    TEST_CHECK(found_loopback);
    socket_dns_result_release(&result);

    socket_dns_result_init(&result);

    if(socket_resolve_dns_ipv6("localhost", &result))
    {
        socket_addr_to_string(socket_dns_result_get(&result, 0), address, sizeof(address));
        logger_log_debug("localhost (ipv6) -> %s", address);
    }

    socket_dns_result_release(&result);

    socket_dns_result_init(&result);
    TEST_CHECK(!socket_resolve_dns_ipv4("does-not-exist.invalid", &result));
    socket_dns_result_release(&result);

    if(getenv("ROMANO_TESTS_NETWORK") != NULL)
    {
        socket_dns_result_init(&result);
        TEST_CHECK(socket_resolve_dns_ipv4("example.com", &result));
        socket_dns_result_release(&result);
    }
}

#define WITH_SOCKET_CONTEXT(func)                   \
    static void func##_with_context(void)           \
    {                                               \
        TEST_ASSERT(socket_context_init());         \
        func();                                     \
        socket_context_release();                   \
    }

WITH_SOCKET_CONTEXT(test_tcp_echo)
WITH_SOCKET_CONTEXT(test_udp)
WITH_SOCKET_CONTEXT(test_connection_refused)
WITH_SOCKET_CONTEXT(test_dns)

TEST_MAIN(
    TEST(test_address_conversions),
    TEST(test_tcp_echo_with_context),
    TEST(test_udp_with_context),
    TEST(test_connection_refused_with_context),
    TEST(test_dns_with_context),
)
