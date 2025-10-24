/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/base64.h"
#include "libromano/logger.h"

#include <string.h>

int main(void)
{
    logger_init();
    logger_set_level(LogLevel_Debug);

    const char* data = "Ma";

    size_t encoded_sz;
    char* encoded = base64_encode((void*)data, strlen(data), &encoded_sz);

    if(encoded == NULL)
    {
        logger_log_error("Error while encoding using base64");
        return 1;
    }

    logger_log_debug("base64 encoding: %s -> %.*s", data, encoded_sz, encoded);

    free(encoded);

    logger_release();

    return 0;
}
