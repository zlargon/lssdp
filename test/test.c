#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lssdp.h"

int log_callback(const char * file, const char * tag, const char * level, int line, const char * func, const char * message) {
    printf("[%s][%s] %s: %s", level, tag, func, message);
    return 0;
}

int test_get_interface_list() {
    puts("\n02. lssdp_get_network_interface");

    lssdp_ctx lssdp = {};
    int ret = lssdp_get_network_interface(&lssdp);
    if (ret != 0) {
        return -1;
    }

    int i;
    for (i = 0; i < LSSDP_INTERFACE_LIST_SIZE; i++) {
        // no more network interface
        if (strlen(lssdp.interface[i].name) <= 0) {
            break;
        }

        printf("%s : %d.%d.%d.%d\n",
            lssdp.interface[i].name,
            lssdp.interface[i].ip[0],
            lssdp.interface[i].ip[1],
            lssdp.interface[i].ip[2],
            lssdp.interface[i].ip[3]
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
