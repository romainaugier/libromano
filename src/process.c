/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/process.h"
#include "libromano/memory.h"
#include "libromano/error.h"

#include <string.h>
#include <stdlib.h>

#if defined(ROMANO_WIN)
#include <Windows.h>
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE)
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <pthread.h>
#if defined(ROMANO_APPLE)
#include <crt_externs.h>
#else
extern char** environ;
#endif /* defined(ROMANO_APPLE) */
#endif /* defined(ROMANO_WIN) */

extern ErrorCode g_current_error;

typedef struct ProcessBuffer {
    char* data;
    size_t size;
    size_t capacity;
} ProcessBuffer;

static bool process_buffer_append(ProcessBuffer* buffer, const char* data, size_t size)
{
    if(buffer->size + size + 1 > buffer->capacity)
    {
        size_t new_capacity = buffer->capacity == 0 ? 4096 : buffer->capacity;
        char* new_data;

        while(buffer->size + size + 1 > new_capacity)
            new_capacity *= 2;

        new_data = (char*)romano_realloc(buffer->data, new_capacity);

        if(new_data == NULL)
        {
            g_current_error = ErrorCode_MemAllocError;
            return false;
        }

        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }

    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    buffer->data[buffer->size] = '\0';

    return true;
}

static void process_buffer_finish(ProcessBuffer* buffer, char** out, size_t* out_sz)
{
    if(buffer->data == NULL)
    {
        buffer->data = (char*)romano_calloc(1, 1);
    }

    *out = buffer->data;
    *out_sz = buffer->size;
}

#if defined(ROMANO_LINUX) || defined(ROMANO_APPLE)

static void process_close_fd(int* fd)
{
    if(*fd >= 0)
    {
        close(*fd);
        *fd = -1;
    }
}

static int64_t process_now_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int process_remaining_ms(int64_t deadline)
{
    int64_t remaining;

    if(deadline < 0)
        return -1;

    remaining = deadline - process_now_ms();

    return remaining > 0 ? (int)remaining : 0;
}

/*
 * Pipes are created close-on-exec, and creating them and forking happen under a lock: otherwise a
 * process started by another thread at the same time inherits the write end of our pipes, and our
 * reads only see the end of file when that other process exits
 */
static pthread_mutex_t g_spawn_mutex = PTHREAD_MUTEX_INITIALIZER;

static int process_pipe(int fds[2])
{
    if(pipe(fds) != 0)
        return -1;

    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);

    return 0;
}

static void process_child_fail(int fd)
{
    int err = errno;
    ssize_t written;

    written = write(fd, &err, sizeof(int));
    ROMANO_UNUSED(written);
    _exit(127);
}

bool process_run(const ProcessOptions* options, ProcessResult* result)
{
    int out_pipe[2] = { -1, -1 };
    int err_pipe[2] = { -1, -1 };
    int exec_pipe[2] = { -1, -1 };
    bool merge;
    bool capture_out;
    bool capture_err;
    ProcessBuffer out_buffer;
    ProcessBuffer err_buffer;
    struct pollfd fds[2];
    char chunk[16384];
    int child_errno;
    int status;
    ssize_t n;
    pid_t pid;
    int64_t deadline;

    ROMANO_ASSERT(options != NULL && options->argv != NULL && options->argv[0] != NULL, "invalid argv");
    ROMANO_ASSERT(result != NULL, "result is NULL");

    memset(result, 0, sizeof(ProcessResult));
    memset(&out_buffer, 0, sizeof(ProcessBuffer));
    memset(&err_buffer, 0, sizeof(ProcessBuffer));

    merge = (options->flags & ProcessFlag_MergeStderr) != 0;
    capture_out = (options->flags & ProcessFlag_CaptureStdout) != 0 || merge;
    capture_err = (options->flags & ProcessFlag_CaptureStderr) != 0 && !merge;

    pthread_mutex_lock(&g_spawn_mutex);

    if((capture_out && process_pipe(out_pipe) != 0) ||
       (capture_err && process_pipe(err_pipe) != 0) ||
       process_pipe(exec_pipe) != 0)
    {
        g_current_error = (ErrorCode)errno;
        pthread_mutex_unlock(&g_spawn_mutex);
        goto fail;
    }

    pid = fork();

    if(pid < 0)
    {
        g_current_error = (ErrorCode)errno;
        pthread_mutex_unlock(&g_spawn_mutex);
        goto fail;
    }

    if(pid == 0)
    {
        close(exec_pipe[0]);

        if(options->timeout_ms > 0)
            setpgid(0, 0);

        if(options->cwd != NULL && chdir(options->cwd) != 0)
            process_child_fail(exec_pipe[1]);

        if(capture_out)
        {
            dup2(out_pipe[1], STDOUT_FILENO);

            if(merge)
                dup2(out_pipe[1], STDERR_FILENO);

            close(out_pipe[0]);
            close(out_pipe[1]);
        }

        if(capture_err)
        {
            dup2(err_pipe[1], STDERR_FILENO);
            close(err_pipe[0]);
            close(err_pipe[1]);
        }

        if(options->env != NULL)
        {
#if defined(ROMANO_APPLE)
            *_NSGetEnviron() = (char**)options->env;
#else
            environ = (char**)options->env;
#endif /* defined(ROMANO_APPLE) */
        }

        execvp(options->argv[0], (char* const*)options->argv);
        process_child_fail(exec_pipe[1]);
    }

    process_close_fd(&exec_pipe[1]);
    process_close_fd(&out_pipe[1]);
    process_close_fd(&err_pipe[1]);
    pthread_mutex_unlock(&g_spawn_mutex);

    if(read(exec_pipe[0], &child_errno, sizeof(int)) == (ssize_t)sizeof(int))
    {
        waitpid(pid, &status, 0);
        g_current_error = (ErrorCode)child_errno;
        goto fail;
    }

    process_close_fd(&exec_pipe[0]);

    deadline = options->timeout_ms > 0 ? process_now_ms() + (int64_t)options->timeout_ms : -1;

    while(out_pipe[0] >= 0 || err_pipe[0] >= 0)
    {
        int poll_result;

        nfds_t count = 0;
        nfds_t i;

        if(out_pipe[0] >= 0)
        {
            fds[count].fd = out_pipe[0];
            fds[count].events = POLLIN;
            count++;
        }

        if(err_pipe[0] >= 0)
        {
            fds[count].fd = err_pipe[0];
            fds[count].events = POLLIN;
            count++;
        }

        poll_result = poll(fds, count, result->timed_out ? 100 : process_remaining_ms(deadline));

        if(poll_result < 0)
        {
            if(errno == EINTR)
                continue;

            break;
        }

        if(poll_result == 0)
        {
            if(result->timed_out)
                break;

            result->timed_out = true;
            kill(-pid, SIGKILL);
            kill(pid, SIGKILL);
            continue;
        }

        for(i = 0; i < count; i++)
        {
            bool is_out = fds[i].fd == out_pipe[0];

            if((fds[i].revents & (POLLIN | POLLHUP | POLLERR)) == 0)
                continue;

            n = read(fds[i].fd, chunk, sizeof(chunk));

            if(n > 0)
            {
                process_buffer_append(is_out ? &out_buffer : &err_buffer, chunk, (size_t)n);
            }
            else if(n == 0 || errno != EINTR)
            {
                process_close_fd(is_out ? &out_pipe[0] : &err_pipe[0]);
            }
        }
    }

    while(true)
    {
        pid_t waited = waitpid(pid, &status, deadline >= 0 && !result->timed_out ? WNOHANG : 0);

        if(waited == pid)
            break;

        if(waited < 0)
        {
            if(errno == EINTR)
                continue;

            g_current_error = (ErrorCode)errno;
            goto fail;
        }

        if(process_remaining_ms(deadline) == 0)
        {
            result->timed_out = true;
            kill(-pid, SIGKILL);
            kill(pid, SIGKILL);
            continue;
        }

        {
            struct timespec pause = { 0, 1000000 };
            nanosleep(&pause, NULL);
        }
    }

    if(WIFEXITED(status))
    {
        result->exit_code = WEXITSTATUS(status);
    }
    else if(WIFSIGNALED(status))
    {
        result->signal = WTERMSIG(status);
        result->exit_code = 128 + result->signal;
    }

    if(capture_out)
        process_buffer_finish(&out_buffer, &result->out, &result->out_sz);

    if(capture_err)
        process_buffer_finish(&err_buffer, &result->err, &result->err_sz);

    return true;

fail:
    process_close_fd(&out_pipe[0]);
    process_close_fd(&out_pipe[1]);
    process_close_fd(&err_pipe[0]);
    process_close_fd(&err_pipe[1]);
    process_close_fd(&exec_pipe[0]);
    process_close_fd(&exec_pipe[1]);
    romano_free(out_buffer.data);
    romano_free(err_buffer.data);

    return false;
}

#elif defined(ROMANO_WIN)

typedef struct ProcessReader {
    HANDLE pipe;
    ProcessBuffer buffer;
} ProcessReader;

static DWORD WINAPI process_reader_thread(LPVOID param)
{
    ProcessReader* reader = (ProcessReader*)param;
    char chunk[16384];
    DWORD n;

    while(ReadFile(reader->pipe, chunk, sizeof(chunk), &n, NULL) && n > 0)
        process_buffer_append(&reader->buffer, chunk, (size_t)n);

    return 0;
}

/* https://learn.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments */
static void process_quote_arg(ProcessBuffer* cmd, const char* arg)
{
    size_t backslashes;
    const char* c;

    if(*arg != '\0' && strpbrk(arg, " \t\n\v\"") == NULL)
    {
        process_buffer_append(cmd, arg, strlen(arg));
        return;
    }

    process_buffer_append(cmd, "\"", 1);

    for(c = arg; ; c++)
    {
        backslashes = 0;

        while(*c == '\\')
        {
            backslashes++;
            c++;
        }

        if(*c == '\0')
        {
            while(backslashes-- > 0)
                process_buffer_append(cmd, "\\\\", 2);

            break;
        }

        if(*c == '"')
        {
            while(backslashes-- > 0)
                process_buffer_append(cmd, "\\\\", 2);

            process_buffer_append(cmd, "\\\"", 2);
        }
        else
        {
            while(backslashes-- > 0)
                process_buffer_append(cmd, "\\", 1);

            process_buffer_append(cmd, c, 1);
        }
    }

    process_buffer_append(cmd, "\"", 1);
}

/* Inheritable pipe ends must not leak into a process created by another thread, see the POSIX version */
static SRWLOCK g_spawn_lock = SRWLOCK_INIT;

static bool process_create_pipe(HANDLE* read_end, HANDLE* write_end)
{
    SECURITY_ATTRIBUTES sa;

    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if(!CreatePipe(read_end, write_end, &sa, 0))
        return false;

    SetHandleInformation(*read_end, HANDLE_FLAG_INHERIT, 0);

    return true;
}

bool process_run(const ProcessOptions* options, ProcessResult* result)
{
    ProcessBuffer cmd;
    ProcessBuffer env_block;
    ProcessReader out_reader;
    ProcessReader err_reader;
    HANDLE out_write = NULL;
    HANDLE err_write = NULL;
    HANDLE threads[2];
    DWORD thread_count = 0;
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD exit_code;
    bool merge;
    bool capture_out;
    bool capture_err;
    bool ok;
    size_t i;

    ROMANO_ASSERT(options != NULL && options->argv != NULL && options->argv[0] != NULL, "invalid argv");
    ROMANO_ASSERT(result != NULL, "result is NULL");

    memset(result, 0, sizeof(ProcessResult));
    memset(&cmd, 0, sizeof(ProcessBuffer));
    memset(&env_block, 0, sizeof(ProcessBuffer));
    memset(&out_reader, 0, sizeof(ProcessReader));
    memset(&err_reader, 0, sizeof(ProcessReader));
    memset(&si, 0, sizeof(STARTUPINFOA));
    memset(&pi, 0, sizeof(PROCESS_INFORMATION));

    merge = (options->flags & ProcessFlag_MergeStderr) != 0;
    capture_out = (options->flags & ProcessFlag_CaptureStdout) != 0 || merge;
    capture_err = (options->flags & ProcessFlag_CaptureStderr) != 0 && !merge;

    for(i = 0; options->argv[i] != NULL; i++)
    {
        if(i > 0)
            process_buffer_append(&cmd, " ", 1);

        process_quote_arg(&cmd, options->argv[i]);
    }

    if(options->env != NULL)
    {
        for(i = 0; options->env[i] != NULL; i++)
            process_buffer_append(&env_block, options->env[i], strlen(options->env[i]) + 1);

        process_buffer_append(&env_block, "\0", 1);
    }

    si.cb = sizeof(STARTUPINFOA);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    AcquireSRWLockExclusive(&g_spawn_lock);

    if(capture_out)
    {
        if(!process_create_pipe(&out_reader.pipe, &out_write))
        {
            ReleaseSRWLockExclusive(&g_spawn_lock);
            goto fail;
        }

        si.hStdOutput = out_write;

        if(merge)
            si.hStdError = out_write;
    }

    if(capture_err)
    {
        if(!process_create_pipe(&err_reader.pipe, &err_write))
        {
            ReleaseSRWLockExclusive(&g_spawn_lock);
            goto fail;
        }

        si.hStdError = err_write;
    }

    ok = CreateProcessA(NULL,
                        cmd.data,
                        NULL,
                        NULL,
                        TRUE,
                        0,
                        options->env != NULL ? env_block.data : NULL,
                        options->cwd,
                        &si,
                        &pi);

    if(out_write != NULL)
        CloseHandle(out_write);

    if(err_write != NULL)
        CloseHandle(err_write);

    out_write = NULL;
    err_write = NULL;
    ReleaseSRWLockExclusive(&g_spawn_lock);

    if(!ok)
        goto fail;

    if(capture_out)
        threads[thread_count++] = CreateThread(NULL, 0, process_reader_thread, &out_reader, 0, NULL);

    if(capture_err)
        threads[thread_count++] = CreateThread(NULL, 0, process_reader_thread, &err_reader, 0, NULL);

    if(WaitForSingleObject(pi.hProcess, options->timeout_ms > 0 ? (DWORD)options->timeout_ms : INFINITE) == WAIT_TIMEOUT)
    {
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, INFINITE);
        result->timed_out = true;
    }

    if(thread_count > 0)
        WaitForMultipleObjects(thread_count, threads, TRUE, INFINITE);

    for(i = 0; i < thread_count; i++)
        CloseHandle(threads[i]);

    GetExitCodeProcess(pi.hProcess, &exit_code);
    result->exit_code = (int)exit_code;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if(capture_out)
    {
        CloseHandle(out_reader.pipe);
        process_buffer_finish(&out_reader.buffer, &result->out, &result->out_sz);
    }

    if(capture_err)
    {
        CloseHandle(err_reader.pipe);
        process_buffer_finish(&err_reader.buffer, &result->err, &result->err_sz);
    }

    romano_free(cmd.data);
    romano_free(env_block.data);

    return true;

fail:
    g_current_error = (ErrorCode)GetLastError();

    if(out_reader.pipe != NULL)
        CloseHandle(out_reader.pipe);

    if(err_reader.pipe != NULL)
        CloseHandle(err_reader.pipe);

    if(out_write != NULL)
        CloseHandle(out_write);

    if(err_write != NULL)
        CloseHandle(err_write);

    romano_free(cmd.data);
    romano_free(env_block.data);

    return false;
}

#else
#error "Unsupported platform"
#endif /* defined(ROMANO_LINUX) || defined(ROMANO_APPLE) */

void process_result_release(ProcessResult* result)
{
    if(result == NULL)
        return;

    romano_free(result->out);
    romano_free(result->err);
    memset(result, 0, sizeof(ProcessResult));
}

const char* process_signal_name(int signal)
{
    switch(signal)
    {
        case 1: return "SIGHUP";
        case 2: return "SIGINT";
        case 3: return "SIGQUIT";
        case 4: return "SIGILL";
        case 5: return "SIGTRAP";
        case 6: return "SIGABRT";
        case 8: return "SIGFPE";
        case 9: return "SIGKILL";
        case 11: return "SIGSEGV";
        case 13: return "SIGPIPE";
        case 14: return "SIGALRM";
        case 15: return "SIGTERM";
#if defined(ROMANO_APPLE)
        case 10: return "SIGBUS";
#else
        case 7: return "SIGBUS";
#endif /* defined(ROMANO_APPLE) */
        default: return "UNKNOWN";
    }
}
