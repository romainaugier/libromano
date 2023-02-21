// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#include "libromano/thread.h"
#include "libromano/libromano.h"

#include <stdlib.h>

#if defined(ROMANO_WIN)
#include <Windows.h>
typedef HANDLE thread_handle;
typedef DWORD thread_id;
#elif defined(ROMANO_LINUX)
#include <pthread.h>
#include <unistd.h>
typedef pthread_t thread_handle;
#endif // defined(ROMANO_WIN)

struct _thread {
    thread_handle _thread_handle;
    thread_id _id;
};

thread* thread_create(thread_func func, void* arg)
{
    thread* new_thread = (thread*)malloc(sizeof(thread));

    memset(new_thread, 0, sizeof(thread));

#if defined(ROMANO_WIN)
    new_thread->_thread_handle = CreateThread(NULL,
                                              0,
                                              (LPTHREAD_START_ROUTINE)func,
                                              arg,
                                              CREATE_SUSPENDED,
                                              &new_thread->_id);
#endif // defined(ROMANO_WIN)

    return new_thread;
}

void thread_start(thread* thread)
{
#if defined(ROMANO_WIN)
    ResumeThread(thread->_thread_handle);
#elif defined(ROMANO_LINUX)
    pthread_create(&new_thread->_thread_handle,
                   NULL,
                   func,
                   arg)
#endif // defined(ROMANO_WIN)
}

void thread_sleep(int milliseconds)
{
#if defined(ROMANO_WIN)
    Sleep((DWORD)milliseconds);
#elif defined(ROMANO_LINUX)
    nanosleep(milliseconds * 1000000);
#endif // defined(ROMANO_WIN)
}

void thread_join(thread* thread)
{
    if(thread == NULL)
    {
        return;
    }

#if defined(ROMANO_WIN)
    WaitForSingleObject(thread->_thread_handle, INFINITE);
    CloseHandle(thread->_thread_handle);
#elif defined(ROMANO_LINUX)
    pthread_join(thread->_thread_handle, NULL);
#endif // defined(ROMANO_WIN)

    free(thread);
}