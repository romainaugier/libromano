#include "romanolib/str.h"
#include <stdio.h>
#include <assert.h>

int main(int argc, char* argv[])
{
    const char t[] = "test string";
    
    str s = str_new(t);

    const size_t len_t = strlen(t);
    const size_t len_s = str_len(s);
    printf("Len t : %d, len s : %d\n", len_t, len_s);

    str_free(s);
    
    return 0;
}