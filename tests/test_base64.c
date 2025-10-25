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

    const char* const data[5] = {
        "light work.",
        "light work",
        "light wor",
        "light wo",
        "light w",
    };

    const char* const data_result[5] = {
        "bGlnaHQgd29yay4=",
        "bGlnaHQgd29yaw==",
        "bGlnaHQgd29y",
        "bGlnaHQgd28=",
        "bGlnaHQgdw==",
    };

    for(size_t i = 0; i < 5; i++)
    {
        size_t encoded_sz;

        const char* data_to_encode = data[i];

        char* encoded = base64_encode((void*)data_to_encode, strlen(data_to_encode), &encoded_sz);

        if(encoded == NULL)
        {
            logger_log_error("Error while encoding using base64");
            return 1;
        }

        logger_log_debug("base64 encoding: %s -> %.*s", data_to_encode, encoded_sz, encoded);

        ROMANO_ASSERT(strncmp(encoded, data_result[i], encoded_sz) == 0, "Base64 encoding is wrong");

        size_t decoded_sz;

        char* decoded = (char*)base64_decode(encoded, encoded_sz, &decoded_sz);

        if(decoded == NULL)
        {
            logger_log_error("Error while decoding using base64");
            return 1;
        }

        logger_log_debug("base64 decoding: %.*s -> %.*s",
                         encoded_sz,
                         encoded,
                         decoded_sz,
                         decoded);

        ROMANO_ASSERT(strncmp(decoded, data_to_encode, decoded_sz) == 0, "Base64 decoding is wrong");

        free(encoded);
        free(decoded);
    }

    logger_release();

    return 0;
}
