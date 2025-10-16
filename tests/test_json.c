/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/json.h"
#include "libromano/logger.h"
#include "libromano/filesystem.h"
#include "libromano/string.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

int main(void)
{
    logger_init();

    logger_set_level(LogLevel_Debug);

    logger_log_info("Starting Json test");

    if(!fs_path_exists(TESTS_DATA_DIR))
    {
        logger_log_warning("Can't find tests data dir, silently failing Json test");
        return 0;
    }

    const char* const jsons[3] = {
        "canada.json",
        "twitter.json",
        "citm_catalog.json"
    };

    for(size_t i = 0; i < 3; i++)
    {
        String json_path = string_newf("%s/json/%s", TESTS_DATA_DIR, jsons[i]);

        if(json_path == NULL)
        {
            logger_log_error("Error while formatting file path");
            return 1;
        }

        logger_log_info("Loading: %s", json_path);

        FileContent* content = fs_file_content_new(json_path, false);

        SCOPED_PROFILE_MS_START(json_loads);
        Json* doc = json_loads(content->content, content->content_sz);
        SCOPED_PROFILE_MS_END(json_loads);

        if(doc == NULL)
        {
            logger_log_error("Error while parsing json from file: %s", json_path);
            fs_file_content_free(content);
            return 1;
        }

        String json_out_path = string_newf("%s/json/out_%s", TESTS_DATA_DIR, jsons[i]);

        size_t dumps_size;

        SCOPED_PROFILE_MS_START(json_dumps);
        char* dumps = json_dumps(doc, 2, &dumps_size);
        SCOPED_PROFILE_MS_END(json_dumps);

        free(dumps);

        json_free(doc);

        string_free(json_path);
        string_free(json_out_path);
        
        fs_file_content_free(content);
    }

    logger_log_info("Finished Json test");

    logger_release();

    return 0;
}