/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#pragma once

#if !defined(__LIBROMANO_SOCKET_SERVER)
#define __LIBROMANO_SOCKET_SERVER

#include "libromano/common.h"
#include "libromano/flag.h"

#include <assert.h>

ROMANO_CPP_ENTER

typedef enum {
    SocketServerFlags_NeedToStop = BIT_FLAG(0),
    SocketServerFlags_IsRunning = BIT_FLAG(1),
    SocketServerFlags_IpMode_Any = BIT_FLAG(2),
    SocketServerFlags_IpMode_LocalHost = BIT_FLAG(3)
} SocketServerFlags;

struct SocketServer;

typedef struct SocketServer SocketServer;

typedef void (*socket_server_callback_func)(char* data, size_t data_sz);

typedef void (*socket_server_log_callback)(int32_t error_code, char* error_msg);

/*
 * Creates a new heap-allocated socket server. Returns NULL on failure
 */
ROMANO_API SocketServer* socket_server_new(uint16_t port,
                                           uint16_t max_connections,
                                           SocketServerFlags ip_mode);

/*
 * Function called whenever the socket server needs to log something.
 * The error code will be set to 0 for information logging, otherwise to the error level the server 
 * can get.
 * The callback function signature must be like this : 
 * void my_log_callback(int32_t code, char* msg)
 */
ROMANO_API void socket_server_set_log_callback(SocketServer* socket_server,
                                               socket_server_log_callback error_callback);

/*
 * Starts the socket server
 */
ROMANO_API void socket_server_start(SocketServer* socket_server);

/*
 * Pushes a new callback to the socket server that will be executed
 * when the socket server receives data
 */
ROMANO_API bool socket_server_push_callback(SocketServer* socket_server,
                                            socket_server_callback_func callback);

/*
 * Returns the last error caught in the socket server
 */
ROMANO_API int32_t socket_server_get_last_error(SocketServer* socket_server);

/*
 * Returns true if the socket server is currently running
 */
ROMANO_API bool socket_server_is_running(SocketServer* socket_server);

/*
 * Stops the socket server
 */
ROMANO_API void socket_server_stop(SocketServer* socket_server);

/*
 * Stops the socket server and frees it
 */
ROMANO_API void socket_server_free(SocketServer* socket_server);

ROMANO_CPP_END

#endif /* !defined(__LIBROMANO_SOCKET_SERVER) */
