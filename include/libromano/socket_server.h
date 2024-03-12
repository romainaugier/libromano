/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_SOCKET_SERVER)
#define __LIBROMANO_SOCKET_SERVER

#include "libromano/libromano.h"
#include "libromano/flag.h"

#include <assert.h>

ROMANO_CPP_ENTER

typedef enum {
    SocketServer_NeedToStop = BIT_FLAG(0),
    SocketServer_IsRunning = BIT_FLAG(1),
    SocketServer_IpMode_Any = BIT_FLAG(2),
    SocketServer_IpMode_LocalHost = BIT_FLAG(3)
} socket_server_flags;

struct socket_server;

typedef struct socket_server socket_server_t;

typedef void (*socket_server_callback_func)(char* data);

typedef void (*socket_server_log_callback)(int32_t error_code, char* error_msg);

ROMANO_API socket_server_t* socket_server_init(const uint16_t port,
                                               const uint16_t max_connections,
                                               const socket_server_flags ip_mode);

/*
 * Function called whenever the socket server needs to log something.
 * The error code will be set to 0 for information logging, otherwise to the error level the server 
 * can get.
 * The callback function signature must be like this : 
 * void my_log_callback(int32_t code, char* msg)
 */
ROMANO_API void socket_server_set_log_callback(socket_server_t* socket_server,
                                               socket_server_log_callback error_callback);

ROMANO_API void socket_server_start(socket_server_t* socket_server);

ROMANO_API void socket_server_push_callback(socket_server_t* socket_server,
                                            socket_server_callback_func callback);

ROMANO_API int32_t socket_server_get_last_error(socket_server_t* socket_server);

ROMANO_API int32_t socket_server_is_running(socket_server_t* socket_server);

ROMANO_API void socket_server_stop(socket_server_t* socket_server);

ROMANO_API void socket_server_release(socket_server_t* socket_server);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_SOCKET_SERVER) */
