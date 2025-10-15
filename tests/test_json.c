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

    const char* json_str = "{\"menu\": {"
"\"id\": \"file\","
"\"value\": \"File\","
"\"popup\": {"
"\"menuitem\": ["
"{\"value\": \"New\", \"onclick\": \"CreateNewDoc()\"},"
"{\"value\": \"Open\", \"onclick\": \"OpenDoc()\"},"
"{\"value\": \"Close\", \"onclick\": \"CloseDoc()\"}"
"]"
"}"
"}}";

    Json* doc = json_loads(json_str, strlen(json_str));

    if(doc == NULL)
    {
        logger_log_error("Error while parsing json string");
        return 1;
    }

    size_t json_dump_size = 0;
    char* json_dump = json_dumps(doc, 2, &json_dump_size);

    if(json_dump == NULL)
    {
        logger_log_error("Error while dumping json");
        return 1;
    }

    logger_log_info("Json doc: %s", json_dump);

    json_free(doc);

    logger_log_info("Finished Json test");

    logger_release();

    return 0;
}