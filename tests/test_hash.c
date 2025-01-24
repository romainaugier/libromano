/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright (c) 2023 - Present Romain Augier */
/* All rights reserved. */

#include "libromano/hash.h"
#include "libromano/random.h"

#define ROMANO_ENABLE_PROFILING
#include "libromano/profiling.h"

#include <stdio.h>
#include <string.h>


static const char* text_to_hash = "Lorem ipsum dolor sit amet, consectetur adipiscing"
"elit. Phasellus in risus finibus, consectetur augue eu, mattis urna. In feugiat vel"
"urna pharetra molestie. Praesent eleifend, arcu a aliquet euismod, velit massa"
"porttitor turpis, et dignissim ipsum risus ut diam. Vestibulum dapibus molestie"
"malesuada. Suspendisse potenti. Aenean et urna lorem. Phasellus et ligula eget"
"est ultricies dapibus ac sit amet enim. Nunc ut mauris nibh. Vivamus elementum"
"sem lacus, eu vulputate arcu varius sed. Nam commodo tincidunt sem convallis"
"consequat. Aenean eleifend arcu quis condimentum rhoncus. Cras justo risus,"
"porta at dictum hendrerit, tincidunt eget ante. Donec lacinia fringilla varius."
"Sed dictum metus nec est egestas porta. Mauris vitae lacinia orci. Etiam pulvinar,"
"enim a accumsan ullamcorper, dui neque efficitur dui, sit amet blandit augue sem"
"pulvinar velit. Mauris venenatis elit ut nunc dapibus, sit amet interdum velit"
"dapibus. Maecenas condimentum sed mauris ut commodo. Praesent aliquam arcu sit"
"amet tellus donec.";


int main(void)
{
    const size_t text_len = strlen(text_to_hash);
    
    PROFILE_NS(hash_fnv1a(text_to_hash, text_len));
    PROFILE_NS(hash_fnv1a_pippip(text_to_hash, text_len));
    PROFILE_NS(hash_murmur3((const void*)text_to_hash, text_len, random_next_uint32()));

    return 0;
}

