/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/socket.h"
#include "libromano/logger.h"

#include <stdlib.h>

#if defined(ROMANO_WIN)
#elif defined(ROMANO_LINUX)
#else
#warning Sockets not defined on this platform
#endif /* defined(ROMANO_WIN) */

void socket_init_ctx(void)
{
#if defined(ROMANO_WIN)
    /* Used to call WS2_32.dll */
    WSADATA wsa_data;
    int result;
    
    result = WSAStartup(MAKEWORD(2, 2), &wsa_data);

    if(result != 0)
    {
        logger_log(LogLevel_Fatal, "WSAStartup failed : %d", result);
        exit(1);
    }

    logger_log(LogLevel_Debug, "WSAStartup done");
#endif /* defined(ROMANO_WIN) */
}

Socket socket_create(int af, int type, int protocol)
{
    return socket(af, type, protocol);
}

void socket_set_timeout(Socket socket, unsigned int timeout)
{
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
}

void socket_destroy(Socket socket)
{
#if defined(ROMANO_WIN)
    closesocket(socket);
#elif defined(ROMANO_LINUX)
    close(socket);
#endif /* defined(ROMANO_WIN) */
}

void socket_release_ctx(void)
{
#if defined(ROMANO_WIN)
    int result = WSACleanup();

    if(result != 0)
    {
        logger_log(LogLevel_Fatal, "WSACleanup failed : %d", result);
        exit(1);
    }

    logger_log(LogLevel_Debug, "WSACleanup done");
#endif /* defined(ROMANO_WIN) */
}

