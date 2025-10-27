/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/socket.h"
#include "libromano/common.h"
#include "libromano/logger.h"

#if defined(ROMANO_WIN)
static LONG g_init_count = 0;
#elif defined(ROMANO_LINUX)
#include <errno.h>
#else
#warning Sockets not defined on this platform
#endif /* defined(ROMANO_WIN) */

bool socket_init_ctx(void)
{
#if defined(ROMANO_WIN)
    WSADATA wsa_data;
    int result;

    if(InterlockedIncrement(&g_init_count) == 1)
    {
        result = WSAStartup(MAKEWORD(2, 2), &wsa_data);

        if(result != 0)
        {
            logger_log(LogLevel_Fatal, "WSAStartup failed : %d", result);
            return false;;
        }
    }
#endif /* defined(ROMANO_WIN) */

    return true;
}

Socket socket_new(int af, int type, int protocol)
{
    Socket s;
    s = (Socket)socket(af, type, protocol);
    return s == ROMANO_INVALID_SOCKET ? ROMANO_INVALID_SOCKET : s;
}

void socket_set_timeout(Socket socket, uint32_t ms)
{
#if defined(ROMANO_WIN)
    DWORD tv;

    tv = (DWORD)ms;

    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sc = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;

    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif /* defined(ROMANO_WIN) */
}

void socket_set_nonblocking(Socket s, bool enable)
{
#if defined(ROMANO_WIN)
    u_long mode = enable ? 1 : 0;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fnctl(s, F_GETFL, 0);

    if(flags == -1)
        return;

    if(enable)
        fcntl(s, F_SETFL, flags | O_NONBLOCK);
    else
        fcntl(s, F_SETFL, flags | ~O_NONBLOCK);
#endif /* defined(ROMANO_WIN) */
}

int socket_bind(Socket s, const SockAddr* addr, SockLen addrlen)
{
    return bind(s, addr, addrlen);
}

int socket_listen(Socket s, int backlog)
{
    return listen(s, backlog);
}

Socket socket_accept(Socket s, SockAddr* addr, SockLen* addrlen)
{
    Socket client = (Socket)accept(s, addr, addrlen);

    return client == ROMANO_INVALID_SOCKET ? ROMANO_INVALID_SOCKET : client;
}

int socket_connect(Socket s, const SockAddr* addr, SockLen addrlen)
{
    return connect(s, addr, addrlen);
}

int socket_shutdown(Socket s, int how)
{
    return shutdown(s, how);
}

size_t socket_send(Socket s, const void* buffer, size_t buffer_sz, int flags)
{
    return (size_t)send(s, buffer, (int)buffer_sz, flags);
}

size_t socket_recv(Socket s, void* buffer, size_t buffer_sz, int flags)
{
    return (size_t)recv(s, buffer, (int)buffer_sz, flags);
}

size_t socket_sendto(Socket s,
                     const void *buffer,
                     size_t buffer_sz,
                     int flags,
                     const SockAddr *dst,
                     SockLen dst_len)
{
    return (size_t)sendto(s, buffer, buffer_sz, flags, dst, dst_len);
}

size_t socket_recvfrom(Socket s,
                       void *buffer,
                       size_t buffer_sz,
                       int flags,
                       SockAddr *src,
                       SockLen *src_len)
{
    return (size_t)recvfrom(s, buffer, buffer_sz, flags, src, src_len);
}

int socket_select(int nfds,
                  FdSet *readfds,
                  FdSet *writefds,
                  FdSet *exceptfds,
                  struct timeval *timeout)
{
#if defined(ROMANO_WIN)
    ROMANO_UNUSED(nfds);
#endif /* defined(ROMANO_WIN) */

    return select(nfds, readfds, writefds, exceptfds, timeout);
}

int socket_inet_pton(int af, const char* src, void* dst)
{
#if defined(ROMANO_WIN)
    return InetPtonA(af, src, dst);
#else
    return inet_pton(af, src, dst);
#endif /* defined(ROMANO_WIN) */
}

const char* socket_inet_ntop(int af, const void* src, char* dst, SockLen len)
{
#if defined(ROMANO_WIN)
    struct sockaddr_in sin4;
    struct sockaddr_in6 sin6;
    DWORD sz = (DWORD)len;

    if(af == AF_INET)
    {
        memset(&sin4, 0, sizeof(sin4));
        sin4.sin_family = AF_INET;
        memcpy(&sin4.sin_addr, src, sizeof(struct in_addr));

        if(WSAAddressToStringA((LPSOCKADDR)&sin4, sizeof(sin4), NULL, dst, &sz) == 0)
            return dst;
    }
    else if(af == AF_INET6)
    {
        memset(&sin6, 0, sizeof(sin6));
        sin6.sin6_family = AF_INET6;
        memcpy(&sin6.sin6_addr, src, sizeof(struct in_addr));

        if(WSAAddressToStringA((LPSOCKADDR)&sin6, sizeof(sin6), NULL, dst, &sz) == 0)
            return dst;
    }

    return NULL;
#else
    return inet_ntop(af, src, dst, len);
#endif /* defined(ROMANO_WIN) */
}

int32_t socket_get_error()
{
#if defined(ROMANO_WIN)
    return WSAGetLastError();
#else
    return errno;
#endif
}

void socket_free(Socket s)
{
    if(s != ROMANO_INVALID_SOCKET)
        closesocket(s);
}

void socket_release_ctx(void)
{
#if defined(ROMANO_WIN)
    if(InterlockedDecrement(&g_init_count) == 0)
        WSACleanup();
#endif /* defined(ROMANO_WIN) */
}
