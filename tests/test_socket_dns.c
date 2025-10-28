/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/socket.h"
#include "libromano/logger.h"

int main(void)
{
    logger_init();
    logger_set_level(LogLevel_Debug);

    if(!socket_context_init())
    {
        logger_log_error("Cannot initialize socket context");
        return 1;
    }

    const char* host = "google.com";

    DNSResolveResult res;
    socket_dns_result_init(&res);

    if(!socket_resolve_dns_ipv4(host, &res))
    {
        logger_log_error("Cannot resolve host: %s", host);
        return 1;
    }

    char addr_buffer[INET6_ADDRSTRLEN];

    for(size_t i = 0; i < socket_dns_result_get_count(&res); i++)
    {
        socket_addr_to_string(socket_dns_result_get(&res, i), addr_buffer, INET6_ADDRSTRLEN);

        logger_log_debug("Found addr: %s (host: %s)", addr_buffer, host);
    }

    socket_dns_result_release(&res);

    socket_context_release();

    logger_release();

    return 0;
}
