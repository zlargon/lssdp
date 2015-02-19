#include <stdio.h>
#include "lssdp.h"

void lssdp_hello() {
#ifdef __OSX__
    puts("hello OSX");
#else
    puts("hello");
#endif
}
