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
#include <WinSock2.h>
#pragma warning(default:4820)
typedef SOCKET socket_t;
#elif defined(ROMANO_LINUX)
typedef int socket_t;
#endif /* defined(ROMANO_WIN) */

/* Initialize the socket context */
ROMANO_API void socket_init_ctx(void);

/* Creates a new socket */
ROMANO_API socket_t socket_create(int af, int type, int protocol);

/* Sets the timeout in milliseconds on the given socket */
ROMANO_API void socket_set_timeout(socket_t socket, unsigned int timeout);

/* Deletes the given socket */
ROMANO_API void socket_destroy(socket_t socket);

/* Release the socket context */
ROMANO_API void socket_release_ctx(void);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_SOCKET) */

