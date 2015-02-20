#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lssdp.h"

int log_callback(const char * file, const char * tag, const char * level, int line, const char * func, const char * message) {
    printf("[%s][%s] %s: %s", level, tag, func, message);
    return 0;
}

int test_get_interface_list() {
    puts("\n02. lssdp_get_interface_list");

    lssdp_interface interface[16] = {};
    int ret = lssdp_get_interface_list(interface, 16);
    if (ret != 0) {
        return -1;
    }

    int i;
    for (i = 0; i < 16 && strlen(interface[i].name) > 0; i++) {
        printf("%s : %d.%d.%d.%d\n",
            interface[i].name,
            interface[i].ip[0],
            interface[i].ip[1],
            interface[i].ip[2],
            interface[i].ip[3]
        );
    }
    puts("");
    return 0;
}

int main() {
    lssdp_set_log_callback(log_callback);
    test_get_interface_list();
    return EXIT_SUCCESS;
}
