/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_SOCKET)
#define __LIBROMANO_SOCKET

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

#if defined(ROMANO_WIN)
#pragma warning(disable:4820)
#define ROMANO_HAS_WINSOCK
#include <WinSock2.h>
#pragma warning(default:4820)
typedef SOCKET socket_t;
typedef SOCKADDR_IN sockaddr_in_t;
typedef SOCKADDR sockaddr_t;
typedef IN_ADDR inaddr_t;
typedef FD_SET fd_set_t;
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
#define FD_SEND SHUT_WR

typedef int socket_t;
typedef struct sockaddr_in sockaddr_in_t;
typedef struct sockaddr sockaddr_t;
typedef struct in_addr inaddr_t;
typedef fd_set fd_set_t;
#endif /* defined(ROMANO_WIN) */

/* Initialize the socket context */
ROMANO_API void socket_init_ctx(void);

/* Creates a new socket */
ROMANO_API socket_t socket_create(int af, int type, int protocol);

/* Sets the timeout in milliseconds on the given socket */
ROMANO_API void socket_set_timeout(socket_t socket, unsigned int timeout);

ROMANO_FORCE_INLINE static int32_t socket_get_error(void) 
{  
#if defined(ROMANO_WIN)
    return WSAGetLastError();
#else
    return 0;
#endif
}

/* Deletes the given socket */
ROMANO_API void socket_destroy(socket_t socket);

/* Release the socket context */
ROMANO_API void socket_release_ctx(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_SOCKET) */
