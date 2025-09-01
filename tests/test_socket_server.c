/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/socket_server.h"
#include "libromano/thread.h"
#include "libromano/logger.h"
#include "libromano/socket.h"

#include <stdio.h>
#include <string.h>

#define CLIENT_MSG_SIZE 512
#define CLIENT_BUFFER_SIZE 32

void callback(char* buffer)
{
    logger_log(LogLevel_Info, buffer);
}

void log_callback(int32_t code, char* msg)
{
    const log_level level = code == 0 ? LogLevel_Info : LogLevel_Error;

    logger_log(level, "%s (%i)", msg, code);
}

int main(void)
{
    socket_server_t* socket_server;

    Socket client_socket;
    SockAddrIn client_addr;
    int32_t client_result;
    char client_msg[CLIENT_MSG_SIZE];
    char client_buffer[CLIENT_BUFFER_SIZE];

    uint32_t i;

    logger_init();
    logger_set_level(LogLevel_Debug),

    logger_log(LogLevel_Debug, "Starting socket server test");

    socket_init_ctx();

    socket_server = socket_server_init(50111, 2, SocketServer_IpMode_LocalHost);

    socket_server_set_log_callback(socket_server, log_callback);
    socket_server_push_callback(socket_server, callback);

    socket_server_start(socket_server);

    thread_sleep(1000);

    logger_log(LogLevel_Debug, "Starting test");

    for(i = 0; i < 2; i++)
    {
        memset(client_msg, 0, CLIENT_MSG_SIZE);
        snprintf(client_msg, CLIENT_MSG_SIZE, "Test message %d", i);

        client_socket = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        if(client_socket == INVALID_SOCKET)
        {
            logger_log(LogLevel_Warning, "Invalid socket (%d)", socket_get_error());
            continue;
        }

        memset(&client_addr, 0, sizeof(SockAddrIn));
        client_addr.sin_family = AF_INET;
        client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        client_addr.sin_port = htons(50111);

        client_result = connect(client_socket, (SockAddr*)&client_addr, sizeof(client_addr));

        if(client_result == SOCKET_ERROR)
        {
            logger_log(LogLevel_Warning, "Invalid connection (%d)", socket_get_error());
            socket_destroy(client_socket);
            continue;
        }

        client_result = send(client_socket, client_msg, (int)strlen(client_msg), 0);

        if(client_result == SOCKET_ERROR)
        {
            logger_log(LogLevel_Warning, "Error during data send (%d)", socket_get_error());
            socket_destroy(client_socket);
            continue;
        }

        client_result = shutdown(client_socket, SD_SEND);

        if(client_result == SOCKET_ERROR)
        {
            logger_log(LogLevel_Warning, "Error during connection shutdown (%d)", socket_get_error());
            socket_destroy(client_socket);
            continue;
        }

        memset(client_buffer, 0, CLIENT_BUFFER_SIZE);

        while(1)
        {
            client_result = recv(client_socket, client_buffer, CLIENT_BUFFER_SIZE, MSG_WAITALL);

            if(!client_result)
            {
                break;
            }

            logger_log(LogLevel_Info, "Received data : %s", client_buffer);
        }

        socket_destroy(client_socket);

        thread_sleep(50);
    }

    socket_server_stop(socket_server);

    socket_server_release(socket_server);

    socket_release_ctx();

    logger_release();

    return 0;
}