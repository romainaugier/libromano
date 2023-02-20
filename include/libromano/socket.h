// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#pragma once

#if !defined __LIBROMANO_SOCKET
#define __LIBROMANO_SOCKET

#include "libromano/libromano.h"

#if defined(ROMANO_WIN)
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
#elif defined(ROMANO_LINUX)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // close
#include <netdb.h>  // gethostbyname

#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)

typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockadd SOCKADDR;
typedef struct in_addr IN_ADDR;
#else
#warning Sockets not defined on this platform
#endif // defined(ROMANO_WIN)

ROMANO_CPP_ENTER

// Initialize the socket context
ROMANO_API void socket_init();

// Release the socket context
ROMANO_API void socket_release();

ROMANO_CPP_END

#endif // __LIBROMANO_SOCKET