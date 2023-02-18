#ifndef __ROMANOLIB
#define __ROMANOLIB

#ifdef _MSC_VER
#define ROMANO_MSVC 1
#elif __GNUC__
#define ROMANO_GCC 1
#endif

#ifndef ROMANO_VERSION_STR
#define ROMANO_VERSION_STR "Debug"
#endif

#include <stdint.h>

#if INTPTR_MAX == INT64_MAX
#define ROMANO_X64 1
#elif INTPTR_MAX == INT32_MAX
#define ROMANO_X86 1
#endif

#ifndef ROMANO_PLATFORM_STR
#ifdef _WIN32
#define ROMANO_WIN 1
#ifdef ROMANO_X64
#define ROMANO_PLATFORM_STR "WIN64"
#else
#define ROMANO_PLATFORM_STR "WIN32"
#endif
#elif __linux__
#define ROMANO_LINUX 1
#ifdef ROMANO_X64
#define ROMANO_PLATFORM_STR "LINUX64"
#else
#define ROMANO_PLATFORM_STR "LINUX32"
#endif
#endif
#endif

#define ROMANO_DLL_EXPORT __declspec(dllexport)
#define ROMANO_DLL_IMPORT __declspec(dllimport)

#ifdef ROMANO_BUILD_DLL
#define ROMANO_API ROMANO_DLL_EXPORT
#else
#define ROMANO_API ROMANO_DLL_IMPORT
#endif

#endif // __ROMANOLIB