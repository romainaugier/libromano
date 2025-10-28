/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/http.h"
#include "libromano/logger.h"
#include "libromano/error.h"

int main(void)
{
    logger_init();
    logger_set_level(LogLevel_Debug);

    if(!socket_context_init())
    {
        logger_log_error("Cannot initialize socket context (%d)",
                         error_get_last());
        return 1;
    }

    HTTPContext ctx;

    if(!http_context_init(&ctx, "httpbin.org", 80))
    {
        logger_log_error("Cannot initialize http context (%d)",
                         error_get_last());
        return 1;
    }

    HTTPRequest req;
    http_request_init(&req, HTTPVersion_1_1, HTTPMethod_GET, "/get");

    http_header_add_entry(&req.headers, "User-Agent", "zclient");
    http_header_add_entry(&req.headers, "Accept", "*/*");

    HTTPResponse resp;
    if(!http_context_send_request(&ctx, &req, &resp))
    {
        logger_log_error("Cannot send http request (%d)",
                         error_get_last());
        return 1;
    }

    http_context_release(&ctx);

    socket_context_release();

    logger_release();

    return 0;
}
