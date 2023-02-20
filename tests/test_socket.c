// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#include "libromano/socket.h"
#include "libromano/logger.h"

#define MAX_CONNECTIONS 5

int main(int argc, char** argv)
{
    socket_init();
    
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(sock == INVALID_SOCKET)
    {
        logger_log(LogLevel_Fatal, "Cannot create socket (%d)", GetLastError());
        socket_release();
        return 1;
    }

    SOCKADDR_IN server = { 0 };
    server.sin_family = AF_INET;
    server.sin_port = htons(45666);
    server.sin_addr.s_addr = INADDR_ANY;

    if(bind(sock, (SOCKADDR*)&server, sizeof(server)) == SOCKET_ERROR)
    {
        logger_log(LogLevel_Fatal, "Cannot bind socket (%d)", GetLastError());
        closesocket(sock);
        socket_release();
        return 1;
    }

    if(listen(sock, MAX_CONNECTIONS) == SOCKET_ERROR)
    {
        logger_log(LogLevel_Fatal, "Cannot listen socket (%d)", GetLastError());
        closesocket(sock);
        socket_release();
        return 1;
    }

    uint32_t end_server = 0;

    while(end_server == 0)
    {
        SOCKET new_connection = accept(sock, NULL, NULL);

        if(new_connection == INVALID_SOCKET)
        {
            logger_log(LogLevel_Error, "Cannot accept socket (%d)", GetLastError());
            continue;
        }

        while(1)
        {
            char reception_buffer[512];

            int32_t result = recv(new_connection, reception_buffer, 512, 0);

            if(result > 0)
            {
                logger_log(LogLevel_Info, "Receiving data : %s", reception_buffer);
                
                if(strcmp(reception_buffer, "stop") == 0)
                {
                    end_server = 1;
                    logger_log(LogLevel_Info, "Stopping server");
                }
            }
            else if(result == 0)
            {
                logger_log(LogLevel_Info, "Connection closing");
                break;
            }
            else
            {
                logger_log(LogLevel_Error, "Failed to receive data (%d)", GetLastError());
                break;
            }
        }

        closesocket(new_connection);
    }

    closesocket(sock);

    return 0;
}