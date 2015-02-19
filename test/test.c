#include <stdio.h>
#include <stdlib.h>
#include "lssdp.h"

int log_callback(const char * file, const char * tag, const char * level, int line, const char * func, const char * message) {
    printf("[%s][%s] %s: %s", level, tag, func, message);
    return 0;
}

int main() {
    lssdp_set_log_callback(log_callback);
    lssdp_hello();
    return EXIT_SUCCESS;
}
