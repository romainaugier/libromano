/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/socket_server.h"
#include "libromano/socket.h"

#include <stdlib.h>

#define GET_CALLBACKS_COUNT(callbacks_ptr) (uint8_t)*callbacks_ptr
#define SET_CALLBACKS_COUNT(callbacks_ptr, count) (uint8_t)*callbacks_ptr = count

#define GET_CALLBACKS_PTR(callbacks_ptr) (char*)callbacks_ptr + 1

struct socket_server
{
    socket_server_callback_func* m_callbacks;

    uint32_t m_flags;
};

ROMANO_API socket_server_t socket_server_init(void)
{
    socket_server_t socket_server = malloc(sizeof(struct socket_server));

    socket_server->m_callbacks = NULL;
    socket_server->m_flags = 0;

    return socket_server;
}

ROMANO_API void socket_server_start(socket_server_t socket_server)
{

}