/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/json.h"
#include "libromano/logger.h"

int main(void)
{
    logger_init();

    logger_set_level(LogLevel_Debug);

    logger_log_info("Starting Json test");

    Json* doc = json_new();

    size_t json_str_size = 0;
    char* json_str = json_dumps(doc, 2, &json_str_size);

    /* TODO: fix when implemented */
    if(json_str == NULL)
        return 0;

    logger_log_info("Json doc: %s", json_str);

    json_free(doc);

    logger_log_info("Finished Json test");

    logger_release();

    return 0;
}