/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_SOCKET)
#define __LIBROMANO_SOCKET

#include "libromano/common.h"

ROMANO_CPP_ENTER

#if defined(ROMANO_WIN)
#pragma warning(disable:4820)
#define ROMANO_HAS_WINSOCK
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma warning(default:4820)
typedef SOCKET Socket;
typedef SOCKADDR_IN SockAddrIn;
typedef SOCKADDR SockAddr;
typedef IN_ADDR InAddr;
typedef FD_SET FdSet;
typedef int SockLen;
#define ROMANO_INVALID_SOCKET INVALID_SOCKET
#define ROMANO_SOCKET_ERROR SOCKET_ERROR
#elif defined(ROMANO_LINUX)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h>  /* gethostbyname */

#define ROMANO_INVALID_SOCKET (-1)
#define ROMANO_SOCKET_ERROR (-1)

#define closesocket(s) close(s)

#define SD_SEND SHUT_WR
#define SD_RECEIVE SHUT_RD
#define SD_BOTH SHUT_RDWR

typedef int Socket;
typedef struct sockaddr_in SockAddrIn;
typedef struct sockaddr SockAddr;
typedef struct in_addr InAddr;
typedef fd_set FdSet;
typedef socklen_t SockLen;
#endif /* defined(ROMANO_WIN) */

#if defined(ROMANO_ENABLE_IPV6)
#if defined(ROMANO_WIN)
typedef SOCKADDR_IN6 SockAddrIn6;
typedef SOCKADDR SockAddr6;
#elif defined(ROMANO_LINUX)
typedef struct sockaddr_in6 SockAddrIn6;
typedef struct sockaddr SockAddr6;
#endif /* defined(ROMANO_WIN) */
#endif /* defined(ROMANO_ENABLE_IPV6) */

/*
 * Initialize the socket context, and returns true on success
 */
ROMANO_API bool socket_init_ctx(void);

/*
 * Creates a new socket
 */
ROMANO_API Socket socket_new(int af, int type, int protocol);

/*
 * Sets the timeout in milliseconds on the given socket
 */
ROMANO_API void socket_set_timeout(Socket socket, uint32_t ms);

/*
 *
 */
ROMANO_API void socket_set_nonblocking(Socket s, bool enable);

/*
 *
 */
ROMANO_API int socket_bind(Socket s, const SockAddr *addr, SockLen addrlen);

/*
 *
 */
ROMANO_API int socket_listen(Socket s, int backlog);

/*
 *
 */
ROMANO_API Socket socket_accept(Socket s, SockAddr *addr, SockLen *addrlen);

/*
 *
 */
ROMANO_API int socket_connect(Socket s, const SockAddr *addr, SockLen addrlen);

/*
 *
 */
ROMANO_API int socket_shutdown(Socket s, int how);

/*
 *
 */
ROMANO_API size_t socket_send(Socket s, const void *buffer, size_t buffer_sz, int flags);

/*
 *
 */
ROMANO_API size_t socket_recv(Socket s, void *buffer, size_t buffer_sz, int flags);

/*
 *
 */
ROMANO_API size_t socket_sendto(Socket s,
                                const void *buffer,
                                size_t buffer_sz,
                                int flags,
                                const SockAddr *dst,
                                SockLen dst_len);

/*
 *
 */
ROMANO_API size_t socket_recvfrom(Socket s,
                                  void *buffer,
                                  size_t buffer_sz,
                                  int flags,
                                  SockAddr *src,
                                  SockLen *src_len);

/*
 *
 */
ROMANO_API int socket_select(int nfds,
                             FdSet *readfds,
                             FdSet *writefds,
                             FdSet *exceptfds,
                             struct timeval *timeout);

/*
 *
 */
ROMANO_API int socket_inet_pton(int af, const char *src, void *dst);

/*
 * Returns NULL on failure
 */
ROMANO_API const char *socket_inet_ntop(int af, const void *src, char *dst, SockLen len);

/*
 * Get last socket error (errno on Posix, WSAError on Windows)
 */
ROMANO_API int32_t socket_get_error(void);

/*
 * Deletes the given socket
 */
ROMANO_API void socket_free(Socket s);

/*
 * Release the socket context
 */
ROMANO_API void socket_release_ctx(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_SOCKET) */
