/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/socket_server.h"
#include "libromano/socket.h"
#include "libromano/thread.h"
#include "libromano/logger.h"
#include "libromano/time.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#if !defined(ROMANO_SOCKET_SERVER_LOG_BUFFER_SIZE)
#define ROMANO_SOCKET_SERVER_LOG_BUFFER_SIZE 1024
#endif

#define GET_CALLBACKS_COUNT(callbacks_ptr) *(size_t*)callbacks_ptr
#define SET_CALLBACKS_COUNT(callbacks_ptr, count) *(size_t*)callbacks_ptr = count

#define GET_CALLBACK_PTR(callbacks_ptr, i) ((socket_server_callback_func*)((char*)callbacks_ptr + 1 * sizeof(size_t)))[i]

#define RECV_SIZE 128 
#define SEND_SIZE 32

struct socket_server
{
    socket_server_callback_func* m_callbacks;

    socket_server_log_callback m_log_callback;

    mutex_t* m_mutex;

    thread_t* m_thread;

    uint32_t m_flags;
    int32_t m_error;

    uint16_t m_port;
    uint16_t m_max_connections;
};

socket_server_t* socket_server_init(const uint16_t port,
                                               const uint16_t max_connections,
                                               const socket_server_flags ip_mode)
{
    socket_server_t* socket_server = malloc(sizeof(struct socket_server));

    socket_server->m_callbacks = NULL;
    socket_server->m_log_callback = NULL;
    socket_server->m_flags = 0;
    socket_server->m_port = port;
    socket_server->m_max_connections= max_connections;
    socket_server->m_mutex = NULL;

    SET_FLAG(socket_server->m_flags, ip_mode);

    return socket_server;
}

void socket_server_set_log_callback(socket_server_t* socket_server,
                                               socket_server_log_callback callback)
{
    ROMANO_ASSERT(socket_server != NULL, "");

    socket_server->m_log_callback = callback;
}

void socket_server_log(socket_server_t* socket_server,
                       int32_t error_code,
                       const char* format,
                       ...)
{
    va_list args;
    char log_buffer[ROMANO_SOCKET_SERVER_LOG_BUFFER_SIZE];
    
    if(socket_server->m_log_callback != NULL)
    {
        va_start(args, format);
        snprintf(log_buffer, ROMANO_SOCKET_SERVER_LOG_BUFFER_SIZE, format, args);
        va_end(args);

        socket_server->m_log_callback(error_code, log_buffer);
    }

    if(error_code != 0)
    {
        socket_server->m_error = error_code;
    }
}

void* socket_server_main_loop(void* _socket_server)
{
    socket_t sock;
    sockaddr_in_t server;

    fd_set_t read_fds;
    timeval_t time_interval;

    socket_t new_connection;

    int32_t select_status;
    int32_t result;

    char* data_buffer;
    char temp_data_buffer[RECV_SIZE];
    uint32_t rec_data_size;

    char send_buffer[SEND_SIZE];
    int32_t sent_data_size;

    size_t i;

    socket_server_t* socket_server = (socket_server_t*)_socket_server;
    
    ROMANO_ASSERT(socket_server != NULL, "");
    
    sock = socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(sock == INVALID_SOCKET)
    {
        socket_server_log(socket_server, socket_get_error(), "Error during socket creation");
        return NULL;
    }

    socket_server_log(socket_server, 0, "Server socket created");

    socket_set_timeout(sock, 1);

    memset(&server, 0, sizeof(sockaddr_in_t));
    server.sin_family = AF_INET;
    server.sin_port = htons(socket_server->m_port);

    if(HAS_FLAG(socket_server->m_flags, SocketServer_IpMode_Any)) server.sin_addr.s_addr = INADDR_ANY;
    else server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(bind(sock, (sockaddr_t*)&server, sizeof(server)) == SOCKET_ERROR)
    {
        socket_destroy(sock);
        socket_server_log(socket_server, socket_get_error(), "Error during socket binding");
        return NULL;
    }

    socket_server_log(socket_server, 0, "Server socket bound to port");

    if(listen(sock, socket_server->m_max_connections) == SOCKET_ERROR)
    {
        socket_destroy(sock);
        socket_server_log(socket_server, socket_get_error(), "Error during socket listening");
        return NULL;
    }

    socket_server_log(socket_server, 0, "Socket started listening");

    memset(&time_interval, 0, sizeof(timeval_t));
    time_interval.tv_sec = 1;

    socket_server_log(socket_server, 0, "Socket server main loop started");

    mutex_lock(socket_server->m_mutex);
    SET_FLAG(socket_server->m_flags, SocketServer_IsRunning);
    mutex_unlock(socket_server->m_mutex);

    while(1)
    {
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);

        mutex_lock(socket_server->m_mutex);

        if(HAS_FLAG(socket_server->m_flags, SocketServer_NeedToStop))
        {
            socket_server_log(socket_server, 0, "Socket server main loop stopped");
            mutex_unlock(socket_server->m_mutex);
            break;
        }
        
        mutex_unlock(socket_server->m_mutex);

        select_status = select(sock + 1, &read_fds, NULL, NULL, &time_interval);

        if(select_status == 0 || select_status == SOCKET_ERROR)
        {
            continue;
        }

        new_connection = accept(sock, NULL, NULL);

        if(new_connection == INVALID_SOCKET)
        {
            socket_server_log(socket_server, socket_get_error(), "Cannot accept incoming connection");
            continue;
        }

        socket_set_timeout(new_connection, 1000);

        data_buffer = (char*)malloc(RECV_SIZE * sizeof(char));
        result = 0;
        rec_data_size = 0;

        while(1)
        {
            result = recv(new_connection, temp_data_buffer, RECV_SIZE, MSG_WAITALL);

            if(!result || data_buffer == NULL)
            {
                break;
            }

            memcpy(data_buffer + rec_data_size, temp_data_buffer, result);
            rec_data_size += result;

            if(result == RECV_SIZE)
            {
                data_buffer = (char*)realloc(data_buffer, rec_data_size + RECV_SIZE);

                if(data_buffer == NULL)
                {
                    socket_server_log(socket_server, 1, "Error during recv data buffer allocation"); 
                    break;
                }
            }
            else
            {
                memset(data_buffer + rec_data_size, 0, 1);
                break;
            }
        }

        if(data_buffer != NULL)
        {
            socket_server_log(socket_server,
                              0,
                              "Executing callbacks");

            for(i = 0; i < GET_CALLBACKS_COUNT(socket_server->m_callbacks); i++)
            {
                GET_CALLBACK_PTR(socket_server->m_callbacks, i)(data_buffer);
            }

            socket_server_log(socket_server, 0, "Sending client infos about the size of received packet");

            snprintf(send_buffer, SEND_SIZE, "%d", rec_data_size);

            sent_data_size = send(new_connection, (const char*)send_buffer, (int)strlen(send_buffer), 0);

            if(!sent_data_size)
            {
                socket_server_log(socket_server, socket_get_error(), "Error during data send back to the client");
            }

            thread_sleep(1);

            free(data_buffer);
        }

        socket_server_log(socket_server, 0, "Finished executing callbacks, removing connection");

        socket_destroy(new_connection);
    }

    socket_destroy(sock);

    mutex_lock(socket_server->m_mutex);
    UNSET_FLAG(socket_server->m_flags, SocketServer_IsRunning);
    mutex_unlock(socket_server->m_mutex);

    return NULL;
}

void socket_server_start(socket_server_t* socket_server)
{
    ROMANO_ASSERT(socket_server != NULL, "");

    if(HAS_FLAG(socket_server->m_flags, SocketServer_IsRunning))
    {
        socket_server_log(socket_server, 1, "Socket server is already running");
        return;
    }

    socket_server->m_mutex = mutex_new();
    mutex_init(socket_server->m_mutex);

    socket_server->m_thread = thread_create(socket_server_main_loop, (void*)socket_server);

    socket_server_log(socket_server, 0, "Starting socket server");

    thread_start(socket_server->m_thread);
}

void socket_server_push_callback(socket_server_t* socket_server,
                                            socket_server_callback_func callback)
{
    size_t callbacks_count;
    size_t alloc_size;

    ROMANO_ASSERT(socket_server != NULL, "");

    if(socket_server->m_callbacks == NULL)
    {
        callbacks_count = 0;
        alloc_size = sizeof(size_t) + 1 * sizeof(socket_server_callback_func);
        socket_server->m_callbacks = (socket_server_callback_func*)malloc(alloc_size);
        memset(socket_server->m_callbacks, 0, alloc_size);
    }
    else
    {
        callbacks_count = GET_CALLBACKS_COUNT(socket_server->m_callbacks);
        alloc_size = sizeof(size_t) + (callbacks_count + 1) * sizeof(socket_server_callback_func);

        socket_server->m_callbacks = (socket_server_callback_func*)realloc(socket_server->m_callbacks,
                                                                           alloc_size);
    }

    GET_CALLBACK_PTR(socket_server->m_callbacks, callbacks_count) = callback;

    SET_CALLBACKS_COUNT(socket_server->m_callbacks, callbacks_count + 1);

    socket_server_log(socket_server, 0, "Added a new callback to the socket server");
}

int32_t socket_server_get_last_error(socket_server_t* socket_server)
{
    int32_t error;

    ROMANO_ASSERT(socket_server != NULL, "");

    error = socket_server->m_error;
    socket_server->m_error = 0;
    return error;
}

int32_t socket_server_is_running(socket_server_t* socket_server)
{ 
    ROMANO_ASSERT(socket_server != NULL, "");
    
    return HAS_FLAG(socket_server->m_flags, SocketServer_IsRunning);
}

void socket_server_stop(socket_server_t* socket_server)
{
    ROMANO_ASSERT(socket_server != NULL, "");

    mutex_lock(socket_server->m_mutex);
    SET_FLAG(socket_server->m_flags, SocketServer_NeedToStop);
    mutex_unlock(socket_server->m_mutex);

    socket_server_log(socket_server, 0, "Stopping socket server");

    thread_join(socket_server->m_thread);

    mutex_lock(socket_server->m_mutex);
    UNSET_FLAG(socket_server->m_flags, SocketServer_NeedToStop);
    mutex_unlock(socket_server->m_mutex);

    mutex_free(socket_server->m_mutex);
}

void socket_server_release(socket_server_t* socket_server)
{
    ROMANO_ASSERT(socket_server != NULL, "");

    if(socket_server->m_callbacks != NULL)
    {
        free(socket_server->m_callbacks);
    }

    free(socket_server);
}
