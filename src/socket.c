// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#include "libromano/socket.h"
#include "libromano/logger.h"

void socket_init()
{
#if defined(ROMANO_WIN)
    // Used to call WS2_32.dll
    WSADATA wsa_data;

    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);

    if(result != 0)
    {
        logger_log(LogLevel_Fatal, "WSAStartup failed : %d", result);
        exit(1);
    }

    logger_log(LogLevel_Debug, "WSAStartup done");
#endif // defined(ROMANO_WIN)
}

void socket_release()
{
#if defined(ROMANO_WIN)
    int result = WSACleanup();

    if(result != 0)
    {
        logger_log(LogLevel_Fatal, "WSACleanup failed : %d", result);
        exit(1);
    }

    logger_log(LogLevel_Debug, "WSACleanup done");
#endif // defined(ROMANO_WIN)
}