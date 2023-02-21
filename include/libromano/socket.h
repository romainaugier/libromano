// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2023 - Present Romain Augier
// All rights reserved.

#pragma once

#if !defined(__LIBROMANO_SOCKET)
#define __LIBROMANO_SOCKET

#include "libromano/libromano.h"

ROMANO_CPP_ENTER

// Initialize the socket context
ROMANO_API void socket_init();

// Release the socket context
ROMANO_API void socket_release();

ROMANO_CPP_END

#endif // !defined(__LIBROMANO_SOCKET)