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
#pragma warning(default:4820)
typedef SOCKET Socket;
typedef SOCKADDR_IN SockAddrIn;
typedef SOCKADDR SockAddr;
typedef IN_ADDR InAddr;
typedef FD_SET FdSet;
#elif defined(ROMANO_LINUX)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h>  /* gethostbyname */

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
#define SD_SEND SHUT_WR
#define SD_RECEIVE SHUT_RD
#define SD_BOTH SHUT_RDWR

typedef int Socket;
typedef struct sockaddr_in SockAddrIn;
typedef struct sockaddr SockAddr;
typedef struct in_addr InAddr;
typedef fd_set FdSet;
#endif /* defined(ROMANO_WIN) */

/* Initialize the socket context */
ROMANO_API void socket_init_ctx(void);

/* Creates a new socket */
ROMANO_API Socket socket_create(int af, int type, int protocol);

/* Sets the timeout in milliseconds on the given socket */
ROMANO_API void socket_set_timeout(Socket socket, unsigned int timeout);

ROMANO_FORCE_INLINE static int32_t socket_get_error(void) 
{  
#if defined(ROMANO_WIN)
    return WSAGetLastError();
#else
    return 0;
#endif
}

/* Deletes the given socket */
ROMANO_API void socket_destroy(Socket socket);

/* Release the socket context */
ROMANO_API void socket_release_ctx(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_SOCKET) */
