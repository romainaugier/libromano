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
    if(!http_request_init(&req, HTTPVersion_1_1, HTTPMethod_GET, "/get"))
    {
        logger_log_error("Error while preparing http request (%d)",
                         error_get_last());
        return 1;
    }

    http_header_add_entry(&req.headers, "User-Agent", 10, "zclient", 7);
    http_header_add_entry(&req.headers, "Accept", 6, "*/*", 3);

    HTTPResponse resp;
    if(!http_response_init(&resp))
    {
        logger_log_error("Error while preparing http response (%d)",
                         error_get_last());
        return 1;
    }

    if(!http_context_send_request(&ctx, &req, &resp))
    {
        logger_log_error("Error while sending http request (%d)",
                         error_get_last());
        return 1;
    }

    HTTPHeaderIterator resp_iterator;
    http_header_iterator_init(&resp_iterator);

    logger_log_debug("HTTP Response headers:");

    HTTPHeaderEntry* resp_entry;
    while((resp_entry = http_header_get_next(&resp.headers, &resp_iterator)))
    {
        logger_log_debug("%s: %s", resp_entry->key, resp_entry->value);
    }

    logger_log_debug("HTTP Response content:");

    logger_log_debug("\n%.*s", (int)resp.content_sz, resp.content);

    http_request_release(&req);

    http_response_release(&resp);

    http_context_release(&ctx);

    socket_context_release();

    logger_release();

    return 0;
}
