/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/os.h"
#include "libromano/env.h"

#include <string.h>
#include <stdlib.h>

#if defined(ROMANO_WIN)
#include <Windows.h>
#include <io.h>
#elif defined(ROMANO_LINUX) || defined(ROMANO_APPLE) || defined(ROMANO_BSD)
#include <unistd.h>
#include <sys/utsname.h>
#include <pwd.h>
#if defined(ROMANO_APPLE)
#include <sys/sysctl.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#elif defined(ROMANO_LINUX)
#include <sys/sysinfo.h>
#endif /* defined(ROMANO_APPLE) */
#endif /* defined(ROMANO_WIN) */

static bool os_copy_str(char* buffer, size_t buffer_sz, const char* str)
{
    size_t sz;

    if(str == NULL)
        return false;

    sz = strlen(str);

    if(sz + 1 > buffer_sz)
        return false;

    memcpy(buffer, str, sz + 1);

    return true;
}

OSKind os_kind(void)
{
#if defined(ROMANO_WIN)
    return OSKind_Windows;
#elif defined(ROMANO_APPLE)
    return OSKind_MacOS;
#elif defined(ROMANO_LINUX)
    return OSKind_Linux;
#else
    return OSKind_BSD;
#endif /* defined(ROMANO_WIN) */
}

OSArch os_arch(void)
{
#if defined(ROMANO_X86_64)
    return OSArch_X86_64;
#elif defined(ROMANO_X86)
    return OSArch_X86;
#elif defined(ROMANO_AARCH64)
    return OSArch_Aarch64;
#else
    return OSArch_Arm;
#endif /* defined(ROMANO_X86_64) */
}

const char* os_kind_str(OSKind kind)
{
    switch(kind)
    {
        case OSKind_Linux: return "linux";
        case OSKind_Windows: return "windows";
        case OSKind_MacOS: return "macos";
        case OSKind_BSD: return "bsd";
    }

    return "unknown";
}

const char* os_arch_str(OSArch arch)
{
    switch(arch)
    {
        case OSArch_X86_64: return "x86_64";
        case OSArch_X86: return "x86";
        case OSArch_Aarch64: return "aarch64";
        case OSArch_Arm: return "arm";
    }

    return "unknown";
}

bool os_version(char* buffer, size_t buffer_sz)
{
#if defined(ROMANO_WIN)
    typedef LONG (WINAPI* RtlGetVersionFunc)(OSVERSIONINFOW*);
    RtlGetVersionFunc rtl_get_version;
    OSVERSIONINFOW info;
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");

    if(ntdll == NULL)
        return false;

    rtl_get_version = (RtlGetVersionFunc)(void*)GetProcAddress(ntdll, "RtlGetVersion");

    if(rtl_get_version == NULL)
        return false;

    memset(&info, 0, sizeof(OSVERSIONINFOW));
    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);

    if(rtl_get_version(&info) != 0)
        return false;

    return snprintf(buffer,
                    buffer_sz,
                    "%lu.%lu.%lu",
                    info.dwMajorVersion,
                    info.dwMinorVersion,
                    info.dwBuildNumber) < (int)buffer_sz;
#elif defined(ROMANO_APPLE)
    size_t sz = buffer_sz;

    return sysctlbyname("kern.osproductversion", buffer, &sz, NULL, 0) == 0;
#else
    struct utsname name;

    if(uname(&name) != 0)
        return false;

    return os_copy_str(buffer, buffer_sz, name.release);
#endif /* defined(ROMANO_WIN) */
}

bool os_hostname(char* buffer, size_t buffer_sz)
{
#if defined(ROMANO_WIN)
    DWORD sz = (DWORD)buffer_sz;

    return GetComputerNameA(buffer, &sz) != 0;
#else
    if(gethostname(buffer, buffer_sz) != 0)
        return false;

    buffer[buffer_sz - 1] = '\0';

    return true;
#endif /* defined(ROMANO_WIN) */
}

bool os_exe_path(char* buffer, size_t buffer_sz)
{
#if defined(ROMANO_WIN)
    DWORD sz = GetModuleFileNameA(NULL, buffer, (DWORD)buffer_sz);

    return sz > 0 && sz < buffer_sz;
#elif defined(ROMANO_APPLE)
    uint32_t sz = (uint32_t)buffer_sz;

    return _NSGetExecutablePath(buffer, &sz) == 0;
#elif defined(ROMANO_LINUX)
    ssize_t sz = readlink("/proc/self/exe", buffer, buffer_sz - 1);

    if(sz <= 0)
        return false;

    buffer[sz] = '\0';

    return true;
#else
    return false;
#endif /* defined(ROMANO_WIN) */
}

bool os_home_dir(char* buffer, size_t buffer_sz)
{
#if defined(ROMANO_WIN)
    return os_copy_str(buffer, buffer_sz, env_get("USERPROFILE"));
#else
    const char* home = env_get("HOME");
    struct passwd* pw;

    if(home != NULL)
        return os_copy_str(buffer, buffer_sz, home);

    pw = getpwuid(getuid());

    return pw != NULL && os_copy_str(buffer, buffer_sz, pw->pw_dir);
#endif /* defined(ROMANO_WIN) */
}

bool os_temp_dir(char* buffer, size_t buffer_sz)
{
#if defined(ROMANO_WIN)
    DWORD sz = GetTempPathA((DWORD)buffer_sz, buffer);

    if(sz == 0 || sz >= buffer_sz)
        return false;

    if(sz > 1 && (buffer[sz - 1] == '\\' || buffer[sz - 1] == '/'))
        buffer[sz - 1] = '\0';

    return true;
#else
    const char* tmp = env_get("TMPDIR");

    return os_copy_str(buffer, buffer_sz, tmp != NULL ? tmp : "/tmp");
#endif /* defined(ROMANO_WIN) */
}

uint64_t os_memory_total(void)
{
#if defined(ROMANO_WIN)
    MEMORYSTATUSEX status;

    status.dwLength = sizeof(MEMORYSTATUSEX);

    return GlobalMemoryStatusEx(&status) ? (uint64_t)status.ullTotalPhys : 0;
#elif defined(ROMANO_APPLE)
    uint64_t mem = 0;
    size_t sz = sizeof(uint64_t);

    return sysctlbyname("hw.memsize", &mem, &sz, NULL, 0) == 0 ? mem : 0;
#elif defined(ROMANO_LINUX)
    struct sysinfo info;

    return sysinfo(&info) == 0 ? (uint64_t)info.totalram * info.mem_unit : 0;
#else
    return 0;
#endif /* defined(ROMANO_WIN) */
}

uint64_t os_memory_available(void)
{
#if defined(ROMANO_WIN)
    MEMORYSTATUSEX status;

    status.dwLength = sizeof(MEMORYSTATUSEX);

    return GlobalMemoryStatusEx(&status) ? (uint64_t)status.ullAvailPhys : 0;
#elif defined(ROMANO_APPLE)
    vm_statistics64_data_t stats;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    vm_size_t page_size;

    if(host_page_size(mach_host_self(), &page_size) != KERN_SUCCESS ||
       host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&stats, &count) != KERN_SUCCESS)
        return 0;

    return ((uint64_t)stats.free_count + stats.inactive_count) * page_size;
#elif defined(ROMANO_LINUX)
    FILE* meminfo = fopen("/proc/meminfo", "r");
    char line[256];
    unsigned long long kb = 0;

    if(meminfo == NULL)
        return 0;

    while(fgets(line, sizeof(line), meminfo) != NULL)
    {
        if(sscanf(line, "MemAvailable: %llu kB", &kb) == 1)
            break;
    }

    fclose(meminfo);

    return (uint64_t)kb * 1024;
#else
    return 0;
#endif /* defined(ROMANO_WIN) */
}

bool os_is_tty(int fd)
{
#if defined(ROMANO_WIN)
    return _isatty(fd) != 0;
#else
    return isatty(fd) != 0;
#endif /* defined(ROMANO_WIN) */
}
