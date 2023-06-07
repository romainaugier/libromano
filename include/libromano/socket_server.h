/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

struct socket_server;

typedef struct socket_server* socket_server_t;

typedef void* (*socket_server_callback_func)(char* msg);

ROMANO_API socket_server_t socket_server_init(void);

ROMANO_API void socket_server_start(socket_server_t socket_server);

ROMANO_API void socket_server_stop(socket_server_t socket_server);

ROMANO_API void socket_server_release(socket_server_t socket_server);

ROMANO_CPP_END